#include "Actors/Demo/AffineViewportDemoActor.h"

#include "AffineUiSurface.h"
#include "DynamicRHI.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "RHIShaderPlatform.h"
#include "RHIStrings.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SConstraintCanvas.h"

DEFINE_LOG_CATEGORY_STATIC(LogAffineViewportDemo, Log, All);

namespace
{
class SAffineDemoImage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAffineDemoImage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		Brush.ImageSize = FVector2D(480, 240);
		ChildSlot[SAssignNew(ImageWidget, SImage).Image(&Brush)];
	}

	void SetTexture(UTextureRenderTarget2D* InTarget)
	{
		Target.Reset(InTarget);
		Brush.SetResourceObject(InTarget);
		if (InTarget) { Brush.ImageSize = FVector2D(InTarget->SizeX, InTarget->SizeY); }
		if (ImageWidget) { ImageWidget->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint); }
		Invalidate(EInvalidateWidgetReason::Layout);
	}

private:
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	TSharedPtr<SImage> ImageWidget;
	FSlateBrush Brush;
};

FAffineUiContent MakeDemoContent()
{
	FAffineUiContent Content;
	Content.Html = TEXT("<html><body><div class=\"stripe\"></div><div class=\"content\">")
		TEXT("<h1>AffineUI viewport</h1><button>Count 0</button></div></body></html>");
	Content.Css = TEXT("body { margin: 0; background-color: #081629; color: #effaff; font-family: sans-serif; }")
		TEXT(".stripe { background-color: #19dbe9; width: 90px; height: 14px; }")
		TEXT(".content { padding: 24px; }")
		TEXT("h1 { font-size: 27px; margin: 0 0 24px 0; }")
		TEXT("button { background-color: #087489; color: #ffffff; padding: 10px 22px; font-size: 18px; }");
	return Content;
}
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
	UGameViewportClient* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Client)
	{
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("No game viewport is available."));
		return;
	}
	FAffineUiSurfaceDesc Desc;
	Desc.ClearColor = FColor(8, 22, 41, 255);
	Desc.Content = MakeDemoContent();
	FString Error;
	Surface = FAffineUiSurface::Create(Desc, Error);
	if (!Surface)
	{
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("Could not create surface: %s"), *Error);
		return;
	}
	TSharedPtr<SAffineDemoImage> Image;
	TSharedRef<SWidget> Widget = SNew(SConstraintCanvas)
		+ SConstraintCanvas::Slot()
		.Anchors(FAnchors(0, 0, 0, 0))
		.Alignment(FVector2D(0, 0))
		.Offset(FMargin(16, 16, 0, 0))
		.AutoSize(true)
		[SAssignNew(Image, SAffineDemoImage)];
	Widget->SetVisibility(EVisibility::HitTestInvisible);
	Client->AddViewportWidgetContent(Widget, 10);
	ViewportClient = Client;
	ViewportWidget = Widget;
	TWeakPtr<SAffineDemoImage> WeakImage = Image;
	Surface->SetOnTextureReady(FAffineUiTextureReady::CreateLambda([WeakImage](UTextureRenderTarget2D* Texture)
	{
		if (TSharedPtr<SAffineDemoImage> Pinned = WeakImage.Pin()) { Pinned->SetTexture(Texture); }
	}));
	if (bLoadFilesOnBeginPlay && !Surface->LoadContentFromFiles(HtmlFilePath, CssFilePath, Error))
	{
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("%s; keeping inline demo content."), *Error);
	}
}

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
	return Surface && Surface->Resize(FIntPoint(Width, Height), DpiScale);
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
	}
	ViewportWidget.Reset();
	ViewportClient.Reset();
	if (Surface) { Surface->Close(); Surface.Reset(); }
	Super::EndPlay(EndPlayReason);
}
