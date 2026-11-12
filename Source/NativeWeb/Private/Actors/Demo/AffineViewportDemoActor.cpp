#include "Actors/Demo/AffineViewportDemoActor.h"

#include "DynamicRHI.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NativeWebView.h"
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogAffineViewportDemo, Log, All);

namespace
{
	FString VueAsset(const TCHAR* Name)
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("NativeWeb/Vue"), Name);
	}

	FString VueDirectory()
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("NativeWeb/Vue"));
	}
}

AAffineViewportDemoActor::AAffineViewportDemoActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAffineViewportDemoActor::BeginPlay()
{
	Super::BeginPlay();
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11)
	{
		LastError = TEXT("NativeWeb requires Win64 D3D11 (-dx11).");
		UE_LOG(LogAffineViewportDemo, Warning, TEXT("%s"), *LastError);
		return;
	}
	ULocalPlayer* Player = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr;
	UNativeWebLocalPlayerSubsystem* NativeWeb = Player
		                                            ? Player->GetSubsystem<UNativeWebLocalPlayerSubsystem>()
		                                            : nullptr;
	if (!NativeWeb)
	{
		LastError = TEXT("No local player NativeWeb subsystem is available.");
		UE_LOG(LogAffineViewportDemo, Error, TEXT("%s"), *LastError);
		return;
	}
	FString Css;
	if (!FFileHelper::LoadFileToString(Css, *VueAsset(TEXT("ui.css"))))
	{
		LastError = TEXT("Could not read the staged Vue stylesheet.");
		UE_LOG(LogAffineViewportDemo, Error, TEXT("%s"), *LastError);
		return;
	}
	FNativeWebViewOptions Options;
	Options.Html = TEXT("<html><body><div id=\"app\"></div></body></html>");
	Options.Css = Css;
	Options.PackagedAssetRoot = VueDirectory();
	Options.ClearColor = FColor(8, 22, 41, 255);
	Options.bCreateViewportOverlay = true;
	Options.ViewportZOrder = 10;
	if (bLoadFilesOnBeginPlay)
	{
		FString HtmlOverride, CssOverride;
		if (FFileHelper::LoadFileToString(HtmlOverride, *HtmlFilePath) && FFileHelper::LoadFileToString(
			CssOverride, *CssFilePath))
		{
			Options.Html = MoveTemp(HtmlOverride);
			Options.Css = MoveTemp(CssOverride);
		}
		else UE_LOG(LogAffineViewportDemo, Warning,
		            TEXT("Could not read HTML/CSS override; keeping the Vue mount shell."));
	}
	FNativeWebError Error;
	View = NativeWeb->CreateView(ViewName, Options, Error);
	if (!View)
	{
		LastError = Error.Message;
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Could not create NativeWeb view: %s"), *LastError);
		return;
	}
	if (!View->EvaluatePackagedScript(TEXT("ui.js"), Error))
	{
		LastError = Error.Message.IsEmpty() ? TEXT("Could not read the staged Vue bundle.") : Error.Message;
		UE_LOG(LogAffineViewportDemo, Error, TEXT("Vue bundle failed: %s"), *LastError);
		NativeWeb->DestroyView(ViewName);
		View = nullptr;
	}
}

bool AAffineViewportDemoActor::SetDemoContent(const FString& Html, const FString& Css)
{
	if (!View) return false;
	FNativeWebError Error;
	const bool bResult = View->SetDocument(Html, Css, Error);
	LastError = Error.Message;
	return bResult;
}

bool AAffineViewportDemoActor::LoadDemoFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError)
{
	if (!View)
	{
		OutError = TEXT("NativeWeb view is not available.");
		return false;
	}
	FString Html, Css;
	if (!FFileHelper::LoadFileToString(Html, *HtmlPath) || !FFileHelper::LoadFileToString(Css, *CssPath))
	{
		OutError = TEXT("Could not read HTML or CSS override file.");
		return false;
	}
	FNativeWebError Error;
	const bool bResult = View->SetDocument(Html, Css, Error);
	OutError = Error.Message;
	LastError = OutError;
	return bResult;
}

bool AAffineViewportDemoActor::ResizeDemoSurface(int32 Width, int32 Height, float DpiScale)
{
	return View && View->Resize(FIntPoint(Width, Height), DpiScale);
}

void AAffineViewportDemoActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (View)
	{
		if (UNativeWebLocalPlayerSubsystem* NativeWeb = View->GetTypedOuter<UNativeWebLocalPlayerSubsystem>())
			NativeWeb->DestroyView(ViewName);
		else View->Close();
		View = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
