

#include "NativeWebView.h"

#include "AffineUiScriptInstance.h"
#include "AffineUiSurface.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NativeWeb.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"
#include "Widgets/NativeWebViewSlate.h"

namespace
{


	FNativeWebError BuildNativeWebError(FName Code, const FString& Message, FName ViewName)
	{
		FNativeWebError Result;
		Result.Code = Code;
		Result.Message = Message;
		Result.ViewName = ViewName;
		return Result;
	}



	int64 GetQueuedMessageByteSize(const FNativeWebMessage& Message)
	{
		if (Message.Type == ENativeWebPayloadType::PAYLOAD_BYTES) return Message.Bytes.Num();


		const FTCHARToUTF8 Utf8(*Message.String, Message.String.Len());
		return Utf8.Length();
	}



	bool ResolvePackagedPath(const FString& Root, FString Relative, FString& OutPath)
	{
		if (Root.IsEmpty() || Relative.IsEmpty() || FPaths::IsRelative(Relative) == false || FCString::Strlen(*Relative)
			!= Relative.Len()) return false;
		Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (Relative.StartsWith(TEXT("/")) || Relative.Contains(TEXT(":"))) return false;
		TArray<FString> Parts;
		Relative.ParseIntoArray(Parts, TEXT("/"), true);
		for (const FString& Part : Parts) if (Part == TEXT("..") || Part == TEXT(".")) return false;
		OutPath = FPaths::Combine(Root, Relative);
		return true;
	}
}

bool UNativeWebView::Initialize(FName InName, const FNativeWebViewOptions& Options, ULocalPlayer* Player,
                                FNativeWebError& OutError)
{
	check(IsInGameThread());
	UE_LOG(LogNativeWebView, Log, TEXT("Initializing view '%s' (size=%dx%d, dpi=%.2f)."),
	       *InName.ToString(), Options.PixelSize.X, Options.PixelSize.Y, Options.DpiScale);
	ViewName = InName;
	OwningPlayer = Player;
	PackagedAssetRoot = Options.PackagedAssetRoot;
#if !PLATFORM_WINDOWS
	OutError = BuildNativeWebError(TEXT("UnsupportedPlatform"), TEXT("NativeWeb views require Win64 D3D11."), ViewName);
	UE_LOG(LogNativeWebView, Error, TEXT("View '%s' initialization rejected: %s"), *ViewName.ToString(),
	       *OutError.Message);
	return false;
#else
	FAffineUiSurfaceDesc Desc;
	Desc.PixelSize = Options.PixelSize;
	Desc.DpiScale = Options.DpiScale;
	Desc.ClearColor = Options.ClearColor;
	Desc.Content.Html = Options.Html;
	Desc.Content.Css = Options.Css;
	if (!Options.PackagedAssetRoot.IsEmpty())
		Desc.FontDirectory = FPaths::Combine(Options.PackagedAssetRoot, TEXT("fonts"));
	FString ErrorMessage;
	Script = FAffineUiScriptInstance::Create(Desc, ErrorMessage);
	if (!Script)
	{
		const FName ErrorCode = ErrorMessage.Contains(TEXT("Only one AffineUI surface"))
			                        ? FName(TEXT("RendererCapacityUnavailable"))
			                        : (ErrorMessage.Contains(TEXT("D3D11")) || ErrorMessage.Contains(TEXT("Win64")))
			                        ? FName(TEXT("UnsupportedRhi"))
			                        : ErrorMessage.Contains(TEXT("Invalid surface"))
			                        ? FName(TEXT("InvalidOptions"))
			                        : FName(TEXT("RuntimeUnavailable"));
		OutError = BuildNativeWebError(ErrorCode, ErrorMessage, ViewName);
		UE_LOG(LogNativeWebView, Error, TEXT("View '%s' runtime creation failed (code=%s): %s"),
		       *ViewName.ToString(), *ErrorCode.ToString(), *ErrorMessage);
		return false;
	}
	Surface = Script->GetSurface();
	const uint64 ThisGeneration = Generation;


	TWeakObjectPtr<UNativeWebView> WeakThis(this);
	Surface->SetOnTextureReady(FAffineUiTextureReady::CreateLambda(
		[WeakThis, ThisGeneration](UTextureRenderTarget2D* Texture)
		{
			if (UNativeWebView* View = WeakThis.Get(); View && !View->bClosed && View->Generation == ThisGeneration)
			{
				UE_LOG(LogNativeWebViewport, VeryVerbose, TEXT("View '%s' published output texture %s."),
				       *View->ViewName.ToString(), Texture ? *Texture->GetName() : TEXT("<null>"));
				View->SetOutputTexture(Texture);
			}
		}));
	Surface->SetOnError(FAffineUiSurfaceError::CreateLambda([WeakThis, ThisGeneration](const FString& Message)
	{
		if (UNativeWebView* View = WeakThis.Get(); View && !View->bClosed && View->Generation == ThisGeneration)
		{
			UE_LOG(LogNativeWebRender, Error, TEXT("View '%s' render error: %s"), *View->ViewName.ToString(), *Message);
			FNativeWebError Error = BuildNativeWebError(TEXT("RenderFailure"), Message, View->ViewName);
			View->Error.Broadcast(Error);
		}
	}));
#if PLATFORM_WINDOWS
	Surface->SetOnClick(FAffineUiClick::CreateLambda([WeakThis, ThisGeneration](uint64 NodeId)
	{
		UNativeWebView* View = WeakThis.Get();
		if (!View || View->bClosed || View->Generation != ThisGeneration || !View->Script) return;
		UE_LOG(LogNativeWebInput, VeryVerbose, TEXT("View '%s' dispatching click for node %llu."),
		       *View->ViewName.ToString(), static_cast<unsigned long long>(NodeId));
		FQuickJSError ScriptError;
		if (!View->Script->DispatchClick(NodeId, ScriptError))
		{
			UE_LOG(LogNativeWebScript, Error, TEXT("Click callback failed in view '%s': %s"),
			       *View->ViewName.ToString(), *ScriptError.Message);
			FNativeWebError Error = BuildNativeWebError(TEXT("ScriptError"), ScriptError.Message, View->ViewName);
			Error.ScriptStack = ScriptError.Stack;
			View->Error.Broadcast(Error);
		}
	}));
#endif
	{


		FScopeLock Lock(&ProducerMutex);
		bInitialized = true;
	}
	UE_LOG(LogNativeWebView, Display, TEXT("View '%s' initialized successfully."), *ViewName.ToString());
	return true;
#endif
}

void UNativeWebView::SetOutputTexture(UTextureRenderTarget2D* Texture)
{
	if (Attachment) Attachment->SetTexture(Texture);
}

bool UNativeWebView::Show()
{
	check(IsInGameThread());
	if (bClosed || !Surface)
	{
		UE_LOG(LogNativeWebView, Warning, TEXT("Cannot show view '%s': it is closed or has no surface."),
		       *ViewName.ToString());
		return false;
	}
	if (!Attachment && !AttachToPlayerViewport(0)) return false;
	bVisible = true;
	if (Attachment) Attachment->SetVisible(true);
	UE_LOG(LogNativeWebViewport, Log, TEXT("View '%s' shown."), *ViewName.ToString());
	return true;
}

void UNativeWebView::Hide()
{
	check(IsInGameThread());
	if (Attachment) Attachment->SetVisible(false);
	bVisible = false;
	UE_LOG(LogNativeWebViewport, Log, TEXT("View '%s' hidden."), *ViewName.ToString());
}

bool UNativeWebView::Resize(FIntPoint PixelSize, float DpiScale)
{
	check(IsInGameThread());
	const bool bResized = !bClosed && Surface && Surface->Resize(PixelSize, DpiScale);
	if (bResized)
	{
		UE_LOG(LogNativeWebViewport, VeryVerbose, TEXT("View '%s' resize queued: %dx%d at DPI %.2f."),
		       *ViewName.ToString(), PixelSize.X, PixelSize.Y, DpiScale);
	}
	else
	{
		UE_LOG(LogNativeWebView, Warning, TEXT("View '%s' resize rejected: %dx%d at DPI %.2f."),
		       *ViewName.ToString(), PixelSize.X, PixelSize.Y, DpiScale);
	}
	return bResized;
}

bool UNativeWebView::AttachToPlayerViewport(int32 ZOrder)
{
	check(IsInGameThread());
	if (bClosed || !Surface || Attachment) return !bClosed && Attachment.IsValid();
	ULocalPlayer* Player = OwningPlayer.Get();
	UGameViewportClient* Viewport = Player ? Player->ViewportClient : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogNativeWebViewport, Warning, TEXT("Cannot attach view '%s': no local-player viewport is available."),
		       *ViewName.ToString());
		return false;
	}
	TSharedRef<SNativeWebView> Widget = SNew(SNativeWebView).View(this).Surface(Surface);
	Attachment = Widget;
	Viewport->AddViewportWidgetContent(Widget, ZOrder);
	if (UTextureRenderTarget2D* Texture = Surface->GetOutputTexture()) Widget->SetTexture(Texture);
	Widget->SetVisible(bVisible);
	UE_LOG(LogNativeWebViewport, Log, TEXT("View '%s' attached to local-player viewport at Z=%d."),
	       *ViewName.ToString(), ZOrder);
	return true;
}

void UNativeWebView::DetachFromPlayerViewport()
{
	check(IsInGameThread());
	if (!Attachment) return;
	UE_LOG(LogNativeWebViewport, Log, TEXT("Detaching view '%s' from local-player viewport."), *ViewName.ToString());
	Attachment->SetVisible(false);
	if (ULocalPlayer* Player = OwningPlayer.Get())
		if (UGameViewportClient* Viewport = Player->ViewportClient) Viewport->RemoveViewportWidgetContent(
			Attachment.ToSharedRef());
	Attachment.Reset();
}

UTextureRenderTarget2D* UNativeWebView::GetTexture() const
{
	check(IsInGameThread());
	return Surface && !bClosed ? Surface->GetOutputTexture() : nullptr;
}

bool UNativeWebView::ForwardInput(const FNativeWebInputEvent& Event)
{
	check(IsInGameThread());
	if (bClosed || !bVisible || !Surface)
	{
		UE_LOG(LogNativeWebInput, VeryVerbose, TEXT("Input rejected for view '%s' (closed=%s visible=%s surface=%s)."),
		       *ViewName.ToString(), bClosed ? TEXT("true") : TEXT("false"),
		       bVisible ? TEXT("true") : TEXT("false"), Surface.IsValid() ? TEXT("true") : TEXT("false"));
		return false;
	}
	UE_LOG(LogNativeWebInput, VeryVerbose, TEXT("Forwarding input type=%d key=%d to view '%s'."),
	       static_cast<int32>(Event.Type), static_cast<int32>(Event.Key), *ViewName.ToString());
	FAffineUiInput Input{};
	switch (Event.Type)
	{
	case ENativeWebInputType::INPUT_POINTER_MOVE: Input.Kind = EAffineUiInputKind::MouseMove;
		break;
	case ENativeWebInputType::INPUT_POINTER_DOWN: Input.Kind = EAffineUiInputKind::MouseDown;
		break;
	case ENativeWebInputType::INPUT_POINTER_UP: Input.Kind = EAffineUiInputKind::MouseUp;
		break;
	case ENativeWebInputType::INPUT_WHEEL: Input.Kind = EAffineUiInputKind::MouseWheel;
		break;
	case ENativeWebInputType::INPUT_KEY_DOWN: Input.Kind = EAffineUiInputKind::KeyDown;
		break;
	case ENativeWebInputType::INPUT_KEY_UP: Input.Kind = EAffineUiInputKind::KeyUp;
		break;
	case ENativeWebInputType::INPUT_FOCUS_GAINED: Input.Kind = EAffineUiInputKind::FocusGained;
		break;
	case ENativeWebInputType::INPUT_FOCUS_LOST: Input.Kind = EAffineUiInputKind::FocusLost;
		break;
	}
	Input.Position = Event.NormalizedPosition;
	Input.WheelDelta = Event.WheelDelta;
	Input.bShift = Event.bShift;
	if (Event.Key == ENativeWebInputKey::INPUT_KEY_TAB) Input.Key = EAffineUiInputKey::Tab;
	else if (Event.Key == ENativeWebInputKey::INPUT_KEY_ENTER) Input.Key = EAffineUiInputKey::Enter;
	else if (Event.Key == ENativeWebInputKey::INPUT_KEY_SPACE) Input.Key = EAffineUiInputKey::Space;
	return Surface->QueueInput(Input);
}

bool UNativeWebView::SetDocument(const FString& Html, const FString& Css, FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	if (bClosed || !Surface)
	{
		OutError = BuildNativeWebError(TEXT("Closed"), TEXT("The NativeWeb view is closed."), ViewName);
		UE_LOG(LogNativeWebView, Warning, TEXT("Cannot set document on closed view '%s'."), *ViewName.ToString());
		return false;
	}
	FAffineUiContent Content;
	Content.Html = Html;
	Content.Css = Css;
	if (!Surface->SetContent(Content))
	{
		OutError = BuildNativeWebError(TEXT("InvalidDocument"), TEXT("The document could not be queued."), ViewName);
		UE_LOG(LogNativeWebView, Error, TEXT("View '%s' rejected document content (html chars=%d css chars=%d)."),
		       *ViewName.ToString(), Html.Len(), Css.Len());
		return false;
	}
#if PLATFORM_WINDOWS
	if (Script) Script->RefreshDocumentEpoch();
#endif
	UE_LOG(LogNativeWebView, Log, TEXT("Document update queued for view '%s' (html chars=%d css chars=%d)."),
	       *ViewName.ToString(), Html.Len(), Css.Len());
	return true;
}

bool UNativeWebView::EvaluateScript(const FString& Source, const FString& SourceName, FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	UE_LOG(LogNativeWebScript, VeryVerbose, TEXT("Evaluating script '%s' in view '%s' (%d chars)."),
	       *SourceName, *ViewName.ToString(), Source.Len());
#if !PLATFORM_WINDOWS
	OutError = BuildNativeWebError(TEXT("UnsupportedPlatform"), TEXT("NativeWeb JavaScript requires Win64 D3D11."),
	                               ViewName);
	UE_LOG(LogNativeWebScript, Error, TEXT("Script evaluation rejected for view '%s': %s"),
	       *ViewName.ToString(), *OutError.Message);
	return false;
#else
	if (bClosed || !Script)
	{
		OutError = BuildNativeWebError(TEXT("Closed"), TEXT("The NativeWeb view is closed."), ViewName);
		UE_LOG(LogNativeWebScript, Warning, TEXT("Cannot evaluate script in closed view '%s'."), *ViewName.ToString());
		return false;
	}
	FQuickJSError ScriptError;
	if (!Script->Evaluate(Source, ScriptError, SourceName))
	{
		OutError = BuildNativeWebError(TEXT("ScriptError"), ScriptError.Message, ViewName);
		OutError.SourceName = SourceName;
		OutError.ScriptStack = ScriptError.Stack;
		UE_LOG(LogNativeWebScript, Error, TEXT("Script '%s' failed in view '%s': %s"),
		       *SourceName, *ViewName.ToString(), *ScriptError.Message);
		Error.Broadcast(OutError);
		return false;
	}
	UE_LOG(LogNativeWebScript, VeryVerbose, TEXT("Script '%s' evaluated in view '%s'."),
	       *SourceName, *ViewName.ToString());
	return true;
#endif
}

bool UNativeWebView::LoadDocumentFiles(const FString& HtmlPath, const FString& CssPath, FNativeWebError& OutError)
{
	check(IsInGameThread());
	FString Html, Css;
	OutError = {};
	if (!FFileHelper::LoadFileToString(Html, *HtmlPath) || !FFileHelper::LoadFileToString(Css, *CssPath))
	{
		OutError = BuildNativeWebError(TEXT("AssetLoadFailure"), TEXT("Could not read HTML or CSS file."), ViewName);
		OutError.SourceName = HtmlPath;
		UE_LOG(LogNativeWebAssets, Error, TEXT("Could not read document assets for view '%s' (html='%s', css='%s')."),
		       *ViewName.ToString(), *HtmlPath, *CssPath);
		return false;
	}
	UE_LOG(LogNativeWebAssets, Log, TEXT("Loaded document assets for view '%s' (html chars=%d css chars=%d)."),
	       *ViewName.ToString(), Html.Len(), Css.Len());
	return SetDocument(Html, Css, OutError);
}

bool UNativeWebView::EvaluateScriptFile(const FString& ScriptPath, FNativeWebError& OutError)
{
	check(IsInGameThread());
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *ScriptPath))
	{
		OutError = BuildNativeWebError(TEXT("AssetLoadFailure"), TEXT("Could not read JavaScript file."), ViewName);
		OutError.SourceName = ScriptPath;
		UE_LOG(LogNativeWebAssets, Error, TEXT("Could not read JavaScript asset for view '%s': %s."),
		       *ViewName.ToString(), *ScriptPath);
		return false;
	}
	UE_LOG(LogNativeWebAssets, Log, TEXT("Loaded JavaScript asset for view '%s' (%d chars): %s."),
	       *ViewName.ToString(), Source.Len(), *ScriptPath);
	return EvaluateScript(Source, ScriptPath, OutError);
}

bool UNativeWebView::LoadPackagedDocument(const FString& HtmlRelativePath, const FString& CssRelativePath,
                                          FNativeWebError& OutError)
{
	check(IsInGameThread());
	FString HtmlPath, CssPath;
	if (!ResolvePackagedPath(PackagedAssetRoot, HtmlRelativePath, HtmlPath) || !ResolvePackagedPath(
		PackagedAssetRoot, CssRelativePath, CssPath))
	{
		OutError = BuildNativeWebError(
			TEXT("InvalidAssetPath"),
			TEXT("Packaged asset paths must be normalized relative paths inside the configured root."), ViewName);
		UE_LOG(LogNativeWebAssets, Error, TEXT("Rejected packaged document paths for view '%s' (html='%s', css='%s')."),
		       *ViewName.ToString(), *HtmlRelativePath, *CssRelativePath);
		return false;
	}
	UE_LOG(LogNativeWebAssets, Verbose, TEXT("Loading packaged document for view '%s' from root '%s'."),
	       *ViewName.ToString(), *PackagedAssetRoot);
	return LoadDocumentFiles(HtmlPath, CssPath, OutError);
}

bool UNativeWebView::EvaluatePackagedScript(const FString& ScriptRelativePath, FNativeWebError& OutError)
{
	check(IsInGameThread());
	FString ScriptPath;
	if (!ResolvePackagedPath(PackagedAssetRoot, ScriptRelativePath, ScriptPath))
	{
		OutError = BuildNativeWebError(
			TEXT("InvalidAssetPath"),
			TEXT("Packaged script paths must be normalized relative paths inside the configured root."), ViewName);
		UE_LOG(LogNativeWebAssets, Error, TEXT("Rejected packaged script path '%s' for view '%s'."),
		       *ScriptRelativePath, *ViewName.ToString());
		return false;
	}
	UE_LOG(LogNativeWebAssets, Verbose, TEXT("Evaluating packaged script for view '%s': %s."),
	       *ViewName.ToString(), *ScriptRelativePath);
	return EvaluateScriptFile(ScriptPath, OutError);
}

ENativeWebQueueResult UNativeWebView::EnqueueStringToJavaScript(FName Channel, FString Value)
{
	FNativeWebMessage Message;
	Message.Type = ENativeWebPayloadType::PAYLOAD_STRING;
	Message.Channel = Channel;
	Message.String = MoveTemp(Value);


	FScopeLock Lock(&ProducerMutex);
	if (bClosed || !bInitialized)
	{
		UE_LOG(LogNativeWebMessaging, VeryVerbose, TEXT("Rejected string message for closed/uninitialized view '%s'."),
		       *ViewName.ToString());
		return ENativeWebQueueResult::RESULT_CLOSED;
	}
	const int64 Size = GetQueuedMessageByteSize(Message);
	if (Size > 1024 * 1024 || ProducerMessages.Num() >= 64 || ProducerBytes + Size > 4 * 1024 * 1024)
	{
		UE_LOG(LogNativeWebMessaging, Verbose,
		       TEXT(
			       "String message rejected by queue limits for view '%s' (channel='%s', bytes=%lld, count=%d, queuedBytes=%lld)."
		       ),
		       *ViewName.ToString(), *Channel.ToString(), Size, ProducerMessages.Num(), ProducerBytes);
		return ENativeWebQueueResult::RESULT_QUEUE_FULL;
	}
	ProducerMessages.Add(MoveTemp(Message));
	ProducerBytes += Size;
	UE_LOG(LogNativeWebMessaging, VeryVerbose,
	       TEXT("String message queued for view '%s' (channel='%s', bytes=%lld, count=%d)."),
	       *ViewName.ToString(), *Channel.ToString(), Size, ProducerMessages.Num());
	return ENativeWebQueueResult::RESULT_QUEUED;
}

ENativeWebQueueResult UNativeWebView::EnqueueBytesToJavaScript(FName Channel, TArray<uint8>&& Value)
{
	FNativeWebMessage Message;
	Message.Type = ENativeWebPayloadType::PAYLOAD_BYTES;
	Message.Channel = Channel;
	Message.Bytes = MoveTemp(Value);


	FScopeLock Lock(&ProducerMutex);
	if (bClosed || !bInitialized)
	{
		UE_LOG(LogNativeWebMessaging, VeryVerbose, TEXT("Rejected byte message for closed/uninitialized view '%s'."),
		       *ViewName.ToString());
		return ENativeWebQueueResult::RESULT_CLOSED;
	}
	const int64 Size = Message.Bytes.Num();
	if (Size > 1024 * 1024 || ProducerMessages.Num() >= 64 || ProducerBytes + Size > 4 * 1024 * 1024)
	{
		UE_LOG(LogNativeWebMessaging, Verbose,
		       TEXT(
			       "Byte message rejected by queue limits for view '%s' (channel='%s', bytes=%lld, count=%d, queuedBytes=%lld)."
		       ),
		       *ViewName.ToString(), *Channel.ToString(), Size, ProducerMessages.Num(), ProducerBytes);
		return ENativeWebQueueResult::RESULT_QUEUE_FULL;
	}
	ProducerMessages.Add(MoveTemp(Message));
	ProducerBytes += Size;
	UE_LOG(LogNativeWebMessaging, VeryVerbose,
	       TEXT("Byte message queued for view '%s' (channel='%s', bytes=%lld, count=%d)."),
	       *ViewName.ToString(), *Channel.ToString(), Size, ProducerMessages.Num());
	return ENativeWebQueueResult::RESULT_QUEUED;
}

bool UNativeWebView::SendStringToJavaScript(FName Channel, FStringView Value)
{
	FString Copy;
	Copy.Reserve(Value.Len());
	for (TCHAR Character : Value) Copy.AppendChar(Character);
	return EnqueueStringToJavaScript(Channel, MoveTemp(Copy)) == ENativeWebQueueResult::RESULT_QUEUED;
}

bool UNativeWebView::SendBytesToJavaScript(FName Channel, TConstArrayView<uint8> Value)
{
	TArray<uint8> Copy;
	Copy.Append(Value.GetData(), Value.Num());
	return EnqueueBytesToJavaScript(Channel, MoveTemp(Copy)) == ENativeWebQueueResult::RESULT_QUEUED;
}

void UNativeWebView::Tick()
{
	check(IsInGameThread());
#if !PLATFORM_WINDOWS
	return;
#else
	if (bClosed || !Script) return;
	TArray<FNativeWebMessage> Outgoing;
	int32 DrainedFromProducer = 0;
	{


		FScopeLock Lock(&ProducerMutex);
		const int32 Count = FMath::Min(64, ProducerMessages.Num());
		DrainedFromProducer = Count;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FNativeWebMessage Message = MoveTemp(ProducerMessages[0]);
			ProducerMessages.RemoveAt(0, 1, EAllowShrinking::No);
			ProducerBytes -= GetQueuedMessageByteSize(Message);
			Outgoing.Add(MoveTemp(Message));
		}
	}
	for (const FNativeWebMessage& Message : Outgoing)
	{
		if (Script->QueueToJavaScript(Message) == ENativeWebQueueResult::RESULT_QUEUED) continue;
		FNativeWebError QueueError = BuildNativeWebError(
			TEXT("QueueFull"), TEXT("JavaScript message queue is full."), ViewName);
		UE_LOG(LogNativeWebMessaging, Warning, TEXT("View '%s' could not transfer message to QuickJS (channel='%s')."),
		       *ViewName.ToString(), *Message.Channel.ToString());
		Error.Broadcast(QueueError);
		if (bClosed) return;
	}
	Outgoing.Reset();
	const int32 ToJavaScriptCount = Script->DrainMessagesToJavaScript(Outgoing, 64);
	UE_LOG(LogNativeWebTick, VeryVerbose,
	       TEXT("View '%s' tick: producer=%d, to-JS=%d."), *ViewName.ToString(), DrainedFromProducer,
	       ToJavaScriptCount);
	for (const FNativeWebMessage& Message : Outgoing)
	{
		if (bClosed) return;
		UE_LOG(LogNativeWebMessaging, VeryVerbose,
		       TEXT("Delivering message to JS for view '%s' (channel='%s', type=%d)."),
		       *ViewName.ToString(), *Message.Channel.ToString(), static_cast<int32>(Message.Type));
		FQuickJSError JsError;
		if (!Script->DeliverMessageToJavaScript(Message, JsError))
		{
			FNativeWebError NativeError = BuildNativeWebError(TEXT("ScriptError"), JsError.Message, ViewName);
			NativeError.ScriptStack = JsError.Stack;
			UE_LOG(LogNativeWebScript, Error, TEXT("Message listener failed in view '%s': %s"),
			       *ViewName.ToString(), *JsError.Message);
			Error.Broadcast(NativeError);
			if (bClosed) return;
		}
	}
	Outgoing.Reset();
	const int32 FromJavaScriptCount = Script->DrainMessagesFromJavaScript(Outgoing, 64);
	if (FromJavaScriptCount > 0)
	{
		UE_LOG(LogNativeWebTick, Verbose, TEXT("View '%s' dispatching %d JS-to-Unreal message(s)."),
		       *ViewName.ToString(), FromJavaScriptCount);
	}
	for (const FNativeWebMessage& Message : Outgoing)
	{
		if (bClosed) return;
		UE_LOG(LogNativeWebMessaging, VeryVerbose,
		       TEXT("Broadcasting JS message for view '%s' (channel='%s', type=%d)."), *ViewName.ToString(),
		       *Message.Channel.ToString(), static_cast<int32>(Message.Type));
		MessageReceived.Broadcast(Message);
	}
	int32 Jobs = 0;
	bool bPending = false;
	FQuickJSError JobError;
	const bool bJobsSucceeded = Script->PumpJobs(256, Jobs, bPending, JobError);
	if (Jobs > 0 || bPending)
	{
		UE_LOG(LogNativeWebTick, VeryVerbose, TEXT("View '%s' QuickJS jobs: executed=%d pending=%s."),
		       *ViewName.ToString(), Jobs, bPending ? TEXT("true") : TEXT("false"));
	}
	if (!bJobsSucceeded && !JobError.Message.IsEmpty())
	{
		FNativeWebError NativeError = BuildNativeWebError(TEXT("ScriptError"), JobError.Message, ViewName);
		NativeError.ScriptStack = JobError.Stack;
		UE_LOG(LogNativeWebScript, Error, TEXT("QuickJS job failed in view '%s': %s"),
		       *ViewName.ToString(), *JobError.Message);
		Error.Broadcast(NativeError);
		if (bClosed) return;
	}
#endif
}

void UNativeWebView::Close()
{
	check(IsInGameThread());
	{


		FScopeLock Lock(&ProducerMutex);
		if (bClosed)
		{
			UE_LOG(LogNativeWebView, VeryVerbose, TEXT("Repeated Close ignored for view '%s'."), *ViewName.ToString());
			return;
		}
		bClosed = true;
		bInitialized = false;
		ProducerMessages.Reset();
		ProducerBytes = 0;
	}
	UE_LOG(LogNativeWebView, Display, TEXT("Closing NativeWeb view '%s'."), *ViewName.ToString());
	bVisible = false;
	++Generation;
	DetachFromPlayerViewport();
#if PLATFORM_WINDOWS
	if (Script)
	{
		Script->Close();
		Script.Reset();
	}
#endif
	if (Surface)
	{
		Surface->Close();
		Surface.Reset();
	}
	MessageReceived.Clear();
	Error.Clear();
	UE_LOG(LogNativeWebView, Verbose, TEXT("NativeWeb view '%s' closed."), *ViewName.ToString());
}

void UNativeWebView::BeginDestroy()
{
	if (!bClosed && IsInGameThread())
	{
		UE_LOG(LogNativeWebView, Warning, TEXT("View '%s' reached BeginDestroy without an explicit Close."),
		       *ViewName.ToString());
		Close();
	}
	Super::BeginDestroy();
}
