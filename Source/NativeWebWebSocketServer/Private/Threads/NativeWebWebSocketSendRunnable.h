/**
 * @file NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketSendRunnable.h
 * @brief Declares NativeWebWebSocketSendRunnable for module NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketSendRunnable.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "HAL/Runnable.h"

/**
 * @brief Declares the f native web web socket server instance type and its operations.
 * @ingroup NativeWebWebSocketServer
 */
class FNativeWebWebSocketServerInstance;
class FEvent;

/** @brief Type declaration. */
class FNativeWebWebSocketSendRunnable : public FRunnable
{
public:
#pragma region Lifecycle
	/** @brief FNativeWebWebSocketSendRunnable API. */
	FNativeWebWebSocketSendRunnable(FNativeWebWebSocketServerInstance* InOwner, FEvent* InWakeEvent);
/**
 * @brief Performs the ~f native web web socket send runnable operation.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ~FNativeWebWebSocketSendRunnable();
 * @endcode
 */

	virtual ~FNativeWebWebSocketSendRunnable() override = default;
#pragma endregion

#pragma region FRunnable
/**
 * @brief Performs the run operation.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Run();
 * @endcode
 */

	virtual uint32 Run() override;
/**
 * @brief Performs the stop operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Stop();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */

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

