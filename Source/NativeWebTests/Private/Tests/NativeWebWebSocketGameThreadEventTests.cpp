

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/NativeWebWebSocketGameThreadEventTestTypes.h"


#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTLS.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "NativeWebWebSocketServer/Private/Server/NativeWebWebSocketGameThreadEvent.h"
#include "UObject/UObjectGlobals.h"

#include "Subsystems/NativeWebWebSocketSubsystem.h"

namespace
{
constexpr EAutomationTestFlags GameThreadEventTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

}


class FNativeWebWebSocketGameThreadEventTestAccess
{
public:


	static void BeginRun(UNativeWebWebSocketGameThreadEventTestServer& InServer, FName InNameId)
	{
		check(IsInGameThread());
		InServer.ClearGameThreadEvents();
		++InServer.GameThreadEventState->Generation;
		InServer.GameThreadEventState->DroppedEvents.Store(0);
		InServer.GameThreadEventState->DroppedMessages.Store(0);
		InServer.GameThreadEventState->bControlOverflowStopRequested.Store(false);
		InServer.GameThreadEventState->bAcceptEvents.Store(true);
		InServer.NameId = InNameId;
		InServer.ClientClass = UNativeWebWebSocketGameThreadEventTestClient::StaticClass();
	}



	static void NotifyConnected(UNativeWebWebSocketServerBase& InServer, const FNativeWebWebSocketClientInfo& InInfo)
	{
		InServer.NotifyClientConnected(InInfo);
	}



	static void NotifyDisconnected(UNativeWebWebSocketServerBase& InServer, int64 InClientId,
		ENativeWebWebSocketCloseReason InReason)
	{
		InServer.NotifyClientDisconnected(InClientId, InReason);
	}



	static void NotifyServerError(UNativeWebWebSocketServerBase& InServer, ENativeWebWebSocketErrorCode InErrorCode,
		const FString& InMessage)
	{
		InServer.NotifyServerError(InErrorCode, InMessage);
	}



	static void NotifyClientError(UNativeWebWebSocketServerBase& InServer, int64 InClientId,
		ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage)
	{
		InServer.NotifyClientError(InClientId, InErrorCode, InMessage);
	}



	static void NotifyMessage(UNativeWebWebSocketServerBase& InServer, int64 InClientId,
		const TArray<uint8>& InPayload, bool bInBinary)
	{
		InServer.NotifyClientMessage(InClientId, InPayload, bInBinary);
	}



	static int32 Drain(UNativeWebWebSocketServerBase& InServer, int32 InMaxEvents)
	{
		return InServer.DrainGameThreadEvents(InMaxEvents);
	}



	static int32 Depth(const UNativeWebWebSocketServerBase& InServer)
	{
		return InServer.GetGameThreadEventQueueDepth();
	}



	static uint64 Generation(const UNativeWebWebSocketServerBase& InServer)
	{
		return InServer.GameThreadEventState->Generation.Load();
	}



	static void Dispatch(UNativeWebWebSocketServerBase& InServer, FNativeWebWebSocketGameThreadEvent&& InEvent)
	{
		InServer.DispatchGameThreadEvent(MoveTemp(InEvent));
	}



	static void PreparePendingClient(UNativeWebWebSocketServerBase& InServer, int64 InClientId)
	{


		FScopeLock lock(&InServer.ClientObjectsLock);
		InServer.PendingClientMessages.FindOrAdd(InClientId);
	}



	static void AddServer(UNativeWebWebSocketSubsystem& InSubsystem, FName InNameId,
		UNativeWebWebSocketServerBase* InServer)
	{
		InSubsystem.Servers.Add(InNameId, InServer);
	}



	static void InvokeServerInitialized(UNativeWebWebSocketServerBase& InServer, UNativeWebWebSocketSubsystem* InSubsystem)
	{
		InServer.ServerInitialized(InSubsystem);
	}

};

namespace
{
class FTestRunnable final : public FRunnable
{
public:
	explicit FTestRunnable(TFunction<void()>&& InFunction)
		: Function(MoveTemp(InFunction))
	{
	}

	virtual uint32 Run() override
	{
		Function();
		return 0;
	}

private:
	TFunction<void()> Function;
};



void RunOnWorker(TFunction<void()>&& InFunction)
{


	FTestRunnable runnable(MoveTemp(InFunction));
	FRunnableThread* thread = FRunnableThread::Create(&runnable, TEXT("NativeWebWebSocketGameThreadEventTest"));
	if (!thread)
	{
		return;
	}
	thread->WaitForCompletion();
	delete thread;
}



void RunOnWorkers(TArray<TFunction<void()>>&& InFunctions)
{
	TArray<TUniquePtr<FTestRunnable>> runnables;
	TArray<FRunnableThread*> threads;
	runnables.Reserve(InFunctions.Num());
	threads.Reserve(InFunctions.Num());
	for (TFunction<void()>& function : InFunctions)
	{
		runnables.Add(MakeUnique<FTestRunnable>(MoveTemp(function)));
		threads.Add(FRunnableThread::Create(runnables.Last().Get(), TEXT("NativeWebWebSocketGameThreadEventTest")));
	}
	for (FRunnableThread* thread : threads)
	{
		if (thread)
		{
			thread->WaitForCompletion();
			delete thread;
		}
	}
}



UNativeWebWebSocketGameThreadEventTestServer* MakeServer(FAutomationTestBase& InTest, FName InNameId = TEXT("Test"))
{
	UNativeWebWebSocketGameThreadEventTestServer* server =
		NewObject<UNativeWebWebSocketGameThreadEventTestServer>(GetTransientPackage());
	InTest.TestNotNull(TEXT("Test server created"), server);
	if (!server)
	{
		return nullptr;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::BeginRun(*server, InNameId);
	server->OnClientConnected.AddDynamic(server,
		&UNativeWebWebSocketGameThreadEventTestServer::RecordConnectedDelegate);
	server->OnClientDisconnected.AddDynamic(server,
		&UNativeWebWebSocketGameThreadEventTestServer::RecordDisconnectedDelegate);
	server->OnServerError.AddDynamic(server,
		&UNativeWebWebSocketGameThreadEventTestServer::RecordServerErrorDelegate);
	server->OnClientError.AddDynamic(server,
		&UNativeWebWebSocketGameThreadEventTestServer::RecordClientErrorDelegate);
	UNativeWebWebSocketGameThreadEventTestClient::ActiveServer = server;
	return server;
}



FNativeWebWebSocketClientInfo MakeClientInfo(int64 InClientId)
{
	FNativeWebWebSocketClientInfo info;
	info.ClientId = InClientId;
	info.RemoteAddress = FString::Printf(TEXT("127.0.0.1:%lld"), InClientId);
	info.ConnectedAt = FDateTime::Now();

	return info;
}



UGameInstance* MakeGameInstance()
{
	return NewObject<UGameInstance>(GetTransientPackage());
}

struct FScopedCVarValue
{
	IConsoleVariable* Variable = nullptr;
	int32 PreviousValue = 0;

	FScopedCVarValue(const TCHAR* InName, int32 InValue)
	{
		Variable = IConsoleManager::Get().FindConsoleVariable(InName);
		if (Variable)
		{
			PreviousValue = Variable->GetInt();
			Variable->Set(InValue, ECVF_SetByCode);
		}
	}

	~FScopedCVarValue()
	{
		if (Variable)
		{
			Variable->Set(PreviousValue, ECVF_SetByCode);
		}
	}
};



int32 CountRecord(const UNativeWebWebSocketGameThreadEventTestServer& InServer, const FString& InRecord)
{
	int32 count = 0;
	for (const FString& record : InServer.Records)
	{
		count += record == InRecord ? 1 : 0;
	}
	return count;
}



void CheckGameThreadCallbacks(FAutomationTestBase& InTest,
	const UNativeWebWebSocketGameThreadEventTestServer& InServer)
{
	const uint32 gameThreadId = FPlatformTLS::GetCurrentThreadId();
	for (const uint32 callbackThreadId : InServer.CallbackThreadIds)
	{
		InTest.TestEqual(TEXT("Callback runs on the game thread"), static_cast<int64>(callbackThreadId),
			static_cast<int64>(gameThreadId));
	}
}
}

UNativeWebWebSocketGameThreadEventTestServer* UNativeWebWebSocketGameThreadEventTestClient::ActiveServer = nullptr;

void UNativeWebWebSocketGameThreadEventTestClient::HandleBytesFromClient(const TArray<uint8>& InData)
{
	if (ActiveServer)
	{
		ActiveServer->RecordClientBytes();
	}
}

void UNativeWebWebSocketGameThreadEventTestClient::HandleStringFromClient(const FString& InMessage)
{
	if (ActiveServer)
	{
		ActiveServer->RecordClientString(InMessage);
	}
}

void UNativeWebWebSocketGameThreadEventTestServer::ResetRecords()
{
	Records.Reset();
	CallbackThreadIds.Reset();
	MessageIds.Reset();
	MessagePayloads.Reset();
	MessageBinaryFlags.Reset();
	MessageTexts.Reset();
	ConnectedClientVisibleInCallbacks.Reset();
	DisconnectedClientVisibleInCallbacks.Reset();
}

void UNativeWebWebSocketGameThreadEventTestServer::RecordClientBytes()
{
	Records.Add(TEXT("ClientBytes"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
}

void UNativeWebWebSocketGameThreadEventTestServer::RecordClientString(const FString& InMessage)
{
	Records.Add(TEXT("ClientString"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
	MessageTexts.Add(InMessage);
}

void UNativeWebWebSocketGameThreadEventTestServer::RecordConnectedDelegate(const FNativeWebWebSocketClientInfo& InClientInfo)
{
	Records.Add(TEXT("ConnectedDelegate"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
	ConnectedClientVisibleInCallbacks.Add(IsValid(GetClient(InClientInfo.ClientId)));
}

void UNativeWebWebSocketGameThreadEventTestServer::RecordDisconnectedDelegate(int64 InClientId,
	ENativeWebWebSocketCloseReason InReason)
{
	Records.Add(TEXT("DisconnectedDelegate"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
	DisconnectedClientVisibleInCallbacks.Add(IsValid(GetClient(InClientId)));
}

void UNativeWebWebSocketGameThreadEventTestServer::RecordServerErrorDelegate(FName InServerName,
	ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage)
{
	Records.Add(FString::Printf(TEXT("ServerErrorDelegate:%d:%s"), static_cast<int32>(InErrorCode), *InMessage));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
}

void UNativeWebWebSocketGameThreadEventTestServer::RecordClientErrorDelegate(int64 InClientId,
	ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage)
{
	Records.Add(FString::Printf(TEXT("ClientErrorDelegate:%lld:%d:%s"), InClientId,
		static_cast<int32>(InErrorCode), *InMessage));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
}

void UNativeWebWebSocketGameThreadEventTestServer::HandleClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo)
{
	Records.Add(TEXT("ConnectedHandler"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
	ConnectedClientVisibleInCallbacks.Add(IsValid(GetClient(InClientInfo.ClientId)));
	if (bStopOnConnected)
	{
		StopServer();
	}
}

void UNativeWebWebSocketGameThreadEventTestServer::HandleClientDisconnected(int64 InClientId,
	ENativeWebWebSocketCloseReason InReason)
{
	Records.Add(TEXT("DisconnectedHandler"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
	DisconnectedClientVisibleInCallbacks.Add(IsValid(GetClient(InClientId)));
}

void UNativeWebWebSocketGameThreadEventTestServer::HandleServerError(ENativeWebWebSocketErrorCode InErrorCode,
	const FString& InMessage)
{
	Records.Add(FString::Printf(TEXT("ServerErrorHandler:%d:%s"), static_cast<int32>(InErrorCode), *InMessage));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
}

void UNativeWebWebSocketGameThreadEventTestServer::HandleClientError(int64 InClientId,
	ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage)
{
	Records.Add(FString::Printf(TEXT("ClientErrorHandler:%lld:%d:%s"), InClientId,
		static_cast<int32>(InErrorCode), *InMessage));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
}

void UNativeWebWebSocketGameThreadEventTestServer::HandleClientMessage(int64 InClientId,
	const TArray<uint8>& InPayload, bool bInBinary)
{
	Records.Add(TEXT("GenericMessage"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
	MessageIds.Add(InClientId);
	MessagePayloads.Add(InPayload);
	MessageBinaryFlags.Add(bInBinary);
}

void UNativeWebWebSocketGameThreadEventTestServer::HandleClientBytes(UNativeWebWebSocketClientBase* InClient,
	const TArray<uint8>& InData)
{
	Records.Add(TEXT("BytesHandler"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
}

void UNativeWebWebSocketGameThreadEventTestServer::HandleClientString(UNativeWebWebSocketClientBase* InClient,
	const FString& InMessage)
{
	Records.Add(TEXT("StringHandler"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
	MessageTexts.Add(InMessage);
}

void UNativeWebWebSocketGameThreadEventTestServer::ServerInitialized(UNativeWebWebSocketSubsystem* InNativeWebWebSocketSubsystem)
{
	Records.Add(TEXT("ServerInitialized"));
	CallbackThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketWorkerThreadDefersDispatchTest,
	"NativeWebWebSocket.GameThreadEvents.WorkerThreadDefersDispatch", GameThreadEventTestFlags)


bool FNativeWebWebSocketWorkerThreadDefersDispatchTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	const FNativeWebWebSocketClientInfo info = MakeClientInfo(1);
	const TArray<uint8> payload{1, 2, 3};
	RunOnWorker([server, info, payload]() {
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, info);
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown, TEXT("server"));
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyClientError(*server, 1, ENativeWebWebSocketErrorCode::InvalidPayload, TEXT("client"));
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 1, payload, true);
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyDisconnected(*server, 1, ENativeWebWebSocketCloseReason::ClientClosed);
	});
	TestEqual(TEXT("No callbacks run before drain"), server->Records.Num(), 0);
	TestEqual(TEXT("All events are queued"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 5);
	FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 5);
	TestTrue(TEXT("Callbacks ran after drain"), server->Records.Num() > 0);
	CheckGameThreadCallbacks(*this, *server);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketGameThreadDispatchSynchronousTest,
	"NativeWebWebSocket.GameThreadEvents.GameThreadDispatchIsSynchronous", GameThreadEventTestFlags)


bool FNativeWebWebSocketGameThreadDispatchSynchronousTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(2));
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown, TEXT("error"));
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyClientError(*server, 2, ENativeWebWebSocketErrorCode::InvalidPayload, TEXT("error"));
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 2, TArray<uint8>{7}, true);
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyDisconnected(*server, 2, ENativeWebWebSocketCloseReason::ClientClosed);
	TestEqual(TEXT("Game-thread notifications do not queue"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 0);
	TestTrue(TEXT("Callbacks run synchronously"), server->Records.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketMixedEventsPreserveFifoTest,
	"NativeWebWebSocket.GameThreadEvents.MixedEventsPreserveFifoAndPayloads", GameThreadEventTestFlags)


bool FNativeWebWebSocketMixedEventsPreserveFifoTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	const TArray<uint8> binaryPayload{0xAA, 0xBB};
	const TArray<uint8> textPayload{ 'o', 'k' };
	RunOnWorker([server, binaryPayload, textPayload]() {
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(3));
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown, TEXT("s"));
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyClientError(*server, 3, ENativeWebWebSocketErrorCode::InvalidPayload, TEXT("c"));
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 3, binaryPayload, true);
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 3, textPayload, false);
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyDisconnected(*server, 3, ENativeWebWebSocketCloseReason::Timeout);
	});
	FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 6);
	TestEqual(TEXT("Message count is preserved"), server->MessagePayloads.Num(), 2);
	if (server->MessagePayloads.Num() >= 2 && server->MessageIds.Num() >= 2 && server->MessageTexts.Num() > 0)
	{
		TestTrue(TEXT("First payload is binary"), server->MessagePayloads[0] == binaryPayload);
		TestTrue(TEXT("Second payload is text bytes"), server->MessagePayloads[1] == textPayload);
		TestEqual(TEXT("Message client ids are preserved"), server->MessageIds[0], int64(3));
		TestEqual(TEXT("Message client ids remain ordered"), server->MessageIds[1], int64(3));
		TestEqual(TEXT("Text content is decoded"), server->MessageTexts.Last(), FString(TEXT("ok")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketFrameBudgetExactTest,
	"NativeWebWebSocket.GameThreadEvents.FrameBudgetIsExact", GameThreadEventTestFlags)


bool FNativeWebWebSocketFrameBudgetExactTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	RunOnWorker([server]() {
		for (int32 index = 0; index < 513; ++index)
		{
			FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown,
				FString::FromInt(index));
		}
	});
	TestEqual(TEXT("Drain executes exactly the budget"), FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 512), 512);
	TestEqual(TEXT("One event remains"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 1);
	TestEqual(TEXT("Final event drains next"), FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 512), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketGlobalBudgetAcrossServersTest,
	"NativeWebWebSocket.GameThreadEvents.GlobalBudgetAcrossServers", GameThreadEventTestFlags)


bool FNativeWebWebSocketGlobalBudgetAcrossServersTest::RunTest(const FString& Parameters)
{


	FScopedCVarValue budget(TEXT("cefws.game_thread_events_per_tick"), 512);
	UNativeWebWebSocketGameThreadEventTestServer* first = MakeServer(*this, TEXT("First"));
	UNativeWebWebSocketGameThreadEventTestServer* second = MakeServer(*this, TEXT("Second"));
	if (!first || !second)
	{
		return false;
	}
	UGameInstance* gameInstance = MakeGameInstance();
	UNativeWebWebSocketSubsystem* subsystem = NewObject<UNativeWebWebSocketSubsystem>(gameInstance);
	FNativeWebWebSocketGameThreadEventTestAccess::AddServer(*subsystem, TEXT("First"), first);
	FNativeWebWebSocketGameThreadEventTestAccess::AddServer(*subsystem, TEXT("Second"), second);
	RunOnWorker([first, second]() {
		for (int32 index = 0; index < 400; ++index)
		{
			FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*first, ENativeWebWebSocketErrorCode::Unknown, TEXT("a"));
			FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*second, ENativeWebWebSocketErrorCode::Unknown, TEXT("b"));
		}
	});
	subsystem->Tick(0.0f);
	TestEqual(TEXT("Combined callbacks obey the global budget"),
		CountRecord(*first, TEXT("ServerErrorHandler:255:a")) + CountRecord(*second, TEXT("ServerErrorHandler:255:b")), 512);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketBusyServerRoundRobinTest,
	"NativeWebWebSocket.GameThreadEvents.BusyServerDoesNotStarveOtherServers", GameThreadEventTestFlags)


bool FNativeWebWebSocketBusyServerRoundRobinTest::RunTest(const FString& Parameters)
{


	FScopedCVarValue budget(TEXT("cefws.game_thread_events_per_tick"), 2);
	UNativeWebWebSocketGameThreadEventTestServer* first = MakeServer(*this, TEXT("Busy"));
	UNativeWebWebSocketGameThreadEventTestServer* second = MakeServer(*this, TEXT("Other"));
	if (!first || !second)
	{
		return false;
	}
	UGameInstance* gameInstance = MakeGameInstance();
	UNativeWebWebSocketSubsystem* subsystem = NewObject<UNativeWebWebSocketSubsystem>(gameInstance);
	FNativeWebWebSocketGameThreadEventTestAccess::AddServer(*subsystem, TEXT("Busy"), first);
	FNativeWebWebSocketGameThreadEventTestAccess::AddServer(*subsystem, TEXT("Other"), second);
	RunOnWorker([first, second]() {
		for (int32 index = 0; index < 8; ++index)
		{
			FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*first, ENativeWebWebSocketErrorCode::Unknown, TEXT("busy"));
		}
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*second, ENativeWebWebSocketErrorCode::Unknown, TEXT("other"));
	});
	subsystem->Tick(0.0f);
	TestEqual(TEXT("Other server receives service in the first pass"),
		CountRecord(*second, TEXT("ServerErrorHandler:255:other")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketConcurrentProducersLosslessTest,
	"NativeWebWebSocket.GameThreadEvents.ConcurrentProducersAreLosslessWithinCapacity", GameThreadEventTestFlags)


bool FNativeWebWebSocketConcurrentProducersLosslessTest::RunTest(const FString& Parameters)
{


	FScopedCVarValue capacity(TEXT("cefws.game_thread_event_queue_max"), 256);
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(4));
	server->ResetRecords();
	TArray<TFunction<void()>> producerFunctions;
	for (int32 producer = 0; producer < 4; ++producer)
	{
		producerFunctions.Add([server, producer]() {
			for (int32 index = 0; index < 16; ++index)
			{
				const TArray<uint8> payload{static_cast<uint8>(producer * 16 + index)};
				FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 4, payload, true);
			}
		});
	}
	RunOnWorkers(MoveTemp(producerFunctions));
	FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 128);
	TSet<uint8> ids;
	for (const TArray<uint8>& payload : server->MessagePayloads)
	{
		if (payload.Num() == 1)
		{
			ids.Add(payload[0]);
		}
	}
	TestEqual(TEXT("Every producer payload is delivered once"), ids.Num(), 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketQueueCapacityAtomicTest,
	"NativeWebWebSocket.GameThreadEvents.QueueCapacityIsAtomic", GameThreadEventTestFlags)


bool FNativeWebWebSocketQueueCapacityAtomicTest::RunTest(const FString& Parameters)
{


	FScopedCVarValue capacity(TEXT("cefws.game_thread_event_queue_max"), 8);
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	constexpr int32 attempts = 64;
	TArray<TFunction<void()>> producerFunctions;
	for (int32 producer = 0; producer < 4; ++producer)
	{
		producerFunctions.Add([server, producer, attempts]() {
			for (int32 index = producer; index < attempts; index += 4)
			{
				FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 5,
					TArray<uint8>{static_cast<uint8>(index)}, true);
			}
		});
	}
	RunOnWorkers(MoveTemp(producerFunctions));
	const int32 accepted = FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server);
	const FNativeWebWebSocketServerStats stats = server->GetStats();
	TestTrue(TEXT("Accepted events do not exceed capacity"), accepted <= 8);
	TestEqual(TEXT("Rejected count is exact"), stats.DroppedGameThreadEvents, int64(attempts - accepted));
	FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, attempts);
	TestEqual(TEXT("Queue depth returns to zero"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketMessageOverflowNoRecursionTest,
	"NativeWebWebSocket.GameThreadEvents.MessageOverflowDoesNotRecurse", GameThreadEventTestFlags)


bool FNativeWebWebSocketMessageOverflowNoRecursionTest::RunTest(const FString& Parameters)
{


	FScopedCVarValue capacity(TEXT("cefws.game_thread_event_queue_max"), 4);
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	RunOnWorker([server]() {
		for (int32 index = 0; index < 4; ++index)
		{
			FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 6, TArray<uint8>{static_cast<uint8>(index)}, true);
		}
	});
	TestEqual(TEXT("Message overflow leaves only reserved capacity events queued"),
		FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 3);
	TestEqual(TEXT("One message is dropped"), server->GetStats().DroppedGameThreadEvents, int64(1));
	TestEqual(TEXT("Overflow does not enqueue an error"), server->Records.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketControlEventsRemainConsistentTest,
	"NativeWebWebSocket.GameThreadEvents.ControlEventsRemainConsistentAtCapacity", GameThreadEventTestFlags)


bool FNativeWebWebSocketControlEventsRemainConsistentTest::RunTest(const FString& Parameters)
{


	FScopedCVarValue capacity(TEXT("cefws.game_thread_event_queue_max"), 4);
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(7));
	server->ResetRecords();
	RunOnWorker([server]() {
		for (int32 index = 0; index < 3; ++index)
		{
			FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 7, TArray<uint8>{static_cast<uint8>(index)}, true);
		}
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyDisconnected(*server, 7, ENativeWebWebSocketCloseReason::Error);
	});
	TestEqual(TEXT("Control event uses reserved capacity"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 4);
	FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 4);
	TestFalse(TEXT("Disconnect removes the client"), IsValid(server->GetClient(7)));
	TestEqual(TEXT("Disconnect callback runs"), CountRecord(*server, TEXT("DisconnectedHandler")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketStopRejectsAndFlushesTest,
	"NativeWebWebSocket.GameThreadEvents.StopRejectsAndFlushesEvents", GameThreadEventTestFlags)


bool FNativeWebWebSocketStopRejectsAndFlushesTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	RunOnWorker([server]() {
		for (int32 index = 0; index < 5; ++index)
		{
			FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown, TEXT("queued"));
		}
	});
	server->StopServer();
	TestEqual(TEXT("Stop flushes queued events"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 0);
	TestEqual(TEXT("No callback runs after stop"), server->Records.Num(), 0);
	RunOnWorker([server]() {
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown, TEXT("rejected"));
	});
	TestEqual(TEXT("Stopped server rejects new events"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketRestartRejectsOldGenerationTest,
	"NativeWebWebSocket.GameThreadEvents.RestartRejectsOldGeneration", GameThreadEventTestFlags)


bool FNativeWebWebSocketRestartRejectsOldGenerationTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	const uint64 oldGeneration = FNativeWebWebSocketGameThreadEventTestAccess::Generation(*server);
	server->StopServer();
	FNativeWebWebSocketGameThreadEventTestAccess::BeginRun(*server, TEXT("Restarted"));
	FNativeWebWebSocketServerErrorEvent stalePayload;
	stalePayload.ErrorCode = ENativeWebWebSocketErrorCode::Unknown;
	stalePayload.Message = TEXT("stale");
	FNativeWebWebSocketGameThreadEventTestAccess::Dispatch(*server,
		FNativeWebWebSocketGameThreadEvent(oldGeneration, MoveTemp(stalePayload)));
	RunOnWorker([server]() {
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown, TEXT("new"));
	});
	FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 1);
	TestEqual(TEXT("Only the current generation is delivered"),
		CountRecord(*server, TEXT("ServerErrorHandler:255:new")), 1);
	TestEqual(TEXT("Stale event is discarded"), CountRecord(*server, TEXT("ServerErrorHandler:255:stale")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketStopDuringCallbackSafeTest,
	"NativeWebWebSocket.GameThreadEvents.StopDuringCallbackIsSafe", GameThreadEventTestFlags)


bool FNativeWebWebSocketStopDuringCallbackSafeTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	server->bStopOnConnected = true;
	RunOnWorker([server]() {
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(8));
		FNativeWebWebSocketGameThreadEventTestAccess::NotifyServerError(*server, ENativeWebWebSocketErrorCode::Unknown, TEXT("later"));
	});
	FNativeWebWebSocketGameThreadEventTestAccess::Drain(*server, 2);
	TestEqual(TEXT("Callback may stop and clear its server safely"), CountRecord(*server, TEXT("ServerErrorHandler:255:later")), 0);
	TestEqual(TEXT("Queue is clear after reentrant stop"), FNativeWebWebSocketGameThreadEventTestAccess::Depth(*server), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketClientConnectedOrderingTest,
	"NativeWebWebSocket.GameThreadEvents.ClientConnectedCreatesClientBeforeCallbacks", GameThreadEventTestFlags)


bool FNativeWebWebSocketClientConnectedOrderingTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(9));
	TestEqual(TEXT("Both connection callbacks see the initialized client"), server->ConnectedClientVisibleInCallbacks.Num(), 2);
	for (const bool bVisible : server->ConnectedClientVisibleInCallbacks)
	{
		TestTrue(TEXT("Client exists before connection callback"), bVisible);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketMessageBeforeClientBufferedTest,
	"NativeWebWebSocket.GameThreadEvents.MessageBeforeClientIsBuffered", GameThreadEventTestFlags)


bool FNativeWebWebSocketMessageBeforeClientBufferedTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::PreparePendingClient(*server, 10);
	const TArray<uint8> payload{42};
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 10, payload, true);
	TestEqual(TEXT("Pending message does not dispatch early"), server->MessagePayloads.Num(), 0);
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(10));
	TestEqual(TEXT("Pending message dispatches once after connection"), server->MessagePayloads.Num(), 1);
	if (server->MessagePayloads.Num() == 1)
	{
		TestTrue(TEXT("Pending payload is preserved"), server->MessagePayloads[0] == payload);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketDisconnectOrderingTest,
	"NativeWebWebSocket.GameThreadEvents.DisconnectRemovesClientBeforeCallbacks", GameThreadEventTestFlags)


bool FNativeWebWebSocketDisconnectOrderingTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(11));
	server->ResetRecords();
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyDisconnected(*server, 11, ENativeWebWebSocketCloseReason::ClientClosed);
	TestEqual(TEXT("Both disconnect callbacks see no client"), server->DisconnectedClientVisibleInCallbacks.Num(), 2);
	for (const bool bVisible : server->DisconnectedClientVisibleInCallbacks)
	{
		TestFalse(TEXT("Client is removed before disconnect callback"), bVisible);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketGenericMessageOnceTest,
	"NativeWebWebSocket.GameThreadEvents.GenericMessageHandlerRunsOnce", GameThreadEventTestFlags)


bool FNativeWebWebSocketGenericMessageOnceTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(12));
	server->ResetRecords();
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 12, TArray<uint8>{1}, true);
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 12, TArray<uint8>{2}, false);
	TestEqual(TEXT("Generic hook runs once per message"), CountRecord(*server, TEXT("GenericMessage")), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketByteAndTextRoutingTest,
	"NativeWebWebSocket.GameThreadEvents.ByteAndTextRoutingMatchesExistingContract", GameThreadEventTestFlags)


bool FNativeWebWebSocketByteAndTextRoutingTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyConnected(*server, MakeClientInfo(13));
	server->ResetRecords();
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 13, TArray<uint8>{'b'}, true);
	TestEqual(TEXT("Binary message runs generic and bytes hooks"), CountRecord(*server, TEXT("GenericMessage")), 1);
	TestEqual(TEXT("Binary message runs bytes hook"), CountRecord(*server, TEXT("BytesHandler")), 1);
	TestEqual(TEXT("Binary message skips string hook"), CountRecord(*server, TEXT("StringHandler")), 0);
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 13, TArray<uint8>{'t'}, false);
	TestEqual(TEXT("Text message runs generic hook"), CountRecord(*server, TEXT("GenericMessage")), 2);
	TestEqual(TEXT("Text message runs bytes hook"), CountRecord(*server, TEXT("BytesHandler")), 2);
	TestEqual(TEXT("Text message runs string hook"), CountRecord(*server, TEXT("StringHandler")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketInvalidClientMessageTest,
	"NativeWebWebSocket.GameThreadEvents.InvalidClientMessageIsDiscarded", GameThreadEventTestFlags)


bool FNativeWebWebSocketInvalidClientMessageTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	if (!server)
	{
		return false;
	}
	FNativeWebWebSocketGameThreadEventTestAccess::NotifyMessage(*server, 999, TArray<uint8>{1}, true);
	TestEqual(TEXT("Unknown client message is discarded"), server->Records.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebWebSocketServerInitializedThreadTest,
	"NativeWebWebSocket.GameThreadEvents.ServerInitializedRunsOnGameThread", GameThreadEventTestFlags)


bool FNativeWebWebSocketServerInitializedThreadTest::RunTest(const FString& Parameters)
{
	UNativeWebWebSocketGameThreadEventTestServer* server = MakeServer(*this);
	UGameInstance* gameInstance = MakeGameInstance();
	UNativeWebWebSocketSubsystem* subsystem = NewObject<UNativeWebWebSocketSubsystem>(gameInstance);
	if (!server || !subsystem)
	{
		return false;
	}
	server->ResetRecords();
	FNativeWebWebSocketGameThreadEventTestAccess::InvokeServerInitialized(*server, subsystem);
	TestEqual(TEXT("ServerInitialized runs once"), CountRecord(*server, TEXT("ServerInitialized")), 1);
	CheckGameThreadCallbacks(*this, *server);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
