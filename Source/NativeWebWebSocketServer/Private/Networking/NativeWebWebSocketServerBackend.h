/**
 * @file NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketServerBackend.h
 * @brief Declares NativeWebWebSocketServerBackend for module NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketServerBackend.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Networking/INativeWebWebSocketServerBackend.h"
#include "Networking/NativeWebWebSocketPrivate.h"

/** @brief the f native web web socket server backend type and its operations. */
class FNativeWebWebSocketServerBackend : public INativeWebWebSocketServerBackend
{
public:
	FNativeWebWebSocketServerBackend() = default;
/**
 * @brief Performs the ~f native web web socket server backend operation.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ~FNativeWebWebSocketServerBackend();
 * @endcode
 */

	virtual ~FNativeWebWebSocketServerBackend() override;

/**
 * @brief Performs the init operation.
* @param InPort Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InOnConnected Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InOnDisconnected Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Init(InPort, InOnConnected, InOnDisconnected);
 * @endcode
 */

	virtual bool Init(uint32 InPort, FNativeWebWebSocketNetworkClientConnectedCallback InOnConnected, FNativeWebWebSocketNetworkClientDisconnectedCallback InOnDisconnected) override;
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
 * @brief Performs the info operation.
 * @return The resulting text or identifier.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Info();
 * @endcode
 */

	virtual FString Info() const override;
/**
 * @brief Returns server port.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetServerPort();
 * @endcode
 */

	virtual uint32 GetServerPort() const override { return ServerPort; }

	/** @brief ConnectedCallback state. */
	FNativeWebWebSocketNetworkClientConnectedCallback ConnectedCallback;
	/** @brief DisconnectedCallback state. */
	FNativeWebWebSocketNetworkClientDisconnectedCallback DisconnectedCallback;
	/** @brief Context state. */
	NativeWebWebSocketInternalContext* Context = nullptr;
	/** @brief Protocols state. */
	NativeWebWebSocketInternalProtocol* Protocols = nullptr;

private:
	/** @brief ServerPort state. */
	uint32 ServerPort = 0;
};

