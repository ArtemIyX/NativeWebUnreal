#include "Subsystems/NativeWebLocalPlayerSubsystem.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NativeWeb.h"
#include "Containers/Ticker.h"

namespace
{
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
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UNativeWebLocalPlayerSubsystem::Tick));
	UE_LOG(LogNativeWebSubsystem, Display, TEXT("NativeWeb local-player subsystem initialized; tick handle valid=%s."),
	       TickHandle.IsValid() ? TEXT("true") : TEXT("false"));
}

void UNativeWebLocalPlayerSubsystem::Deinitialize()
{
	check(IsInGameThread());
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
	Super::Deinitialize();
	UE_LOG(LogNativeWebSubsystem, Verbose, TEXT("NativeWeb local-player subsystem shutdown complete."));
}

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreatePackagedView(FName Name, const FString& ContentRelativeDirectory,
                                                                   FNativeWebError& OutError)
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

	FNativeWebViewOptions Options;
	Options.Html = TEXT("<html><body><div id=\"app\"></div></body></html>");
	Options.Css = MoveTemp(Css);
	Options.PackagedAssetRoot = AssetRoot;
	Options.bCreateViewportOverlay = true;

	UNativeWebView* View = CreateView(Name, Options, OutError);
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

UNativeWebView* UNativeWebLocalPlayerSubsystem::CreateView(FName Name, const FNativeWebViewOptions& Options,
                                                           FNativeWebError& OutError)
{
	check(IsInGameThread());
	OutError = {};
	UE_LOG(LogNativeWebSubsystem, Verbose,
	       TEXT("Creating NativeWeb view '%s' (size=%dx%d, overlay=%s)."), *Name.ToString(),
	       Options.PixelSize.X, Options.PixelSize.Y, Options.bCreateViewportOverlay ? TEXT("true") : TEXT("false"));
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
	UNativeWebView* View = NewObject<UNativeWebView>(this);
	if (!View->Initialize(Name, Options, GetLocalPlayer(), OutError))
	{
		UE_LOG(LogNativeWebSubsystem, Error, TEXT("View '%s' initialization failed (code=%s): %s"),
		       *Name.ToString(), *OutError.Code.ToString(), *OutError.Message);
		View->Close();
		return nullptr;
	}
	Views.Add(Name, View);
	if (Options.bCreateViewportOverlay)
	{
		if (!View->AttachToPlayerViewport(Options.ViewportZOrder))
		{
			View->Close();
			Views.Remove(Name);
			OutError.Code = TEXT("ViewportUnavailable");
			OutError.Message = TEXT("Could not attach the NativeWeb view to the local player viewport.");
			OutError.ViewName = Name;
			UE_LOG(LogNativeWebSubsystem, Error, TEXT("View '%s' could not attach to its local player viewport."),
			       *Name.ToString());
			return nullptr;
		}
		View->Show();
	}
	UE_LOG(LogNativeWebSubsystem, Display, TEXT("NativeWeb view '%s' created."), *Name.ToString());
	return View;
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
