#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/NativeWebContentAssetActions.h"
#include "Assets/NativeWebContentAssets.h"
#include "Assets/NativeWebAppAsset.h"
#include "EditorValidatorSubsystem.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebContentAssetImportExportUndoTest,
	"NativeWeb.Editor.ContentImportExportClearAndTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebContentAssetImportExportUndoTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GEditor || !GEditor->Trans)
	{
		AddWarning(TEXT("Editor transaction buffer is unavailable in this context; transaction assertions are skipped."));
		return true;
	}
	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NativeWebAssetTests"), Unique);
	IFileManager::Get().MakeDirectory(*Root, true);
	const FString ImportPath = FPaths::Combine(Root, TEXT("source.html"));
	const FString ExportPath = FPaths::Combine(Root, TEXT("export.html"));
	TArray<uint8> Original = {0xEF, 0xBB, 0xBF, '<', 'p', '>', 0xC3, 0xA9, '<', '/', 'p', '>'};
	FFileHelper::SaveArrayToFile(Original, *ImportPath);
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/NativeWebAssetTest_%s"), *Unique));
	UNativeWebHtmlAsset* Asset = NewObject<UNativeWebHtmlAsset>(Package, FName(TEXT("Html")), RF_Transactional);
	FString Error;
	TestTrue(TEXT("Matching HTML path is accepted"), NativeWebContentAssetActions::IsPathAllowed(*Asset, ImportPath, Error));
	TestFalse(TEXT("Wrong extension is rejected"), NativeWebContentAssetActions::IsPathAllowed(*Asset, FPaths::Combine(Root, TEXT("bad.js")), Error));
	TestFalse(TEXT("Missing source path is a read failure"), NativeWebContentAssetActions::ImportFile(*Asset, FPaths::Combine(Root, TEXT("missing.html")), Error));
	TestTrue(TEXT("Import preserves BOM and source bytes"), NativeWebContentAssetActions::ImportFile(*Asset, ImportPath, Error));
	TestEqual(TEXT("Imported filename is the source leaf"), Asset->GetImportedFilename(), FString(TEXT("source.html")));
	TestEqual(TEXT("Imported payload byte count is exact"), Asset->GetBytes().Num(), Original.Num());
	TestTrue(TEXT("Import dirties the asset package"), Package->IsDirty());
	TestTrue(TEXT("Export writes the exact original bytes"), NativeWebContentAssetActions::ExportFile(*Asset, ExportPath, Error));
	TArray<uint8> Exported;
	FFileHelper::LoadFileToArray(Exported, *ExportPath);
	TestTrue(TEXT("BOM and UTF-8 bytes round-trip unchanged"), Exported == Original);
	TestTrue(TEXT("Editor transaction undo restores the empty asset"), GEditor->Trans->Undo());
	TestTrue(TEXT("Undo restores both bytes and imported filename"), Asset->GetBytes().IsEmpty() && Asset->GetImportedFilename().IsEmpty());
	TestTrue(TEXT("Editor transaction redo reapplies the import"), GEditor->Trans->Redo());
	TestTrue(TEXT("Redo restores bytes and leaf metadata"), Asset->GetBytes() == Original && Asset->GetImportedFilename() == TEXT("source.html"));
	TestTrue(TEXT("Clear removes imported state"), NativeWebContentAssetActions::Clear(*Asset, Error));
	TestTrue(TEXT("Clear resets bytes and filename"), Asset->GetBytes().IsEmpty() && Asset->GetImportedFilename().IsEmpty());
	TestTrue(TEXT("Clear of an already empty asset is a no-op"), NativeWebContentAssetActions::Clear(*Asset, Error));
	IFileManager::Get().Delete(*ImportPath);
	IFileManager::Get().Delete(*ExportPath);
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebContentAssetFontImportTest,
	"NativeWeb.Editor.FontImportMetadataAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebContentAssetFontImportTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<uint8> FontBytes;
	FontBytes.SetNumZeroed(29);
	FontBytes[1] = 1;
	FontBytes[5] = 1;
	FontBytes[23] = 28;
	FontBytes[27] = 1;
	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NativeWebAssetTests"), Unique);
	IFileManager::Get().MakeDirectory(*Root, true);
	const FString Path = FPaths::Combine(Root, TEXT("Sample.TTF"));
	FFileHelper::SaveArrayToFile(FontBytes, *Path);
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/NativeWebFontTest_%s"), *Unique));
	UNativeWebFontAsset* Asset = NewObject<UNativeWebFontAsset>(Package, FName(TEXT("Font")), RF_Transactional);
	FString Error;
	TestTrue(TEXT("Valid SFNT font import succeeds"), NativeWebContentAssetActions::ImportFile(*Asset, Path, Error));
	TestEqual(TEXT("Original font leaf capitalization is retained"), Asset->GetImportedFilename(), FString(TEXT("Sample.TTF")));
	TestEqual(TEXT("Imported binary bytes are retained"), Asset->GetBytes(), FontBytes);
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().DeleteDirectory(*Root, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppDuplicateAppIdValidationTest,
	"NativeWeb.Editor.DuplicateAppIdDataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppDuplicateAppIdValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GEditor)
	{
		AddError(TEXT("The editor is unavailable for asset validation."));
		return false;
	}
	UEditorValidatorSubsystem* ValidatorSubsystem = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
	if (!ValidatorSubsystem)
	{
		AddError(TEXT("The editor asset validator subsystem is unavailable."));
		return false;
	}

	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString FirstPackageName = FString::Printf(TEXT("/Game/__NativeWebAppValidation_%s_First"), *Unique);
	const FString SecondPackageName = FString::Printf(TEXT("/Game/__NativeWebAppValidation_%s_Second"), *Unique);
	TestTrue(TEXT("First fixture path is a valid long package name"), FPackageName::IsValidLongPackageName(FirstPackageName));
	TestTrue(TEXT("Second fixture path is a valid long package name"), FPackageName::IsValidLongPackageName(SecondPackageName));

	TStrongObjectPtr<UPackage> FirstPackage(CreatePackage(*FirstPackageName));
	TStrongObjectPtr<UPackage> SecondPackage(CreatePackage(*SecondPackageName));
	TestNotNull(TEXT("First unsaved fixture package is created"), FirstPackage.Get());
	TestNotNull(TEXT("Second unsaved fixture package is created"), SecondPackage.Get());
	if (!FirstPackage.IsValid() || !SecondPackage.IsValid()) return false;

	TStrongObjectPtr<UNativeWebAppAsset> FirstApp(
		NewObject<UNativeWebAppAsset>(FirstPackage.Get(), FName(TEXT("FirstApp")), RF_Public | RF_Standalone));
	TStrongObjectPtr<UNativeWebAppAsset> SecondApp(
		NewObject<UNativeWebAppAsset>(SecondPackage.Get(), FName(TEXT("SecondApp")), RF_Public | RF_Standalone));
	TestNotNull(TEXT("First unsaved app asset is created"), FirstApp.Get());
	TestNotNull(TEXT("Second unsaved app asset is created"), SecondApp.Get());
	if (!FirstApp.IsValid() || !SecondApp.IsValid()) return false;

	const FName SharedAppId(*FString::Printf(TEXT("NativeWebCoverage_%s"), *Unique));
	FirstApp->AppId = SharedAppId;
	SecondApp->AppId = FName(*FString::Printf(TEXT("NativeWebCoverage_Other_%s"), *Unique));
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const EDataValidationResult UniqueResult = ValidatorSubsystem->IsObjectValid(
		FirstApp.Get(), Errors, Warnings, EDataValidationUsecase::Script);
	TestTrue(TEXT("A unique app ID validates successfully"), UniqueResult == EDataValidationResult::Valid);
	TestTrue(TEXT("A unique app ID produces no validation errors"), Errors.IsEmpty());
	TestTrue(TEXT("A unique app ID produces no validation warnings"), Warnings.IsEmpty());

	SecondApp->AppId = FName(*SharedAppId.ToString().ToLower());
	Errors.Reset();
	Warnings.Reset();
	const EDataValidationResult DuplicateResult = ValidatorSubsystem->IsObjectValid(
		FirstApp.Get(), Errors, Warnings, EDataValidationUsecase::Script);
	TestTrue(TEXT("A second unsaved asset with the same ID invalidates the first"),
		DuplicateResult == EDataValidationResult::Invalid);
	const FString SharedIdText = SharedAppId.ToString();
	const FString ConflictingObjectPath = SecondApp->GetPathName();
	bool bErrorContainsSharedId = false;
	bool bErrorContainsConflictingPath = false;
	bool bErrorContainsDuplicateDiagnostic = false;
	for (const FText& Error : Errors)
	{
		const FString ErrorText = Error.ToString();
		bErrorContainsSharedId |= ErrorText.Contains(SharedIdText);
		bErrorContainsConflictingPath |= ErrorText.Contains(ConflictingObjectPath);
		bErrorContainsDuplicateDiagnostic |= ErrorText.Contains(TEXT("already used by"));
	}
	TestTrue(TEXT("Duplicate diagnostic names the shared app ID"), bErrorContainsSharedId);
	TestTrue(TEXT("Duplicate diagnostic identifies the conflicting unsaved object path"), bErrorContainsConflictingPath);
	TestTrue(TEXT("Duplicate diagnostic explains the ID is already used"), bErrorContainsDuplicateDiagnostic);

	SecondApp->AppId = FName(*FString::Printf(TEXT("NativeWebCoverage_Other_%s"), *Unique));
	Errors.Reset();
	Warnings.Reset();
	const EDataValidationResult RestoredResult = ValidatorSubsystem->IsObjectValid(
		FirstApp.Get(), Errors, Warnings, EDataValidationUsecase::Script);
	TestTrue(TEXT("Changing the conflicting ID restores valid status"), RestoredResult == EDataValidationResult::Valid);
	TestTrue(TEXT("Restored validation leaves no errors"), Errors.IsEmpty());

	FirstApp->AppId = NAME_None;
	Errors.Reset();
	Warnings.Reset();
	const EDataValidationResult MissingIdResult = ValidatorSubsystem->IsObjectValid(
		FirstApp.Get(), Errors, Warnings, EDataValidationUsecase::Script);
	TestTrue(TEXT("A blank AppId is invalid"), MissingIdResult == EDataValidationResult::Invalid);
	bool bErrorContainsMissingIdDiagnostic = false;
	for (const FText& Error : Errors)
	{
		bErrorContainsMissingIdDiagnostic |= Error.ToString().Contains(TEXT("AppId must be set to a non-empty value."));
	}
	TestTrue(TEXT("Blank AppId returns its source-specific diagnostic"), bErrorContainsMissingIdDiagnostic);

	FirstApp->ClearFlags(RF_Standalone);
	SecondApp->ClearFlags(RF_Standalone);
	FirstPackage->SetDirtyFlag(false);
	SecondPackage->SetDirtyFlag(false);
	return true;
}

#endif
