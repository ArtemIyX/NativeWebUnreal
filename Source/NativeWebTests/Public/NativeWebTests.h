/**
 * @file Source/NativeWebTests/Public/NativeWebTests.h
 * @brief Declares native web tests for the NativeWebTests module.
 * @details Uncooked automation tests for the NativeWeb plugin.
 * @ingroup NativeWebTests
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * @brief Declares the f native web tests module type and its operations.
 * @ingroup NativeWebTests
 */
class FNativeWebTestsModule : public IModuleInterface
{
public:
/**
 * @brief Creates module-level services and registers module hooks.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StartupModule();
 * @endcode
 */

    virtual void StartupModule() override;
/**
 * @brief Stops and releases module-level services.
 * @note Automation entry points run through Unreal Automation in uncooked sessions.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ShutdownModule();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */

    virtual void ShutdownModule() override;
};
