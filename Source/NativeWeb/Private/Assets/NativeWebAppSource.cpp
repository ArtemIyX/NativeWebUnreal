#include "Assets/NativeWebAppSource.h"

namespace
{
	void SetError(FNativeWebError& Error, const TCHAR* Code, const FString& Message, const FString& Source)
	{
		Error.Code = FName(Code);
		Error.Message = Message;
		Error.SourceName = Source;
	}

	bool AddSelectedPath(const FSoftObjectPath& Path, const TCHAR* Code, const TCHAR* Slot,
	                     TArray<FSoftObjectPath>& Paths, FNativeWebError& Error)
	{
		if (Path.IsNull())
		{
			SetError(Error, Code, FString::Printf(TEXT("The selected %s asset reference is empty."), Slot), Slot);
			return false;
		}
		if (!Paths.Contains(Path)) Paths.Add(Path);
		return true;
	}

	bool DecodeText(const TArray<uint8>& Bytes, const TCHAR* InvalidCode, const FString& Source,
	                FString& OutText, FNativeWebError& Error)
	{
		FString Reason;
		if (!UNativeWebContentAsset::DecodeTextPayload(Bytes, OutText, Reason))
		{
			SetError(Error, InvalidCode, Reason, Source);
			return false;
		}
		return true;
	}

	bool ResolveTextAsset(const UNativeWebContentAsset* Asset, const TCHAR* LoadCode, const TCHAR* InvalidCode,
	                      const FString& SourceName, FString& OutText, FNativeWebError& Error)
	{
		if (!Asset)
		{
			SetError(Error, LoadCode, TEXT("The selected content asset could not be loaded."), SourceName);
			return false;
		}
		return DecodeText(Asset->GetBytes(), InvalidCode, SourceName, OutText, Error);
	}

	bool ContainsNul(const FString& Value)
	{
		for (TCHAR Character : Value)
		{
			if (Character == 0) return true;
		}
		return false;
	}
}

namespace NativeWebAppSource
{
	const TCHAR* DefaultHtmlShell = TEXT("<html><body><div id=\"app\"></div></body></html>");

	bool BuildSnapshot(const UNativeWebAppAsset& App, FNativeWebAppSourceSnapshot& OutSnapshot, FNativeWebError& OutError)
	{
		if (App.AppId.IsNone())
		{
			SetError(OutError, TEXT("InvalidAppAsset"), TEXT("AppId must be set before launching this app."), App.GetPathName());
			return false;
		}

		OutSnapshot.AppId = App.AppId;
		OutSnapshot.HtmlMode = App.HtmlMode;
		OutSnapshot.CssMode = App.CssMode;
		OutSnapshot.JavaScriptMode = App.JavaScriptMode;
		OutSnapshot.bEvaluateJavaScriptOnStart = App.JavaScriptMode != ENativeWebOptionalSourceMode::None &&
		                                         App.bEvaluateJavaScriptOnStart;
		if (App.HtmlMode == ENativeWebHtmlSourceMode::Asset) OutSnapshot.HtmlAsset = App.HtmlAsset;
		else if (App.HtmlMode == ENativeWebHtmlSourceMode::String) OutSnapshot.HtmlString = App.HtmlString;
		else if (App.HtmlMode != ENativeWebHtmlSourceMode::Default)
		{
			SetError(OutError, TEXT("InvalidHtmlSource"), TEXT("The HTML source mode is unknown."), TEXT("HTML"));
			return false;
		}

		if (App.CssMode == ENativeWebOptionalSourceMode::Asset) OutSnapshot.CssAsset = App.CssAsset;
		else if (App.CssMode == ENativeWebOptionalSourceMode::String) OutSnapshot.CssString = App.CssString;
		else if (App.CssMode != ENativeWebOptionalSourceMode::None)
		{
			SetError(OutError, TEXT("InvalidCssSource"), TEXT("The CSS source mode is unknown."), TEXT("CSS"));
			return false;
		}

		if (App.JavaScriptMode == ENativeWebOptionalSourceMode::Asset) OutSnapshot.JavaScriptAsset = App.JavaScriptAsset;
		else if (App.JavaScriptMode == ENativeWebOptionalSourceMode::String) OutSnapshot.JavaScriptString = App.JavaScriptString;
		else if (App.JavaScriptMode != ENativeWebOptionalSourceMode::None)
		{
			SetError(OutError, TEXT("InvalidJavaScriptSource"), TEXT("The JavaScript source mode is unknown."), TEXT("JavaScript"));
			return false;
		}

		if (App.JavaScriptMode != ENativeWebOptionalSourceMode::None &&
			ContainsNul(OutSnapshot.JavaScriptString))
		{
			SetError(OutError, TEXT("InvalidJavaScriptSource"), TEXT("JavaScript contains an embedded NUL character."), TEXT("JavaScript"));
			return false;
		}
		if (App.HtmlMode == ENativeWebHtmlSourceMode::String &&
			(OutSnapshot.HtmlString.IsEmpty() || ContainsNul(OutSnapshot.HtmlString)))
		{
			SetError(OutError, TEXT("InvalidHtmlSource"), TEXT("Inline HTML must be non-empty and cannot contain an embedded NUL."), TEXT("HTML"));
			return false;
		}
		if (App.CssMode == ENativeWebOptionalSourceMode::String && ContainsNul(OutSnapshot.CssString))
		{
			SetError(OutError, TEXT("InvalidCssSource"), TEXT("CSS contains an embedded NUL character."), TEXT("CSS"));
			return false;
		}

		if (App.HtmlMode == ENativeWebHtmlSourceMode::Asset && !AddSelectedPath(App.HtmlAsset.ToSoftObjectPath(), TEXT("MissingHtmlAsset"), TEXT("HTML"), OutSnapshot.LoadPaths, OutError)) return false;
		if (App.CssMode == ENativeWebOptionalSourceMode::Asset && !AddSelectedPath(App.CssAsset.ToSoftObjectPath(), TEXT("MissingCssAsset"), TEXT("CSS"), OutSnapshot.LoadPaths, OutError)) return false;
		if (App.JavaScriptMode == ENativeWebOptionalSourceMode::Asset && !AddSelectedPath(App.JavaScriptAsset.ToSoftObjectPath(), TEXT("MissingJavaScriptAsset"), TEXT("JavaScript"), OutSnapshot.LoadPaths, OutError)) return false;

		OutSnapshot.Fonts = App.Fonts;
		for (const TSoftObjectPtr<UNativeWebFontAsset>& Font : OutSnapshot.Fonts)
		{
			if (!AddSelectedPath(Font.ToSoftObjectPath(), TEXT("MissingFontAsset"), TEXT("Font"), OutSnapshot.LoadPaths, OutError)) return false;
		}
		return true;
	}

	bool IsValidFont(const FString& Filename, TConstArrayView<uint8> Bytes, FString& OutReason)
	{
		return UNativeWebFontAsset::ValidatePayload(Filename, Bytes, OutReason);
	}

	bool ResolveSnapshot(const FNativeWebAppSourceSnapshot& Snapshot, FNativeWebResolvedAppSource& OutSource,
	                     FNativeWebError& OutError)
	{
		OutSource.bEvaluateJavaScriptOnStart = Snapshot.bEvaluateJavaScriptOnStart;
		if (Snapshot.HtmlMode == ENativeWebHtmlSourceMode::Default)
		{
			OutSource.Html = DefaultHtmlShell;
		}
		else if (Snapshot.HtmlMode == ENativeWebHtmlSourceMode::String)
		{
			OutSource.Html = Snapshot.HtmlString;
		}
		else if (!ResolveTextAsset(Snapshot.HtmlAsset.Get(), TEXT("HtmlAssetLoadFailed"), TEXT("InvalidHtmlSource"),
		                          Snapshot.HtmlAsset.ToSoftObjectPath().ToString(), OutSource.Html, OutError)) return false;

		if (Snapshot.CssMode == ENativeWebOptionalSourceMode::String) OutSource.Css = Snapshot.CssString;
		else if (Snapshot.CssMode == ENativeWebOptionalSourceMode::Asset &&
		         !ResolveTextAsset(Snapshot.CssAsset.Get(), TEXT("CssAssetLoadFailed"), TEXT("InvalidCssSource"),
		                          Snapshot.CssAsset.ToSoftObjectPath().ToString(), OutSource.Css, OutError)) return false;

		OutSource.bHasJavaScript = Snapshot.JavaScriptMode != ENativeWebOptionalSourceMode::None;
		if (Snapshot.JavaScriptMode == ENativeWebOptionalSourceMode::String)
		{
			OutSource.JavaScript = Snapshot.JavaScriptString;
			OutSource.JavaScriptSourceName = FString::Printf(TEXT("<app-id:%s:JavaScript>"), *Snapshot.AppId.ToString());
		}
		else if (Snapshot.JavaScriptMode == ENativeWebOptionalSourceMode::Asset)
		{
			OutSource.JavaScriptSourceName = Snapshot.JavaScriptAsset.ToSoftObjectPath().ToString();
			if (!ResolveTextAsset(Snapshot.JavaScriptAsset.Get(), TEXT("JavaScriptAssetLoadFailed"), TEXT("InvalidJavaScriptSource"),
			                      OutSource.JavaScriptSourceName, OutSource.JavaScript, OutError)) return false;
		}

		TMap<FString, TArray<uint8>> Fonts;
		TMap<FString, FString> OriginalAssets;
		for (const TSoftObjectPtr<UNativeWebFontAsset>& FontReference : Snapshot.Fonts)
		{
			const UNativeWebFontAsset* Font = FontReference.Get();
			const FString FontSource = FontReference.ToSoftObjectPath().ToString();
			if (!Font)
			{
				SetError(OutError, TEXT("FontAssetLoadFailed"), TEXT("The selected font asset could not be loaded."), FontSource);
				return false;
			}
			const FString Filename = Font->GetImportedFilename();
			FString Reason;
			if (!IsValidFont(Filename, Font->GetBytes(), Reason))
			{
				SetError(OutError, TEXT("InvalidFontSource"), Reason, FontSource);
				return false;
			}
			const FString Key = Filename.ToLower();
			if (const FString* FirstAsset = OriginalAssets.Find(Key))
			{
				SetError(OutError, TEXT("DuplicateFontFilename"), FString::Printf(TEXT("Font '%s' duplicates the filename selected from '%s'."), *FontSource, **FirstAsset), FontSource);
				return false;
			}
			OriginalAssets.Add(Key, FontSource);
			Fonts.Add(FString(TEXT("fonts/")) + Filename, Font->GetBytes());
		}
		OutSource.FontAssets = MakeShared<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe>(MoveTemp(Fonts));
		return true;
	}
}
