/**
 * @file NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketServerBackend.h
 * @brief Declares NativeWebWebSocketServerBackend for module NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketServerBackend.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Networking/INativeWebWebSocketServerBackend.h"
#include "Networking/NativeWebWebSocketPrivate.h"

/** @brief Type declaration. */
class FNativeWebWebSocketServerBackend : public INativeWebWebSocketServerBackend
{
public:
	FNativeWebWebSocketServerBackend() = default;
	virtual ~FNativeWebWebSocketServerBackend() override;

	virtual bool Init(uint32 InPort, FNativeWebWebSocketNetworkClientConnectedCallback InOnConnected, FNativeWebWebSocketNetworkClientDisconnectedCallback InOnDisconnected) override;
	virtual void Tick() override;
	virtual FString Info() const override;
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

