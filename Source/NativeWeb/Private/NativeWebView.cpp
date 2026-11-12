#include "NativeWebView.h"

#include "AffineUiScriptInstance.h"
#include "AffineUiSurface.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
	if (Message.Type == ENativeWebPayloadType::Bytes) return Message.Bytes.Num();
	const FTCHARToUTF8 Utf8(*Message.String, Message.String.Len());
	return Utf8.Length();
}

bool ResolvePackagedPath(const FString& Root, FString Relative, FString& OutPath)
{
	if (Root.IsEmpty() || Relative.IsEmpty() || FPaths::IsRelative(Relative) == false || FCString::Strlen(*Relative) != Relative.Len()) return false;
	Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (Relative.StartsWith(TEXT("/")) || Relative.Contains(TEXT(":"))) return false;
	TArray<FString> Parts;
	Relative.ParseIntoArray(Parts, TEXT("/"), true);
	for (const FString& Part : Parts) if (Part == TEXT("..") || Part == TEXT(".")) return false;
	OutPath = FPaths::Combine(Root, Relative);
	return true;
}
}

bool UNativeWebView::Initialize(FName InName, const FNativeWebViewOptions& Options, ULocalPlayer* Player, FNativeWebError& OutError)
{
	check(IsInGameThread());
	ViewName = InName;
	OwningPlayer = Player;
	PackagedAssetRoot = Options.PackagedAssetRoot;
#if !PLATFORM_WINDOWS
	OutError = BuildNativeWebError(TEXT("UnsupportedPlatform"), TEXT("NativeWeb views require Win64 D3D11."), ViewName);
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
		const FName ErrorCode = ErrorMessage.Contains(TEXT("Only one AffineUI surface")) ? FName(TEXT("RendererCapacityUnavailable")) :
			(ErrorMessage.Contains(TEXT("D3D11")) || ErrorMessage.Contains(TEXT("Win64"))) ? FName(TEXT("UnsupportedRhi")) :
			ErrorMessage.Contains(TEXT("Invalid surface")) ? FName(TEXT("InvalidOptions")) : FName(TEXT("RuntimeUnavailable"));
		OutError = BuildNativeWebError(ErrorCode, ErrorMessage, ViewName);
		return false;
	}
	Surface = Script->GetSurface();
	const uint64 ThisGeneration = Generation;
	TWeakObjectPtr<UNativeWebView> WeakThis(this);
	Surface->SetOnTextureReady(FAffineUiTextureReady::CreateLambda([WeakThis, ThisGeneration](UTextureRenderTarget2D* Texture)
	{
		if (UNativeWebView* View = WeakThis.Get(); View && !View->bClosed && View->Generation == ThisGeneration)
			View->SetOutputTexture(Texture);
	}));
	Surface->SetOnError(FAffineUiSurfaceError::CreateLambda([WeakThis, ThisGeneration](const FString& Message)
	{
		if (UNativeWebView* View = WeakThis.Get(); View && !View->bClosed && View->Generation == ThisGeneration)
		{
			FNativeWebError Error = BuildNativeWebError(TEXT("RenderFailure"), Message, View->ViewName);
			View->Error.Broadcast(Error);
		}
	}));
#if PLATFORM_WINDOWS
	Surface->SetOnClick(FAffineUiClick::CreateLambda([WeakThis, ThisGeneration](uint64 NodeId)
	{
		UNativeWebView* View = WeakThis.Get();
		if (!View || View->bClosed || View->Generation != ThisGeneration || !View->Script) return;
		FQuickJSError ScriptError;
		if (!View->Script->DispatchClick(NodeId, ScriptError))
		{
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
	if (bClosed || !Surface) return false;
	if (!Attachment && !AttachToPlayerViewport(0)) return false;
	bVisible = true;
	if (Attachment) Attachment->SetVisible(true);
	return true;
}

void UNativeWebView::Hide()
{
	check(IsInGameThread());
	if (Attachment) Attachment->SetVisible(false);
	bVisible = false;
}

bool UNativeWebView::Resize(FIntPoint PixelSize, float DpiScale)
{
	check(IsInGameThread());
	return !bClosed && Surface && Surface->Resize(PixelSize, DpiScale);
}

bool UNativeWebView::AttachToPlayerViewport(int32 ZOrder)
{
	check(IsInGameThread());
	if (bClosed || !Surface || Attachment) return !bClosed && Attachment.IsValid();
	ULocalPlayer* Player = OwningPlayer.Get();
	UGameViewportClient* Viewport = Player ? Player->ViewportClient : nullptr;
	if (!Viewport) return false;
	TSharedRef<SNativeWebView> Widget = SNew(SNativeWebView).View(this).Surface(Surface);
	Attachment = Widget;
	Viewport->AddViewportWidgetContent(Widget, ZOrder);
	if (UTextureRenderTarget2D* Texture = Surface->GetOutputTexture()) Widget->SetTexture(Texture);
	Widget->SetVisible(bVisible);
	return true;
}

void UNativeWebView::DetachFromPlayerViewport()
{
	check(IsInGameThread());
	if (!Attachment) return;
	Attachment->SetVisible(false);
	if (ULocalPlayer* Player = OwningPlayer.Get())
		if (UGameViewportClient* Viewport = Player->ViewportClient) Viewport->RemoveViewportWidgetContent(Attachment.ToSharedRef());
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
	if (bClosed || !bVisible || !Surface) return false;
	FAffineUiInput Input{};
	switch (Event.Type)
	{
	case ENativeWebInputType::PointerMove: Input.Kind = EAffineUiInputKind::MouseMove; break;
	case ENativeWebInputType::PointerDown: Input.Kind = EAffineUiInputKind::MouseDown; break;
	case ENativeWebInputType::PointerUp: Input.Kind = EAffineUiInputKind::MouseUp; break;
	case ENativeWebInputType::Wheel: Input.Kind = EAffineUiInputKind::MouseWheel; break;
	case ENativeWebInputType::KeyDown: Input.Kind = EAffineUiInputKind::KeyDown; break;
	case ENativeWebInputType::KeyUp: Input.Kind = EAffineUiInputKind::KeyUp; break;
	case ENativeWebInputType::FocusGained: Input.Kind = EAffineUiInputKind::FocusGained; break;
	case ENativeWebInputType::FocusLost: Input.Kind = EAffineUiInputKind::FocusLost; break;
	}
	Input.Position = Event.NormalizedPosition;
	Input.WheelDelta = Event.WheelDelta;
	Input.bShift = Event.bShift;
	if (Event.Key == ENativeWebInputKey::Tab) Input.Key = EAffineUiInputKey::Tab;
	else if (Event.Key == ENativeWebInputKey::Enter) Input.Key = EAffineUiInputKey::Enter;
	else if (Event.Key == ENativeWebInputKey::Space) Input.Key = EAffineUiInputKey::Space;
	return Surface->QueueInput(Input);
}

bool UNativeWebView::SetDocument(const FString& Html, const FString& Css, FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	if (bClosed || !Surface) { OutError = BuildNativeWebError(TEXT("Closed"), TEXT("The NativeWeb view is closed."), ViewName); return false; }
	FAffineUiContent Content; Content.Html = Html; Content.Css = Css;
	if (!Surface->SetContent(Content)) { OutError = BuildNativeWebError(TEXT("InvalidDocument"), TEXT("The document could not be queued."), ViewName); return false; }
#if PLATFORM_WINDOWS
	if (Script) Script->RefreshDocumentEpoch();
#endif
	return true;
}

bool UNativeWebView::EvaluateScript(const FString& Source, const FString& SourceName, FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
#if !PLATFORM_WINDOWS
	OutError = BuildNativeWebError(TEXT("UnsupportedPlatform"), TEXT("NativeWeb JavaScript requires Win64 D3D11."), ViewName);
	return false;
#else
	if (bClosed || !Script) { OutError = BuildNativeWebError(TEXT("Closed"), TEXT("The NativeWeb view is closed."), ViewName); return false; }
	FQuickJSError ScriptError;
	if (!Script->Evaluate(Source, ScriptError, SourceName))
	{
		OutError = BuildNativeWebError(TEXT("ScriptError"), ScriptError.Message, ViewName);
		OutError.SourceName = SourceName;
		OutError.ScriptStack = ScriptError.Stack;
		Error.Broadcast(OutError);
		return false;
	}
	return true;
#endif
}

bool UNativeWebView::LoadDocumentFiles(const FString& HtmlPath, const FString& CssPath, FNativeWebError& OutError)
{
	FString Html, Css;
	OutError = {};
	if (!FFileHelper::LoadFileToString(Html, *HtmlPath) || !FFileHelper::LoadFileToString(Css, *CssPath))
	{
		OutError = BuildNativeWebError(TEXT("AssetLoadFailure"), TEXT("Could not read HTML or CSS file."), ViewName);
		OutError.SourceName = HtmlPath;
		return false;
	}
	return SetDocument(Html, Css, OutError);
}

bool UNativeWebView::EvaluateScriptFile(const FString& ScriptPath, FNativeWebError& OutError)
{
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *ScriptPath))
	{
		OutError = BuildNativeWebError(TEXT("AssetLoadFailure"), TEXT("Could not read JavaScript file."), ViewName);
		OutError.SourceName = ScriptPath;
		return false;
	}
	return EvaluateScript(Source, ScriptPath, OutError);
}

bool UNativeWebView::LoadPackagedDocument(const FString& HtmlRelativePath, const FString& CssRelativePath, FNativeWebError& OutError)
{
	FString HtmlPath, CssPath;
	if (!ResolvePackagedPath(PackagedAssetRoot, HtmlRelativePath, HtmlPath) || !ResolvePackagedPath(PackagedAssetRoot, CssRelativePath, CssPath))
	{
		OutError = BuildNativeWebError(TEXT("InvalidAssetPath"), TEXT("Packaged asset paths must be normalized relative paths inside the configured root."), ViewName);
		return false;
	}
	return LoadDocumentFiles(HtmlPath, CssPath, OutError);
}

bool UNativeWebView::EvaluatePackagedScript(const FString& ScriptRelativePath, FNativeWebError& OutError)
{
	FString ScriptPath;
	if (!ResolvePackagedPath(PackagedAssetRoot, ScriptRelativePath, ScriptPath))
	{
		OutError = BuildNativeWebError(TEXT("InvalidAssetPath"), TEXT("Packaged script paths must be normalized relative paths inside the configured root."), ViewName);
		return false;
	}
	return EvaluateScriptFile(ScriptPath, OutError);
}

ENativeWebQueueResult UNativeWebView::EnqueueStringToJavaScript(FName Channel, FString Value)
{
	FNativeWebMessage Message; Message.Type = ENativeWebPayloadType::String; Message.Channel = Channel; Message.String = MoveTemp(Value);
	FScopeLock Lock(&ProducerMutex);
	if (bClosed || !bInitialized) return ENativeWebQueueResult::Closed;
	const int64 Size = GetQueuedMessageByteSize(Message);
	if (Size > 1024 * 1024 || ProducerMessages.Num() >= 64 || ProducerBytes + Size > 4 * 1024 * 1024) return ENativeWebQueueResult::QueueFull;
	ProducerMessages.Add(MoveTemp(Message)); ProducerBytes += Size;
	return ENativeWebQueueResult::Queued;
}

ENativeWebQueueResult UNativeWebView::EnqueueBytesToJavaScript(FName Channel, TArray<uint8>&& Value)
{
	FNativeWebMessage Message; Message.Type = ENativeWebPayloadType::Bytes; Message.Channel = Channel; Message.Bytes = MoveTemp(Value);
	FScopeLock Lock(&ProducerMutex);
	if (bClosed || !bInitialized) return ENativeWebQueueResult::Closed;
	const int64 Size = Message.Bytes.Num();
	if (Size > 1024 * 1024 || ProducerMessages.Num() >= 64 || ProducerBytes + Size > 4 * 1024 * 1024) return ENativeWebQueueResult::QueueFull;
	ProducerMessages.Add(MoveTemp(Message)); ProducerBytes += Size;
	return ENativeWebQueueResult::Queued;
}

bool UNativeWebView::SendStringToJavaScript(FName Channel, FStringView Value)
{
	FString Copy;
	Copy.Reserve(Value.Len());
	for (TCHAR Character : Value) Copy.AppendChar(Character);
	return EnqueueStringToJavaScript(Channel, MoveTemp(Copy)) == ENativeWebQueueResult::Queued;
}

bool UNativeWebView::SendBytesToJavaScript(FName Channel, TConstArrayView<uint8> Value)
{
	TArray<uint8> Copy; Copy.Append(Value.GetData(), Value.Num());
	return EnqueueBytesToJavaScript(Channel, MoveTemp(Copy)) == ENativeWebQueueResult::Queued;
}

void UNativeWebView::Tick()
{
	check(IsInGameThread());
	#if !PLATFORM_WINDOWS
	return;
	#else
	if (bClosed || !Script) return;
	TArray<FNativeWebMessage> Outgoing;
	{
		FScopeLock Lock(&ProducerMutex);
		const int32 Count = FMath::Min(64, ProducerMessages.Num());
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
		if (Script->QueueToJavaScript(Message) == ENativeWebQueueResult::Queued) continue;
		FNativeWebError QueueError = BuildNativeWebError(TEXT("QueueFull"), TEXT("JavaScript message queue is full."), ViewName);
		Error.Broadcast(QueueError);
		if (bClosed) return;
	}
	Outgoing.Reset();
	Script->DrainMessagesToJavaScript(Outgoing, 64);
	for (const FNativeWebMessage& Message : Outgoing)
	{
		if (bClosed) return;
		FQuickJSError JsError;
		if (!Script->DeliverMessageToJavaScript(Message, JsError))
		{
			FNativeWebError NativeError = BuildNativeWebError(TEXT("ScriptError"), JsError.Message, ViewName);
			NativeError.ScriptStack = JsError.Stack;
			Error.Broadcast(NativeError);
			if (bClosed) return;
		}
	}
	Outgoing.Reset();
	Script->DrainMessagesFromJavaScript(Outgoing, 64);
	for (const FNativeWebMessage& Message : Outgoing)
	{
		if (bClosed) return;
		MessageReceived.Broadcast(Message);
	}
	int32 Jobs = 0; bool bPending = false; FQuickJSError JobError;
	if (!Script->PumpJobs(256, Jobs, bPending, JobError) && !JobError.Message.IsEmpty())
	{
		FNativeWebError NativeError = BuildNativeWebError(TEXT("ScriptError"), JobError.Message, ViewName);
		NativeError.ScriptStack = JobError.Stack; Error.Broadcast(NativeError);
		if (bClosed) return;
	}
	#endif
}

void UNativeWebView::Close()
{
	check(IsInGameThread());
	{
		FScopeLock Lock(&ProducerMutex);
		if (bClosed) return;
		bClosed = true;
		bInitialized = false;
		ProducerMessages.Reset(); ProducerBytes = 0;
	}
	bVisible = false;
	++Generation;
	DetachFromPlayerViewport();
#if PLATFORM_WINDOWS
	if (Script) { Script->Close(); Script.Reset(); }
#endif
	if (Surface) { Surface->Close(); Surface.Reset(); }
	MessageReceived.Clear(); Error.Clear();
}

void UNativeWebView::BeginDestroy()
{
	if (!bClosed && IsInGameThread()) Close();
	Super::BeginDestroy();
}
