#include "Subsystems/NativeWebLocalPlayerSubsystem.h"

#include "Assets/NativeWebAppSource.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NativeWeb.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"

struct UNativeWebLocalPlayerSubsystem::FPendingAppRequest
{
	uint64 Generation = 0;
	TSoftObjectPtr<UNativeWebAppAsset> App;
	FNativeWebSurfaceConfig SurfaceConfig;
	FNativeWebPresentationConfig PresentationConfig;
	FNativeWebCreateAppViewComplete Completion;
	FNativeWebAppSourceSnapshot Snapshot;
	TSharedPtr<FStreamableHandle> AppHandle;
	TSharedPtr<FStreamableHandle> DependencyHandle;
};

namespace
{
	FName GetDependencyLoadErrorCode(const FNativeWebAppSourceSnapshot& Snapshot, const FSoftObjectPath& Path)
	{
		if (Snapshot.HtmlAsset.ToSoftObjectPath() == Path) return TEXT("HtmlAssetLoadFailed");
		if (Snapshot.CssAsset.ToSoftObjectPath() == Path) return TEXT("CssAssetLoadFailed");
		if (Snapshot.JavaScriptAsset.ToSoftObjectPath() == Path) return TEXT("JavaScriptAssetLoadFailed");
		return TEXT("FontAssetLoadFailed");
	}

	bool ResolveProjectContentDirectory(const FString& RelativeDirectory, FString& OutDirectory)
	{
		if (RelativeDirectory.IsEmpty() || !FPaths::IsRelative(RelativeDirectory)) return false;

		FString NormalizedDirectory = RelativeDirectory;
		NormalizedDirectory.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (NormalizedDirectory.StartsWith(TEXT("/")) || NormalizedDirectory.Contains(TEXT(":"))) return false;

		TArray<FString> PathComponents;
		NormalizedDirectory.ParseIntoArray(PathComponents, TEXT("/"), false);
		if (PathComponents.IsEmpty()) return false;
		for (const FString& Component : PathComponents)
		{
			if (Component.IsEmpty() || Component == TEXT(".") || Component == TEXT("..")) return false;
		}

		OutDirectory = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectContentDir(), NormalizedDirectory));
		FPaths::NormalizeDirectoryName(OutDirectory);
		return true;
	}
}

void UNativeWebLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bShuttingDown = false;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UNativeWebLocalPlayerSubsystem::Tick));
	UE_LOG(LogNativeWebSubsystem, Display, TEXT("NativeWeb local-player subsystem initialized; tick handle valid=%s."),
	       TickHandle.IsValid() ? TEXT("true") : TEXT("false"));
}

void UNativeWebLocalPlayerSubsystem::Deinitialize()
{
	check(IsInGameThread());
	bShuttingDown = true;
	TMap<FName, TSharedPtr<FPendingAppRequest>> DetachedRequests = MoveTemp(PendingAppRequests);
	PendingAppRequests.Reset();
	ReservedAppRequests.Reset();
	ReservedNames.Reset();
#if WITH_DEV_AUTOMATION_TESTS
	bHoldAppCompletionForTests = false;
	bHoldDependencyCompletionForTests = false;
	HeldAppCompletionsForTests.Reset();
	HeldDependencyCompletionsForTests.Reset();
#endif
	for (TPair<FName, TSharedPtr<FPendingAppRequest>>& Pair : DetachedRequests)
	{
		if (Pair.Value->AppHandle.IsValid()) Pair.Value->AppHandle->CancelHandle();
		if (Pair.Value->DependencyHandle.IsValid()) Pair.Value->DependencyHandle->CancelHandle();
	}
	UE_LOG(LogNativeWebSubsystem, Display,
	       TEXT("NativeWeb local-player subsystem shutting down with %d owned view(s)."),
	       Views.Num());
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	for (TPair<FName, TObjectPtr<UNativeWebView>>& Pair : Views)
		if (Pair.Value) Pair.Value->Close();
	Views.Reset();
	for (TPair<FName, TSharedPtr<FPendingAppRequest>>& Pair : DetachedRequests)
	{
		FNativeWebError Error;
		Error.Code = TEXT("LaunchCancelled");
		Error.Message = TEXT("NativeWeb subsystem teardown cancelled the pending app launch.");
		Error.ViewName = Pair.Key;
		if (Pair.Value->Completion.IsBound()) Pair.Value->Completion.Execute(nullptr, Error);
	}
	Super::Deinitialize();
	UE_LOG(LogNativeWebSubsystem, Verbose, TEXT("NativeWeb local-player subsystem shutdown complete."));
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreatePackagedView(
	FName Name, const FString& ContentRelativeDirectory, const FNativeWebSurfaceConfig& SurfaceConfig,
	const FNativeWebPresentationConfig& PresentationConfig, FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	UE_LOG(LogNativeWebSubsystem, Log,
	       TEXT("Creating packaged NativeWeb view '%s' from Content/%s."), *Name.ToString(),
	       *ContentRelativeDirectory);

	if (Name.IsNone())
	{
		OutError.Code = TEXT("InvalidName");
		OutError.Message = TEXT("A NativeWeb view requires a non-empty local name.");
		UE_LOG(LogNativeWebSubsystem, Error, TEXT("Packaged view creation failed: %s"), *OutError.Message);
		return nullptr;
	}

	FString AssetRoot;
	if (!ResolveProjectContentDirectory(ContentRelativeDirectory, AssetRoot))
	{
		OutError.Code = TEXT("InvalidAssetPath");
		OutError.Message = TEXT(
			"The packaged view directory must be a normalized relative path under the project's Content directory.");
		OutError.SourceName = ContentRelativeDirectory;
		OutError.ViewName = Name;
		UE_LOG(LogNativeWebAssets, Error,
		       TEXT("Rejected packaged asset directory '%s' for view '%s': paths must remain under Content."),
		       *ContentRelativeDirectory, *Name.ToString());
		return nullptr;
	}

	const FString CssPath = FPaths::Combine(AssetRoot, TEXT("ui.css"));
	FString Css;
	if (!FFileHelper::LoadFileToString(Css, *CssPath))
	{
		OutError.Code = TEXT("AssetLoadFailure");
		OutError.Message = TEXT("Could not read the packaged NativeWeb stylesheet 'ui.css'.");
		OutError.SourceName = CssPath;
		OutError.ViewName = Name;
		UE_LOG(LogNativeWebAssets, Error, TEXT("Could not load stylesheet for view '%s': %s"),
		       *Name.ToString(), *CssPath);
		return nullptr;
	}

	UNativeWebView* View = CreateConfiguredView(Name, NativeWebAppSource::DefaultHtmlShell,
	                                            Css, AssetRoot, SurfaceConfig, PresentationConfig, OutError);
	if (!View)
	{
		UE_LOG(LogNativeWebSubsystem, Error,
		       TEXT("Could not create packaged view '%s' (code=%s): %s"), *Name.ToString(),
		       *OutError.Code.ToString(), *OutError.Message);
		return nullptr;
	}

	if (!View->EvaluatePackagedScript(TEXT("ui.js"), OutError))
	{
		UE_LOG(LogNativeWebAssets, Error,
		       TEXT("Could not start packaged view '%s' from script '%s': %s"), *Name.ToString(),
		       *OutError.SourceName, *OutError.Message);
		const FNativeWebError StartupError = OutError;
		DestroyView(Name);
		OutError = StartupError;
		return nullptr;
	}

	UE_LOG(LogNativeWebSubsystem, Display,
	       TEXT("Packaged NativeWeb view '%s' started from '%s'."), *Name.ToString(), *AssetRoot);
	return View;
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreateInlineView(
	FName Name, const FString& Html, const FString& Css, const FNativeWebSurfaceConfig& SurfaceConfig,
	const FNativeWebPresentationConfig& PresentationConfig, FNativeWebError& OutError)
{
	return CreateConfiguredView(Name, Html, Css, FString(), SurfaceConfig, PresentationConfig, OutError);
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreateConfiguredView(
	FName Name, const FString& Html, const FString& Css, const FString& PackagedAssetRoot,
	const FNativeWebSurfaceConfig& SurfaceConfig, const FNativeWebPresentationConfig& PresentationConfig,
	FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	UE_LOG(LogNativeWebSubsystem, Verbose,
	       TEXT("Creating NativeWeb view '%s' (size=%dx%d, presentation=%s)."), *Name.ToString(),
	       SurfaceConfig.PixelSize.X, SurfaceConfig.PixelSize.Y,
	       *UEnum::GetValueAsString(PresentationConfig.PresentationMode));
	if (Name.IsNone())
	{
		OutError.Code = TEXT("InvalidName");
		OutError.Message = TEXT("A NativeWeb view requires a non-empty local name.");
		UE_LOG(LogNativeWebSubsystem, Error, TEXT("View creation failed: %s"), *OutError.Message);
		return nullptr;
	}
	if (Views.Contains(Name))
	{
		OutError.Code = TEXT("NameInUse");
		OutError.Message = FString::Printf(TEXT("NativeWeb view '%s' already exists."), *Name.ToString());
		OutError.ViewName = Name;
		UE_LOG(LogNativeWebSubsystem, Error, TEXT("View creation failed: %s"), *OutError.Message);
		return nullptr;
	}
	if (ReservedNames.Contains(Name))
	{
		OutError.Code = TEXT("NameInUse");
		OutError.Message = FString::Printf(TEXT("NativeWeb view name '%s' is reserved by an app launch."), *Name.ToString());
		OutError.ViewName = Name;
		return nullptr;
	}
	if (!ValidateViewOptions(Name, SurfaceConfig, PresentationConfig, OutError))
	{
		UE_LOG(LogNativeWebSubsystem, Error, TEXT("View creation failed: %s"), *OutError.Message);
		return nullptr;
	}
	UNativeWebView* View = NewObject<UNativeWebView>(this);
	if (!View->Initialize(Name, Html, Css, PackagedAssetRoot, SurfaceConfig, PresentationConfig, GetLocalPlayer(),
	                      OutError))
	{
		UE_LOG(LogNativeWebSubsystem, Error, TEXT("View '%s' initialization failed (code=%s): %s"),
		       *Name.ToString(), *OutError.Code.ToString(), *OutError.Message);
		View->Close();
		return nullptr;
	}
	Views.Add(Name, View);
	View->Show();
	UE_LOG(LogNativeWebSubsystem, Display, TEXT("NativeWeb view '%s' created."), *Name.ToString());
	return View;
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreateAppView(
	FName Name, UNativeWebAppAsset* App, const FNativeWebSurfaceConfig& SurfaceConfig,
	const FNativeWebPresentationConfig& PresentationConfig, FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	if (!IsValid(App))
	{
		OutError.Code = TEXT("MissingAppAsset");
		OutError.Message = TEXT("A valid NativeWeb app asset is required.");
		OutError.ViewName = Name;
		return nullptr;
	}
	TStrongObjectPtr<UNativeWebAppAsset> StrongApp(App);
	FNativeWebAppSourceSnapshot Snapshot;
	if (!NativeWebAppSource::BuildSnapshot(*StrongApp, Snapshot, OutError))
	{
		OutError.ViewName = Name;
		return nullptr;
	}
	if (bShuttingDown)
	{
		OutError.Code = TEXT("LaunchCancelled");
		OutError.Message = TEXT("The NativeWeb subsystem is shutting down.");
		OutError.ViewName = Name;
		return nullptr;
	}
	if (!ValidateViewOptions(Name, SurfaceConfig, PresentationConfig, OutError)) return nullptr;
	if (Views.Contains(Name) || ReservedNames.Contains(Name))
	{
		OutError.Code = TEXT("NameInUse");
		OutError.Message = FString::Printf(TEXT("NativeWeb view '%s' already exists or is reserved."), *Name.ToString());
		OutError.ViewName = Name;
		return nullptr;
	}
	const uint64 RequestGeneration = NextRequestGeneration++;
	ReservedNames.Add(Name);
	ReservedAppRequests.Add(Name, RequestGeneration);
	TSharedPtr<FStreamableHandle> DependencyHandle;
	if (!Snapshot.LoadPaths.IsEmpty())
	{
		DependencyHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(Snapshot.LoadPaths);
	}
	for (const FSoftObjectPath& Path : Snapshot.LoadPaths)
	{
		if (!Path.ResolveObject())
		{
			OutError.Code = GetDependencyLoadErrorCode(Snapshot, Path);
			OutError.Message = FString::Printf(TEXT("Selected asset '%s' did not load."), *Path.ToString());
			OutError.SourceName = Path.ToString();
			OutError.ViewName = Name;
			ReservedAppRequests.Remove(Name);
			ReservedNames.Remove(Name);
			return nullptr;
		}
	}
	FNativeWebResolvedAppSource Source;
	const bool bResolved = NativeWebAppSource::ResolveSnapshot(Snapshot, Source, OutError);
	if (bResolved)
	{
		UNativeWebView* View = CreateResolvedAppView(Name, Source, SurfaceConfig, PresentationConfig,
		                                             RequestGeneration, OutError);
		ReservedAppRequests.Remove(Name);
		ReservedNames.Remove(Name);
		return View;
	}
	ReservedAppRequests.Remove(Name);
	ReservedNames.Remove(Name);
	OutError.ViewName = Name;
	return nullptr;
}

void UNativeWebLocalPlayerSubsystem::CreateAppViewAsync(
	TSoftObjectPtr<UNativeWebAppAsset> App, FName Name, const FNativeWebSurfaceConfig& SurfaceConfig,
	const FNativeWebPresentationConfig& PresentationConfig, FNativeWebCreateAppViewComplete Completion)
{
	check(IsInGameThread());
	FNativeWebError Error;
	if (bShuttingDown)
	{
		Error.Code = TEXT("LaunchCancelled");
		Error.Message = TEXT("The NativeWeb subsystem is shutting down.");
		Error.ViewName = Name;
		if (Completion.IsBound()) Completion.Execute(nullptr, Error);
		return;
	}
	if (!ValidateViewOptions(Name, SurfaceConfig, PresentationConfig, Error))
	{
		if (Completion.IsBound()) Completion.Execute(nullptr, Error);
		return;
	}
	if (Views.Contains(Name) || ReservedNames.Contains(Name))
	{
		Error.Code = TEXT("NameInUse");
		Error.Message = FString::Printf(TEXT("NativeWeb view '%s' already exists or is reserved."), *Name.ToString());
		Error.ViewName = Name;
		if (Completion.IsBound()) Completion.Execute(nullptr, Error);
		return;
	}
	if (App.IsNull())
	{
		Error.Code = TEXT("MissingAppAsset");
		Error.Message = TEXT("A valid NativeWeb app asset reference is required.");
		Error.SourceName = TEXT("App");
		Error.ViewName = Name;
		if (Completion.IsBound()) Completion.Execute(nullptr, Error);
		return;
	}
	const uint64 RequestGeneration = NextRequestGeneration++;
	TSharedPtr<FPendingAppRequest> Request = MakeShared<FPendingAppRequest>();
	Request->Generation = RequestGeneration;
	Request->App = App;
	Request->SurfaceConfig = SurfaceConfig;
	Request->PresentationConfig = PresentationConfig;
	Request->Completion = MoveTemp(Completion);
	PendingAppRequests.Add(Name, Request);
	ReservedNames.Add(Name);
	ReservedAppRequests.Add(Name, RequestGeneration);
	TWeakObjectPtr<UNativeWebLocalPlayerSubsystem> WeakThis(this);
	Request->AppHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		App.ToSoftObjectPath(), FStreamableDelegate::CreateLambda([WeakThis, Name, RequestGeneration]()
		{
			if (UNativeWebLocalPlayerSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->BeginAppDependencyLoad(Name, RequestGeneration);
			}
		}), FStreamableManager::DefaultAsyncLoadPriority, false, true);
	if (!Request->AppHandle.IsValid())
	{
		Error.Code = TEXT("AppAssetLoadFailed");
		Error.Message = TEXT("The asset manager could not create an app load request.");
		Error.SourceName = App.ToSoftObjectPath().ToString();
		Error.ViewName = Name;
		CompletePendingApp(Name, RequestGeneration, nullptr, Error);
		return;
	}
	Request->AppHandle->StartStalledHandle();
}

void UNativeWebLocalPlayerSubsystem::BeginAppDependencyLoad(FName Name, uint64 RequestGeneration)
{
	check(IsInGameThread());
	if (!IsRequestActive(Name, RequestGeneration) || bShuttingDown) return;
#if WITH_DEV_AUTOMATION_TESTS
	if (bHoldAppCompletionForTests)
	{
		HeldAppCompletionsForTests.Add(Name);
		return;
	}
#endif
	TSharedPtr<FPendingAppRequest> Request = PendingAppRequests.FindRef(Name);
	UNativeWebAppAsset* App = Request->App.Get();
	if (!IsValid(App))
	{
		FNativeWebError Error;
		Error.Code = TEXT("AppAssetLoadFailed");
		Error.Message = TEXT("The requested NativeWeb app asset could not be loaded.");
		Error.SourceName = Request->App.ToSoftObjectPath().ToString();
		Error.ViewName = Name;
		CompletePendingApp(Name, RequestGeneration, nullptr, Error);
		return;
	}
	FNativeWebError SnapshotError;
	if (!NativeWebAppSource::BuildSnapshot(*App, Request->Snapshot, SnapshotError))
	{
		SnapshotError.ViewName = Name;
		CompletePendingApp(Name, RequestGeneration, nullptr, SnapshotError);
		return;
	}
	if (Request->Snapshot.LoadPaths.IsEmpty())
	{
		FNativeWebResolvedAppSource Source;
		FNativeWebError Error;
		if (NativeWebAppSource::ResolveSnapshot(Request->Snapshot, Source, Error) &&
			IsRequestActive(Name, RequestGeneration) && !bShuttingDown)
		{
			UNativeWebView* View = CreateResolvedAppView(Name, Source, Request->SurfaceConfig,
			                                             Request->PresentationConfig, RequestGeneration, Error);
			CompletePendingApp(Name, RequestGeneration, View, Error);
		}
		else
		{
			CompletePendingApp(Name, RequestGeneration, nullptr, Error);
		}
		return;
	}

	TWeakObjectPtr<UNativeWebLocalPlayerSubsystem> WeakThis(this);
	Request->DependencyHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Request->Snapshot.LoadPaths, FStreamableDelegate::CreateLambda([WeakThis, Name, RequestGeneration]()
		{
			UNativeWebLocalPlayerSubsystem* Subsystem = WeakThis.Get();
			if (!Subsystem || !Subsystem->IsRequestActive(Name, RequestGeneration) || Subsystem->bShuttingDown) return;
#if WITH_DEV_AUTOMATION_TESTS
			if (Subsystem->bHoldDependencyCompletionForTests)
			{
				Subsystem->HeldDependencyCompletionsForTests.Add(Name);
				return;
			}
#endif
			Subsystem->CompleteAppDependencyLoad(Name, RequestGeneration);
		}), FStreamableManager::DefaultAsyncLoadPriority, false, true);
	if (!Request->DependencyHandle.IsValid())
	{
		FNativeWebError Error;
		Error.Code = TEXT("AppAssetLoadFailed");
		Error.Message = TEXT("The asset manager could not create a dependency load request.");
		Error.ViewName = Name;
		CompletePendingApp(Name, RequestGeneration, nullptr, Error);
		return;
	}
	Request->DependencyHandle->StartStalledHandle();
}

void UNativeWebLocalPlayerSubsystem::CompleteAppDependencyLoad(FName Name, uint64 RequestGeneration)
{
	check(IsInGameThread());
	if (!IsRequestActive(Name, RequestGeneration) || bShuttingDown) return;
	TSharedPtr<FPendingAppRequest> Active = PendingAppRequests.FindRef(Name);
	for (const FSoftObjectPath& Path : Active->Snapshot.LoadPaths)
	{
		if (!Path.ResolveObject())
		{
			FNativeWebError LoadError;
			LoadError.Code = GetDependencyLoadErrorCode(Active->Snapshot, Path);
			LoadError.Message = FString::Printf(TEXT("Selected asset '%s' did not load."), *Path.ToString());
			LoadError.SourceName = Path.ToString();
			LoadError.ViewName = Name;
			CompletePendingApp(Name, RequestGeneration, nullptr, LoadError);
			return;
		}
	}
	FNativeWebResolvedAppSource Source;
	FNativeWebError ResolveError;
	if (NativeWebAppSource::ResolveSnapshot(Active->Snapshot, Source, ResolveError) &&
		IsRequestActive(Name, RequestGeneration) && !bShuttingDown)
	{
		UNativeWebView* View = CreateResolvedAppView(Name, Source, Active->SurfaceConfig,
		                                             Active->PresentationConfig, RequestGeneration, ResolveError);
		CompletePendingApp(Name, RequestGeneration, View, ResolveError);
	}
	else
	{
		CompletePendingApp(Name, RequestGeneration, nullptr, ResolveError);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UNativeWebLocalPlayerSubsystem::SetAutomationLoadCompletionHolds(bool bHoldAfterApp, bool bHoldAfterDependencies)
{
	bHoldAppCompletionForTests = bHoldAfterApp;
	bHoldDependencyCompletionForTests = bHoldAfterDependencies;
}

void UNativeWebLocalPlayerSubsystem::ReleaseAutomationLoadCompletionHold(FName Name, bool bAfterDependencies)
{
	check(IsInGameThread());
	if (!bAfterDependencies && HeldAppCompletionsForTests.Remove(Name) > 0)
	{
		bHoldAppCompletionForTests = false;
		if (const TSharedPtr<FPendingAppRequest>* Request = PendingAppRequests.Find(Name))
		{
			BeginAppDependencyLoad(Name, (*Request)->Generation);
		}
	}
	else if (bAfterDependencies && HeldDependencyCompletionsForTests.Remove(Name) > 0)
	{
		bHoldDependencyCompletionForTests = false;
		if (const TSharedPtr<FPendingAppRequest>* Request = PendingAppRequests.Find(Name))
		{
			CompleteAppDependencyLoad(Name, (*Request)->Generation);
		}
	}
}

bool UNativeWebLocalPlayerSubsystem::IsAutomationLoadCompletionHeld(FName Name, bool bAfterDependencies) const
{
	return bAfterDependencies ? HeldDependencyCompletionsForTests.Contains(Name) : HeldAppCompletionsForTests.Contains(Name);
}
#endif

void UNativeWebLocalPlayerSubsystem::CompletePendingApp(FName Name, uint64 RequestGeneration,
	UNativeWebView* View, const FNativeWebError& InError)
{
	check(IsInGameThread());
	if (!IsRequestActive(Name, RequestGeneration)) return;
	TSharedPtr<FPendingAppRequest> Request = PendingAppRequests.FindRef(Name);
	PendingAppRequests.Remove(Name);
	ReservedNames.Remove(Name);
	ReservedAppRequests.Remove(Name);
	Request->AppHandle.Reset();
	Request->DependencyHandle.Reset();
	FNativeWebError Error = InError;
	Error.ViewName = Name;
	if (Request->Completion.IsBound()) Request->Completion.Execute(View, Error);
}

bool UNativeWebLocalPlayerSubsystem::IsRequestActive(FName Name, uint64 RequestGeneration) const
{
	const TSharedPtr<FPendingAppRequest>* Found = PendingAppRequests.Find(Name);
	const uint64* ReservedGeneration = ReservedAppRequests.Find(Name);
	return Found && Found->IsValid() && (*Found)->Generation == RequestGeneration &&
	       ReservedGeneration && *ReservedGeneration == RequestGeneration;
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreateResolvedAppView(
	FName Name, const FNativeWebResolvedAppSource& Source, const FNativeWebSurfaceConfig& SurfaceConfig,
	const FNativeWebPresentationConfig& PresentationConfig, uint64 RequestGeneration, FNativeWebError& OutError)
{
	check(IsInGameThread());
	if (!ValidateViewOptions(Name, SurfaceConfig, PresentationConfig, OutError)) return nullptr;
	const uint64* ReservedGeneration = ReservedAppRequests.Find(Name);
	if (bShuttingDown || !ReservedGeneration || *ReservedGeneration != RequestGeneration ||
		Views.Contains(Name))
	{
		OutError.Code = bShuttingDown ? FName(TEXT("LaunchCancelled")) : FName(TEXT("NameInUse"));
		OutError.Message = bShuttingDown ? TEXT("NativeWeb subsystem shutdown cancelled app publication.")
		                                : TEXT("The requested view name is no longer reserved by this app launch.");
		OutError.ViewName = Name;
		return nullptr;
	}
	TStrongObjectPtr<UNativeWebView> View(NewObject<UNativeWebView>(this));
	if (!View->Initialize(Name, Source.Html, Source.Css, FString(), SurfaceConfig, PresentationConfig,
	                      GetLocalPlayer(), OutError, Source.FontAssets))
	{
		View->Close();
		return nullptr;
	}
	View->SetAppScript(Source.JavaScript, Source.JavaScriptSourceName, Source.bHasJavaScript);
	if (Source.bEvaluateJavaScriptOnStart && !View->EvaluateAppScript(OutError))
	{
		const FNativeWebError StartupError = OutError;
		View->Close();
		OutError = StartupError;
		return nullptr;
	}
	ReservedGeneration = ReservedAppRequests.Find(Name);
	if (bShuttingDown || !ReservedGeneration || *ReservedGeneration != RequestGeneration || Views.Contains(Name))
	{
		View->Close();
		OutError.Code = bShuttingDown ? FName(TEXT("LaunchCancelled")) : FName(TEXT("NameInUse"));
		OutError.Message = TEXT("The app launch lost ownership before view publication.");
		OutError.ViewName = Name;
		return nullptr;
	}
	Views.Add(Name, View.Get());
	View->Show();
	return View.Get();
}

bool UNativeWebLocalPlayerSubsystem::ValidateViewOptions(
	FName Name, const FNativeWebSurfaceConfig& SurfaceConfig,
	const FNativeWebPresentationConfig& PresentationConfig, FNativeWebError& OutError) const
{
	if (Name.IsNone())
	{
		OutError.Code = TEXT("InvalidName");
		OutError.Message = TEXT("A NativeWeb view requires a non-empty local name.");
		return false;
	}
	const bool bValidSurface = SurfaceConfig.PixelSize.X > 0 && SurfaceConfig.PixelSize.Y > 0 &&
		SurfaceConfig.PixelSize.X <= 4096 && SurfaceConfig.PixelSize.Y <= 4096 &&
		FMath::IsFinite(SurfaceConfig.DpiScale) && SurfaceConfig.DpiScale > 0.0f &&
		SurfaceConfig.DpiScale <= 8.0f && SurfaceConfig.ClearColor.A == 255;
	const bool bValidPresentation = PresentationConfig.PresentationMode == ENativeWebPresentationMode::GpuViewport ||
		PresentationConfig.PresentationMode == ENativeWebPresentationMode::Slate ||
		PresentationConfig.PresentationMode == ENativeWebPresentationMode::None;
	if (!bValidSurface || !bValidPresentation)
	{
		OutError.Code = TEXT("InvalidOptions");
		OutError.Message = TEXT("Invalid NativeWeb surface or presentation configuration.");
		OutError.ViewName = Name;
		return false;
	}
	return true;
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::FindView(FName Name) const
{
	check(IsInGameThread());
	if (const TObjectPtr<UNativeWebView>* Found = Views.Find(Name))
	{
		UE_LOG(LogNativeWebSubsystem, VeryVerbose, TEXT("Found NativeWeb view '%s'."), *Name.ToString());
		return Found->Get();
	}
	UE_LOG(LogNativeWebSubsystem, VeryVerbose, TEXT("No NativeWeb view named '%s' exists."), *Name.ToString());
	return nullptr;
}

bool UNativeWebLocalPlayerSubsystem::DestroyView(FName Name)
{
	check(IsInGameThread());
	TObjectPtr<UNativeWebView>* Found = Views.Find(Name);
	if (!Found)
	{
		UE_LOG(LogNativeWebSubsystem, Warning, TEXT("Cannot destroy NativeWeb view '%s': name not found."),
		       *Name.ToString());
		return false;
	}
	if (*Found) (*Found)->Close();
	Views.Remove(Name);
	UE_LOG(LogNativeWebSubsystem, Display, TEXT("NativeWeb view '%s' destroyed."), *Name.ToString());
	return true;
}

bool UNativeWebLocalPlayerSubsystem::Tick(float)
{
	check(IsInGameThread());
	if (!Views.IsEmpty())
	{
		UE_LOG(LogNativeWebTick, VeryVerbose, TEXT("Pumping %d NativeWeb view(s)."), Views.Num());
	}
	TArray<TObjectPtr<UNativeWebView>> Snapshot;
	Views.GenerateValueArray(Snapshot);
	for (const TObjectPtr<UNativeWebView>& View : Snapshot)
		if (View && !View->IsClosed()) View->Tick();
	return true;
}
