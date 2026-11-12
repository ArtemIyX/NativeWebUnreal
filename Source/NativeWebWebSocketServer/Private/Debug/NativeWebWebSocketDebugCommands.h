/**
 * @file NativeWebWebSocketServer\Private\Debug\NativeWebWebSocketDebugCommands.h
 * @brief Declares NativeWebWebSocketDebugCommands for module NativeWebWebSocketServer\Private\Debug\NativeWebWebSocketDebugCommands.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"

class IConsoleObject;

/** @brief Type declaration. */
class FNativeWebWebSocketDebugCommands
{
public:
#pragma region Lifecycle
	FNativeWebWebSocketDebugCommands() = default;
	~FNativeWebWebSocketDebugCommands() = default;

	/** @brief Startup API. */
	void Startup();
	/** @brief Shutdown API. */
	void Shutdown();
#pragma endregion

private:
#pragma region Commands
	/** @brief HandleList API. */
	void HandleList(const TArray<FString>& InArgs) const;
	/** @brief HandleStop API. */
	void HandleStop(const TArray<FString>& InArgs) const;
	/** @brief HandleKick API. */
	void HandleKick(const TArray<FString>& InArgs) const;
	/** @brief HandleStats API. */
	void HandleStats(const TArray<FString>& InArgs) const;
	/** @brief HandleClients API. */
	void HandleClients(const TArray<FString>& InArgs) const;
	/** @brief HandleCVars API. */
	void HandleCVars(const TArray<FString>& InArgs) const;
	/** @brief HandleBenchSend API. */
	void HandleBenchSend(const TArray<FString>& InArgs) const;
#pragma endregion

private:
#pragma region Helpers
	/** @brief ResolveSubsystem API. */
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

