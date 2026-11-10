#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "AffineUiScriptInstance.h"
#include "DynamicRHI.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffineUiScriptInstanceBridgeTest,
	"NativeWeb.AffineUi.QuickJS.BridgeLifecycleAndTextUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffineUiScriptInstanceBridgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11)
	{
		AddWarning(TEXT("This test requires an isolated D3D11 editor session (-dx11)."));
		return true;
	}
	FAffineUiSurfaceDesc Desc;
	Desc.Content.Html = TEXT("<html><body><p id=\"status\">initial</p></body></html>");
	FString Error;
	TUniquePtr<FAffineUiScriptInstance> Instance = FAffineUiScriptInstance::Create(Desc, Error);
	if (!TestTrue(TEXT("QuickJS and AffineUI are created together"), Instance.IsValid())) { AddError(Error); return false; }
	FQuickJSError JsError;
	TestTrue(TEXT("A text mutation is accepted"), Instance->Evaluate(TEXT("if (nativeWeb.setText('status', 'first') !== true) throw new Error('first mutation rejected')"), JsError));
	TestTrue(TEXT("A later update to the same stable element ID is accepted"), Instance->Evaluate(TEXT("if (nativeWeb.setText('status', 'updated') !== true) throw new Error('incremental update rejected')"), JsError));
	TestFalse(TEXT("An empty element ID is rejected"), Instance->Evaluate(TEXT("nativeWeb.setText('', 'invalid')"), JsError));
	TestFalse(TEXT("Non-string bridge arguments are rejected"), Instance->Evaluate(TEXT("nativeWeb.setText(17, 'invalid')"), JsError));
	TestFalse(TEXT("JavaScript exceptions are reported"), Instance->Evaluate(TEXT("throw new Error('fixture failure')"), JsError));
	TestTrue(TEXT("The JavaScript error is surfaced"), JsError.Message.Contains(TEXT("fixture failure")));
	TestTrue(TEXT("JavaScript can enqueue a deferred document update"), Instance->Evaluate(TEXT("Promise.resolve().then(() => nativeWeb.setText('status', 'promise update'))"), JsError));
	int32 JobsRun = 0;
	bool bPending = false;
	TestTrue(TEXT("Pending JavaScript jobs can be explicitly pumped"), Instance->PumpJobs(32, JobsRun, bPending, JsError));
	TestTrue(TEXT("The pending promise job executed"), JobsRun > 0);
	Instance->Close();
	Instance->Close();
	TestFalse(TEXT("Closed instances reject evaluation"), Instance->Evaluate(TEXT("1 + 1"), JsError));
	Instance.Reset();
	Instance = FAffineUiScriptInstance::Create(Desc, Error);
	TestTrue(TEXT("A new bridge instance can be created after teardown"), Instance.IsValid());
	if (Instance) { Instance->Close(); }
	return true;
}

#endif
