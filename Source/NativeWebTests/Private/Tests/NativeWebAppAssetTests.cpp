#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/NativeWebAppAsset.h"
#include "Assets/NativeWebAppSource.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppAssetDefaultsAndIdTest,
	"NativeWeb.AppAsset.DefaultsAndStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppAssetDefaultsAndIdTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UNativeWebAppAsset* App = NewObject<UNativeWebAppAsset>();
	TestEqual(TEXT("HTML defaults to the standard shell"), App->HtmlMode, ENativeWebHtmlSourceMode::Default);
	TestEqual(TEXT("CSS defaults to omitted"), App->CssMode, ENativeWebOptionalSourceMode::None);
	TestEqual(TEXT("JavaScript defaults to omitted"), App->JavaScriptMode, ENativeWebOptionalSourceMode::None);
	TestFalse(TEXT("Startup evaluation defaults off"), App->bEvaluateJavaScriptOnStart);
	TestFalse(TEXT("NAME_None produces an invalid primary ID"), App->GetPrimaryAssetId().IsValid());
	App->AppId = TEXT("MainMenu");
	const FPrimaryAssetId Id = App->GetPrimaryAssetId();
	TestTrue(TEXT("Non-empty AppId produces a primary ID"), Id.IsValid());
	TestEqual(TEXT("Primary type is stable"), Id.PrimaryAssetType, FName(TEXT("NativeWebApp")));
	TestEqual(TEXT("Primary name comes from AppId"), Id.PrimaryAssetName, FName(TEXT("MainMenu")));
	App->Rename(TEXT("RenamedObject"));
	TestTrue(TEXT("Primary ID does not depend on UObject name"), App->GetPrimaryAssetId() == Id);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppSourceSnapshotModesTest,
	"NativeWeb.AppAsset.SourceModesAndInactiveFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppSourceSnapshotModesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UNativeWebAppAsset* App = NewObject<UNativeWebAppAsset>();
	App->AppId = TEXT("InlineApp");
	App->HtmlString = TEXT("<main>Inline</main>");
	App->HtmlAsset = TSoftObjectPtr<UNativeWebHtmlAsset>(FSoftObjectPath(TEXT("/Game/Missing.Html")));
	App->CssMode = ENativeWebOptionalSourceMode::String;
	App->CssString = TEXT("");
	App->JavaScriptMode = ENativeWebOptionalSourceMode::String;
	App->JavaScriptString = TEXT("");
	App->bEvaluateJavaScriptOnStart = true;
	FNativeWebAppSourceSnapshot Snapshot;
	FNativeWebError Error;
	TestTrue(TEXT("Inactive stale HTML asset is ignored"), NativeWebAppSource::BuildSnapshot(*App, Snapshot, Error));
	TestTrue(TEXT("No asset dependency is queued for inactive HTML asset"), Snapshot.LoadPaths.IsEmpty());
	FNativeWebResolvedAppSource Source;
	TestTrue(TEXT("Selected empty CSS and JavaScript strings are valid"), NativeWebAppSource::ResolveSnapshot(Snapshot, Source, Error));
	TestTrue(TEXT("Selected empty JavaScript remains present"), Source.bHasJavaScript);
	TestEqual(TEXT("Empty JavaScript can use the app ID source name"), Source.JavaScriptSourceName,
	          FString(TEXT("<app-id:InlineApp:JavaScript>")));
	TestTrue(TEXT("Default HTML was chosen"), Source.Html.Contains(TEXT("id=\"app\"")));
	TArray<uint8> Utf8Bytes;
	const uint8 Encoded[] = {0xEF, 0xBB, 0xBF, 'H', 0xC3, 0xA9};
	Utf8Bytes.Append(Encoded, UE_ARRAY_COUNT(Encoded));
	FString Decoded;
	FString Reason;
	TestTrue(TEXT("UTF-8 BOM and Unicode decode without losing source bytes"),
	         UNativeWebContentAsset::DecodeTextPayload(Utf8Bytes, Decoded, Reason));
	TestEqual(TEXT("UTF-8 Unicode text is decoded"), Decoded, FString(TEXT("Hé")));
	Utf8Bytes = {0xC0, 0xAF};
	TestFalse(TEXT("Overlong UTF-8 encoding is rejected"),
	          UNativeWebContentAsset::DecodeTextPayload(Utf8Bytes, Decoded, Reason));
	Utf8Bytes = {'A', 0, 'B'};
	TestFalse(TEXT("Embedded NUL is rejected"), UNativeWebContentAsset::DecodeTextPayload(Utf8Bytes, Decoded, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppSourceValidationTest,
	"NativeWeb.AppAsset.FontValidationAndSelectedReferenceErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppSourceValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<uint8> ValidFont;
	ValidFont.SetNumZeroed(29);
	ValidFont[0] = 0;
	ValidFont[1] = 1;
	ValidFont[4] = 0;
	ValidFont[5] = 1;
	ValidFont[20] = 0;
	ValidFont[21] = 0;
	ValidFont[22] = 0;
	ValidFont[23] = 28;
	ValidFont[24] = 0;
	ValidFont[25] = 0;
	ValidFont[26] = 0;
	ValidFont[27] = 1;
	FString Reason;
	TestTrue(TEXT("Bounded TrueType SFNT table is accepted"), NativeWebAppSource::IsValidFont(TEXT("DM-Sans.ttf"), ValidFont, Reason));
	TestFalse(TEXT("Font paths are rejected"), NativeWebAppSource::IsValidFont(TEXT("../font.ttf"), ValidFont, Reason));
	TestFalse(TEXT("Bad font extension is rejected"), NativeWebAppSource::IsValidFont(TEXT("font.woff"), ValidFont, Reason));
	ValidFont[24] = 0xFF;
	TestFalse(TEXT("Out of bounds table data is rejected"), NativeWebAppSource::IsValidFont(TEXT("font.ttf"), ValidFont, Reason));

	UNativeWebAppAsset* App = NewObject<UNativeWebAppAsset>();
	App->AppId = TEXT("MissingHtml");
	App->HtmlMode = ENativeWebHtmlSourceMode::Asset;
	App->HtmlAsset = TSoftObjectPtr<UNativeWebHtmlAsset>(FSoftObjectPath(TEXT("/Game/Missing.Html")));
	FNativeWebAppSourceSnapshot Snapshot;
	FNativeWebError Error;
	TestTrue(TEXT("Selected soft reference is accepted into snapshot"), NativeWebAppSource::BuildSnapshot(*App, Snapshot, Error));
	TestEqual(TEXT("Selected path is retained"), Snapshot.LoadPaths.Num(), 1);
	FNativeWebResolvedAppSource Source;
	TestFalse(TEXT("Unloaded selected HTML fails with source-specific error"), NativeWebAppSource::ResolveSnapshot(Snapshot, Source, Error));
	TestEqual(TEXT("Selected HTML reports load failure"), Error.Code, FName(TEXT("HtmlAssetLoadFailed")));
	return true;
}

#endif
