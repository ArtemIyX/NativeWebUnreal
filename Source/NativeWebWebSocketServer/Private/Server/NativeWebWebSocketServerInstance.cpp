#include "Server/NativeWebWebSocketServerInstance.h"

#include "Config/NativeWebWebSocketCVars.h"
#include "Logging/NativeWebWebSocketLog.h"
#include "Networking/NativeWebWebSocketServerBackend.h"
#include "Networking/INativeWebWebSocketNetwork.h"
#include "Pipeline/NativeWebWebSocketDefaultCodecs.h"
#include "Pipeline/INativeWebWebSocketPacketCodec.h"
#include "Server/NativeWebWebSocketServerBase.h"
#include "Stats/NativeWebWebSocketStats.h"
#include "Threads/NativeWebWebSocketHandleRunnable.h"
#include "Threads/NativeWebWebSocketReadRunnable.h"
#include "Threads/NativeWebWebSocketSendRunnable.h"
#include "Threads/NativeWebWebSocketWriteRunnable.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/RunnableThread.h"

namespace
{
uint16 ResolveCloseStatusCode(ENativeWebWebSocketCloseReason InReason)
{
	switch (InReason)
	{
	case ENativeWebWebSocketCloseReason::ServerShutdown:
		return 1001; // going away
	case ENativeWebWebSocketCloseReason::Kicked:
		return 1008; // policy violation
	case ENativeWebWebSocketCloseReason::Timeout:
		return 1001; // going away
	case ENativeWebWebSocketCloseReason::Error:
		return 1011; // internal error
	case ENativeWebWebSocketCloseReason::ClientClosed:
	default:
		return 1000; // normal closure
	}
}

FString ResolveCloseReasonText(ENativeWebWebSocketCloseReason InReason)
{
	switch (InReason)
	{
	case ENativeWebWebSocketCloseReason::ServerShutdown:
		return TEXT("server_shutdown");
	case ENativeWebWebSocketCloseReason::Kicked:
		return TEXT("kicked");
	case ENativeWebWebSocketCloseReason::Timeout:
		return TEXT("timeout");
	case ENativeWebWebSocketCloseReason::Error:
		return TEXT("error");
	case ENativeWebWebSocketCloseReason::ClientClosed:
	default:
		return TEXT("normal");
	}
}
}

FNativeWebWebSocketServerInstance::FNativeWebWebSocketServerInstance(FName InNameId, int32 InBoundPort,
                                                         TWeakObjectPtr<UNativeWebWebSocketServerBase> InOwnerServer)
	: NameId(InNameId)
	, BoundPort(InBoundPort)
	, OwnerServer(InOwnerServer)
{
}

FNativeWebWebSocketServerInstance::~FNativeWebWebSocketServerInstance()
{
	Stop();
}

bool FNativeWebWebSocketServerInstance::Start()
{
	if (bRunning.Load())
	{
		return true;
	}

	Backend = MakeUnique<FNativeWebWebSocketServerBackend>();
	FNativeWebWebSocketNetworkClientConnectedCallback onConnected;
	onConnected.BindRaw(this, &FNativeWebWebSocketServerInstance::HandleClientConnected);
	FNativeWebWebSocketNetworkClientDisconnectedCallback onDisconnected;
	onDisconnected.BindRaw(this, &FNativeWebWebSocketServerInstance::HandleClientDisconnected);

	if (!Backend->Init(static_cast<uint32>(BoundPort), onConnected, onDisconnected))
	{
		if (OwnerServer.IsValid())
		{
			OwnerServer->NotifyServerError(
				ENativeWebWebSocketErrorCode::ServerInitFailed,
				FString::Printf(TEXT("Failed to start server %s on port %d"), *NameId.ToString(), BoundPort));
		}
		Backend.Reset();
		return false;
	}

	{
		FScopeLock codecLock(&CodecLock);
		BinaryCodec = MakeShared<FNativeWebWebSocketBinaryPassthroughCodec>();
		Utf8Codec = MakeShared<FNativeWebWebSocketUtf8StringCodec>(ENativeWebWebSocketPayloadFormat::Utf8String);
		JsonCodec = MakeShared<FNativeWebWebSocketUtf8StringCodec>(ENativeWebWebSocketPayloadFormat::JsonString);
		XmlCodec = MakeShared<FNativeWebWebSocketUtf8StringCodec>(ENativeWebWebSocketPayloadFormat::XmlString);
	}

	bRunning.Store(true);
	LastRateSampleTimeSec = FPlatformTime::Seconds();
	LastRateRxBytes = 0;
	LastRateTxBytes = 0;
	QueueDepthAccum = 0;
	QueueDepthSamples = 0;
	InboundQueueDepth.Store(0);
	SendQueueDepth.Store(0);

	HandleWakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
	SendWakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
	WriteWakeEvent = FPlatformProcess::GetSynchEventFromPool(false);

	ReadRunnable = MakeUnique<FNativeWebWebSocketReadRunnable>(this);
	HandleRunnable = MakeUnique<FNativeWebWebSocketHandleRunnable>(this, HandleWakeEvent);
	SendRunnable = MakeUnique<FNativeWebWebSocketSendRunnable>(this, SendWakeEvent);
	WriteRunnable = MakeUnique<FNativeWebWebSocketWriteRunnable>(this, WriteWakeEvent);

	ReadThread = FRunnableThread::Create(ReadRunnable.Get(), *FString::Printf(TEXT("NativeWebWebSocketRead_%s"), *NameId.ToString()));
	HandleThread = FRunnableThread::Create(HandleRunnable.Get(), *FString::Printf(TEXT("NativeWebWebSocketHandle_%s"), *NameId.ToString()));
	SendThread = FRunnableThread::Create(SendRunnable.Get(), *FString::Printf(TEXT("NativeWebWebSocketSend_%s"), *NameId.ToString()));
	WriteThread = FRunnableThread::Create(WriteRunnable.Get(), *FString::Printf(TEXT("NativeWebWebSocketWrite_%s"), *NameId.ToString()));
	if (!ReadThread || !HandleThread || !SendThread || !WriteThread)
	{
		Stop();
		if (OwnerServer.IsValid())
		{
			OwnerServer->NotifyServerError(ENativeWebWebSocketErrorCode::ServerInitFailed,
			                               TEXT("Failed to create websocket server threads"));
		}
		return false;
	}

	BoundPort = static_cast<int32>(Backend->GetServerPort());
	UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("Server '%s' started on port %d"), *NameId.ToString(), BoundPort);
	return true;
}

void FNativeWebWebSocketServerInstance::Stop()
{
	if (!bRunning.Load() && !Backend.IsValid())
	{
		return;
	}

	const int32 shutdownDrainMs = FMath::Max(0, NativeWebWebSocketCVars::GetShutdownDrainMs());
	if (bRunning.Load() && shutdownDrainMs > 0)
	{
		const double deadlineSec = FPlatformTime::Seconds() + (static_cast<double>(shutdownDrainMs) / 1000.0);
		while (bRunning.Load() && !AreQueuesDrained() && FPlatformTime::Seconds() < deadlineSec)
		{
			WakeHandleThread();
			WakeSendThread();
			WakeWriteThread();
			FPlatformProcess::SleepNoStats(0.001f);
		}
	}

	bRunning.Store(false);

	if (ReadRunnable)
	{
		ReadRunnable->Stop();
	}
	if (HandleRunnable)
	{
		HandleRunnable->Stop();
	}
	if (SendRunnable)
	{
		SendRunnable->Stop();
	}
	if (WriteRunnable)
	{
		WriteRunnable->Stop();
	}
	WakeHandleThread();
	WakeSendThread();
	WakeWriteThread();

	if (ReadThread)
	{
		ReadThread->WaitForCompletion();
		delete ReadThread;
		ReadThread = nullptr;
	}
	if (HandleThread)
	{
		HandleThread->WaitForCompletion();
		delete HandleThread;
		HandleThread = nullptr;
	}
	if (SendThread)
	{
		SendThread->WaitForCompletion();
		delete SendThread;
		SendThread = nullptr;
	}
	if (WriteThread)
	{
		WriteThread->WaitForCompletion();
		delete WriteThread;
		WriteThread = nullptr;
	}

	ReadRunnable.Reset();
	HandleRunnable.Reset();
	SendRunnable.Reset();
	WriteRunnable.Reset();

	FNativeWebWebSocketInboundPacket inboundPacket;
	while (InboundQueue.Dequeue(inboundPacket))
	{
	}
	FNativeWebWebSocketSendRequest sendRequest;
	while (SendQueue.Dequeue(sendRequest))
	{
	}
	InboundQueueDepth.Store(0);
	SendQueueDepth.Store(0);

	TArray<INativeWebWebSocketNetwork*> socketsToClose;
	{
		FWriteScopeLock lock(ClientLock);
		for (TPair<int64, FNativeWebWebSocketClientState>& pair : Clients)
		{
			if (pair.Value.Socket)
			{
				socketsToClose.Add(pair.Value.Socket);
			}
		}
		Clients.Empty();
		ClientIdsBySocket.Empty();
		Stats = FNativeWebWebSocketServerStats();
		LastRateSampleTimeSec = 0.0;
		LastRateRxBytes = 0;
		LastRateTxBytes = 0;
		QueueDepthAccum = 0;
		QueueDepthSamples = 0;
	}

	for (INativeWebWebSocketNetwork* socket : socketsToClose)
	{
		socket->Close(ResolveCloseStatusCode(ENativeWebWebSocketCloseReason::ServerShutdown),
		              ResolveCloseReasonText(ENativeWebWebSocketCloseReason::ServerShutdown));
	}

	Backend.Reset();
	if (HandleWakeEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(HandleWakeEvent);
		HandleWakeEvent = nullptr;
	}
	if (SendWakeEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(SendWakeEvent);
		SendWakeEvent = nullptr;
	}
	if (WriteWakeEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WriteWakeEvent);
		WriteWakeEvent = nullptr;
	}
}

bool FNativeWebWebSocketServerInstance::IsRunning() const
{
	return bRunning.Load();
}

void FNativeWebWebSocketServerInstance::SetPipelineConfig(const FNativeWebWebSocketPipelineConfig& InConfig)
{
	FScopeLock lock(&CodecLock);
	PipelineConfig = InConfig;
	PayloadFormat = InConfig.InPayloadFormat;
}

void FNativeWebWebSocketServerInstance::SetPayloadFormat(ENativeWebWebSocketPayloadFormat InPayloadFormat)
{
	FScopeLock lock(&CodecLock);
	PayloadFormat = InPayloadFormat;
}

void FNativeWebWebSocketServerInstance::SetPacketCodec(const TSharedPtr<INativeWebWebSocketPacketCodec>& InCodec)
{
	FScopeLock lock(&CodecLock);
	CustomCodec = InCodec;
}

bool FNativeWebWebSocketServerInstance::TickBackendOnReadThread()
{
	SCOPE_CYCLE_COUNTER(STAT_NativeWebWebSocket_ReadPump);
	if (!bRunning.Load() || !Backend)
	{
		return false;
	}
	Backend->Tick();
	SweepConnectionHealthOnReadThread();
	return bReadActivity.Exchange(false);
}

bool FNativeWebWebSocketServerInstance::PumpInboundOnHandleThread()
{
	SCOPE_CYCLE_COUNTER(STAT_NativeWebWebSocket_HandlePump);
	if (!bRunning.Load())
	{
		return false;
	}

	bool bDidWork = false;
	FNativeWebWebSocketInboundPacket packet;
	while (InboundQueue.Dequeue(packet))
	{
		bDidWork = true;
		InboundQueueDepth.Store(FMath::Max<int64>(0, InboundQueueDepth.Load() - 1));

		FNativeWebWebSocketInboundPacket decodedPacket;
		FString error;
		TSharedPtr<INativeWebWebSocketPacketCodec> codec = ResolveCodec(PayloadFormat);
		if (!codec.IsValid() || !codec->DecodeInbound(packet, decodedPacket, error))
		{
			if (OwnerServer.IsValid())
			{
				OwnerServer->NotifyClientError(packet.ClientId, ENativeWebWebSocketErrorCode::InvalidPayload,
				                               error.IsEmpty() ? TEXT("Inbound decode failed") : error);
			}
			continue;
		}

		if (OwnerServer.IsValid())
		{
			OwnerServer->NotifyClientMessage(decodedPacket.ClientId, decodedPacket.Payload, decodedPacket.bBinary);
		}
	}

	Stats.InInboundQueueDepth = InboundQueueDepth.Load();
	Stats.InHandleQueueDepth = Stats.InInboundQueueDepth;
	SET_DWORD_STAT(STAT_NativeWebWebSocket_InboundQueueDepth, static_cast<uint32>(FMath::Min<int64>(Stats.InInboundQueueDepth, MAX_uint32)));
	return bDidWork;
}

bool FNativeWebWebSocketServerInstance::PumpOutgoingOnSendThread()
{
	SCOPE_CYCLE_COUNTER(STAT_NativeWebWebSocket_SendPump);
	if (!bRunning.Load())
	{
		return false;
	}

	bool bDidWork = false;
	FNativeWebWebSocketSendRequest sendRequest;
	while (SendQueue.Dequeue(sendRequest))
	{
		bDidWork = true;
		SendQueueDepth.Store(FMath::Max<int64>(0, SendQueueDepth.Load() - 1));

		if (sendRequest.bBroadcast && sendRequest.TargetClientIds.Num() == 0)
		{
			FReadScopeLock lock(ClientLock);
			Clients.GetKeys(sendRequest.TargetClientIds);
		}

		TSharedPtr<INativeWebWebSocketPacketCodec> codec = ResolveCodec(sendRequest.PayloadFormat);
		if (!codec.IsValid())
		{
			continue;
		}

		TArray<FNativeWebWebSocketWritePacket> writePackets;
		FString error;
		if (!codec->EncodeSendRequest(sendRequest, writePackets, error))
		{
			if (OwnerServer.IsValid())
			{
				OwnerServer->NotifyServerError(ENativeWebWebSocketErrorCode::JsonSerializeFailed,
				                               error.IsEmpty() ? TEXT("Send encode failed") : error);
			}
			continue;
		}

		for (const FNativeWebWebSocketWritePacket& writePacket : writePackets)
		{
			if (!EnqueueWritePacket(writePacket.ClientId, writePacket.Payload.GetData(), writePacket.Payload.Num(),
			                       writePacket.bBinary))
			{
				if (OwnerServer.IsValid())
				{
					OwnerServer->NotifyClientError(writePacket.ClientId, ENativeWebWebSocketErrorCode::QueueOverflow,
					                               TEXT("Write queue overflow"));
				}
			}
		}
	}

	Stats.InSendQueueDepth = SendQueueDepth.Load();
	SET_DWORD_STAT(STAT_NativeWebWebSocket_SendQueueDepth, static_cast<uint32>(FMath::Min<int64>(Stats.InSendQueueDepth, MAX_uint32)));
	return bDidWork;
}

bool FNativeWebWebSocketServerInstance::PumpOutgoingOnWriteThread()
{
	SCOPE_CYCLE_COUNTER(STAT_NativeWebWebSocket_WritePump);
	if (!bRunning.Load())
	{
		return false;
	}

	struct FSendWork
	{
		int64 ClientId = 0;
		INativeWebWebSocketNetwork* Socket = nullptr;
		FNativeWebWebSocketOutboundMessage Message;
	};

	TArray<FSendWork> workItems;
	const int32 maxBatchMessagesPerClient = FMath::Max(1, NativeWebWebSocketCVars::GetWriteBatchMaxMessages());
	const int32 maxBatchBytesPerClient = FMath::Max(1, NativeWebWebSocketCVars::GetWriteBatchMaxBytes());
	{
		FWriteScopeLock lock(ClientLock);
		for (TPair<int64, FNativeWebWebSocketClientState>& pair : Clients)
		{
			if (!pair.Value.Outbox)
			{
				continue;
			}

			int32 drainedMessages = 0;
			int32 drainedBytes = 0;
			FNativeWebWebSocketOutboundMessage message;
			while (drainedMessages < maxBatchMessagesPerClient &&
				drainedBytes < maxBatchBytesPerClient &&
				pair.Value.Outbox->Dequeue(message))
			{
				pair.Value.QueueMessages = FMath::Max(0, pair.Value.QueueMessages - 1);
				pair.Value.QueueBytes = FMath::Max<int64>(0, pair.Value.QueueBytes - message.Payload.Num());
				Stats.QueueDepth = FMath::Max<int64>(0, Stats.QueueDepth - 1);
				RecordQueueDepthSample_NoLock();
				FSendWork& item = workItems.AddDefaulted_GetRef();
				item.ClientId = pair.Key;
				item.Socket = pair.Value.Socket;
				item.Message = MoveTemp(message);
				drainedMessages += 1;
				drainedBytes += item.Message.Payload.Num();
			}
		}
	}

	if (workItems.Num() == 0)
	{
		Stats.InWriteQueueDepth = Stats.QueueDepth;
		SET_DWORD_STAT(STAT_NativeWebWebSocket_WriteQueueDepth, static_cast<uint32>(FMath::Min<int64>(Stats.InWriteQueueDepth, MAX_uint32)));
		return false;
	}

	int64 sentBytes = 0;
	const int32 maxTxBytesPerSec = NativeWebWebSocketCVars::GetMaxTxBytesPerSecPerClient();
	for (const FSendWork& item : workItems)
	{
		if (!item.Socket || item.Message.Payload.Num() == 0)
		{
			continue;
		}

		bool bTxLimited = false;
		{
			FWriteScopeLock lock(ClientLock);
			if (FNativeWebWebSocketClientState* state = Clients.Find(item.ClientId))
			{
				const double nowSec = FPlatformTime::Seconds();
				if (state->TxWindowStartSec <= 0.0 || nowSec - state->TxWindowStartSec >= 1.0)
				{
					state->TxWindowStartSec = nowSec;
					state->TxBytesInWindow = 0;
				}
				if (maxTxBytesPerSec > 0 && state->TxBytesInWindow + item.Message.Payload.Num() > maxTxBytesPerSec)
				{
					bTxLimited = true;
				}
				else
				{
					state->TxBytesInWindow += item.Message.Payload.Num();
				}
			}
		}
		if (bTxLimited)
		{
			Stats.DroppedMessages += 1;
			continue;
		}

		if (!item.Socket->Send(item.Message.Payload.GetData(), static_cast<uint32>(item.Message.Payload.Num()), false))
		{
			if (OwnerServer.IsValid())
			{
				OwnerServer->NotifyClientError(item.ClientId, ENativeWebWebSocketErrorCode::SocketSendFailed,
				                               TEXT("Socket send failed"));
			}
			continue;
		}
		sentBytes += item.Message.Payload.Num();
	}

	{
		FWriteScopeLock lock(ClientLock);
		Stats.TxBytes += sentBytes;
		UpdateRateStats_NoLock();
		Stats.InWriteQueueDepth = Stats.QueueDepth;
	}

	SET_DWORD_STAT(STAT_NativeWebWebSocket_ActiveClients, Stats.ActiveClients);
	SET_DWORD_STAT(STAT_NativeWebWebSocket_QueueDepth, static_cast<uint32>(FMath::Min<int64>(Stats.QueueDepth, MAX_uint32)));
	SET_DWORD_STAT(STAT_NativeWebWebSocket_WriteQueueDepth, static_cast<uint32>(FMath::Min<int64>(Stats.InWriteQueueDepth, MAX_uint32)));
	SET_DWORD_STAT(STAT_NativeWebWebSocket_TxBytes, static_cast<uint32>(FMath::Min<int64>(Stats.TxBytes, MAX_uint32)));
	SET_DWORD_STAT(STAT_NativeWebWebSocket_DroppedMessages,
	               static_cast<uint32>(FMath::Min<int64>(Stats.DroppedMessages, MAX_uint32)));
	return true;
}

void FNativeWebWebSocketServerInstance::HandleClientConnected(INativeWebWebSocketNetwork* InSocket)
{
	if (!InSocket)
	{
		return;
	}

	FNativeWebWebSocketClientInfo info;
	info.ClientId = NextClientId++;
	info.RemoteAddress = InSocket->RemoteEndPoint(true);
	info.ConnectedAt = FDateTime::UtcNow();
	const double nowSec = FPlatformTime::Seconds();

	FNativeWebWebSocketNetworkPacketReceivedCallback onReceive;
	onReceive.BindLambda([this, InSocket](void* data, int32 count, bool bBinary)
	{
		HandleClientPacket(InSocket, static_cast<const uint8*>(data), count, bBinary);
	});
	InSocket->SetReceiveCallBack(onReceive);

	{
		FWriteScopeLock lock(ClientLock);
		FNativeWebWebSocketClientState& state = Clients.FindOrAdd(info.ClientId);
		state.Info = info;
		state.Socket = InSocket;
		state.LastActivityTimeSec = nowSec;
		state.LastHeartbeatSentTimeSec = 0.0;
		state.RxWindowStartSec = nowSec;
		state.TxWindowStartSec = nowSec;
		state.RxBytesInWindow = 0;
		state.TxBytesInWindow = 0;
		ClientIdsBySocket.Add(InSocket, info.ClientId);
		Stats.ActiveClients = Clients.Num();
		RecordQueueDepthSample_NoLock();
	}
	bReadActivity.Store(true);

	if (OwnerServer.IsValid())
	{
		OwnerServer->NotifyClientConnected(info);
	}
}

void FNativeWebWebSocketServerInstance::HandleClientDisconnected(INativeWebWebSocketNetwork* InSocket)
{
	if (!InSocket)
	{
		return;
	}

	int64 clientId = 0;
	{
		FWriteScopeLock lock(ClientLock);
		if (int64* found = ClientIdsBySocket.Find(InSocket))
		{
			clientId = *found;
			if (const FNativeWebWebSocketClientState* state = Clients.Find(clientId))
			{
				Stats.QueueDepth = FMath::Max<int64>(0, Stats.QueueDepth - state->QueueMessages);
			}
			ClientIdsBySocket.Remove(InSocket);
			Clients.Remove(clientId);
			Stats.ActiveClients = Clients.Num();
			RecordQueueDepthSample_NoLock();
		}
	}
	bReadActivity.Store(true);

	if (clientId != 0 && OwnerServer.IsValid())
	{
		OwnerServer->NotifyClientDisconnected(clientId, ENativeWebWebSocketCloseReason::ClientClosed);
	}
}

void FNativeWebWebSocketServerInstance::HandleClientPacket(INativeWebWebSocketNetwork* InSocket, const uint8* InData, int32 InCount,
                                                     bool bInBinary)
{
	if (!InSocket || !InData || InCount <= 0)
	{
		return;
	}
	if (InCount > NativeWebWebSocketCVars::GetMaxMessageBytes())
	{
		int64 clientId = 0;
		{
			FWriteScopeLock lock(ClientLock);
			if (int64* found = ClientIdsBySocket.Find(InSocket))
			{
				clientId = *found;
			}
		}
		if (clientId != 0 && OwnerServer.IsValid())
		{
			OwnerServer->NotifyClientError(clientId, ENativeWebWebSocketErrorCode::InvalidPayload,
			                               TEXT("Incoming message exceeds cefws.max_message_bytes"));
		}
		return;
	}
	if (!bInBinary && InCount > NativeWebWebSocketCVars::GetMaxTextMessageBytes())
	{
		int64 clientId = 0;
		{
			FWriteScopeLock lock(ClientLock);
			if (int64* found = ClientIdsBySocket.Find(InSocket))
			{
				clientId = *found;
			}
		}
		if (clientId != 0 && OwnerServer.IsValid())
		{
			OwnerServer->NotifyClientError(clientId, ENativeWebWebSocketErrorCode::InvalidPayload,
			                               TEXT("Incoming text message exceeds cefws.max_text_message_bytes"));
		}
		return;
	}

	int64 clientId = 0;
	bool bRxLimited = false;
	const int32 maxRxBytesPerSec = NativeWebWebSocketCVars::GetMaxRxBytesPerSecPerClient();
	{
		FWriteScopeLock lock(ClientLock);
		if (int64* found = ClientIdsBySocket.Find(InSocket))
		{
			clientId = *found;
			if (FNativeWebWebSocketClientState* state = Clients.Find(clientId))
			{
				const double nowSec = FPlatformTime::Seconds();
				if (state->RxWindowStartSec <= 0.0 || nowSec - state->RxWindowStartSec >= 1.0)
				{
					state->RxWindowStartSec = nowSec;
					state->RxBytesInWindow = 0;
				}
				if (maxRxBytesPerSec > 0 && state->RxBytesInWindow + InCount > maxRxBytesPerSec)
				{
					bRxLimited = true;
				}
				else
				{
					state->RxBytesInWindow += InCount;
					state->LastActivityTimeSec = nowSec;
					Stats.RxBytes += InCount;
					UpdateRateStats_NoLock();
				}
			}
		}
	}
	bReadActivity.Store(true);

	if (clientId == 0 || bRxLimited)
	{
		if (bRxLimited && OwnerServer.IsValid())
		{
			OwnerServer->NotifyClientError(clientId, ENativeWebWebSocketErrorCode::QueueOverflow,
			                               TEXT("Inbound bandwidth cap exceeded"));
		}
		return;
	}

	if (InboundQueueDepth.Load() >= FMath::Max(1, PipelineConfig.InInboundQueueMax))
	{
		Stats.DroppedMessages += 1;
		if (OwnerServer.IsValid())
		{
			OwnerServer->NotifyClientError(clientId, ENativeWebWebSocketErrorCode::QueueOverflow,
			                               TEXT("Inbound queue overflow"));
		}
		return;
	}

	FNativeWebWebSocketInboundPacket inboundPacket;
	inboundPacket.ClientId = clientId;
	inboundPacket.Payload.Append(InData, InCount);
	inboundPacket.bBinary = bInBinary;
	InboundQueue.Enqueue(MoveTemp(inboundPacket));
	InboundQueueDepth.Store(InboundQueueDepth.Load() + 1);
	Stats.InInboundQueueDepth = InboundQueueDepth.Load();
	WakeHandleThread();

	SET_DWORD_STAT(STAT_NativeWebWebSocket_RxBytes, static_cast<uint32>(FMath::Min<int64>(Stats.RxBytes, MAX_uint32)));
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::QueueSendRequest(FNativeWebWebSocketSendRequest&& InRequest)
{
	if (!IsRunning())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}

	int32 payloadSizeBytes = InRequest.BytesPayload.Num();
	if (payloadSizeBytes <= 0 && !InRequest.TextPayload.IsEmpty())
	{
		FTCHARToUTF8 utf8Payload(*InRequest.TextPayload);
		payloadSizeBytes = utf8Payload.Length();
	}
	if (payloadSizeBytes > NativeWebWebSocketCVars::GetMaxOutboundMessageBytes())
	{
		return ENativeWebWebSocketSendResult::TooLarge;
	}

	if (SendQueueDepth.Load() >= FMath::Max(1, PipelineConfig.InSendQueueMax))
	{
		Stats.DroppedMessages += 1;
		return ENativeWebWebSocketSendResult::QueueFull;
	}

	SendQueue.Enqueue(MoveTemp(InRequest));
	SendQueueDepth.Store(SendQueueDepth.Load() + 1);
	Stats.InSendQueueDepth = SendQueueDepth.Load();
	WakeSendThread();
	return ENativeWebWebSocketSendResult::Ok;
}

bool FNativeWebWebSocketServerInstance::EnqueueWritePacket(int64 InClientId, const uint8* InData, int32 InCount, bool bInBinary)
{
	if (!IsRunning() || !InData || InCount <= 0)
	{
		return false;
	}

	FWriteScopeLock lock(ClientLock);
	FNativeWebWebSocketClientState* found = Clients.Find(InClientId);
	if (!found || !found->Socket)
	{
		return false;
	}

	const int32 maxMessages = FMath::Max(1, NativeWebWebSocketCVars::GetMaxQueueMessagesPerClient());
	const int32 maxBytes = FMath::Max(1, NativeWebWebSocketCVars::GetMaxQueueBytesPerClient());
	const bool bRejectNewOnOverflow = NativeWebWebSocketCVars::GetQueueDropPolicy() == 1;
	const int32 configuredMaxMessages = FMath::Max(1, PipelineConfig.InWriteQueueMaxPerClient);
	const int32 finalMaxMessages = FMath::Min(maxMessages, configuredMaxMessages);

	while (!bRejectNewOnOverflow &&
		(found->QueueMessages >= finalMaxMessages || found->QueueBytes + InCount > maxBytes) &&
		found->QueueMessages > 0)
	{
		FNativeWebWebSocketOutboundMessage dropped;
		if (!found->Outbox || !found->Outbox->Dequeue(dropped))
		{
			break;
		}
		found->QueueMessages = FMath::Max(0, found->QueueMessages - 1);
		found->QueueBytes = FMath::Max<int64>(0, found->QueueBytes - dropped.Payload.Num());
		Stats.DroppedMessages += 1;
		Stats.QueueDepth = FMath::Max<int64>(0, Stats.QueueDepth - 1);
		RecordQueueDepthSample_NoLock();
	}

	if (found->QueueMessages >= finalMaxMessages || found->QueueBytes + InCount > maxBytes)
	{
		Stats.DroppedMessages += 1;
		return false;
	}

	FNativeWebWebSocketOutboundMessage message;
	message.Payload.Append(InData, InCount);
	message.bBinary = bInBinary;
	if (!found->Outbox)
	{
		found->Outbox = MakeUnique<TQueue<FNativeWebWebSocketOutboundMessage, EQueueMode::Mpsc>>();
	}
	found->Outbox->Enqueue(MoveTemp(message));
	found->QueueMessages += 1;
	found->QueueBytes += InCount;
	Stats.QueueDepth += 1;
	Stats.InWriteQueueDepth = Stats.QueueDepth;
	RecordQueueDepthSample_NoLock();
	WakeWriteThread();
	return true;
}

void FNativeWebWebSocketServerInstance::WakeHandleThread()
{
	if (HandleWakeEvent)
	{
		HandleWakeEvent->Trigger();
	}
}

void FNativeWebWebSocketServerInstance::WakeSendThread()
{
	if (SendWakeEvent)
	{
		SendWakeEvent->Trigger();
	}
}

void FNativeWebWebSocketServerInstance::WakeWriteThread()
{
	if (WriteWakeEvent)
	{
		WriteWakeEvent->Trigger();
	}
}

TSharedPtr<INativeWebWebSocketPacketCodec> FNativeWebWebSocketServerInstance::ResolveCodec(ENativeWebWebSocketPayloadFormat InPayloadFormat) const
{
	FScopeLock lock(&CodecLock);
	if (CustomCodec.IsValid())
	{
		return CustomCodec;
	}

	switch (InPayloadFormat)
	{
	case ENativeWebWebSocketPayloadFormat::Binary:
		return BinaryCodec;
	case ENativeWebWebSocketPayloadFormat::Utf8String:
		return Utf8Codec;
	case ENativeWebWebSocketPayloadFormat::JsonString:
		return JsonCodec;
	case ENativeWebWebSocketPayloadFormat::XmlString:
		return XmlCodec;
	case ENativeWebWebSocketPayloadFormat::Custom:
	default:
		return BinaryCodec;
	}
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::SendToClientString(int64 InClientId, const FString& InMessage)
{
	if (!IsRunning())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}

	FNativeWebWebSocketSendRequest request;
	request.TargetClientIds.Add(InClientId);
	request.PayloadFormat = PayloadFormat;
	request.TextPayload = InMessage;
	return QueueSendRequest(MoveTemp(request));
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::SendToClientBytes(int64 InClientId, const TArray<uint8>& InBytes)
{
	if (!IsRunning())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	if (InBytes.Num() == 0)
	{
		return ENativeWebWebSocketSendResult::InternalError;
	}

	FNativeWebWebSocketSendRequest request;
	request.TargetClientIds.Add(InClientId);
	request.PayloadFormat = ENativeWebWebSocketPayloadFormat::Binary;
	request.BytesPayload = InBytes;
	return QueueSendRequest(MoveTemp(request));
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::BroadcastString(const FString& InMessage)
{
	FNativeWebWebSocketSendRequest request;
	request.bBroadcast = true;
	request.PayloadFormat = PayloadFormat;
	request.TextPayload = InMessage;
	return QueueSendRequest(MoveTemp(request));
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::BroadcastBytes(const TArray<uint8>& InBytes)
{
	if (InBytes.Num() == 0)
	{
		return ENativeWebWebSocketSendResult::InternalError;
	}

	FNativeWebWebSocketSendRequest request;
	request.bBroadcast = true;
	request.PayloadFormat = ENativeWebWebSocketPayloadFormat::Binary;
	request.BytesPayload = InBytes;
	return QueueSendRequest(MoveTemp(request));
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::BroadcastStringExcept(int64 InExcludedClientId, const FString& InMessage)
{
	FNativeWebWebSocketSendRequest request;
	request.bBroadcast = true;
	request.ExcludedClientId = InExcludedClientId;
	request.PayloadFormat = PayloadFormat;
	request.TextPayload = InMessage;
	return QueueSendRequest(MoveTemp(request));
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::BroadcastBytesExcept(int64 InExcludedClientId, const TArray<uint8>& InBytes)
{
	if (InBytes.Num() == 0)
	{
		return ENativeWebWebSocketSendResult::InternalError;
	}

	FNativeWebWebSocketSendRequest request;
	request.bBroadcast = true;
	request.ExcludedClientId = InExcludedClientId;
	request.PayloadFormat = ENativeWebWebSocketPayloadFormat::Binary;
	request.BytesPayload = InBytes;
	return QueueSendRequest(MoveTemp(request));
}

ENativeWebWebSocketSendResult FNativeWebWebSocketServerInstance::DisconnectClient(int64 InClientId,
                                                                      ENativeWebWebSocketCloseReason InReason)
{
	INativeWebWebSocketNetwork* socket = nullptr;
	{
		FReadScopeLock lock(ClientLock);
		if (FNativeWebWebSocketClientState* found = Clients.Find(InClientId))
		{
			socket = found->Socket;
		}
	}
	if (!socket)
	{
		return ENativeWebWebSocketSendResult::InvalidClient;
	}

	socket->Close(ResolveCloseStatusCode(InReason), ResolveCloseReasonText(InReason));
	return ENativeWebWebSocketSendResult::Ok;
}

TArray<FNativeWebWebSocketClientInfo> FNativeWebWebSocketServerInstance::GetClients() const
{
	FReadScopeLock lock(ClientLock);
	TArray<FNativeWebWebSocketClientInfo> outClients;
	outClients.Reserve(Clients.Num());
	for (const TPair<int64, FNativeWebWebSocketClientState>& pair : Clients)
	{
		outClients.Add(pair.Value.Info);
	}
	return outClients;
}

FNativeWebWebSocketServerStats FNativeWebWebSocketServerInstance::GetStats() const
{
	FWriteScopeLock lock(ClientLock);
	const_cast<FNativeWebWebSocketServerInstance*>(this)->UpdateRateStats_NoLock();
	if (QueueDepthSamples > 0)
	{
		const_cast<FNativeWebWebSocketServerInstance*>(this)->Stats.AvgQueueDepth =
			static_cast<float>(static_cast<double>(QueueDepthAccum) / static_cast<double>(QueueDepthSamples));
	}
	else
	{
		const_cast<FNativeWebWebSocketServerInstance*>(this)->Stats.AvgQueueDepth = static_cast<float>(Stats.QueueDepth);
	}
	const_cast<FNativeWebWebSocketServerInstance*>(this)->Stats.InInboundQueueDepth = InboundQueueDepth.Load();
	const_cast<FNativeWebWebSocketServerInstance*>(this)->Stats.InHandleQueueDepth = InboundQueueDepth.Load();
	const_cast<FNativeWebWebSocketServerInstance*>(this)->Stats.InSendQueueDepth = SendQueueDepth.Load();
	const_cast<FNativeWebWebSocketServerInstance*>(this)->Stats.InWriteQueueDepth = Stats.QueueDepth;
	return Stats;
}

FNativeWebWebSocketClientInfo FNativeWebWebSocketServerInstance::FindClientInfo(int64 InClientId) const
{
	FReadScopeLock lock(ClientLock);
	if (const FNativeWebWebSocketClientState* found = Clients.Find(InClientId))
	{
		return found->Info;
	}
	return FNativeWebWebSocketClientInfo();
}

void FNativeWebWebSocketServerInstance::RecordQueueDepthSample_NoLock()
{
	QueueDepthAccum += Stats.QueueDepth;
	QueueDepthSamples += 1;
	if (QueueDepthSamples > 0)
	{
		Stats.AvgQueueDepth = static_cast<float>(static_cast<double>(QueueDepthAccum) / static_cast<double>(QueueDepthSamples));
	}
}

void FNativeWebWebSocketServerInstance::UpdateRateStats_NoLock()
{
	const double nowSec = FPlatformTime::Seconds();
	if (LastRateSampleTimeSec <= 0.0)
	{
		LastRateSampleTimeSec = nowSec;
		LastRateRxBytes = Stats.RxBytes;
		LastRateTxBytes = Stats.TxBytes;
		Stats.RxBytesPerSec = 0.0f;
		Stats.TxBytesPerSec = 0.0f;
		return;
	}

	const double deltaSec = nowSec - LastRateSampleTimeSec;
	if (deltaSec < 0.05)
	{
		return;
	}

	const int64 deltaRx = Stats.RxBytes - LastRateRxBytes;
	const int64 deltaTx = Stats.TxBytes - LastRateTxBytes;
	Stats.RxBytesPerSec = static_cast<float>(static_cast<double>(deltaRx) / deltaSec);
	Stats.TxBytesPerSec = static_cast<float>(static_cast<double>(deltaTx) / deltaSec);
	LastRateSampleTimeSec = nowSec;
	LastRateRxBytes = Stats.RxBytes;
	LastRateTxBytes = Stats.TxBytes;
}

void FNativeWebWebSocketServerInstance::SweepConnectionHealthOnReadThread()
{
	const float heartbeatIntervalSec = NativeWebWebSocketCVars::GetHeartbeatIntervalSec();
	const float idleTimeoutSec = NativeWebWebSocketCVars::GetIdleTimeoutSec();
	if (heartbeatIntervalSec <= 0.0f && idleTimeoutSec <= 0.0f)
	{
		return;
	}

	const double nowSec = FPlatformTime::Seconds();
	TArray<INativeWebWebSocketNetwork*> socketsToHeartbeat;
	TArray<INativeWebWebSocketNetwork*> socketsToClose;

	{
		FWriteScopeLock lock(ClientLock);
		for (TPair<int64, FNativeWebWebSocketClientState>& pair : Clients)
		{
			FNativeWebWebSocketClientState& state = pair.Value;
			if (!state.Socket)
			{
				continue;
			}

			if (idleTimeoutSec > 0.0f && state.LastActivityTimeSec > 0.0 &&
				nowSec - state.LastActivityTimeSec >= static_cast<double>(idleTimeoutSec))
			{
				socketsToClose.Add(state.Socket);
				continue;
			}

			if (heartbeatIntervalSec > 0.0f &&
				(nowSec - state.LastHeartbeatSentTimeSec) >= static_cast<double>(heartbeatIntervalSec))
			{
				socketsToHeartbeat.Add(state.Socket);
				state.LastHeartbeatSentTimeSec = nowSec;
			}
		}
	}

	static const uint8 heartbeatPayload[] = {'_', '_', 'c', 'e', 'f', 'w', 's', '_', 'h', 'b', '_', '_'};
	for (INativeWebWebSocketNetwork* socket : socketsToHeartbeat)
	{
		if (!socket)
		{
			continue;
		}
		socket->Send(heartbeatPayload, static_cast<uint32>(UE_ARRAY_COUNT(heartbeatPayload)), false);
	}

	for (INativeWebWebSocketNetwork* socket : socketsToClose)
	{
		if (!socket)
		{
			continue;
		}
		socket->Close(ResolveCloseStatusCode(ENativeWebWebSocketCloseReason::Timeout),
		              ResolveCloseReasonText(ENativeWebWebSocketCloseReason::Timeout));
	}
}

bool FNativeWebWebSocketServerInstance::AreQueuesDrained() const
{
	if (InboundQueueDepth.Load() > 0 || SendQueueDepth.Load() > 0)
	{
		return false;
	}

	FReadScopeLock lock(ClientLock);
	if (Stats.QueueDepth > 0)
	{
		return false;
	}
	for (const TPair<int64, FNativeWebWebSocketClientState>& pair : Clients)
	{
		if (pair.Value.QueueMessages > 0)
		{
			return false;
		}
	}
	return true;
}





