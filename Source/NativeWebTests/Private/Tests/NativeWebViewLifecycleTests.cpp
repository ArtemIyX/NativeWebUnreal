#if WITH_DEV_AUTOMATION_TESTS

#include "NativeWebView.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebViewExplicitCloseTest,
	"NativeWeb.View.ExplicitCloseIsIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebViewExplicitCloseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UNativeWebView* View = NewObject<UNativeWebView>(GetTransientPackage());
	TestNotNull(TEXT("A view UObject can be allocated before renderer creation"), View);
	if (!View) return false;
	TestTrue(TEXT("Uninitialized view rejects producer messages"),
		View->EnqueueStringToJavaScript(NAME_None, TEXT("early")) == ENativeWebQueueResult::RESULT_CLOSED);
	View->Close();
	View->Close();
	TestTrue(TEXT("Repeated explicit close leaves the view closed"), View->IsClosed());
	TestNull(TEXT("Closed view has no output texture"), View->GetTexture());
	TestTrue(TEXT("Closed view rejects later producer messages"),
		View->EnqueueBytesToJavaScript(NAME_None, TArray<uint8>()) == ENativeWebQueueResult::RESULT_CLOSED);
	return true;
}

#endif
