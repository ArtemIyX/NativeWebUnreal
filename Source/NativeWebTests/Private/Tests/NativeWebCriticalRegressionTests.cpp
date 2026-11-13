#if WITH_DEV_AUTOMATION_TESTS

#include "AffineUI/AffineUiSurface.h"
#include "Misc/AutomationTest.h"
#include "NativeWebMessageDelivery.h"
#include "NativeWebView.h"

namespace
{
	FAffineUiMutation MakeMutation(EAffineUiMutationKind Kind, uint64 Epoch, uint64 NodeId,
	                               uint64 ParentId = 0, const TCHAR* Value = TEXT(""))
	{
		FAffineUiMutation Mutation;
		Mutation.Kind = Kind;
		Mutation.DocumentEpoch = Epoch;
		Mutation.NodeId = NodeId;
		Mutation.ParentId = ParentId;
		Mutation.Value = Value;
		return Mutation;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebMessageCallbackCloseTest,
	"NativeWeb.View.MessageCallbackCloseStopsTick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebMessageCallbackCloseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UNativeWebView* View = NewObject<UNativeWebView>(GetTransientPackage());
	if (!TestNotNull(TEXT("The callback owns a test view"), View)) { return false; }
	TArray<int32> Messages{1};
	int32 Delivered = 0;
	const bool bCompleted = NativeWebDeliverMessagesWhileAlive(Messages,
		[&]() { return IsValid(View) && !View->IsClosed(); }, [&](int32) { ++Delivered; View->Close(); });
	TestFalse(TEXT("A single callback that closes its view stops the tick"), bCompleted);
	TestEqual(TEXT("The single message callback ran once"), Delivered, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebMessageCallbackDestroyBatchTest,
	"NativeWeb.View.LastMessageCallbackDestroyStopsTick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebMessageCallbackDestroyBatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UNativeWebView* View = NewObject<UNativeWebView>(GetTransientPackage());
	if (!TestNotNull(TEXT("The callback owns a test view"), View)) { return false; }
	TArray<int32> Messages{1, 2, 3};
	int32 Delivered = 0;
	const bool bCompleted = NativeWebDeliverMessagesWhileAlive(Messages,
		[&]() { return IsValid(View) && !View->IsClosed(); }, [&](int32 Message)
		{
			++Delivered;
			if (Message == 3) { View->ConditionalBeginDestroy(); }
		});
	TestFalse(TEXT("Destroying the view from the final batch callback stops the tick"), bCompleted);
	TestEqual(TEXT("Every message through the final callback was delivered"), Delivered, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffineUiDeferredMutationRetryTest,
	"NativeWeb.AffineUi.Mutations.DeferredRetryPreservesOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffineUiDeferredMutationRetryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr uint64 Epoch = 7;
	TArray<FAffineUiMutation> Initial;
	Initial.Add(MakeMutation(EAffineUiMutationKind::CreateElement, Epoch, 10));
	Initial.Add(MakeMutation(EAffineUiMutationKind::InsertBefore, Epoch, 10, 1));
	FAffineUiDeferredMutationBatch Deferred;
	Deferred.Begin(MoveTemp(Initial), Epoch);
	TestEqual(TEXT("The first draw retains its dependent operations"), Deferred.GetInFlight().Num(), 2);

	TArray<FAffineUiMutation> QueuedWhileDeferred;
	QueuedWhileDeferred.Add(MakeMutation(EAffineUiMutationKind::SetAttribute, Epoch, 10, 0, TEXT("ready")));
	Deferred.RestoreIfDeferred(Epoch, QueuedWhileDeferred);
	TestEqual(TEXT("Retry starts with the previously deferred batch"), QueuedWhileDeferred.Num(), 3);
	TestEqual(TEXT("Element creation precedes its insertion"), QueuedWhileDeferred[0].Kind,
	          EAffineUiMutationKind::CreateElement);
	TestEqual(TEXT("Insertion precedes its attribute update"), QueuedWhileDeferred[1].Kind,
	          EAffineUiMutationKind::InsertBefore);
	TestEqual(TEXT("The newer mutation remains last"), QueuedWhileDeferred[2].Kind,
	          EAffineUiMutationKind::SetAttribute);

	Deferred.Begin(MoveTemp(QueuedWhileDeferred), Epoch);
	Deferred.MarkConsumed();
	TArray<FAffineUiMutation> LaterPending;
	LaterPending.Add(MakeMutation(EAffineUiMutationKind::SetText, Epoch, 10, 0, TEXT("done")));
	Deferred.RestoreIfDeferred(Epoch, LaterPending);
	TestEqual(TEXT("A consumed batch is not replayed after success"), LaterPending.Num(), 1);
	TestEqual(TEXT("Only the later mutation remains queued"), LaterPending[0].Kind,
	          EAffineUiMutationKind::SetText);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffineUiDeferredMutationDocumentReplacementTest,
	"NativeWeb.AffineUi.Mutations.DeferredBatchDiscardedOnDocumentReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffineUiDeferredMutationDocumentReplacementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<FAffineUiMutation> OldDocument;
	OldDocument.Add(MakeMutation(EAffineUiMutationKind::CreateElement, 4, 20));
	OldDocument.Add(MakeMutation(EAffineUiMutationKind::InsertBefore, 4, 20, 1));
	FAffineUiDeferredMutationBatch Deferred;
	Deferred.Begin(MoveTemp(OldDocument), 4);

	TArray<FAffineUiMutation> NewDocument;
	NewDocument.Add(MakeMutation(EAffineUiMutationKind::MountRoot, 5, 1));
	Deferred.RestoreIfDeferred(5, NewDocument);
	TestEqual(TEXT("Retry contains only the replacement document's mutations"), NewDocument.Num(), 1);
	TestEqual(TEXT("The new document root remains queued"), NewDocument[0].Kind, EAffineUiMutationKind::MountRoot);
	TestEqual(TEXT("The stale root creation was discarded"), NewDocument[0].DocumentEpoch, uint64(5));
	return true;
}

#endif
