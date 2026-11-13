/**
 * @file NativeWebWebSocketServer\Public\Networking\INativeWebWebSocketNetwork.h
 * @brief Declares the low-level WebSocket connection interface used by the server backend.
 */
#pragma once

#include "CoreMinimal.h"
#include "Networking/NativeWebWebSocketNetworkDelegates.h"

/** @brief Represents one active network WebSocket connection. */
class NATIVEWEBWEBSOCKETSERVER_API INativeWebWebSocketNetwork
{
public:
/**
 * @brief Performs the ~i native web web socket network operation.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ~INativeWebWebSocketNetwork();
 * @endcode
 */

	virtual ~INativeWebWebSocketNetwork() = default;

	/** @brief Sets the callback invoked after the connection is established.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetConnectedCallBack(InCallBack);
 * @endcode
 */
	virtual void SetConnectedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) = 0;
	/** @brief Sets the callback invoked when a connection error occurs.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetConnectionErrorCallBack(InCallBack);
 * @endcode
 */
	virtual void SetConnectionErrorCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) = 0;
	/** @brief Sets the callback invoked when a packet is received.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetReceiveCallBack(InCallBack);
 * @endcode
 */
	virtual void SetReceiveCallBack(FNativeWebWebSocketNetworkPacketReceivedCallback InCallBack) = 0;
	/** @brief Sets the callback invoked after the connection closes.
* @param InCallBack Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetSocketClosedCallBack(InCallBack);
 * @endcode
 */
	virtual void SetSocketClosedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) = 0;

	/** @brief Queues data for transmission. @param InData Data buffer. @param InSize Number of bytes in InData. @param bInPrependSize Whether to prepend the payload size. @return true when the data is accepted.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Send(InData, InSize, bInPrependSize);
 * @endcode
 */
	virtual bool Send(const uint8* InData, uint32 InSize, bool bInPrependSize = true) = 0;
	/** @brief Advances the connection's network processing.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Tick();
 * @endcode
 */
	virtual void Tick() = 0;
	/** @brief Flushes pending outgoing data.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Flush();
 * @endcode
 */
	virtual void Flush() = 0;
	/** @brief Closes the connection. @param InStatusCode WebSocket close status code. @param InReason Human-readable close reason.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Close(InStatusCode, InReason);
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	virtual void Close(uint16 InStatusCode = 1000, const FString& InReason = FString()) = 0;

	/** @brief Returns the remote endpoint address. @param bInAppendPort Whether to append the port.
 * @return The resulting text or identifier.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RemoteEndPoint(bInAppendPort);
 * @endcode
 */
	virtual FString RemoteEndPoint(bool bInAppendPort) = 0;
	/** @brief Returns the local endpoint address. @param bInAppendPort Whether to append the port.
 * @return The resulting text or identifier.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * LocalEndPoint(bInAppendPort);
 * @endcode
 */
	virtual FString LocalEndPoint(bool bInAppendPort) = 0;
};

