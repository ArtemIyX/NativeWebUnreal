/**
 * @file NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketHandleRunnable.h
 * @brief Declares NativeWebWebSocketHandleRunnable for module NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketHandleRunnable.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "HAL/Runnable.h"

class FNativeWebWebSocketServerInstance;
class FEvent;

/** @brief Type declaration. */
class FNativeWebWebSocketHandleRunnable : public FRunnable
{
public:
#pragma region Lifecycle
	/** @brief FNativeWebWebSocketHandleRunnable API. */
	FNativeWebWebSocketHandleRunnable(FNativeWebWebSocketServerInstance* InOwner, FEvent* InWakeEvent);
	virtual ~FNativeWebWebSocketHandleRunnable() override = default;
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

