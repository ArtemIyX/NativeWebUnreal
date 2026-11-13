/**
 * @file NativeWebWebSocketServer\Public\NativeWebWebSocketServer.h
 * @brief Declares NativeWebWebSocketServer for module NativeWebWebSocketServer\Public\NativeWebWebSocketServer.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** @brief the f native web web socket server module type and its operations. */
class FNativeWebWebSocketServerModule : public IModuleInterface
{
public:
#pragma region Lifecycle
/**
 * @brief Creates module-level services and registers module hooks.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StartupModule();
 * @endcode
 */

	virtual void StartupModule() override;
/**
 * @brief Stops and releases module-level services.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ShutdownModule();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */

	virtual void ShutdownModule() override;
#pragma endregion

private:
#pragma region InternalState
	/** @brief DebugCommands state. */
	TUniquePtr<class FNativeWebWebSocketDebugCommands> DebugCommands;
#pragma endregion
};

