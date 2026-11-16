#pragma once

#include "CoreMinimal.h"
#include "Assets/NativeWebAppAsset.h"
#include "Data/Structs/NativeWebError.h"

struct FNativeWebAppSourceSnapshot
{
	FName AppId = NAME_None;
	ENativeWebHtmlSourceMode HtmlMode = ENativeWebHtmlSourceMode::Default;
	ENativeWebOptionalSourceMode CssMode = ENativeWebOptionalSourceMode::None;
	ENativeWebOptionalSourceMode JavaScriptMode = ENativeWebOptionalSourceMode::None;
	FString HtmlString;
	FString CssString;
	FString JavaScriptString;
	TSoftObjectPtr<UNativeWebHtmlAsset> HtmlAsset;
	TSoftObjectPtr<UNativeWebCssAsset> CssAsset;
	TSoftObjectPtr<UNativeWebJavaScriptAsset> JavaScriptAsset;
	TArray<TSoftObjectPtr<UNativeWebFontAsset>> Fonts;
	bool bEvaluateJavaScriptOnStart = false;
	TArray<FSoftObjectPath> LoadPaths;
};

struct FNativeWebResolvedAppSource
{
	FString Html;
	FString Css;
	FString JavaScript;
	FString JavaScriptSourceName;
	bool bHasJavaScript = false;
	bool bEvaluateJavaScriptOnStart = false;
	TSharedPtr<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe> FontAssets;
};

namespace NativeWebAppSource
{
	extern const TCHAR* DefaultHtmlShell;
	NATIVEWEB_API bool BuildSnapshot(const UNativeWebAppAsset& App, FNativeWebAppSourceSnapshot& OutSnapshot, FNativeWebError& OutError);
	NATIVEWEB_API bool ResolveSnapshot(const FNativeWebAppSourceSnapshot& Snapshot, FNativeWebResolvedAppSource& OutSource,
	                     FNativeWebError& OutError);
	NATIVEWEB_API bool IsValidFont(const FString& Filename, TConstArrayView<uint8> Bytes, FString& OutReason);
}
