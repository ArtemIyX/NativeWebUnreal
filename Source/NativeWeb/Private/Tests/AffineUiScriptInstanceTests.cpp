#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "AffineUiScriptInstance.h"
#include "DynamicRHI.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

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
	Desc.Content.Html = TEXT("<html><body><main id=\"app\"></main><p id=\"status\">initial</p></body></html>");
	FString Error;
	TUniquePtr<FAffineUiScriptInstance> Instance = FAffineUiScriptInstance::Create(Desc, Error);
	if (!TestTrue(TEXT("QuickJS and AffineUI are created together"), Instance.IsValid())) { AddError(Error); return false; }
	const uint64 InitialDocumentEpoch = Instance->GetSurface()->GetDocumentEpoch();
	FQuickJSError JsError;
	TestTrue(TEXT("A text mutation is accepted"), Instance->Evaluate(TEXT("if (nativeWeb.setText('status', 'first') !== true) throw new Error('first mutation rejected')"), JsError));
	TestTrue(TEXT("A later update to the same stable element ID is accepted"), Instance->Evaluate(TEXT("if (nativeWeb.setText('status', 'updated') !== true) throw new Error('incremental update rejected')"), JsError));
	TestFalse(TEXT("An empty element ID is rejected"), Instance->Evaluate(TEXT("nativeWeb.setText('', 'invalid')"), JsError));
	TestFalse(TEXT("Non-string bridge arguments are rejected"), Instance->Evaluate(TEXT("nativeWeb.setText(17, 'invalid')"), JsError));
	TestFalse(TEXT("JavaScript exceptions are reported"), Instance->Evaluate(TEXT("throw new Error('fixture failure')"), JsError));
	TestTrue(TEXT("The JavaScript error is surfaced"), JsError.Message.Contains(TEXT("fixture failure")));
	const TCHAR* Fixture = TEXT("const root=nativeWeb.mountRoot('app'); const counter=nativeWeb.createElement('span'); const count=nativeWeb.createText('0'); nativeWeb.insertBefore(root,counter,null); nativeWeb.insertBefore(counter,count,null); const other=nativeWeb.createElement('b'); nativeWeb.insertBefore(root,other,null); nativeWeb.insertBefore(root,counter,other); Promise.resolve().then(()=>{ if(!nativeWeb.setNodeText(count,'1')) throw new Error('counter update rejected'); globalThis.updated=true; });");
	TestTrue(TEXT("The fixture creates a counter through the ordered node bridge"), Instance->Evaluate(Fixture, JsError));
	int32 JobsRun = 0;
	bool bPending = false;
	TestTrue(TEXT("Pending JavaScript jobs can be explicitly pumped"), Instance->PumpJobs(32, JobsRun, bPending, JsError));
	TestTrue(TEXT("The pending promise job executed"), JobsRun > 0);
	TestFalse(TEXT("The fixture leaves no Promise jobs pending"), bPending);
	TestTrue(TEXT("The Promise job queued the counter text update"), Instance->Evaluate(TEXT("if (globalThis.updated !== true) throw new Error('Promise mutation did not run')"), JsError));
	TestEqual(TEXT("Incremental mutations do not replace the document"), Instance->GetSurface()->GetDocumentEpoch(), InitialDocumentEpoch);
	Instance->Close();
	Instance->Close();
	TestFalse(TEXT("Closed instances reject evaluation"), Instance->Evaluate(TEXT("1 + 1"), JsError));
	Instance.Reset();
	Instance = FAffineUiScriptInstance::Create(Desc, Error);
	TestTrue(TEXT("A new bridge instance can be created after teardown"), Instance.IsValid());
	if (Instance) { Instance->Close(); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffineUiVueBundleTest,
	"NativeWeb.AffineUi.QuickJS.VueAppBundle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffineUiVueBundleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11)
	{
		AddWarning(TEXT("This test requires an isolated D3D11 editor session (-dx11)."));
		return true;
	}
	const FString AssetDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("NativeWeb/Vue"));
	const FString ScriptPath = FPaths::Combine(AssetDirectory, TEXT("ui.js"));
	const FString CssPath = FPaths::Combine(AssetDirectory, TEXT("ui.css"));
	FString ScriptSource;
	FString CssSource;
	TestTrue(TEXT("The Vue JS bundle is staged as project content"), FFileHelper::LoadFileToString(ScriptSource, *ScriptPath));
	TestTrue(TEXT("The Vue stylesheet is staged as project content"), FFileHelper::LoadFileToString(CssSource, *CssPath));
	if (ScriptSource.IsEmpty() || CssSource.IsEmpty()) return false;
	TestTrue(TEXT("The staged stylesheet includes the shared app card"), CssSource.Contains(TEXT(".counter-card"), ESearchCase::CaseSensitive));
	FAffineUiSurfaceDesc Desc;
	Desc.Content.Html = TEXT("<html><body><div id=\"app\"></div></body></html>");
	Desc.Content.Css = CssSource;
	FString Error;
	TUniquePtr<FAffineUiScriptInstance> Instance = FAffineUiScriptInstance::Create(Desc, Error);
	if (!TestTrue(TEXT("Vue bundle surface and QuickJS instance are created"), Instance.IsValid()))
	{
		AddError(Error);
		return false;
	}
	const uint64 InitialDocumentEpoch = Instance->GetSurface()->GetDocumentEpoch();
	FQuickJSError JsError;
	if (!TestTrue(TEXT("The shared Vue app bundle evaluates in QuickJS"), Instance->Evaluate(ScriptSource, JsError, ScriptPath)))
	{
		AddError(JsError.Message);
		return false;
	}
	TestTrue(TEXT("The Vue app exposes the NativeWeb click entry point"), Instance->Evaluate(
		TEXT("if (typeof globalThis.__nativeWebClick !== 'function') throw new Error('NativeWeb click entry point is missing')"), JsError));
	if (!JsError.Message.IsEmpty()) AddError(JsError.Message);
	int32 JobsExecuted = 0;
	bool bPending = false;
	TestTrue(TEXT("Vue Promise and scheduler jobs pump successfully"), Instance->PumpJobs(256, JobsExecuted, bPending, JsError));
	if (!JsError.Message.IsEmpty()) AddError(JsError.Message);
	TestFalse(TEXT("The Vue app leaves no pending jobs after mounting"), bPending);
	TestEqual(TEXT("Vue mutations preserve the loaded document"), Instance->GetSurface()->GetDocumentEpoch(), InitialDocumentEpoch);
	Instance->Close();
	return true;
}

#endif
