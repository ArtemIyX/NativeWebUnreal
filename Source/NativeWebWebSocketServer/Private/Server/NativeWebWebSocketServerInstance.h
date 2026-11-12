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
/**
 * @brief Declares the f runnable thread type and its operations.
 * @ingroup NativeWebWebSocketServer
 */
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
	/** @brief Start API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Start();
 * @endcode
 */
	bool Start();
	/** @brief Stop API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Stop();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	void Stop();
	/** @brief IsRunning API.
 * @return True when the named condition holds.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * IsRunning();
 * @endcode
 */
	bool IsRunning() const;

	/** @brief SendToClientString API.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendToClientString(InClientId, InMessage);
 * @endcode
 */
	ENativeWebWebSocketSendResult SendToClientString(int64 InClientId, const FString& InMessage);
	/** @brief SendToClientBytes API.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InBytes Opaque payload bytes; this operation does not impose an application schema.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendToClientBytes(InClientId, InBytes);
 * @endcode
 */
	ENativeWebWebSocketSendResult SendToClientBytes(int64 InClientId, const TArray<uint8>& InBytes);
	/** @brief BroadcastString API.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastString(InMessage);
 * @endcode
 */
	ENativeWebWebSocketSendResult BroadcastString(const FString& InMessage);
	/** @brief BroadcastBytes API.
* @param InBytes Opaque payload bytes; this operation does not impose an application schema.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastBytes(InBytes);
 * @endcode
 */
	ENativeWebWebSocketSendResult BroadcastBytes(const TArray<uint8>& InBytes);
	/** @brief BroadcastStringExcept API.
* @param InExcludedClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMessage Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastStringExcept(InExcludedClientId, InMessage);
 * @endcode
 */
	ENativeWebWebSocketSendResult BroadcastStringExcept(int64 InExcludedClientId, const FString& InMessage);
	/** @brief BroadcastBytesExcept API.
* @param InExcludedClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InBytes Opaque payload bytes; this operation does not impose an application schema.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastBytesExcept(InExcludedClientId, InBytes);
 * @endcode
 */
	ENativeWebWebSocketSendResult BroadcastBytesExcept(int64 InExcludedClientId, const TArray<uint8>& InBytes);
	/** @brief DisconnectClient API.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InReason Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DisconnectClient(InClientId, InReason);
 * @endcode
 */
	ENativeWebWebSocketSendResult DisconnectClient(int64 InClientId, ENativeWebWebSocketCloseReason InReason);

	/** @brief SetPipelineConfig API.
* @param InConfig Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetPipelineConfig(InConfig);
 * @endcode
 */
	void SetPipelineConfig(const FNativeWebWebSocketPipelineConfig& InConfig);
	/** @brief SetPayloadFormat API.
* @param InPayloadFormat Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetPayloadFormat(InPayloadFormat);
 * @endcode
 */
	void SetPayloadFormat(ENativeWebWebSocketPayloadFormat InPayloadFormat);
	/** @brief SetPacketCodec API.
* @param InCodec Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetPacketCodec(InCodec);
 * @endcode
 */
	void SetPacketCodec(const TSharedPtr<INativeWebWebSocketPacketCodec>& InCodec);

	/** @brief GetClients API.
 * @return The resulting collection or snapshot.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetClients();
 * @endcode
 */
	TArray<FNativeWebWebSocketClientInfo> GetClients() const;
	/** @brief GetStats API.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetStats();
 * @endcode
 */
	FNativeWebWebSocketServerStats GetStats() const;
	/** @brief FindClientInfo API.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * FindClientInfo(InClientId);
 * @endcode
 */
	FNativeWebWebSocketClientInfo FindClientInfo(int64 InClientId) const;
/**
 * @brief Returns name id.
 * @return The resulting text or identifier.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetNameId();
 * @endcode
 */

	FName GetNameId() const { return NameId; }
/**
 * @brief Returns bound port.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetBoundPort();
 * @endcode
 */

	int32 GetBoundPort() const { return BoundPort; }
#pragma endregion

public:
#pragma region ThreadApi
	/** @brief TickBackendOnReadThread API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * TickBackendOnReadThread();
 * @endcode
 */
	bool TickBackendOnReadThread();
	/** @brief PumpInboundOnHandleThread API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * PumpInboundOnHandleThread();
 * @endcode
 */
	bool PumpInboundOnHandleThread();
	/** @brief PumpOutgoingOnSendThread API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * PumpOutgoingOnSendThread();
 * @endcode
 */
	bool PumpOutgoingOnSendThread();
	/** @brief PumpOutgoingOnWriteThread API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * PumpOutgoingOnWriteThread();
 * @endcode
 */
	bool PumpOutgoingOnWriteThread();
#pragma endregion
/**
 * @brief Defines the f native web web socket outbound message data structure.
 * @ingroup NativeWebWebSocketServer
 */

private:
#pragma region InternalTypes
	/** @brief Type declaration. */
	struct FNativeWebWebSocketOutboundMessage
/**
 * @brief Defines the f native web web socket client state data structure.
 * @ingroup NativeWebWebSocketServer
 */
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
	/** @brief HandleClientConnected API.
* @param InSocket Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientConnected(InSocket);
 * @endcode
 */
	void HandleClientConnected(INativeWebWebSocketNetwork* InSocket);
	/** @brief HandleClientDisconnected API.
* @param InSocket Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientDisconnected(InSocket);
 * @endcode
 */
	void HandleClientDisconnected(INativeWebWebSocketNetwork* InSocket);
	/** @brief HandleClientPacket API.
* @param InSocket Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InData Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InCount Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInBinary Boolean option controlling this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClientPacket(InSocket, InData, InCount, bInBinary);
 * @endcode
 */
	void HandleClientPacket(INativeWebWebSocketNetwork* InSocket, const uint8* InData, int32 InCount, bool bInBinary);

	/** @brief QueueSendRequest API.
* @param InRequest Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return An enumerated status describing the outcome.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * QueueSendRequest(InRequest);
 * @endcode
 */
	ENativeWebWebSocketSendResult QueueSendRequest(FNativeWebWebSocketSendRequest&& InRequest);
	/** @brief WakeHandleThread API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * WakeHandleThread();
 * @endcode
 */
	void WakeHandleThread();
	/** @brief WakeSendThread API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * WakeSendThread();
 * @endcode
 */
	void WakeSendThread();
	/** @brief WakeWriteThread API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * WakeWriteThread();
 * @endcode
 */
	void WakeWriteThread();

	/** @brief ResolveCodec API.
* @param InPayloadFormat Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return A shared result; it is invalid when the resource is unavailable.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ResolveCodec(InPayloadFormat);
 * @endcode
 */
	TSharedPtr<INativeWebWebSocketPacketCodec> ResolveCodec(ENativeWebWebSocketPayloadFormat InPayloadFormat) const;
	/** @brief EnqueueWritePacket API.
* @param InClientId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InData Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InCount Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInBinary Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EnqueueWritePacket(InClientId, InData, InCount, bInBinary);
 * @endcode
 */
	bool EnqueueWritePacket(int64 InClientId, const uint8* InData, int32 InCount, bool bInBinary);
	/** @brief RecordQueueDepthSample_NoLock API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RecordQueueDepthSample_NoLock();
 * @endcode
 */
	void RecordQueueDepthSample_NoLock();
	/** @brief UpdateRateStats_NoLock API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * UpdateRateStats_NoLock();
 * @endcode
 */
	void UpdateRateStats_NoLock();
	/** @brief SweepConnectionHealthOnReadThread API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SweepConnectionHealthOnReadThread();
 * @endcode
 */
	void SweepConnectionHealthOnReadThread();
	/** @brief AreQueuesDrained API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * AreQueuesDrained();
 * @endcode
 */
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

