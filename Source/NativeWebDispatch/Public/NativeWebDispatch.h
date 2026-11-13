/**
 * @file NativeWebDispatch\Public\NativeWebDispatch.h
 * @brief Declares NativeWebDispatch for module NativeWebDispatch\Public\NativeWebDispatch.h.
 * @details Contains dispatch registry and value plumbing used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Dispatch/NativeWebDispatchRegistry.h"

/** @brief the f native web dispatch module type and its operations. */
class FNativeWebDispatchModule : public IModuleInterface
{
public:
	static FNativeWebDispatchModule& Get();

	static bool IsAvailable();

	/**
	 * @brief Creates module-level services and registers module hooks.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * StartupModule();
	 * @endcode
	 */

	virtual void StartupModule() override;
	/**
	 * @brief Stops and releases module-level services.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * ShutdownModule();
	 * @endcode
	 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
	 */

	virtual void ShutdownModule() override;


	/**
	* @brief Returns the module-owned exact-key factory registry.
	* @return A shared result; it is invalid when the resource is unavailable.
	* @note Route keys match exact type and value; registry access is synchronized.
	* @par Example
	* Invocation example; substitute arguments that satisfy the function contract.
	* @code{.cpp}
	* GetDispatchRegistry();
	* @endcode
	*/
	TSharedPtr<class FNativeWebDispatchRegistry> GetDispatchRegistry() const { return DispatchRegistry; }
	/**
	 * @brief Registers deferred factory.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param InRouteKey Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
	 * @param InFactory Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
	 * @param bInAllowReplace Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @note Exact-key factory decoding and typed handler routing.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	RegisterDeferredFactory(InRouteKey, InFactory, bInAllowReplace);
	@endcode
	 */

	static void RegisterDeferredFactory(FNativeWebDispatchRouteKey InRouteKey,
	                                    FNativeWebDispatchRegistry::FNativeWebDispatchFactory InFactory,
	                                    /** @brief Function API. */
	                                    bool bInAllowReplace = false);

private:
	/** @brief Registry state. */
	TSharedPtr<class FNativeWebDispatchRegistry> DispatchRegistry;
};
