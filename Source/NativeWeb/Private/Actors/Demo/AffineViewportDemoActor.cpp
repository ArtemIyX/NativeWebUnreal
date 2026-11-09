#include "Actors/Demo/AffineViewportDemoActor.h"
#include "Actors/Demo/AffineViewportDemoRenderer.h"

#include "DynamicRHI.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "RHIShaderPlatform.h"
#include "RHIStrings.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SConstraintCanvas.h"

DEFINE_LOG_CATEGORY_STATIC(LogAffineViewportDemo, Log, All);

namespace
{
TWeakObjectPtr<AAffineViewportDemoActor> ActiveDemo;

// Slate may retain the widget after removal, so its brush and texture outlive the actor's reference.
class SAffineDemoImage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAffineDemoImage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, UTextureRenderTarget2D* InTarget)
	{
		Target.Reset(InTarget);
		Brush.SetResourceObject(InTarget);
		Brush.ImageSize = FVector2D(480, 240);
		ChildSlot[SNew(SImage).Image(&Brush)];
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		if (!bLoggedGeometry && FAffineViewportDemoRenderer::DiagnosticsEnabled())
		{
			bLoggedGeometry = true;
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const FVector2D Position = AllottedGeometry.GetAbsolutePosition();
			UE_LOG(LogAffineViewportDemo, Display,
				TEXT("Slate image geometry: local size=(%.1f,%.1f) absolute position=(%.1f,%.1f) cull=(%.1f,%.1f,%.1f,%.1f) brush=%s target=%s"),
				Size.X, Size.Y, Position.X, Position.Y,
				MyCullingRect.Left, MyCullingRect.Top, MyCullingRect.Right, MyCullingRect.Bottom,
				*Brush.GetResourceName().ToString(), *GetNameSafe(Target.Get()));
		}
		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

private:
	mutable bool bLoggedGeometry = false;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	FSlateBrush Brush;
};
}

AAffineViewportDemoActor::AAffineViewportDemoActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAffineViewportDemoActor::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogAffineViewportDemo, Display, TEXT("RHI: %s; shader platform: %s"),
		GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none"), *LexToString(GMaxRHIShaderPlatform));
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11)
	{
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("Demo requires D3D11. Launch with -dx11."));
		return;
	}
	if (ActiveDemo.IsValid())
	{
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("Only one viewport demo can be active."));
		return;
	}
	UGameViewportClient* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Client)
	{
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("No game viewport is available."));
		return;
	}
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->bNeedsTwoCopies = false;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->ClearColor = FLinearColor(0.015f, 0.04f, 0.12f, 1.0f);
	RenderTarget->InitCustomFormat(480, 240, PF_B8G8R8A8, false);
	RenderTarget->UpdateResourceImmediate(true);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, RenderTarget->ClearColor);
	DemoRenderer = FAffineViewportDemoRenderer::Start(RenderTarget->GameThread_GetRenderTargetResource());
	if (!DemoRenderer)
	{
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Could not schedule AffineUI rendering; displaying fallback."));
	}

	TSharedRef<SWidget> Widget = SNew(SConstraintCanvas)
		+ SConstraintCanvas::Slot()
		.Anchors(FAnchors(0, 0, 0, 0))
		.Alignment(FVector2D(0, 0))
		.Offset(FMargin(16, 16, 480, 240))
		[SNew(SAffineDemoImage, RenderTarget)];
	Widget->SetVisibility(EVisibility::HitTestInvisible);
	Client->AddViewportWidgetContent(Widget, 10);
	ViewportClient = Client;
	ViewportWidget = Widget;
	ActiveDemo = this;
	if (DemoRenderer && FAffineViewportDemoRenderer::DiagnosticsEnabled())
	{
		GetWorldTimerManager().SetTimer(DiagnosticTimer, this, &AAffineViewportDemoActor::SampleViewportDiagnostic, 1.0f, false);
	}
	UE_LOG(LogAffineViewportDemo, Display, TEXT("Viewport probe started (480 x 240); AffineUI draw is queued."));
}

void AAffineViewportDemoActor::SampleViewportDiagnostic()
{
	if (DemoRenderer && RenderTarget)
	{
		FAffineViewportDemoRenderer::SampleLater(DemoRenderer, RenderTarget->GameThread_GetRenderTargetResource());
	}
}

void AAffineViewportDemoActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DiagnosticTimer);
	if (UGameViewportClient* Client = ViewportClient.Get(); Client && ViewportWidget.IsValid())
	{
		Client->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
	}
	ViewportWidget.Reset();
	ViewportClient.Reset();
	FAffineViewportDemoRenderer::Stop(DemoRenderer);
	DemoRenderer = nullptr;
	RenderTarget = nullptr;
	if (ActiveDemo.Get() == this)
	{
		ActiveDemo.Reset();
	}
	Super::EndPlay(EndPlayReason);
}
