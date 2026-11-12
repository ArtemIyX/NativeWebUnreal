/**
 * @file NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketNetwork.h
 * @brief Declares NativeWebWebSocketNetwork for module NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketNetwork.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "Networking/INativeWebWebSocketNetwork.h"
#include "Networking/NativeWebWebSocketPrivate.h"

/** @brief the f native web web socket network type and its operations. */
class FNativeWebWebSocketNetwork : public INativeWebWebSocketNetwork
{
public:
	/** @brief FNativeWebWebSocketNetwork API. */
	FNativeWebWebSocketNetwork(NativeWebWebSocketInternalContext* InContext, NativeWebWebSocketInternal* InWsi);
/**
 * @brief Performs the ~f native web web socket network operation.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ~FNativeWebWebSocketNetwork();
 * @endcode
 */

	virtual ~FNativeWebWebSocketNetwork() override;

/**
 * @brief Sets connected call back.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetConnectedCallBack(InCallBack);
 * @endcode
 */

	virtual void SetConnectedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) override;
/**
 * @brief Sets connection error call back.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetConnectionErrorCallBack(InCallBack);
 * @endcode
 */

	virtual void SetConnectionErrorCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) override;
/**
 * @brief Sets receive call back.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetReceiveCallBack(InCallBack);
 * @endcode
 */

	virtual void SetReceiveCallBack(FNativeWebWebSocketNetworkPacketReceivedCallback InCallBack) override;
/**
 * @brief Sets socket closed call back.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetSocketClosedCallBack(InCallBack);
 * @endcode
 */

	virtual void SetSocketClosedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) override;

/**
 * @brief Performs the send operation.
* @param InData Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InSize Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInPrependSize Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Send(InData, InSize, bInPrependSize);
 * @endcode
 */

	virtual bool Send(const uint8* InData, uint32 InSize, bool bInPrependSize = true) override;
/**
 * @brief Advances bounded work owned by this object.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Tick();
 * @endcode
 */

	virtual void Tick() override;
/**
 * @brief Performs the flush operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Flush();
 * @endcode
 */

	virtual void Flush() override;
/**
 * @brief Closes this resource and releases its owned state.
* @param InStatusCode Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InReason Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Close(InStatusCode, InReason);
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */

	virtual void Close(uint16 InStatusCode = 1000, const FString& InReason = FString()) override;

/**
 * @brief Performs the remote end point operation.
* @param bInAppendPort Boolean option controlling this operation.
 * @return The resulting text or identifier.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RemoteEndPoint(bInAppendPort);
 * @endcode
 */

	virtual FString RemoteEndPoint(bool bInAppendPort) override;
/**
 * @brief Performs the local end point operation.
* @param bInAppendPort Boolean option controlling this operation.
 * @return The resulting text or identifier.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * LocalEndPoint(bInAppendPort);
 * @endcode
 */

	virtual FString LocalEndPoint(bool bInAppendPort) override;

	/** @brief OnReceive API.
* @param InData Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InSize Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInIsBinary Boolean option controlling this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * OnReceive(InData, InSize, bInIsBinary);
 * @endcode
 */
	void OnReceive(void* InData, uint32 InSize, bool bInIsBinary);
	/** @brief OnRawWebSocketWritable API.
* @param InWsi Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * OnRawWebSocketWritable(InWsi);
 * @endcode
 */
	void OnRawWebSocketWritable(NativeWebWebSocketInternal* InWsi);
	/** @brief OnClose API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * OnClose();
 * @endcode
 */
	void OnClose();

private:
	/** @brief FlushInternal API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * FlushInternal();
 * @endcode
 */
	void FlushInternal();

public:
	/** @brief ReceivedCallback state. */
	FNativeWebWebSocketNetworkPacketReceivedCallback ReceivedCallback;
	/** @brief ConnectedCallBack state. */
	FNativeWebWebSocketNetworkInfoCallback ConnectedCallBack;
	/** @brief ErrorCallBack state. */
	FNativeWebWebSocketNetworkInfoCallback ErrorCallBack;
	/** @brief SocketClosedCallback state. */
	FNativeWebWebSocketNetworkInfoCallback SocketClosedCallback;

	/** @brief OutgoingBuffer state. */
	TArray<TArray<uint8>> OutgoingBuffer;
	/** @brief OutgoingLock state. */
	FCriticalSection OutgoingLock;
	/** @brief Context state. */
	NativeWebWebSocketInternalContext* Context = nullptr;
	/** @brief Wsi state. */
	NativeWebWebSocketInternal* Wsi = nullptr;
	/** @brief RemoteIp state. */
	FString RemoteIp;
	/** @brief RemotePort state. */
	int32 RemotePort = 0;
};

