

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS


#include "DynamicRHI.h"
#include "AffineUI/AffineUiScriptInstance.h"
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
	if (!TestTrue(TEXT("QuickJS and AffineUI are created together"), Instance.IsValid()))
	{
		AddError(Error);
		return false;
	}
	const uint64 InitialDocumentEpoch = Instance->GetSurface()->GetDocumentEpoch();
	FQuickJSError JsError;
	TestTrue(TEXT("A text mutation is accepted"), Instance->Evaluate(
		         TEXT("if (nativeWeb.setText('status', 'first') !== true) throw new Error('first mutation rejected')"),
		         JsError));
	TestTrue(TEXT("A later update to the same stable element ID is accepted"), Instance->Evaluate(
		         TEXT(
			         "if (nativeWeb.setText('status', 'updated') !== true) throw new Error('incremental update rejected')"),
		         JsError));
	TestFalse(TEXT("An empty element ID is rejected"), Instance->Evaluate(
		          TEXT("nativeWeb.setText('', 'invalid')"), JsError));
	TestFalse(TEXT("Non-string bridge arguments are rejected"), Instance->Evaluate(
		          TEXT("nativeWeb.setText(17, 'invalid')"), JsError));
	TestFalse(TEXT("JavaScript exceptions are reported"), Instance->Evaluate(
		          TEXT("throw new Error('fixture failure')"), JsError));
	TestTrue(TEXT("The JavaScript error is surfaced"), JsError.Message.Contains(TEXT("fixture failure")));
	const TCHAR* Fixture = TEXT(
		"const root=nativeWeb.mountRoot('app'); const counter=nativeWeb.createElement('span'); const count=nativeWeb.createText('0'); nativeWeb.insertBefore(root,counter,null); nativeWeb.insertBefore(counter,count,null); const other=nativeWeb.createElement('b'); nativeWeb.insertBefore(root,other,null); nativeWeb.insertBefore(root,counter,other); Promise.resolve().then(()=>{ if(!nativeWeb.setNodeText(count,'1')) throw new Error('counter update rejected'); globalThis.updated=true; });");
	TestTrue(
		TEXT("The fixture creates a counter through the ordered node bridge"), Instance->Evaluate(Fixture, JsError));
	int32 JobsRun = 0;
	bool bPending = false;
	TestTrue(
		TEXT("Pending JavaScript jobs can be explicitly pumped"), Instance->PumpJobs(32, JobsRun, bPending, JsError));
	TestTrue(TEXT("The pending promise job executed"), JobsRun > 0);
	TestFalse(TEXT("The fixture leaves no Promise jobs pending"), bPending);
	TestTrue(TEXT("The Promise job queued the counter text update"), Instance->Evaluate(
		         TEXT("if (globalThis.updated !== true) throw new Error('Promise mutation did not run')"), JsError));
	TestEqual(TEXT("Incremental mutations do not replace the document"), Instance->GetSurface()->GetDocumentEpoch(),
	          InitialDocumentEpoch);
	Instance->Close();
	Instance->Close();
	TestFalse(TEXT("Closed instances reject evaluation"), Instance->Evaluate(TEXT("1 + 1"), JsError));
	Instance.Reset();
	Instance = FAffineUiScriptInstance::Create(Desc, Error);
	TestTrue(TEXT("A new bridge instance can be created after teardown"), Instance.IsValid());
	if (Instance) { Instance->Close(); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffineUiMessageBridgeTest,
                                 "NativeWeb.AffineUi.QuickJS.TwoWayMessagingAndBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)



bool FAffineUiMessageBridgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11)
	{
		AddWarning(TEXT("This test requires an isolated D3D11 editor session (-dx11)."));
		return true;
	}
	FAffineUiSurfaceDesc Desc;
	Desc.Content.Html = TEXT("<html><body></body></html>");
	FString CreateError;
	TUniquePtr<FAffineUiScriptInstance> Instance = FAffineUiScriptInstance::Create(Desc, CreateError);
	if (!TestTrue(TEXT("Messaging host is created"), Instance.IsValid()))
	{
		AddError(CreateError);
		return false;
	}

	FQuickJSError JsError;
	const TCHAR* InboundFixture = TEXT(
		"if(!nativeWeb.postString('hello \\u{1F30D}','greeting'))throw new Error('string post rejected'); const buffer=new Uint8Array([99,7,8,100]).buffer; if(!nativeWeb.postBytes(new DataView(buffer,1,2),'packet'))throw new Error('DataView post rejected'); const typed=new Uint8Array([99,7,8,100]); if(!nativeWeb.postBytes(typed.subarray(1,3),'typed-offset'))throw new Error('typed array view post rejected'); if(!nativeWeb.postBytes(new Uint16Array([0x1234])))throw new Error('typed array post rejected');");
	const bool bInboundAccepted = Instance->Evaluate(InboundFixture, JsError);
	if (!bInboundAccepted) AddError(JsError.Message);
	TestTrue(TEXT("JS string and byte posts are accepted"), bInboundAccepted);
	TArray<FNativeWebMessage> Inbound;
	TestEqual(
		TEXT("Four copied JS messages are available after evaluation returns"),
		Instance->DrainMessagesFromJavaScript(Inbound, 64), 4);
	if (Inbound.Num() == 4)
	{
		TestEqual(TEXT("String channel is preserved"), Inbound[0].Channel, FName(TEXT("greeting")));
		TestEqual(TEXT("Unicode string is preserved"), Inbound[0].String, FString(TEXT("hello 🌍")));
		TestTrue(TEXT("DataView offset and length are honored"), Inbound[1].Bytes == TArray<uint8>({7, 8}));
		TestEqual(TEXT("Typed array subview offset and length are honored"), Inbound[2].Bytes.Num(), 2);
		TestEqual(TEXT("Multi-byte typed arrays copy their byte length"), Inbound[3].Bytes.Num(), 2);
	}

	TestFalse(TEXT("Oversized JS string throws RangeError"), Instance->Evaluate(
		          TEXT("nativeWeb.postString('x'.repeat(1024*1024+1))"), JsError));
	TestTrue(TEXT("Oversized JS payload reports RangeError"), JsError.Message.Contains(TEXT("1 MiB")));
	TestTrue(TEXT("A full inbound queue applies back-pressure with false"), Instance->Evaluate(
		         TEXT(
			         "globalThis.queueAccepted=true; for(let i=0;i<65;i++){if(!nativeWeb.postString('q')){queueAccepted=false;break}} if(queueAccepted)throw new Error('queue did not fill')"),
		         JsError));
	TArray<FNativeWebMessage> Bounded;
	TestEqual(TEXT("At most 64 messages drain from a JS burst"), Instance->DrainMessagesFromJavaScript(Bounded, 1000),
	          64);

	TestTrue(TEXT("Listener API, including unsubscribe, is installed"), Instance->Evaluate(
		         TEXT(
			         "globalThis.listenerCount=0;globalThis.lastMessage=null;globalThis.stop=nativeWeb.addMessageListener(m=>{listenerCount++;lastMessage=m});nativeWeb.addMessageListener(()=>{throw new Error('listener boom')});nativeWeb.addMessageListener(m=>{globalThis.afterThrow=m.type})"),
		         JsError));
	FNativeWebMessage Outbound;
	Outbound.Type = ENativeWebPayloadType::PAYLOAD_BYTES;
	Outbound.Channel = TEXT("binary");
	Outbound.Bytes = {4, 5, 6};
	TestTrue(
		TEXT("Outbound message enters bounded queue"),
		Instance->QueueToJavaScript(Outbound) == ENativeWebQueueResult::RESULT_QUEUED);
	TArray<FNativeWebMessage> ToJavaScript;
	TestEqual(
		TEXT("Outbound message is drained on the game thread"), Instance->DrainMessagesToJavaScript(ToJavaScript, 64),
		1);
	TestFalse(
		TEXT("Listener exception is reported after remaining listeners run"),
		Instance->DeliverMessageToJavaScript(ToJavaScript[0], JsError));
	TestTrue(TEXT("Listener error is surfaced"), JsError.Message.Contains(TEXT("listener boom")));
	TestTrue(TEXT("Bytes and later listeners are still delivered"), Instance->Evaluate(
		         TEXT(
			         "if(lastMessage.type!=='bytes'||lastMessage.channel!=='binary'||lastMessage.value.length!==3||lastMessage.value[2]!==6||afterThrow!=='bytes')throw new Error('outbound bytes mismatch')"),
		         JsError));
	TestTrue(TEXT("Listener can unsubscribe more than once"), Instance->Evaluate(TEXT("stop();stop()"), JsError));
	FNativeWebMessage AfterUnsubscribe;
	AfterUnsubscribe.Type = ENativeWebPayloadType::PAYLOAD_STRING;
	AfterUnsubscribe.String = TEXT("second");
	TestTrue(
		TEXT("Second outbound message is queued"),
		Instance->QueueToJavaScript(AfterUnsubscribe) == ENativeWebQueueResult::RESULT_QUEUED);
	ToJavaScript.Reset();
	Instance->DrainMessagesToJavaScript(ToJavaScript, 64);
	TestFalse(
		TEXT("Remaining listener errors do not prevent dispatch"),
		Instance->DeliverMessageToJavaScript(ToJavaScript[0], JsError));
	TestTrue(TEXT("Unsubscribed callback is no longer invoked"), Instance->Evaluate(
		         TEXT("if(listenerCount!==1)throw new Error('unsubscribed listener was called')"), JsError));
	for (int32 Index = 0; Index < 64; ++Index)
		TestTrue(
			TEXT("Outbound queue accepts its configured capacity"),
			Instance->QueueToJavaScript(Outbound) == ENativeWebQueueResult::RESULT_QUEUED);
	TestTrue(
		TEXT("Outbound queue rejects excess entries"),
		Instance->QueueToJavaScript(Outbound) == ENativeWebQueueResult::RESULT_QUEUE_FULL);
	TestTrue(TEXT("A message can be pending at close"), Instance->Evaluate(
		         TEXT("nativeWeb.postString('stale')"), JsError));
	Instance->Close();
	ToJavaScript.Reset();
	TestEqual(TEXT("Close discards pending outbound messages"), Instance->DrainMessagesToJavaScript(ToJavaScript, 64),
	          0);
	TArray<FNativeWebMessage> PendingInbound;
	TestEqual(
		TEXT("Close discards pending inbound messages"), Instance->DrainMessagesFromJavaScript(PendingInbound, 64), 0);
	TestTrue(
		TEXT("Closed script host rejects messages"),
		Instance->QueueToJavaScript(Outbound) == ENativeWebQueueResult::RESULT_CLOSED);
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
	TestTrue(
		TEXT("The Vue JS bundle is staged as project content"),
		FFileHelper::LoadFileToString(ScriptSource, *ScriptPath));
	TestTrue(
		TEXT("The Vue stylesheet is staged as project content"), FFileHelper::LoadFileToString(CssSource, *CssPath));
	if (ScriptSource.IsEmpty() || CssSource.IsEmpty()) return false;
	TestTrue(TEXT("The staged stylesheet includes the shared app card"), CssSource.Contains(
		         TEXT(".counter-card"), ESearchCase::CaseSensitive));
	FAffineUiSurfaceDesc Desc;
	Desc.Content.Html = TEXT("<html><body><div id=\"app\"></div></body></html>");
	Desc.Content.Css = CssSource;
	Desc.FontDirectory = FPaths::Combine(AssetDirectory, TEXT("fonts"));
	FString Error;
	TUniquePtr<FAffineUiScriptInstance> Instance = FAffineUiScriptInstance::Create(Desc, Error);
	if (!TestTrue(TEXT("Vue bundle surface and QuickJS instance are created"), Instance.IsValid()))
	{
		AddError(Error);
		return false;
	}
	const uint64 InitialDocumentEpoch = Instance->GetSurface()->GetDocumentEpoch();
	FQuickJSError JsError;
	if (!TestTrue(
		TEXT("The shared Vue app bundle evaluates in QuickJS"), Instance->Evaluate(ScriptSource, JsError, ScriptPath)))
	{
		AddError(JsError.Message);
		return false;
	}
	TestTrue(TEXT("The Vue app exposes the NativeWeb click entry point"), Instance->Evaluate(
		         TEXT(
			         "if (typeof globalThis.__nativeWebClick !== 'function') throw new Error('NativeWeb click entry point is missing')"),
		         JsError));
	if (!JsError.Message.IsEmpty()) AddError(JsError.Message);
	int32 JobsExecuted = 0;
	bool bPending = false;
	TestTrue(
		TEXT("Vue Promise and scheduler jobs pump successfully"),
		Instance->PumpJobs(256, JobsExecuted, bPending, JsError));
	if (!JsError.Message.IsEmpty()) AddError(JsError.Message);
	TestFalse(TEXT("The Vue app leaves no pending jobs after mounting"), bPending);
	TestEqual(TEXT("Vue mutations preserve the loaded document"), Instance->GetSurface()->GetDocumentEpoch(),
	          InitialDocumentEpoch);
	Instance->Close();
	return true;
}

#endif
