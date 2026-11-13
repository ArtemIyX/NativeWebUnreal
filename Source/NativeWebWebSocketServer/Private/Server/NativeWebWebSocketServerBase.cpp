

#include "Server/NativeWebWebSocketServerBase.h"

#include "Config/NativeWebWebSocketCVars.h"
#include "Data/NativeWebWebSocketStructs.h"
#include "Logging/NativeWebWebSocketLog.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Server/NativeWebWebSocketClientBase.h"
#include "Server/NativeWebWebSocketGameThreadEvent.h"
#include "Server/NativeWebWebSocketServerInstance.h"
#include "Stats/NativeWebWebSocketStats.h"

namespace
{
constexpr int32 GameThreadControlEventReserve = 16;
}

void FNativeWebWebSocketGameThreadEventStateDeleter::operator()(FNativeWebWebSocketGameThreadEventState* InState) const
{
	delete InState;
}

UNativeWebWebSocketServerBase::UNativeWebWebSocketServerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GameThreadEventState(new FNativeWebWebSocketGameThreadEventState())
{
}

UNativeWebWebSocketServerBase::~UNativeWebWebSocketServerBase() = default;

void UNativeWebWebSocketServerBase::BeginDestroy()
{
	StopServer();
	Super::BeginDestroy();
}

bool UNativeWebWebSocketServerBase::StartServerInternal(FName InNameId, int32 InBoundPort,
	TSubclassOf<UNativeWebWebSocketClientBase> InClientClass,
	const FNativeWebWebSocketPipelineConfig& InPipelineConfig)
{
	check(IsInGameThread());
	check(GameThreadEventState.IsValid());

	// A new generation makes events from a previous run harmless if they outlive a restart.
	ClearGameThreadEvents();
	++GameThreadEventState->Generation;
	GameThreadEventState->DroppedEvents.Store(0);
	GameThreadEventState->DroppedMessages.Store(0);
	GameThreadEventState->bControlOverflowStopRequested.Store(false);
	GameThreadEventState->bAcceptEvents.Store(true);

	NameId = InNameId;
	BoundPort = InBoundPort;
	ClientClass = InClientClass;
	PipelineConfig = InPipelineConfig;
	PayloadFormat = PipelineConfig.InPayloadFormat;

	if (!Instance.IsValid())
	{
		Instance = MakeShared<FNativeWebWebSocketServerInstance>(NameId, BoundPort, this);
	}
	Instance->SetPipelineConfig(PipelineConfig);
	Instance->SetPayloadFormat(PayloadFormat);
	Instance->SetPacketCodec(PacketCodec);

	const bool bStarted = Instance->Start();
	if (!bStarted)
	{
		GameThreadEventState->bAcceptEvents.Store(false);
		ClearGameThreadEvents();
	}
	return bStarted;
}

void UNativeWebWebSocketServerBase::AttachInstance(const TSharedPtr<FNativeWebWebSocketServerInstance>& InInstance)
{
	Instance = InInstance;
}

bool UNativeWebWebSocketServerBase::IsRunning() const
{
	return Instance.IsValid() && Instance->IsRunning();
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::SendToClientString(int64 InClientId, const FString& InMessage)
{
	if (!Instance.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return Instance->SendToClientString(InClientId, InMessage);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::SendToClientBytes(int64 InClientId, const TArray<uint8>& InBytes)
{
	if (!Instance.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return Instance->SendToClientBytes(InClientId, InBytes);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::SendToClientJson(int64 InClientId,
	const TSharedPtr<FJsonObject>& InJsonObject,
	const TSharedPtr<FJsonValue>& InJsonValue)
{
	if (!InJsonObject.IsValid() && !InJsonValue.IsValid())
	{
		return ENativeWebWebSocketSendResult::SerializeFailed;
	}

	FString serialized;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&serialized);
	if (InJsonObject.IsValid())
	{
		if (!FJsonSerializer::Serialize(InJsonObject.ToSharedRef(), writer))
		{
			return ENativeWebWebSocketSendResult::SerializeFailed;
		}
	}
	else
	{
		if (!FJsonSerializer::Serialize(InJsonValue.ToSharedRef(), TEXT(""), writer))
		{
			return ENativeWebWebSocketSendResult::SerializeFailed;
		}
	}

	return SendToClientString(InClientId, serialized);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::BroadcastString(const FString& InMessage)
{
	if (!Instance.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return Instance->BroadcastString(InMessage);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::BroadcastBytes(const TArray<uint8>& InBytes)
{
	if (!Instance.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return Instance->BroadcastBytes(InBytes);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::BroadcastJson(const TSharedPtr<FJsonObject>& InJsonObject,
	const TSharedPtr<FJsonValue>& InJsonValue)
{
	if (!InJsonObject.IsValid() && !InJsonValue.IsValid())
	{
		return ENativeWebWebSocketSendResult::SerializeFailed;
	}

	FString serialized;
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&serialized);
	if (InJsonObject.IsValid())
	{
		if (!FJsonSerializer::Serialize(InJsonObject.ToSharedRef(), writer))
		{
			return ENativeWebWebSocketSendResult::SerializeFailed;
		}
	}
	else
	{
		if (!FJsonSerializer::Serialize(InJsonValue.ToSharedRef(), TEXT(""), writer))
		{
			return ENativeWebWebSocketSendResult::SerializeFailed;
		}
	}

	return BroadcastString(serialized);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::BroadcastStringExcept(int64 InExcludedClientId,
	const FString& InMessage)
{
	if (!Instance.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return Instance->BroadcastStringExcept(InExcludedClientId, InMessage);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::BroadcastBytesExcept(int64 InExcludedClientId,
	const TArray<uint8>& InBytes)
{
	if (!Instance.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return Instance->BroadcastBytesExcept(InExcludedClientId, InBytes);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketServerBase::DisconnectClient(int64 InClientId, ENativeWebWebSocketCloseReason InReason)
{
	if (!Instance.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return Instance->DisconnectClient(InClientId, InReason);
}

void UNativeWebWebSocketServerBase::StopServer()
{
	check(IsInGameThread());
	check(GameThreadEventState.IsValid());
	GameThreadEventState->bAcceptEvents.Store(false);
	if (Instance.IsValid())
	{
		Instance->Stop();
	}
	ClearGameThreadEvents();


	FScopeLock lock(&ClientObjectsLock);
	ClientObjects.Empty();
	PendingClientMessages.Empty();
}

TArray<UNativeWebWebSocketClientBase*> UNativeWebWebSocketServerBase::GetClients() const
{
	TArray<UNativeWebWebSocketClientBase*> outClients;


	FScopeLock lock(&ClientObjectsLock);
	for (const TPair<int64, TObjectPtr<UNativeWebWebSocketClientBase>>& Pair : ClientObjects)
	{
		if (Pair.Value)
		{
			outClients.Add(Pair.Value);
		}
	}
	return outClients;
}

UNativeWebWebSocketClientBase* UNativeWebWebSocketServerBase::GetClient(int64 InClientId) const
{


	FScopeLock lock(&ClientObjectsLock);
	if (const TObjectPtr<UNativeWebWebSocketClientBase>* Found = ClientObjects.Find(InClientId))
	{
		return *Found;
	}
	return nullptr;
}

FNativeWebWebSocketServerStats UNativeWebWebSocketServerBase::GetStats() const
{
	check(GameThreadEventState.IsValid());
	FNativeWebWebSocketServerStats stats;
	if (!Instance.IsValid())
	{
		stats = FNativeWebWebSocketServerStats();
	}
	else
	{
		stats = Instance->GetStats();
	}
	stats.GameThreadEventQueueDepth = GameThreadEventState->EventCount.Load();
	stats.DroppedGameThreadEvents = GameThreadEventState->DroppedEvents.Load();
	stats.DroppedMessages += GameThreadEventState->DroppedMessages.Load();
	SET_DWORD_STAT(STAT_NativeWebWebSocket_GameThreadEventQueueDepth,
	               static_cast<uint32>(FMath::Clamp<int64>(stats.GameThreadEventQueueDepth, 0, MAX_uint32)));
	SET_DWORD_STAT(STAT_NativeWebWebSocket_DroppedGameThreadEvents,
	               static_cast<uint32>(FMath::Clamp<int64>(stats.DroppedGameThreadEvents, 0, MAX_uint32)));
	return stats;
}

void UNativeWebWebSocketServerBase::SetPayloadFormat(ENativeWebWebSocketPayloadFormat InPayloadFormat)
{
	PayloadFormat = InPayloadFormat;
	if (Instance.IsValid())
	{
		Instance->SetPayloadFormat(InPayloadFormat);
	}
}

void UNativeWebWebSocketServerBase::SetPipelineConfig(const FNativeWebWebSocketPipelineConfig& InPipelineConfig)
{
	PipelineConfig = InPipelineConfig;
	if (Instance.IsValid())
	{
		Instance->SetPipelineConfig(InPipelineConfig);
	}
}

void UNativeWebWebSocketServerBase::SetPacketCodec(const TSharedPtr<INativeWebWebSocketPacketCodec>& InCodec)
{
	PacketCodec = InCodec;
	if (Instance.IsValid())
	{
		Instance->SetPacketCodec(InCodec);
	}
}

void UNativeWebWebSocketServerBase::NotifyClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo)
{
	FNativeWebWebSocketClientConnectedEvent payload;
	payload.ClientInfo = InClientInfo;
	if (IsInGameThread())
	{
		DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
		return;
	}
	EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
}

void UNativeWebWebSocketServerBase::NotifyClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason)
{
	FNativeWebWebSocketClientDisconnectedEvent payload;
	payload.ClientId = InClientId;
	payload.Reason = InReason;
	if (IsInGameThread())
	{
		DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
		return;
	}
	EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
}

void UNativeWebWebSocketServerBase::NotifyServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage)
{
	FNativeWebWebSocketServerErrorEvent payload;
	payload.ErrorCode = InErrorCode;
	payload.Message = InMessage;
	if (IsInGameThread())
	{
		DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
		return;
	}
	EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
}

void UNativeWebWebSocketServerBase::NotifyClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode,
	const FString& InMessage)
{
	FNativeWebWebSocketClientErrorEvent payload;
	payload.ClientId = InClientId;
	payload.ErrorCode = InErrorCode;
	payload.Message = InMessage;
	if (IsInGameThread())
	{
		DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
		return;
	}
	EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
}

void UNativeWebWebSocketServerBase::NotifyClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary)
{
	FNativeWebWebSocketClientMessageEvent payload;
	payload.ClientId = InClientId;
	payload.Payload = InPayload;
	payload.bBinary = bInBinary;
	if (IsInGameThread())
	{
		DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
		return;
	}
	EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent(GameThreadEventState->Generation.Load(), MoveTemp(payload)));
}

bool UNativeWebWebSocketServerBase::EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent&& InEvent)
{
	check(!IsInGameThread());
	if (!GameThreadEventState.IsValid() || !GameThreadEventState->bAcceptEvents.Load())
	{
		return false;
	}

	const bool bMessageEvent = InEvent.Payload.IsType<FNativeWebWebSocketClientMessageEvent>();
	const int32 maxEvents = FMath::Max(1, NativeWebWebSocketCVars::GetGameThreadEventQueueMax());
	// Keep control events in reserved capacity so lifecycle state wins over message throughput.
	const int32 controlReserve = FMath::Min(GameThreadControlEventReserve, maxEvents / 4);
	const int32 reservationLimit = bMessageEvent ? maxEvents - controlReserve : maxEvents;
	const auto dropEvent = [this, bMessageEvent]() {
		++GameThreadEventState->DroppedEvents;
		if (bMessageEvent)
		{
			++GameThreadEventState->DroppedMessages;
			return;
		}

		bool bExpectedStopRequest = false;
		if (GameThreadEventState->bControlOverflowStopRequested.CompareExchange(bExpectedStopRequest, true))
		{
			GameThreadEventState->bAcceptEvents.Store(false);
			UE_LOG(LogNativeWebWebSocketServer, Error,
			       TEXT("Server '%s' dropped a control event because the game-thread queue is full"),
			       *NameId.ToString());
		}
	};
	int32 observedCount = GameThreadEventState->EventCount.Load();
	while (observedCount < reservationLimit)
	{
		if (!GameThreadEventState->bAcceptEvents.Load())
		{
			return false;
		}

		int32 expectedCount = observedCount;
		if (GameThreadEventState->EventCount.CompareExchange(expectedCount, observedCount + 1))
		{
			if (!GameThreadEventState->bAcceptEvents.Load())
			{
				--GameThreadEventState->EventCount;
				return false;
			}
			// Reserve before enqueue so concurrent producers cannot exceed capacity.
			if (GameThreadEventState->Events.Enqueue(MoveTemp(InEvent)))
			{
				return true;
			}
			--GameThreadEventState->EventCount;
			dropEvent();
			return false;
		}
		observedCount = expectedCount;
	}

	dropEvent();
	return false;
}

int32 UNativeWebWebSocketServerBase::DrainGameThreadEvents(int32 InMaxEvents)
{
	check(IsInGameThread());
	if (!GameThreadEventState.IsValid() || InMaxEvents <= 0)
	{
		return 0;
	}

	int32 drainedCount = 0;
	FNativeWebWebSocketGameThreadEvent event;
	while (drainedCount < InMaxEvents && GameThreadEventState->Events.Dequeue(event))
	{
		--GameThreadEventState->EventCount;
		DispatchGameThreadEvent(MoveTemp(event));
		++drainedCount;
	}
	return drainedCount;
}

void UNativeWebWebSocketServerBase::DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent&& InEvent)
{
	check(IsInGameThread());
	check(GameThreadEventState.IsValid());
	if (InEvent.Generation != GameThreadEventState->Generation.Load())
	{
		return;
	}

	// Keep variant routing small; each handler owns one event's state transition and callbacks.
	if (InEvent.Payload.IsType<FNativeWebWebSocketClientConnectedEvent>())
	{
		DispatchClientConnectedEvent(InEvent.Payload.Get<FNativeWebWebSocketClientConnectedEvent>());
		return;
	}

	if (InEvent.Payload.IsType<FNativeWebWebSocketClientDisconnectedEvent>())
	{
		DispatchClientDisconnectedEvent(InEvent.Payload.Get<FNativeWebWebSocketClientDisconnectedEvent>());
		return;
	}

	if (InEvent.Payload.IsType<FNativeWebWebSocketServerErrorEvent>())
	{
		DispatchServerErrorEvent(InEvent.Payload.Get<FNativeWebWebSocketServerErrorEvent>());
		return;
	}

	if (InEvent.Payload.IsType<FNativeWebWebSocketClientErrorEvent>())
	{
		DispatchClientErrorEvent(InEvent.Payload.Get<FNativeWebWebSocketClientErrorEvent>());
		return;
	}

	DispatchClientMessageEvent(InEvent.Payload.Get<FNativeWebWebSocketClientMessageEvent>());
}

void UNativeWebWebSocketServerBase::DispatchClientConnectedEvent(const FNativeWebWebSocketClientConnectedEvent& InEvent)
{
	check(IsInGameThread());
	const FNativeWebWebSocketClientInfo& clientInfo = InEvent.ClientInfo;
	{


		FScopeLock lock(&ClientObjectsLock);
		PendingClientMessages.FindOrAdd(clientInfo.ClientId);
	}

	UClass* clientUClass = ClientClass ? ClientClass.Get() : UNativeWebWebSocketClientBase::StaticClass();
	UNativeWebWebSocketClientBase* clientObject = NewObject<UNativeWebWebSocketClientBase>(this, clientUClass);
	if (!IsValid(clientObject))
	{
		{


			FScopeLock lock(&ClientObjectsLock);
			PendingClientMessages.Remove(clientInfo.ClientId);
		}
		NotifyClientError(clientInfo.ClientId, ENativeWebWebSocketErrorCode::Unknown,
			TEXT("Failed to create client object"));
		return;
	}

	TArray<FNativeWebWebSocketInboundPacket> pendingMessages;
	clientObject->InitializeClient(this, clientInfo);
	{


		FScopeLock lock(&ClientObjectsLock);
		ClientObjects.Add(clientInfo.ClientId, clientObject);
		if (TArray<FNativeWebWebSocketInboundPacket>* foundPendingMessages = PendingClientMessages.Find(clientInfo.ClientId))
		{
			pendingMessages = MoveTemp(*foundPendingMessages);
			PendingClientMessages.Remove(clientInfo.ClientId);
		}
	}

	check(IsInGameThread());
	OnClientConnected.Broadcast(clientInfo);
	check(IsInGameThread());
	HandleClientConnected(clientInfo);

	for (const FNativeWebWebSocketInboundPacket& pendingMessage : pendingMessages)
	{
		NotifyClientMessage(pendingMessage.ClientId, pendingMessage.Payload, pendingMessage.bBinary);
	}
}

void UNativeWebWebSocketServerBase::DispatchClientDisconnectedEvent(const FNativeWebWebSocketClientDisconnectedEvent& InEvent)
{
	check(IsInGameThread());
	{


		FScopeLock lock(&ClientObjectsLock);
		PendingClientMessages.Remove(InEvent.ClientId);
		ClientObjects.Remove(InEvent.ClientId);
	}
	check(IsInGameThread());
	OnClientDisconnected.Broadcast(InEvent.ClientId, InEvent.Reason);
	check(IsInGameThread());
	HandleClientDisconnected(InEvent.ClientId, InEvent.Reason);
}

void UNativeWebWebSocketServerBase::DispatchServerErrorEvent(const FNativeWebWebSocketServerErrorEvent& InEvent)
{
	check(IsInGameThread());
	OnServerError.Broadcast(NameId, InEvent.ErrorCode, InEvent.Message);
	check(IsInGameThread());
	HandleServerError(InEvent.ErrorCode, InEvent.Message);
}

void UNativeWebWebSocketServerBase::DispatchClientErrorEvent(const FNativeWebWebSocketClientErrorEvent& InEvent)
{
	check(IsInGameThread());
	OnClientError.Broadcast(InEvent.ClientId, InEvent.ErrorCode, InEvent.Message);
	check(IsInGameThread());
	HandleClientError(InEvent.ClientId, InEvent.ErrorCode, InEvent.Message);
}

void UNativeWebWebSocketServerBase::DispatchClientMessageEvent(const FNativeWebWebSocketClientMessageEvent& InEvent)
{
	check(IsInGameThread());
	UNativeWebWebSocketClientBase* client = nullptr;
	{


		FScopeLock lock(&ClientObjectsLock);
		if (const TObjectPtr<UNativeWebWebSocketClientBase>* found = ClientObjects.Find(InEvent.ClientId))
		{
			client = *found;
		}
		else if (PendingClientMessages.Contains(InEvent.ClientId))
		{
			FNativeWebWebSocketInboundPacket& pendingMessage = PendingClientMessages.FindOrAdd(InEvent.ClientId).AddDefaulted_GetRef();
			pendingMessage.ClientId = InEvent.ClientId;
			pendingMessage.Payload = InEvent.Payload;
			pendingMessage.bBinary = InEvent.bBinary;
			return;
		}
	}
	if (!IsValid(client))
	{
		UE_LOG(LogNativeWebWebSocketServer, Verbose,
			TEXT("Discarded message for unknown client %lld"), InEvent.ClientId);
		return;
	}

	// The generic hook intentionally precedes the legacy byte and text hooks.
	check(IsInGameThread());
	HandleClientMessage(InEvent.ClientId, InEvent.Payload, InEvent.bBinary);
	check(IsInGameThread());
	client->HandleBytesFromClient(InEvent.Payload);
	check(IsInGameThread());
	HandleClientBytes(client, InEvent.Payload);

	if (!InEvent.bBinary)
	{


		FUTF8ToTCHAR converter(reinterpret_cast<const ANSICHAR*>(InEvent.Payload.GetData()), InEvent.Payload.Num());


		const FString text(converter.Length(), converter.Get());
		check(IsInGameThread());
		client->HandleStringFromClient(text);
		check(IsInGameThread());
		HandleClientString(client, text);
	}
}

void UNativeWebWebSocketServerBase::ClearGameThreadEvents()
{
	check(IsInGameThread());
	check(GameThreadEventState.IsValid());
	FNativeWebWebSocketGameThreadEvent event;
	while (GameThreadEventState->Events.Dequeue(event))
	{
	}
	GameThreadEventState->EventCount.Store(0);
}

int32 UNativeWebWebSocketServerBase::GetGameThreadEventQueueDepth() const
{
	check(GameThreadEventState.IsValid());
	return FMath::Max(0, GameThreadEventState->EventCount.Load());
}

bool UNativeWebWebSocketServerBase::ConsumeControlEventOverflowStopRequest()
{
	check(IsInGameThread());
	check(GameThreadEventState.IsValid());
	return GameThreadEventState->bControlOverflowStopRequested.Exchange(false);
}

void UNativeWebWebSocketServerBase::HandleClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo) {}

void UNativeWebWebSocketServerBase::HandleClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason) {}

void UNativeWebWebSocketServerBase::HandleServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage) {}

void UNativeWebWebSocketServerBase::HandleClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage) {}

void UNativeWebWebSocketServerBase::HandleClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary) {}

void UNativeWebWebSocketServerBase::HandleClientBytes(UNativeWebWebSocketClientBase* InClient, const TArray<uint8>& InData) {}

void UNativeWebWebSocketServerBase::HandleClientString(UNativeWebWebSocketClientBase* InClient, const FString& InMessage) {}

void UNativeWebWebSocketServerBase::ServerInitialized(UNativeWebWebSocketSubsystem* InNativeWebWebSocketSubsystem)
{
	check(IsInGameThread());
	(void)InNativeWebWebSocketSubsystem;
}
