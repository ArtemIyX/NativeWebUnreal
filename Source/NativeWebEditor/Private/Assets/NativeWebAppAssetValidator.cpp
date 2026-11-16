#include "Assets/NativeWebAppAssetValidator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Assets/NativeWebAppAsset.h"
#include "Assets/NativeWebContentAssets.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "NativeWebAppAssetValidator"

bool UNativeWebAppAssetValidator::CanValidateAsset_Implementation(const FAssetData& AssetData, UObject* Asset,
	FDataValidationContext& Context) const
{
	(void)Asset;
	(void)Context;
	return !AssetData.IsRedirector() && AssetData.IsInstanceOf(UNativeWebAppAsset::StaticClass());
}

EDataValidationResult UNativeWebAppAssetValidator::ValidateLoadedAsset_Implementation(
	const FAssetData& AssetData, UObject* Asset, FDataValidationContext& Context)
{
	(void)AssetData;
	(void)Context;
	UNativeWebAppAsset* App = Cast<UNativeWebAppAsset>(Asset);
	if (!IsValid(App) || App->HasAnyFlags(RF_Transient | RF_ClassDefaultObject) ||
		!App->GetOutermost() || App->GetOutermost() == GetTransientPackage())
	{
		return EDataValidationResult::NotValidated;
	}
	if (App->AppId.IsNone())
	{
		AssetFails(App, LOCTEXT("MissingAppId", "AppId must be set to a non-empty value."));
		return EDataValidationResult::Invalid;
	}

	TSet<FString> ConflictingPaths;
	TSet<FString> LoadedPaths;
	for (TObjectIterator<UNativeWebAppAsset> It; It; ++It)
	{
		UNativeWebAppAsset* Other = *It;
		if (!IsValid(Other) || Other == App || Other->HasAnyFlags(RF_Transient | RF_ClassDefaultObject) ||
			!Other->GetOutermost() || Other->GetOutermost() == GetTransientPackage()) continue;
		const FString OtherPath = Other->GetPathName();
		LoadedPaths.Add(OtherPath);
		if (Other->AppId == App->AppId) ConflictingPaths.Add(OtherPath);
	}

	FAssetRegistryModule& RegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AppAssets;
	RegistryModule.Get().GetAssetsByClass(UNativeWebAppAsset::StaticClass()->GetClassPathName(), AppAssets, true);
	for (const FAssetData& OtherData : AppAssets)
	{
		const FString OtherPath = OtherData.GetObjectPathString();
		if (OtherPath == App->GetPathName() || LoadedPaths.Contains(OtherPath)) continue;
		FName RegisteredId = NAME_None;
		if (OtherData.GetTagValue<FName>(TEXT("AppId"), RegisteredId) && RegisteredId == App->AppId)
		{
			if (!ConflictingPaths.Contains(OtherPath)) ConflictingPaths.Add(OtherPath);
		}
	}
	if (!ConflictingPaths.IsEmpty())
	{
		TArray<FString> SortedPaths = ConflictingPaths.Array();
		SortedPaths.Sort();
		AssetFails(App, FText::Format(LOCTEXT("DuplicateAppId", "AppId '{0}' is already used by {1}."),
		                              FText::FromName(App->AppId), FText::FromString(SortedPaths[0])));
		return EDataValidationResult::Invalid;
	}

	const bool bKnownHtmlMode = App->HtmlMode == ENativeWebHtmlSourceMode::Default ||
	                            App->HtmlMode == ENativeWebHtmlSourceMode::String ||
	                            App->HtmlMode == ENativeWebHtmlSourceMode::Asset;
	const bool bKnownOptionalModes =
		(App->CssMode == ENativeWebOptionalSourceMode::None || App->CssMode == ENativeWebOptionalSourceMode::String ||
		 App->CssMode == ENativeWebOptionalSourceMode::Asset) &&
		(App->JavaScriptMode == ENativeWebOptionalSourceMode::None ||
		 App->JavaScriptMode == ENativeWebOptionalSourceMode::String ||
		 App->JavaScriptMode == ENativeWebOptionalSourceMode::Asset);
	const bool bMissingSelection =
		(App->HtmlMode == ENativeWebHtmlSourceMode::Asset && App->HtmlAsset.IsNull()) ||
		(App->CssMode == ENativeWebOptionalSourceMode::Asset && App->CssAsset.IsNull()) ||
		(App->JavaScriptMode == ENativeWebOptionalSourceMode::Asset && App->JavaScriptAsset.IsNull()) ||
		App->Fonts.ContainsByPredicate([](const TSoftObjectPtr<UNativeWebFontAsset>& Font) { return Font.IsNull(); });
	if (!bKnownHtmlMode || !bKnownOptionalModes || bMissingSelection)
	{
		AssetFails(App, LOCTEXT("InvalidSelection", "A selected source mode or soft asset reference is invalid."));
		return EDataValidationResult::Invalid;
	}
	TMap<FString, FString> FontOwners;
	for (const TSoftObjectPtr<UNativeWebFontAsset>& FontReference : App->Fonts)
	{
		const UNativeWebFontAsset* Font = FontReference.Get();
		if (!Font) continue;
		const FString Filename = Font->GetImportedFilename();
		FString Reason;
		const TArray<uint8>* FontBytes = nullptr;
		if (!Font->TryGetBytes(FontBytes, Reason) || !UNativeWebFontAsset::ValidatePayload(Filename, *FontBytes, Reason))
		{
			AssetFails(App, FText::FromString(Reason));
			return EDataValidationResult::Invalid;
		}
		const FString Key = Filename.ToLower();
		if (const FString* First = FontOwners.Find(Key))
		{
			AssetFails(App, FText::Format(LOCTEXT("DuplicateFont", "Font filename '{0}' collides with '{1}'."),
			                              FText::FromString(Filename), FText::FromString(*First)));
			return EDataValidationResult::Invalid;
		}
		FontOwners.Add(Key, Font->GetPathName());
	}
	AssetPasses(App);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
