/**
 * @file NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketSendRunnable.h
 * @brief Declares NativeWebWebSocketSendRunnable for module NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketSendRunnable.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "HAL/Runnable.h"

class FNativeWebWebSocketServerInstance;
class FEvent;

/** @brief Type declaration. */
class FNativeWebWebSocketSendRunnable : public FRunnable
{
public:
#pragma region Lifecycle
	/** @brief FNativeWebWebSocketSendRunnable API. */
	FNativeWebWebSocketSendRunnable(FNativeWebWebSocketServerInstance* InOwner, FEvent* InWakeEvent);
	virtual ~FNativeWebWebSocketSendRunnable() override = default;
#pragma endregion

#pragma region FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;
#pragma endregion

private:
#pragma region InternalState
	/** @brief Owner state. */
	FNativeWebWebSocketServerInstance* Owner = nullptr;
	/** @brief WakeEvent state. */
	FEvent* WakeEvent = nullptr;
	/** @brief bStopRequested state. */
	TAtomic<bool> bStopRequested = false;
#pragma endregion
};

