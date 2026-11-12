/**
 * @file NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketReadRunnable.h
 * @brief Declares NativeWebWebSocketReadRunnable for module NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketReadRunnable.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "HAL/Runnable.h"

class FNativeWebWebSocketServerInstance;

/** @brief Type declaration. */
class FNativeWebWebSocketReadRunnable : public FRunnable
{
public:
#pragma region Lifecycle
	/** @brief FNativeWebWebSocketReadRunnable API. */
	explicit FNativeWebWebSocketReadRunnable(FNativeWebWebSocketServerInstance* InOwner);
	virtual ~FNativeWebWebSocketReadRunnable() override = default;
#pragma endregion

#pragma region FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;
#pragma endregion

private:
#pragma region InternalState
	/** @brief Owner state. */
	FNativeWebWebSocketServerInstance* Owner = nullptr;
	/** @brief bStopRequested state. */
	TAtomic<bool> bStopRequested = false;
#pragma endregion
};

