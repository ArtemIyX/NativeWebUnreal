/**
 * @file NativeWebWebSocketServer\Public\Networking\INativeWebWebSocketServerBackend.h
 * @brief Declares the backend interface used to host WebSocket connections.
 */
#pragma once

#include "CoreMinimal.h"
#include "Networking/NativeWebWebSocketNetworkDelegates.h"

/** @brief Platform-specific WebSocket server backend. */
class NATIVEWEBWEBSOCKETSERVER_API INativeWebWebSocketServerBackend
{
public:
	/**
	 * @brief Performs the ~i native web web socket server backend operation.
	 * @return The value produced by this operation.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * ~INativeWebWebSocketServerBackend();
	 * @endcode
	 */
	virtual ~INativeWebWebSocketServerBackend() = default;

	/** @brief Initializes the backend and starts listening on a port. @param InPort Requested listening port. @param InOnConnected Callback for new clients. @param InOnDisconnected Callback for disconnected clients. @return true when initialization succeeds.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Init(InPort, InOnConnected, InOnDisconnected);
	 * @endcode
	 */
	virtual bool Init(uint32 InPort, FNativeWebWebSocketNetworkClientConnectedCallback InOnConnected,
	                  FNativeWebWebSocketNetworkClientDisconnectedCallback InOnDisconnected) = 0;
	/** @brief Advances backend network processing.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Tick();
	 * @endcode
	 */
	virtual void Tick() = 0;
	/** @brief Returns backend diagnostic information.
	 * @return The resulting text or identifier.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Info();
	 * @endcode
	 */
	virtual FString Info() const = 0;
		/** @brief Returns the actual listening port.
	 * @return The resulting count, size, index, hash, or numeric value.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetServerPort();
	 * @endcode
	 */
	virtual uint32 GetServerPort() const = 0;
};
