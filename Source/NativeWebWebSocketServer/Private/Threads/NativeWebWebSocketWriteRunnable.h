/**
 * @file NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketWriteRunnable.h
 * @brief Declares NativeWebWebSocketWriteRunnable for module NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketWriteRunnable.h.
 * @details Contains IPC reader/writer primitives used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "HAL/Runnable.h"

class FNativeWebWebSocketServerInstance;
class FEvent;

/** @brief Type declaration. */
class FNativeWebWebSocketWriteRunnable : public FRunnable
{
public:
#pragma region Lifecycle
	/** @brief FNativeWebWebSocketWriteRunnable API. */
	FNativeWebWebSocketWriteRunnable(FNativeWebWebSocketServerInstance* InOwner, FEvent* InWakeEvent);
	virtual ~FNativeWebWebSocketWriteRunnable() override = default;
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

