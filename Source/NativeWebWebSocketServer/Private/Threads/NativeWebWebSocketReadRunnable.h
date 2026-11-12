/**
 * @file NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketReadRunnable.h
 * @brief Declares NativeWebWebSocketReadRunnable for module NativeWebWebSocketServer\Private\Threads\NativeWebWebSocketReadRunnable.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "HAL/Runnable.h"

class FNativeWebWebSocketServerInstance;

/** @brief Reads available frames from the server's listening socket on its worker thread. */
class FNativeWebWebSocketReadRunnable : public FRunnable
{
public:
#pragma region Lifecycle
	/** @brief FNativeWebWebSocketReadRunnable API.
* @param InOwner Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * FNativeWebWebSocketReadRunnable(InOwner);
 * @endcode
 */
	explicit FNativeWebWebSocketReadRunnable(FNativeWebWebSocketServerInstance* InOwner);
/**
 * @brief Performs the ~f native web web socket read runnable operation.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ~FNativeWebWebSocketReadRunnable();
 * @endcode
 */

	virtual ~FNativeWebWebSocketReadRunnable() override = default;
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
	/** @brief bStopRequested state. */
	TAtomic<bool> bStopRequested = false;
#pragma endregion
};

