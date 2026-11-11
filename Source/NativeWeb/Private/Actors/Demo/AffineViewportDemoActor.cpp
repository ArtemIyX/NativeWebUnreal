#include "Actors/Demo/AffineViewportDemoActor.h"

#include "AffineUiScriptInstance.h"
#include "AffineUiSurface.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHIShaderPlatform.h"
#include "RHIStrings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Images/SImage.h"

DEFINE_LOG_CATEGORY_STATIC(LogAffineViewportDemo, Log, All);

namespace
{
void FitTextureToViewport(FVector2D ViewportSize, FIntPoint& OutSize, float& InOutDpiScale)
{
	const double Scale = FMath::Min(1.0, 4096.0 / FMath::Max(ViewportSize.X, ViewportSize.Y));
	OutSize.X = FMath::Max(1, FMath::RoundToInt32(ViewportSize.X * Scale));
	OutSize.Y = FMath::Max(1, FMath::RoundToInt32(ViewportSize.Y * Scale));
	InOutDpiScale = FMath::Clamp(static_cast<float>(InOutDpiScale * Scale), 0.01f, 8.0f);
}

class SAffineDemoImage : public SCompoundWidget
{
public:
	using FSurfacePtr = TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe>;

	SLATE_BEGIN_ARGS(SAffineDemoImage) {}
		SLATE_ARGUMENT(FSurfacePtr, Surface)
		SLATE_ARGUMENT(TFunction<void(FIntPoint, float)>, ResizeSurface)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args)
	{
		Surface = Args._Surface;
		ResizeSurface = Args._ResizeSurface;
		SetCanTick(true);
		ChildSlot[SAssignNew(ImageWidget, SImage).Image(&Brush).Visibility(EVisibility::HitTestInvisible)];
	}

	void SetTexture(UTextureRenderTarget2D* InTarget)
	{
		Target.Reset(InTarget);
		Brush.SetResourceObject(InTarget);
		if (InTarget) { Brush.ImageSize = FVector2D(InTarget->SizeX, InTarget->SizeY); }
		if (ImageWidget) { ImageWidget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint); }
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	bool SupportsKeyboardFocus() const override { return true; }

	void Tick(const FGeometry& Geometry, const double, const float) override
	{
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (!ResizeSurface || LocalSize.X <= 0.0 || LocalSize.Y <= 0.0) { return; }
		float DpiScale = Geometry.GetAccumulatedLayoutTransform().GetScale();
		FIntPoint PixelSize;
		FitTextureToViewport(LocalSize * DpiScale, PixelSize, DpiScale);
		ResizeSurface(PixelSize, DpiScale);
	}

	FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() != EKeys::LeftMouseButton ||
			!ForwardPointer(EAffineUiInputKind::MouseDown, Geometry, Event))
		{
			return FReply::Unhandled();
		}
		bPointerPressed = true;
		return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
	}

	FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		return OnMouseButtonDown(Geometry, Event);
	}

	FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (!bPointerPressed || Event.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}
		bPointerPressed = false;
		ForwardPointer(EAffineUiInputKind::MouseUp, Geometry, Event);
		return FReply::Handled().ReleaseMouseCapture();
	}

	FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		return ForwardPointer(EAffineUiInputKind::MouseMove, Geometry, Event) ?
			FReply::Handled() : FReply::Unhandled();
	}

	void OnMouseLeave(const FPointerEvent&) override
	{
		if (!bPointerPressed) { ForwardInput({EAffineUiInputKind::MouseMove, FVector2D(-1.0, -1.0)}); }
	}

	void OnMouseCaptureLost(const FCaptureLostEvent&) override
	{
		if (!bPointerPressed) { return; }
		bPointerPressed = false;
		ForwardInput({EAffineUiInputKind::MouseUp, FVector2D(-1.0, -1.0)});
	}

	FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		return ForwardPointer(EAffineUiInputKind::MouseWheel, Geometry, Event) ?
			FReply::Handled() : FReply::Unhandled();
	}

	FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override
	{
		ForwardInput({EAffineUiInputKind::FocusGained});
		return FReply::Handled();
	}

	void OnFocusLost(const FFocusEvent&) override
	{
		ForwardInput({EAffineUiInputKind::FocusLost});
	}

	FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
	{
		return ForwardKey(EAffineUiInputKind::KeyDown, Event);
	}

	FReply OnKeyUp(const FGeometry&, const FKeyEvent& Event) override
	{
		return ForwardKey(EAffineUiInputKind::KeyUp, Event);
	}

private:
	bool ForwardInput(const FAffineUiInput& Input) const
	{
		const TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Pinned = Surface.Pin();
		return Pinned && Pinned->QueueInput(Input);
	}

	bool ForwardPointer(EAffineUiInputKind Kind, const FGeometry& Geometry, const FPointerEvent& Event) const
	{
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (LocalSize.X <= 0.0 || LocalSize.Y <= 0.0) { return false; }
		const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		FAffineUiInput Input{Kind, FVector2D(Local.X / LocalSize.X, Local.Y / LocalSize.Y)};
		Input.WheelDelta = Event.GetWheelDelta();
		Input.bShift = Event.IsShiftDown();
		return ForwardInput(Input);
	}

	FReply ForwardKey(EAffineUiInputKind Kind, const FKeyEvent& Event) const
	{
		FAffineUiInput Input{Kind};
		if (Event.GetKey() == EKeys::Tab) { Input.Key = EAffineUiInputKey::Tab; }
		else if (Event.GetKey() == EKeys::Enter) { Input.Key = EAffineUiInputKey::Enter; }
		else if (Event.GetKey() == EKeys::SpaceBar) { Input.Key = EAffineUiInputKey::Space; }
		else { return FReply::Unhandled(); }
		Input.bShift = Event.IsShiftDown();
		return ForwardInput(Input) ? FReply::Handled() : FReply::Unhandled();
	}

	TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TFunction<void(FIntPoint, float)> ResizeSurface;
	bool bPointerPressed = false;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	TSharedPtr<SImage> ImageWidget;
	FSlateBrush Brush;
};

FAffineUiContent MakeVueDemoContent(const FString& Css)
{
	FAffineUiContent Content;
	Content.Html = TEXT("<html><body><div id=\"app\"></div></body></html>");
	Content.Css = Css;
	return Content;
}

FString GetVueAssetPath(const TCHAR* Filename)
{
	return FPaths::Combine(FPaths::ProjectContentDir(),
		FString::Printf(TEXT("NativeWeb/Vue/%s"), Filename));
}
}

AAffineViewportDemoActor::AAffineViewportDemoActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

AAffineViewportDemoActor::~AAffineViewportDemoActor()
{
#if PLATFORM_WINDOWS
	StopVueJobPump();
	if (ScriptInstance) { ScriptInstance->Close(); ScriptInstance.Reset(); }
#endif
}

void AAffineViewportDemoActor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogAffineViewportDemo, Display, TEXT("RHI: %s; shader platform: %s"),
		GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none"), *LexToString(GMaxRHIShaderPlatform));
#if !PLATFORM_WINDOWS
	UE_LOG(LogAffineViewportDemo, Warning, TEXT("The Vue NativeWeb demo requires Win64 D3D11."));
	return;
#else
	UGameViewportClient* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Client)
	{
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("No game viewport is available."));
		return;
	}
	FString Error;
	const FString ScriptPath = GetVueAssetPath(TEXT("ui.js"));
	const FString CssPath = GetVueAssetPath(TEXT("ui.css"));
	FString ScriptSource;
	FString CssSource;
	if (!FFileHelper::LoadFileToString(ScriptSource, *ScriptPath))
	{
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Could not read Vue bundle: %s"), *ScriptPath);
		return;
	}
	if (!FFileHelper::LoadFileToString(CssSource, *CssPath))
	{
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Could not read Vue stylesheet: %s"), *CssPath);
		return;
	}
	FAffineUiContent Content = MakeVueDemoContent(CssSource);
	if (bLoadFilesOnBeginPlay)
	{
		FAffineUiContent OverrideContent;
		if (FFileHelper::LoadFileToString(OverrideContent.Html, *HtmlFilePath) &&
			FFileHelper::LoadFileToString(OverrideContent.Css, *CssFilePath))
		{
			Content = MoveTemp(OverrideContent);
		}
		else
		{
			UE_LOG(LogAffineViewportDemo, Warning, TEXT("Could not read optional HTML/CSS override; keeping the Vue mount shell."));
		}
	}
	FAffineUiSurfaceDesc Desc;
	FVector2D ViewportSize = FVector2D::ZeroVector;
	Client->GetViewportSize(ViewportSize);
	if (ViewportSize.X > 0.0 && ViewportSize.Y > 0.0)
	{
		FitTextureToViewport(ViewportSize, Desc.PixelSize, Desc.DpiScale);
	}
	Desc.FontDirectory = FPaths::ConvertRelativePathToFull(GetVueAssetPath(TEXT("fonts")));
	Desc.ClearColor = FColor(8, 22, 41, 255);
	Desc.Content = Content;
	ScriptInstance = FAffineUiScriptInstance::Create(Desc, Error);
	if (!ScriptInstance)
	{
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Could not create Vue script instance: %s"), *Error);
		return;
	}
	Surface = ScriptInstance->GetSurface();
	FQuickJSError JsError;
	if (!ScriptInstance->Evaluate(ScriptSource, JsError, ScriptPath))
	{
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Vue bundle evaluation failed (%s): %s"), *ScriptPath, *JsError.Message);
		ScriptInstance->Close();
		ScriptInstance.Reset();
		Surface.Reset();
		return;
	}
	const TWeakObjectPtr<AAffineViewportDemoActor> WeakThis(this);
	const TSharedRef<SAffineDemoImage> Image = SNew(SAffineDemoImage)
		.Surface(Surface)
		.ResizeSurface([WeakThis](FIntPoint Size, float DpiScale)
		{
			AAffineViewportDemoActor* Actor = WeakThis.Get();
			if (Actor && Actor->Surface && Actor->bFollowViewport)
			{
				Actor->Surface->Resize(Size, DpiScale);
			}
		});
	Client->AddViewportWidgetContent(Image, 10);
	ViewportClient = Client;
	ViewportWidget = Image;
	if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
	{
		InputController = Controller;
		bPreviousShowMouseCursor = Controller->bShowMouseCursor;
		PreviousCaptureMode = Client->GetMouseCaptureMode();
		PreviousLockMode = Client->GetMouseLockMode();
		bPreviousIgnoreInput = Client->IgnoreInput();
		bPreviousHideCursorDuringCapture = Client->HideCursorDuringCapture();
		Controller->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(Image).SetHideCursorDuringCapture(false);
		Controller->SetInputMode(InputMode);
	}
	Surface->SetOnClick(FAffineUiClick::CreateLambda([WeakThis](uint64 NodeId)
	{
		AAffineViewportDemoActor* Actor = WeakThis.Get();
		if (!Actor || !Actor->ScriptInstance) { return; }
		FQuickJSError Error;
		if (!Actor->ScriptInstance->DispatchClick(NodeId, Error))
		{
			UE_LOG(LogAffineViewportDemo, Error, TEXT("Vue click handler failed: %s"), *Error.Message);
			return;
		}
		Actor->StartVueJobPump();
	}));
	TWeakPtr<SAffineDemoImage> WeakImage = Image;
	Surface->SetOnTextureReady(FAffineUiTextureReady::CreateLambda([WeakImage](UTextureRenderTarget2D* Texture)
	{
		if (TSharedPtr<SAffineDemoImage> Pinned = WeakImage.Pin()) { Pinned->SetTexture(Texture); }
	}));
	StartVueJobPump();
#endif
}

#if PLATFORM_WINDOWS
void AAffineViewportDemoActor::StartVueJobPump()
{
	StopVueJobPump();
	if (!ScriptInstance) { return; }
	int32 JobsExecuted = 0;
	bool bPending = false;
	FQuickJSError JsError;
	if (!ScriptInstance->PumpJobs(256, JobsExecuted, bPending, JsError))
	{
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Vue Promise job failed: %s"), *JsError.Message);
		return;
	}
	if (bPending)
	{
		TWeakObjectPtr<AAffineViewportDemoActor> WeakThis(this);
		VueJobTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[WeakThis](float DeltaTime)
			{
				if (AAffineViewportDemoActor* Actor = WeakThis.Get())
				{
					return Actor->PumpVueJobs(DeltaTime);
				}
				return false;
			}));
	}
}

bool AAffineViewportDemoActor::PumpVueJobs(float)
{
	if (!ScriptInstance) { VueJobTicker.Reset(); return false; }
	int32 JobsExecuted = 0;
	bool bPending = false;
	FQuickJSError JsError;
	if (!ScriptInstance->PumpJobs(256, JobsExecuted, bPending, JsError))
	{
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Vue Promise job failed: %s"), *JsError.Message);
		VueJobTicker.Reset();
		return false;
	}
	if (!bPending) VueJobTicker.Reset();
	return bPending;
}

void AAffineViewportDemoActor::StopVueJobPump()
{
	if (VueJobTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(VueJobTicker);
		VueJobTicker.Reset();
	}
}
#endif

bool AAffineViewportDemoActor::SetDemoContent(const FString& Html, const FString& Css)
{
	if (!Surface) { return false; }
	FAffineUiContent Content;
	Content.Html = Html; Content.Css = Css;
	return Surface->SetContent(Content);
}

bool AAffineViewportDemoActor::LoadDemoFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError)
{
	if (!Surface) { OutError = TEXT("Demo surface is not available."); return false; }
	return Surface->LoadContentFromFiles(HtmlPath, CssPath, OutError);
}

bool AAffineViewportDemoActor::ResizeDemoSurface(int32 Width, int32 Height, float DpiScale)
{
	if (!Surface || !Surface->Resize(FIntPoint(Width, Height), DpiScale)) { return false; }
	bFollowViewport = false;
	return true;
}

FString AAffineViewportDemoActor::GetDemoLastError() const
{
	return Surface ? Surface->GetLastError() : TEXT("Demo surface is not available.");
}

void AAffineViewportDemoActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameViewportClient* Client = ViewportClient.Get(); Client && ViewportWidget.IsValid())
	{
		Client->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
#if PLATFORM_WINDOWS
		if (APlayerController* Controller = InputController.Get())
		{
			Controller->bShowMouseCursor = bPreviousShowMouseCursor;
			Client->SetMouseCaptureMode(PreviousCaptureMode);
			Client->SetMouseLockMode(PreviousLockMode);
			Client->SetIgnoreInput(bPreviousIgnoreInput);
			Client->SetHideCursorDuringCapture(bPreviousHideCursorDuringCapture);
		}
		InputController.Reset();
#endif
	}
	ViewportWidget.Reset();
	ViewportClient.Reset();
#if PLATFORM_WINDOWS
	StopVueJobPump();
	if (ScriptInstance) { ScriptInstance->Close(); ScriptInstance.Reset(); }
#endif
	Surface.Reset();
	Super::EndPlay(EndPlayReason);
}
