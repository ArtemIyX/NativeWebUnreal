#if WITH_DEV_AUTOMATION_TESTS

#include "Input/NativeWebInputRouting.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebInputRoutingConsolePolicyTest,
	"NativeWeb.Input.ConsolePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebInputRoutingConsolePolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestFalse(TEXT("NativeWeb input remains enabled when the console is inactive"),
	          NativeWebInputRouting::ShouldSuppressInput(false));
	TestTrue(TEXT("NativeWeb input is suppressed when the console is active"),
	         NativeWebInputRouting::ShouldSuppressInput(true));
	TestFalse(TEXT("Missing player state fails open safely"),
	          NativeWebInputRouting::IsConsoleActive(nullptr));
	return true;
}

#endif
