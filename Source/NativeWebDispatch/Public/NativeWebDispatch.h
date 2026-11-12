/**
 * @file NativeWebDispatch\Public\NativeWebDispatch.h
 * @brief Declares NativeWebDispatch for module NativeWebDispatch\Public\NativeWebDispatch.h.
 * @details Contains dispatch registry and value plumbing used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Dispatch/NativeWebDispatchRegistry.h"

/** @brief Type declaration. */
class FNativeWebDispatchModule : public IModuleInterface
{
public:
	/** @brief Get API. */
	static FNativeWebDispatchModule& Get();
	/** @brief IsAvailable API. */
	static bool IsAvailable();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<class FNativeWebDispatchRegistry> GetDispatchRegistry() const { return DispatchRegistry; }
	static void RegisterDeferredFactory(FNativeWebDispatchRouteKey InRouteKey, FNativeWebDispatchRegistry::FNativeWebDispatchFactory InFactory,
	                                    /** @brief Function API. */
	                                    bool bInAllowReplace = false);

private:
	/** @brief Registry state. */
	TSharedPtr<class FNativeWebDispatchRegistry> DispatchRegistry;
};
