#include "Assets/NativeWebAssetStyle.h"

#include "Assets/NativeWebAppAsset.h"
#include "Assets/NativeWebContentAssets.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FNativeWebAssetStyle::StyleSet;

void FNativeWebAssetStyle::Initialize()
{
	if (StyleSet.IsValid()) return;

	const TSharedPtr<IPlugin> NativeWebPlugin = IPluginManager::Get().FindPlugin(TEXT("NativeWeb"));
	if (!NativeWebPlugin.IsValid()) return;

	StyleSet = MakeShared<FSlateStyleSet>(TEXT("NativeWebAssetStyle"));
	StyleSet->SetContentRoot(FPaths::Combine(NativeWebPlugin->GetBaseDir(), TEXT("Resources/AssetIcons")));

	RegisterAssetClassBrushes(UNativeWebAppAsset::StaticClass(), TEXT("App"));
	RegisterAssetClassBrushes(UNativeWebHtmlAsset::StaticClass(), TEXT("Html"));
	RegisterAssetClassBrushes(UNativeWebCssAsset::StaticClass(), TEXT("Css"));
	RegisterAssetClassBrushes(UNativeWebJavaScriptAsset::StaticClass(), TEXT("JavaScript"));
	RegisterAssetClassBrushes(UNativeWebFontAsset::StaticClass(), TEXT("Font"));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FNativeWebAssetStyle::Shutdown()
{
	if (!StyleSet.IsValid()) return;

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}

void FNativeWebAssetStyle::RegisterAssetClassBrushes(const UClass* AssetClass, const TCHAR* ResourceName)
{
	if (!AssetClass || !StyleSet.IsValid()) return;

	const FString ClassName = AssetClass->GetName();
	const FString IconPath = StyleSet->RootToContentDir(FString::Printf(TEXT("%s_16"), ResourceName), TEXT(".svg"));
	const FString ThumbnailPath = StyleSet->RootToContentDir(FString::Printf(TEXT("%s_64"), ResourceName), TEXT(".svg"));

	StyleSet->Set(*FString::Printf(TEXT("ClassIcon.%s"), *ClassName),
	               new FSlateVectorImageBrush(IconPath, FVector2D(16.0f, 16.0f)));
	StyleSet->Set(*FString::Printf(TEXT("ClassThumbnail.%s"), *ClassName),
	               new FSlateVectorImageBrush(ThumbnailPath, FVector2D(64.0f, 64.0f)));
}
