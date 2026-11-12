/**
 * @file NativeWebContentHttpServer/Public/NativeWebContentHttpServer.h
 * @brief Module entry and shared service accessors.
 */
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** @brief Log category for NativeWebContentHttpServer. */
NATIVEWEBCONTENTHTTPSERVER_API DECLARE_LOG_CATEGORY_EXTERN(LogNativeWebContentHttpServer, Log, All);

/** @brief Module root for content HTTP services. */
class FNativeWebContentHttpServerModule : public IModuleInterface
{
public:
	/** @brief Loads and returns module singleton. */
	static FNativeWebContentHttpServerModule& Get();
	/** @brief Returns true if module is currently loaded. */
	static bool IsAvailable();

	/** @brief Creates module-owned services. */
	virtual void StartupModule() override;
	/** @brief Stops and releases module-owned services. */
	virtual void ShutdownModule() override;

	/** @brief Returns module image cache service, may be null during shutdown. */
	class FNativeWebContentImageCacheService* GetImageCacher() const;
	/** @brief Returns module image encoder service, may be null during shutdown. */
	class FNativeWebContentImageEncodeService* GetImageEncoder() const;
	/** @brief Returns module HTTP runtime service, may be null during shutdown. */
	class FNativeWebContentHttpServerRuntimeService* GetHttpServerService() const;

private:
	TSharedPtr<class FNativeWebContentImageCacheService> ImageCacher;
	TSharedPtr<class FNativeWebContentImageEncodeService> ImageEncoder;
	TSharedPtr<class FNativeWebContentHttpServerRuntimeService> HttpServerService;
};
