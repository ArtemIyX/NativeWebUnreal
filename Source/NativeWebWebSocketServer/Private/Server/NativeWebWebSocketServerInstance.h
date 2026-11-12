/**
 * @file NativeWebWebSocketServer\Private\Server\NativeWebWebSocketServerInstance.h
 * @brief Declares NativeWebWebSocketServerInstance for module NativeWebWebSocketServer\Private\Server\NativeWebWebSocketServerInstance.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/NativeWebWebSocketStructs.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "Containers/Queue.h"
#include "Pipeline/NativeWebWebSocketPipelineTypes.h"

class INativeWebWebSocketNetwork;
class INativeWebWebSocketServerBackend;
class INativeWebWebSocketPacketCodec;
class FNativeWebWebSocketReadRunnable;
class FNativeWebWebSocketHandleRunnable;
class FNativeWebWebSocketSendRunnable;
class FNativeWebWebSocketWriteRunnable;
class FRunnableThread;
class UNativeWebWebSocketServerBase;

/** @brief Type declaration. */
class FNativeWebWebSocketServerInstance : public TSharedFromThis<FNativeWebWebSocketServerInstance>
{
public:
#pragma region Lifecycle
	/** @brief FNativeWebWebSocketServerInstance API. */
	FNativeWebWebSocketServerInstance(FName InNameId, int32 InBoundPort, TWeakObjectPtr<UNativeWebWebSocketServerBase> InOwnerServer);
	/** @brief FNativeWebWebSocketServerInstance API. */
	~FNativeWebWebSocketServerInstance();
#pragma endregion

public:
#pragma region PublicApi
	/** @brief Start API. */
	bool Start();
	/** @brief Stop API. */
	void Stop();
	/** @brief IsRunning API. */
	bool IsRunning() const;

	/** @brief SendToClientString API. */
	ENativeWebWebSocketSendResult SendToClientString(int64 InClientId, const FString& InMessage);
	/** @brief SendToClientBytes API. */
	ENativeWebWebSocketSendResult SendToClientBytes(int64 InClientId, const TArray<uint8>& InBytes);
	/** @brief BroadcastString API. */
	ENativeWebWebSocketSendResult BroadcastString(const FString& InMessage);
	/** @brief BroadcastBytes API. */
	ENativeWebWebSocketSendResult BroadcastBytes(const TArray<uint8>& InBytes);
	/** @brief BroadcastStringExcept API. */
	ENativeWebWebSocketSendResult BroadcastStringExcept(int64 InExcludedClientId, const FString& InMessage);
	/** @brief BroadcastBytesExcept API. */
	ENativeWebWebSocketSendResult BroadcastBytesExcept(int64 InExcludedClientId, const TArray<uint8>& InBytes);
	/** @brief DisconnectClient API. */
	ENativeWebWebSocketSendResult DisconnectClient(int64 InClientId, ENativeWebWebSocketCloseReason InReason);

	/** @brief SetPipelineConfig API. */
	void SetPipelineConfig(const FNativeWebWebSocketPipelineConfig& InConfig);
	/** @brief SetPayloadFormat API. */
	void SetPayloadFormat(ENativeWebWebSocketPayloadFormat InPayloadFormat);
	/** @brief SetPacketCodec API. */
	void SetPacketCodec(const TSharedPtr<INativeWebWebSocketPacketCodec>& InCodec);

	/** @brief GetClients API. */
	TArray<FNativeWebWebSocketClientInfo> GetClients() const;
	/** @brief GetStats API. */
	FNativeWebWebSocketServerStats GetStats() const;
	/** @brief FindClientInfo API. */
	FNativeWebWebSocketClientInfo FindClientInfo(int64 InClientId) const;
	FName GetNameId() const { return NameId; }
	int32 GetBoundPort() const { return BoundPort; }
#pragma endregion

public:
#pragma region ThreadApi
	/** @brief TickBackendOnReadThread API. */
	bool TickBackendOnReadThread();
	/** @brief PumpInboundOnHandleThread API. */
	bool PumpInboundOnHandleThread();
	/** @brief PumpOutgoingOnSendThread API. */
	bool PumpOutgoingOnSendThread();
	/** @brief PumpOutgoingOnWriteThread API. */
	bool PumpOutgoingOnWriteThread();
#pragma endregion

private:
#pragma region InternalTypes
	/** @brief Type declaration. */
	struct FNativeWebWebSocketOutboundMessage
	{
		/** @brief Payload state. */
		TArray<uint8> Payload;
		/** @brief bBinary state. */
		bool bBinary = true;
	};

	/** @brief Type declaration. */
	struct FNativeWebWebSocketClientState
	{
		FNativeWebWebSocketClientState()
			: Outbox(MakeUnique<TQueue<FNativeWebWebSocketOutboundMessage, EQueueMode::Mpsc>>())
		{
		}

		/** @brief Info state. */
		FNativeWebWebSocketClientInfo Info;
		/** @brief Socket state. */
		INativeWebWebSocketNetwork* Socket = nullptr;
		/** @brief Outbox state. */
		TUniquePtr<TQueue<FNativeWebWebSocketOutboundMessage, EQueueMode::Mpsc>> Outbox;
		/** @brief QueueBytes state. */
		int64 QueueBytes = 0;
		/** @brief QueueMessages state. */
		int32 QueueMessages = 0;
		/** @brief LastActivityTimeSec state. */
		double LastActivityTimeSec = 0.0;
		/** @brief LastHeartbeatSentTimeSec state. */
		double LastHeartbeatSentTimeSec = 0.0;
		/** @brief RxWindowStartSec state. */
		double RxWindowStartSec = 0.0;
		/** @brief TxWindowStartSec state. */
		double TxWindowStartSec = 0.0;
		/** @brief RxBytesInWindow state. */
		int64 RxBytesInWindow = 0;
		/** @brief TxBytesInWindow state. */
		int64 TxBytesInWindow = 0;
	};
#pragma endregion

private:
#pragma region InternalMethods
	/** @brief HandleClientConnected API. */
	void HandleClientConnected(INativeWebWebSocketNetwork* InSocket);
	/** @brief HandleClientDisconnected API. */
	void HandleClientDisconnected(INativeWebWebSocketNetwork* InSocket);
	/** @brief HandleClientPacket API. */
	void HandleClientPacket(INativeWebWebSocketNetwork* InSocket, const uint8* InData, int32 InCount, bool bInBinary);

	/** @brief QueueSendRequest API. */
	ENativeWebWebSocketSendResult QueueSendRequest(FNativeWebWebSocketSendRequest&& InRequest);
	/** @brief WakeHandleThread API. */
	void WakeHandleThread();
	/** @brief WakeSendThread API. */
	void WakeSendThread();
	/** @brief WakeWriteThread API. */
	void WakeWriteThread();

	/** @brief ResolveCodec API. */
	TSharedPtr<INativeWebWebSocketPacketCodec> ResolveCodec(ENativeWebWebSocketPayloadFormat InPayloadFormat) const;
	/** @brief EnqueueWritePacket API. */
	bool EnqueueWritePacket(int64 InClientId, const uint8* InData, int32 InCount, bool bInBinary);
	/** @brief RecordQueueDepthSample_NoLock API. */
	void RecordQueueDepthSample_NoLock();
	/** @brief UpdateRateStats_NoLock API. */
	void UpdateRateStats_NoLock();
	/** @brief SweepConnectionHealthOnReadThread API. */
	void SweepConnectionHealthOnReadThread();
	/** @brief AreQueuesDrained API. */
	bool AreQueuesDrained() const;
#pragma endregion

private:
#pragma region InternalState
	/** @brief NameId state. */
	FName NameId = NAME_None;
	/** @brief BoundPort state. */
	int32 BoundPort = 0;
	/** @brief OwnerServer state. */
	TWeakObjectPtr<UNativeWebWebSocketServerBase> OwnerServer;
	/** @brief bRunning state. */
	TAtomic<bool> bRunning = false;

	/** @brief ClientLock state. */
	mutable FRWLock ClientLock;
	/** @brief Clients state. */
	TMap<int64, FNativeWebWebSocketClientState> Clients;
	/** @brief ClientIdsBySocket state. */
	TMap<INativeWebWebSocketNetwork*, int64> ClientIdsBySocket;

	/** @brief CodecLock state. */
	mutable FCriticalSection CodecLock;
	/** @brief PayloadFormat state. */
	ENativeWebWebSocketPayloadFormat PayloadFormat = ENativeWebWebSocketPayloadFormat::Binary;
	/** @brief CustomCodec state. */
	TSharedPtr<INativeWebWebSocketPacketCodec> CustomCodec;
	/** @brief BinaryCodec state. */
	TSharedPtr<INativeWebWebSocketPacketCodec> BinaryCodec;
	/** @brief Utf8Codec state. */
	TSharedPtr<INativeWebWebSocketPacketCodec> Utf8Codec;
	/** @brief JsonCodec state. */
	TSharedPtr<INativeWebWebSocketPacketCodec> JsonCodec;
	/** @brief XmlCodec state. */
	TSharedPtr<INativeWebWebSocketPacketCodec> XmlCodec;

	/** @brief InboundQueue state. */
	TQueue<FNativeWebWebSocketInboundPacket, EQueueMode::Mpsc> InboundQueue;
	/** @brief SendQueue state. */
	TQueue<FNativeWebWebSocketSendRequest, EQueueMode::Mpsc> SendQueue;
	/** @brief InboundQueueDepth state. */
	TAtomic<int64> InboundQueueDepth = 0;
	/** @brief SendQueueDepth state. */
	TAtomic<int64> SendQueueDepth = 0;
	/** @brief PipelineConfig state. */
	FNativeWebWebSocketPipelineConfig PipelineConfig;

	/** @brief Backend state. */
	TUniquePtr<INativeWebWebSocketServerBackend> Backend;
	/** @brief NextClientId state. */
	int64 NextClientId = 1;
	/** @brief Stats state. */
	FNativeWebWebSocketServerStats Stats;
	/** @brief LastRateSampleTimeSec state. */
	double LastRateSampleTimeSec = 0.0;
	/** @brief LastRateRxBytes state. */
	int64 LastRateRxBytes = 0;
	/** @brief LastRateTxBytes state. */
	int64 LastRateTxBytes = 0;
	/** @brief QueueDepthAccum state. */
	int64 QueueDepthAccum = 0;
	/** @brief QueueDepthSamples state. */
	int64 QueueDepthSamples = 0;

	/** @brief ReadRunnable state. */
	TUniquePtr<FNativeWebWebSocketReadRunnable> ReadRunnable;
	/** @brief HandleRunnable state. */
	TUniquePtr<FNativeWebWebSocketHandleRunnable> HandleRunnable;
	/** @brief SendRunnable state. */
	TUniquePtr<FNativeWebWebSocketSendRunnable> SendRunnable;
	/** @brief WriteRunnable state. */
	TUniquePtr<FNativeWebWebSocketWriteRunnable> WriteRunnable;
	/** @brief ReadThread state. */
	FRunnableThread* ReadThread = nullptr;
	/** @brief HandleThread state. */
	FRunnableThread* HandleThread = nullptr;
	/** @brief SendThread state. */
	FRunnableThread* SendThread = nullptr;
	/** @brief WriteThread state. */
	FRunnableThread* WriteThread = nullptr;
	/** @brief HandleWakeEvent state. */
	FEvent* HandleWakeEvent = nullptr;
	/** @brief SendWakeEvent state. */
	FEvent* SendWakeEvent = nullptr;
	/** @brief WriteWakeEvent state. */
	FEvent* WriteWakeEvent = nullptr;
	/** @brief bReadActivity state. */
	TAtomic<bool> bReadActivity = false;
#pragma endregion
};

