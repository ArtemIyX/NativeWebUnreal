/**
 * @file NativeWebWebSocketServer\Private\Debug\NativeWebWebSocketDebugCommands.h
 * @brief Declares NativeWebWebSocketDebugCommands for module NativeWebWebSocketServer\Private\Debug\NativeWebWebSocketDebugCommands.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"

class IConsoleObject;

/** @brief Registers and releases the module's WebSocket debug console commands. */
class FNativeWebWebSocketDebugCommands
{
public:
#pragma region Lifecycle
	FNativeWebWebSocketDebugCommands() = default;
	~FNativeWebWebSocketDebugCommands() = default;

	/** @brief Startup API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Startup();
 * @endcode
 */
	void Startup();
	/** @brief Shutdown API.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Shutdown();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	void Shutdown();
#pragma endregion

private:
#pragma region Commands
	/** @brief HandleList API.
* @param InArgs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleList(InArgs);
 * @endcode
 */
	void HandleList(const TArray<FString>& InArgs) const;
	/** @brief HandleStop API.
* @param InArgs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleStop(InArgs);
 * @endcode
 */
	void HandleStop(const TArray<FString>& InArgs) const;
	/** @brief HandleKick API.
* @param InArgs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleKick(InArgs);
 * @endcode
 */
	void HandleKick(const TArray<FString>& InArgs) const;
	/** @brief HandleStats API.
* @param InArgs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleStats(InArgs);
 * @endcode
 */
	void HandleStats(const TArray<FString>& InArgs) const;
	/** @brief HandleClients API.
* @param InArgs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleClients(InArgs);
 * @endcode
 */
	void HandleClients(const TArray<FString>& InArgs) const;
	/** @brief HandleCVars API.
* @param InArgs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleCVars(InArgs);
 * @endcode
 */
	void HandleCVars(const TArray<FString>& InArgs) const;
	/** @brief HandleBenchSend API.
* @param InArgs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleBenchSend(InArgs);
 * @endcode
 */
	void HandleBenchSend(const TArray<FString>& InArgs) const;
#pragma endregion

private:
#pragma region Helpers
	/** @brief ResolveSubsystem API.
 * @return The requested object/result, or null when unavailable.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ResolveSubsystem();
 * @endcode
 */
	class UNativeWebWebSocketSubsystem* ResolveSubsystem() const;
#pragma endregion

private:
#pragma region InternalState
	/** @brief CommandList state. */
	IConsoleObject* CommandList = nullptr;
	/** @brief CommandStop state. */
	IConsoleObject* CommandStop = nullptr;
	/** @brief CommandKick state. */
	IConsoleObject* CommandKick = nullptr;
	/** @brief CommandStats state. */
	IConsoleObject* CommandStats = nullptr;
	/** @brief CommandClients state. */
	IConsoleObject* CommandClients = nullptr;
	/** @brief CommandCVars state. */
	IConsoleObject* CommandCVars = nullptr;
	/** @brief CommandBenchSend state. */
	IConsoleObject* CommandBenchSend = nullptr;
#pragma endregion
};

