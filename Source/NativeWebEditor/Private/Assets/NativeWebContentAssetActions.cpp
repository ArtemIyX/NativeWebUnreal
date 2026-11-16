#include "Assets/NativeWebContentAssetActions.h"

#include "Assets/NativeWebContentAssets.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"

namespace
{
	const TCHAR* ExtensionFor(const UNativeWebContentAsset& Asset)
	{
		if (Asset.IsA<UNativeWebHtmlAsset>()) return TEXT("html");
		if (Asset.IsA<UNativeWebCssAsset>()) return TEXT("css");
		if (Asset.IsA<UNativeWebJavaScriptAsset>()) return TEXT("js");
		return TEXT("font");
	}

}

bool NativeWebContentAssetActions::IsPathAllowed(const UNativeWebContentAsset& Asset, const FString& Path, FString& OutError)
{
	const FString Extension = FPaths::GetExtension(Path, false).ToLower();
	const FString Kind = ExtensionFor(Asset);
	const bool bAllowed = Kind == TEXT("font")
		? Extension == TEXT("ttf") || Extension == TEXT("otf")
		: Extension == Kind || (Kind == TEXT("html") && Extension == TEXT("htm"));
	if (!bAllowed)
	{
		OutError = FString::Printf(TEXT("This asset accepts a .%s file%s."), *Kind,
		                           Kind == TEXT("font") ? TEXT(" or .otf") : Kind == TEXT("html") ? TEXT(" or .htm") : TEXT(""));
		return false;
	}
	return true;
}

bool NativeWebContentAssetActions::ImportFile(UNativeWebContentAsset& Asset, const FString& Path, FString& OutError)
{
	if (!IsPathAllowed(Asset, Path, OutError)) return false;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("Could not read imported content from '%s'."), *Path);
		return false;
	}
	const FString LeafName = FPaths::GetCleanFilename(Path);
	if (Asset.IsA<UNativeWebFontAsset>())
	{
		if (!UNativeWebFontAsset::ValidatePayload(LeafName, Bytes, OutError)) return false;
	}
	else
	{
		FString Decoded;
		if (!UNativeWebContentAsset::DecodeTextPayload(Bytes, Decoded, OutError)) return false;
	}
	const TArray<uint8>* ExistingBytes = nullptr;
	FString ExistingError;
	if (Asset.TryGetBytes(ExistingBytes, ExistingError) && *ExistingBytes == Bytes && Asset.GetImportedFilename() == LeafName) return true;
	Asset.SetFlags(RF_Transactional);
	FScopedTransaction Transaction(NSLOCTEXT("NativeWeb", "ImportContent", "Import NativeWeb content"));
	Asset.Modify();
	Asset.SetImportedContent(MoveTemp(Bytes), LeafName);
	Asset.PostEditChange();
	Asset.MarkPackageDirty();
	return true;
}

bool NativeWebContentAssetActions::ExportFile(const UNativeWebContentAsset& Asset, const FString& Path, FString& OutError)
{
	if (!IsPathAllowed(Asset, Path, OutError)) return false;
	const TArray<uint8>* Bytes = nullptr;
	if (!Asset.TryGetBytes(Bytes, OutError)) return false;
	if (Bytes->IsEmpty())
	{
		OutError = TEXT("There is no imported content to export.");
		return false;
	}
	if (!FFileHelper::SaveArrayToFile(*Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("Could not write exported content to '%s'."), *Path);
		return false;
	}
	return true;
}

bool NativeWebContentAssetActions::Clear(UNativeWebContentAsset& Asset, FString& OutError)
{
	(void)OutError;
	if (Asset.GetImportedSize() == 0 && Asset.GetImportedFilename().IsEmpty()) return true;
	Asset.SetFlags(RF_Transactional);
	FScopedTransaction Transaction(NSLOCTEXT("NativeWeb", "ClearContent", "Clear NativeWeb content"));
	Asset.Modify();
	Asset.SetImportedContent({}, FString());
	Asset.PostEditChange();
	Asset.MarkPackageDirty();
	return true;
}
