#include "Commandlets/NativeWebBuildAssetExampleCommandlet.h"

#include "Assets/NativeWebAppAsset.h"
#include "Assets/NativeWebContentAssets.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	const FString ExampleDirectory(TEXT("/Game/NativeWeb/AssetExample"));

	template<typename TAsset>
	TAsset* CreateOrFindAsset(const FString& ObjectName)
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), *ExampleDirectory, *ObjectName);
		UPackage* Package = CreatePackage(*PackageName);
		TAsset* Asset = FindObject<TAsset>(Package, *ObjectName);
		if (!Asset) Asset = NewObject<TAsset>(Package, FName(*ObjectName), RF_Public | RF_Standalone);
		Asset->SetFlags(RF_Public | RF_Standalone);
		return Asset;
	}

	bool SaveAsset(UObject* Asset, FString& OutReport)
	{
		UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
		if (!Package) return false;
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
		OutReport += FString::Printf(TEXT("%s=%s\n"), *Asset->GetPathName(), bSaved ? TEXT("saved") : TEXT("FAILED"));
		return bSaved;
	}
}

UNativeWebBuildAssetExampleCommandlet::UNativeWebBuildAssetExampleCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UNativeWebBuildAssetExampleCommandlet::Main(const FString& Params)
{
	(void)Params;
	const FString FontPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("NativeWeb/Vue/fonts/dm-sans-regular.ttf"));
	const FString LicensePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("NativeWeb/Vue/fonts/DM-Sans-OFL.txt"));
	TArray<uint8> FontBytes;
	if (!FFileHelper::LoadFileToArray(FontBytes, *FontPath) || FontBytes.Num() != 56776 ||
		!IFileManager::Get().FileExists(*LicensePath))
	{
		UE_LOG(LogTemp, Error, TEXT("NativeWeb example requires dm-sans-regular.ttf (56,776 bytes) and its adjacent DM-Sans-OFL.txt."));
		return 1;
	}

	UNativeWebHtmlAsset* Html = CreateOrFindAsset<UNativeWebHtmlAsset>(TEXT("NA_Html"));
	UNativeWebCssAsset* Css = CreateOrFindAsset<UNativeWebCssAsset>(TEXT("NA_Css"));
	UNativeWebJavaScriptAsset* JavaScript = CreateOrFindAsset<UNativeWebJavaScriptAsset>(TEXT("NA_JavaScript"));
	UNativeWebFontAsset* Font = CreateOrFindAsset<UNativeWebFontAsset>(TEXT("NA_Font"));
	UNativeWebAppAsset* App = CreateOrFindAsset<UNativeWebAppAsset>(TEXT("NA_App"));
	const FString HtmlText = TEXT("<!doctype html><html><head><meta charset=\"utf-8\"></head><body><main id=\"status\">Asset app — gatavs</main></body></html>");
	const FString CssText = TEXT("@font-face { font-family: 'DM Sans Asset'; src: url('fonts/dm-sans-regular.ttf') format('truetype'); font-weight: 400; }\n#status { font-family: 'DM Sans Asset', sans-serif; font-size: 24px; }");
	const FString JavaScriptText = TEXT("if (!nativeWeb.setText('status', 'Mājas renderētā lietotne')) { throw new Error('nativeWeb.setText did not queue'); }");
	auto ToBytes = [](const FString& Text)
	{
		TArray<uint8> Result;
		FTCHARToUTF8 Encoded(*Text, Text.Len());
		Result.Append(reinterpret_cast<const uint8*>(Encoded.Get()), Encoded.Length());
		return Result;
	};
	Html->SetImportedContent(ToBytes(HtmlText), TEXT("example.html"));
	Css->SetImportedContent(ToBytes(CssText), TEXT("example.css"));
	JavaScript->SetImportedContent(ToBytes(JavaScriptText), TEXT("example.js"));
	Font->SetImportedContent(MoveTemp(FontBytes), TEXT("dm-sans-regular.ttf"));
	App->AppId = TEXT("AssetExample");
	App->HtmlMode = ENativeWebHtmlSourceMode::Asset;
	App->HtmlAsset = Html;
	App->CssMode = ENativeWebOptionalSourceMode::Asset;
	App->CssAsset = Css;
	App->JavaScriptMode = ENativeWebOptionalSourceMode::Asset;
	App->JavaScriptAsset = JavaScript;
	App->bEvaluateJavaScriptOnStart = true;
	App->Fonts = {TSoftObjectPtr<UNativeWebFontAsset>(Font)};

	FString Report = TEXT("NativeWeb asset example generation\n");
	bool bSaved = true;
	bSaved &= SaveAsset(Html, Report);
	bSaved &= SaveAsset(Css, Report);
	bSaved &= SaveAsset(JavaScript, Report);
	bSaved &= SaveAsset(Font, Report);
	bSaved &= SaveAsset(App, Report);
	const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NativeWeb/AssetExample/generation-report.txt"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("NativeWeb example generation report: %s"), *ReportPath);
	return bSaved ? 0 : 2;
}
