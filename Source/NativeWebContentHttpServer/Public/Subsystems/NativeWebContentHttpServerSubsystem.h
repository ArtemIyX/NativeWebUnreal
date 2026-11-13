/**
 * @file NativeWebContentHttpServer/Public/Subsystems/NativeWebContentHttpServerSubsystem.h
 * @brief Game instance subsystem API for content HTTP server lifecycle.
 */
#pragma once

#include "CoreMinimal.h"
#include "Handlers/NativeWebContentHttpImageRequestHandler.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NativeWebContentHttpServerSubsystem.generated.h"

class UNativeWebContentHttpImageRequestHandler;
class UTexture2D;

/** @brief Subsystem facade for HTTP image serving and cache access. */
UCLASS(BlueprintType)
class NATIVEWEBCONTENTHTTPSERVER_API UNativeWebContentHttpServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UNativeWebContentHttpServerSubsystem();

public:
	/** @brief Starts the configured HTTP image service when this game instance initializes.
 * @param InCollection Engine subsystem collection used during initialization.
 * @details Initialization creates the configured handler object and starts the listener with HttpPort.
 * @note Game-thread lifecycle callback. @ref Deinitialize stops the service during shutdown.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Initialize(InCollection);
 * @endcode
 */
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	/** @brief Stops the HTTP service and releases its runtime handler references.
	 * @details The image cache remains available unless explicitly cleared.
	 * @note Game-thread lifecycle callback; the subsystem lifetime is bounded by its game instance.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Deinitialize();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	virtual void Deinitialize() override;

public:

 /**
 * @brief Retrieves an image through the configured content image handler and cache.
 * @param InPackagePath Content package path understood by the image service.
 * @param bOutSuccess Set true when a cached or newly encoded texture is returned.
 * @return The requested texture, or nullptr when the path cannot be served.
 * @note Call from the game thread; returned UTexture2D objects are UObject-managed.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetImageByPackagePath(InPackagePath, bOutSuccess);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	UTexture2D* GetImageByPackagePath(const FString& InPackagePath, bool& bOutSuccess);

	/** @brief Evicts all cached image entries owned by this game-instance service.
	 * @details Subsequent requests may decode and encode those images again.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ClearCachedImages();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	void ClearCachedImages();

	/** @brief Returns the number of entries currently held by the image cache.
	 * @return Number of cached image entries.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetCachedImageCount();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebContentHttpServer")
	int32 GetCachedImageCount() const;


 /**
 * @brief Starts the HTTP listener, optionally overriding the configured port.
 * @param InPortOverride Port to bind, or zero to use the configured HttpPort.
 * @return True if the listener starts or is already running; false if binding/configuration fails.
 * @note Game-thread only. Port conflicts are reported through module logs.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StartServer(InPortOverride);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	bool StartServer(int32 InPortOverride = 0);

	/** @brief Stops the listener if it is running.
	 * @details Handler configuration and image cache remain available, so the server can be restarted.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StopServer();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	void StopServer();

	/** @brief Reports whether the HTTP listener is currently active.
	 * @return True while the listener is running; otherwise false.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * IsServerRunning();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebContentHttpServer")
	bool IsServerRunning() const;


 /**
 * @brief Selects the UObject handler class used to answer image requests.
 * @param InRequestHandlerClass Concrete handler subclass; null restores the module default.
 * @param bInRestartIfRunning If true, restart the active listener so it uses the new handler.
 * @details The subsystem creates and owns the handler instance.
 * @note Game-thread only.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetRequestHandlerClass(InRequestHandlerClass, bInRestartIfRunning);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	void SetRequestHandlerClass(TSubclassOf<UNativeWebContentHttpImageRequestHandler> InRequestHandlerClass, bool bInRestartIfRunning = true);

private:
	/** @brief Creates the configured request-handler UObject with this subsystem as its outer.
	 * @return True if a valid handler instance is available; otherwise false.
	 * @details Private lifecycle helper; public callers configure handlers through @ref SetRequestHandlerClass.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * CreateRequestHandlerInstance();
 * @endcode
 */
	bool CreateRequestHandlerInstance();

private:
	UPROPERTY(EditAnywhere, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `HttpPort`. */
	int32 HttpPort = 18080;

	UPROPERTY(EditAnywhere, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `RequestHandlerClass`. */
	TSubclassOf<UNativeWebContentHttpImageRequestHandler> RequestHandlerClass;

	UPROPERTY(Transient)
	/** @brief Reflected data field `RequestHandlerInstance`. */
	TObjectPtr<UNativeWebContentHttpImageRequestHandler> RequestHandlerInstance;
};
