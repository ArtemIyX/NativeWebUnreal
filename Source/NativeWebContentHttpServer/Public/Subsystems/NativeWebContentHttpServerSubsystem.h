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
	/** @brief Starts HTTP server automatically with default settings. */
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	/** @brief Stops HTTP server and clears runtime references. */
	virtual void Deinitialize() override;

public:
	/**
	 * @brief Gets image from module cache service by asset path.
	 * @param InPackagePath Asset path.
	 * @param bOutSuccess True when image exists.
	 * @return Texture or null.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	UTexture2D* GetImageByPackagePath(const FString& InPackagePath, bool& bOutSuccess);

	/** @brief Clears module image cache. */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	void ClearCachedImages();

	/** @brief Returns current module cache size. */
	UFUNCTION(BlueprintPure, Category = "NativeWebContentHttpServer")
	int32 GetCachedImageCount() const;

	/**
	 * @brief Starts HTTP server on configured port or override.
	 * @param InPortOverride Optional port override. Use 0 to keep configured port.
	 * @return True when server is running.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	bool StartServer(int32 InPortOverride = 0);

	/** @brief Stops HTTP server if running. */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	void StopServer();

	/** @brief Returns true if HTTP server is running. */
	UFUNCTION(BlueprintPure, Category = "NativeWebContentHttpServer")
	bool IsServerRunning() const;

	/**
	 * @brief Sets request handler class used for /img route handling.
	 * @param InRequestHandlerClass Handler UObject class.
	 * @param bInRestartIfRunning Restart server when handler changes and server is running.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer")
	void SetRequestHandlerClass(TSubclassOf<UNativeWebContentHttpImageRequestHandler> InRequestHandlerClass, bool bInRestartIfRunning = true);

private:
	/** @brief Creates handler instance from configured class. */
	bool CreateRequestHandlerInstance();

private:
	UPROPERTY(EditAnywhere, Category = "NativeWebContentHttpServer")
	int32 HttpPort = 18080;

	UPROPERTY(EditAnywhere, Category = "NativeWebContentHttpServer")
	TSubclassOf<UNativeWebContentHttpImageRequestHandler> RequestHandlerClass;

	UPROPERTY(Transient)
	TObjectPtr<UNativeWebContentHttpImageRequestHandler> RequestHandlerInstance;
};
