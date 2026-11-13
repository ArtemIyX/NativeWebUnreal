
#include "NativeWebContentHttpServer.h"

#include "Services/NativeWebContentImageCacheService.h"
#include "Services/NativeWebContentImageEncodeService.h"
#include "Services/NativeWebContentHttpServerRuntimeService.h"
#include "Stats/NativeWebContentHttpServerStats.h"

#define LOCTEXT_NAMESPACE "FNativeWebContentHttpServerModule"

DEFINE_LOG_CATEGORY(LogNativeWebContentHttpServer);

FNativeWebContentHttpServerModule& FNativeWebContentHttpServerModule::Get()
{
	return FModuleManager::LoadModuleChecked<FNativeWebContentHttpServerModule>(TEXT("NativeWebContentHttpServer"));
}

bool FNativeWebContentHttpServerModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("NativeWebContentHttpServer"));
}

void FNativeWebContentHttpServerModule::StartupModule()
{
	UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("NativeWebContentHttpServer module startup"));
	ImageCacher = MakeShared<FNativeWebContentImageCacheService>();
	ImageEncoder = MakeShared<FNativeWebContentImageEncodeService>();
	HttpServerService = MakeShared<FNativeWebContentHttpServerRuntimeService>();
	SET_DWORD_STAT(STAT_NativeWebContentHttpServer_CachedImages, 0);
	UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Module services created"));
}

void FNativeWebContentHttpServerModule::ShutdownModule()
{
	UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("NativeWebContentHttpServer module shutdown"));
	if (HttpServerService.IsValid())
	{
		HttpServerService->Stop();
		HttpServerService.Reset();
		UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Http server service destroyed"));
	}
	if (ImageCacher.IsValid())
	{
		ImageCacher->ClearCache();
		ImageCacher.Reset();
		UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Image cache service destroyed"));
	}
	if (ImageEncoder.IsValid())
	{
		ImageEncoder->ClearEncodedCache();
		ImageEncoder.Reset();
		UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Image encode service destroyed"));
	}
	SET_DWORD_STAT(STAT_NativeWebContentHttpServer_CachedImages, 0);
}

FNativeWebContentImageCacheService* FNativeWebContentHttpServerModule::GetImageCacher() const
{
	return ImageCacher.Get();
}

FNativeWebContentImageEncodeService* FNativeWebContentHttpServerModule::GetImageEncoder() const
{
	return ImageEncoder.Get();
}

FNativeWebContentHttpServerRuntimeService* FNativeWebContentHttpServerModule::GetHttpServerService() const
{
	return HttpServerService.Get();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNativeWebContentHttpServerModule, NativeWebContentHttpServer)
