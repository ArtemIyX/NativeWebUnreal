/**
 * @file NativeWebContentHttpServer/Public/NativeWebContentHttpServer.h
 * @brief Module entry and shared service accessors.
 */
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** @brief Log category for NativeWebContentHttpServer.
* @param LogNativeWebContentHttpServer Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Log Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param All Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebContentHttpServer, Log, All);
 * @endcode
 */
NATIVEWEBCONTENTHTTPSERVER_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebContentHttpServer, Log, All);

/** @brief Module root for content HTTP services. */
class FNativeWebContentHttpServerModule : public IModuleInterface
{
public:
	/** @brief Loads and returns module singleton.
 * @return The value produced by this operation.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance content HTTP and image request services.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
Get();
@endcode
 */
	static FNativeWebContentHttpServerModule& Get();
	/** @brief Returns true if module is currently loaded.
 * @return True when the is available condition is satisfied.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance content HTTP and image request services.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
IsAvailable();
@endcode
 */
	static bool IsAvailable();

	/** @brief Creates module-owned services.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StartupModule();
 * @endcode
 */
	virtual void StartupModule() override;
	/** @brief Stops and releases module-owned services.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ShutdownModule();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	virtual void ShutdownModule() override;

	/** @brief Returns module image cache service, may be null during shutdown.
 * @return The requested object/result, or null when unavailable.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetImageCacher();
 * @endcode
 */
	class FNativeWebContentImageCacheService* GetImageCacher() const;
	/** @brief Returns module image encoder service, may be null during shutdown.
 * @return The requested object/result, or null when unavailable.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetImageEncoder();
 * @endcode
 */
	class FNativeWebContentImageEncodeService* GetImageEncoder() const;
	/** @brief Returns module HTTP runtime service, may be null during shutdown.
 * @return The requested object/result, or null when unavailable.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetHttpServerService();
 * @endcode
 */
	class FNativeWebContentHttpServerRuntimeService* GetHttpServerService() const;

private:
	TSharedPtr<class FNativeWebContentImageCacheService> ImageCacher;
	TSharedPtr<class FNativeWebContentImageEncodeService> ImageEncoder;
	TSharedPtr<class FNativeWebContentHttpServerRuntimeService> HttpServerService;
};
