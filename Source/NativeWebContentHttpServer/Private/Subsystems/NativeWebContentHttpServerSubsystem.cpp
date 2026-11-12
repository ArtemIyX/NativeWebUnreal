/**
 * @file NativeWebContentHttpServer/Private/Subsystems/NativeWebContentHttpServerSubsystem.cpp
 * @brief Subsystem lifecycle and module-service forwarding.
 */
#include "Subsystems/NativeWebContentHttpServerSubsystem.h"

#include "NativeWebContentHttpServer.h"
#include "Handlers/NativeWebContentDefaultImageRequestHandler.h"
#include "Services/NativeWebContentImageCacheService.h"
#include "Services/NativeWebContentImageEncodeService.h"
#include "Services/NativeWebContentHttpServerRuntimeService.h"
#include "Stats/NativeWebContentHttpServerStats.h"

UNativeWebContentHttpServerSubsystem::UNativeWebContentHttpServerSubsystem()
{
}

void UNativeWebContentHttpServerSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);
	if (!RequestHandlerClass)
	{
		RequestHandlerClass = UNativeWebContentDefaultImageRequestHandler::StaticClass();
	}
	StartServer();
}

void UNativeWebContentHttpServerSubsystem::Deinitialize()
{
	UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("NativeWebContentHttpServer subsystem deinitialize"));
	StopServer();
	RequestHandlerInstance = nullptr;
	Super::Deinitialize();
}

UTexture2D* UNativeWebContentHttpServerSubsystem::GetImageByPackagePath(const FString& InPackagePath, bool& bOutSuccess)
{
	bOutSuccess = false;
	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("GetImageByPackagePath failed: module is not available"));
		return nullptr;
	}

	FNativeWebContentImageCacheService* const imageCacher = FNativeWebContentHttpServerModule::Get().GetImageCacher();
	if (!imageCacher)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("GetImageByPackagePath failed: image cacher is not available"));
		return nullptr;
	}

	UTexture2D* image = nullptr;
	bOutSuccess = imageCacher->GetImageByPackagePath(InPackagePath, image);
	if (!bOutSuccess)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("GetImageByPackagePath failed for '%s'"), *InPackagePath);
	}
	return image;
}

void UNativeWebContentHttpServerSubsystem::ClearCachedImages()
{
	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		UE_LOG(LogNativeWebContentHttpServer, Warning, TEXT("ClearCachedImages skipped: module is not available"));
		SET_DWORD_STAT(STAT_NativeWebContentHttpServer_CachedImages, 0);
		return;
	}

	FNativeWebContentImageCacheService* const imageCacher = FNativeWebContentHttpServerModule::Get().GetImageCacher();
	if (!imageCacher)
	{
		UE_LOG(LogNativeWebContentHttpServer, Warning, TEXT("ClearCachedImages skipped: image cacher is not available"));
		SET_DWORD_STAT(STAT_NativeWebContentHttpServer_CachedImages, 0);
		return;
	}

	imageCacher->ClearCache();

	FNativeWebContentImageEncodeService* const imageEncoder = FNativeWebContentHttpServerModule::Get().GetImageEncoder();
	if (imageEncoder)
	{
		imageEncoder->ClearEncodedCache();
	}
}

int32 UNativeWebContentHttpServerSubsystem::GetCachedImageCount() const
{
	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		return 0;
	}

	FNativeWebContentImageCacheService* const imageCacher = FNativeWebContentHttpServerModule::Get().GetImageCacher();
	if (!imageCacher)
	{
		return 0;
	}

	return imageCacher->GetCachedImageCount();
}

bool UNativeWebContentHttpServerSubsystem::StartServer(int32 InPortOverride)
{
	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("StartServer failed: module is not available"));
		return false;
	}

	if (!CreateRequestHandlerInstance())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("StartServer failed: request handler creation failed"));
		return false;
	}

	FNativeWebContentHttpServerRuntimeService* const httpServerService = FNativeWebContentHttpServerModule::Get().GetHttpServerService();
	if (!httpServerService)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("StartServer failed: http server service is null"));
		return false;
	}

	const int32 listenPort = InPortOverride > 0 ? InPortOverride : HttpPort;
	HttpPort = listenPort;
	return httpServerService->Start(listenPort, RequestHandlerInstance);
}

void UNativeWebContentHttpServerSubsystem::StopServer()
{
	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		return;
	}

	FNativeWebContentHttpServerRuntimeService* const httpServerService = FNativeWebContentHttpServerModule::Get().GetHttpServerService();
	if (httpServerService)
	{
		httpServerService->Stop();
	}
}

bool UNativeWebContentHttpServerSubsystem::IsServerRunning() const
{
	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		return false;
	}

	FNativeWebContentHttpServerRuntimeService* const httpServerService = FNativeWebContentHttpServerModule::Get().GetHttpServerService();
	return httpServerService && httpServerService->IsRunning();
}

void UNativeWebContentHttpServerSubsystem::SetRequestHandlerClass(TSubclassOf<UNativeWebContentHttpImageRequestHandler> InRequestHandlerClass, bool bInRestartIfRunning)
{
	RequestHandlerClass = InRequestHandlerClass;
	const bool bWasRunning = IsServerRunning();
	if (!CreateRequestHandlerInstance())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("SetRequestHandlerClass failed: could not instantiate request handler"));
		return;
	}

	if (bInRestartIfRunning && bWasRunning)
	{
		StopServer();
		StartServer();
	}
}

bool UNativeWebContentHttpServerSubsystem::CreateRequestHandlerInstance()
{
	if (!RequestHandlerClass)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("CreateRequestHandlerInstance failed: RequestHandlerClass is null"));
		return false;
	}

	UNativeWebContentHttpImageRequestHandler* const newHandler = NewObject<UNativeWebContentHttpImageRequestHandler>(this, RequestHandlerClass);
	if (!newHandler)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("CreateRequestHandlerInstance failed: NewObject returned null for class %s"), *RequestHandlerClass->GetName());
		return false;
	}

	RequestHandlerInstance = newHandler;
	UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Request handler instantiated: %s"), *RequestHandlerClass->GetName());
	return true;
}
