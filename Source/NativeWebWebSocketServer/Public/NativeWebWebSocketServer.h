/**
 * @file NativeWebWebSocketServer\Public\NativeWebWebSocketServer.h
 * @brief Declares NativeWebWebSocketServer for module NativeWebWebSocketServer\Public\NativeWebWebSocketServer.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** @brief Type declaration. */
class FNativeWebWebSocketServerModule : public IModuleInterface
{
public:
#pragma region Lifecycle
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
#pragma endregion

private:
#pragma region InternalState
	/** @brief DebugCommands state. */
	TUniquePtr<class FNativeWebWebSocketDebugCommands> DebugCommands;
#pragma endregion
};

