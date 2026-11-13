
#include "Handlers/NativeWebContentDefaultImageRequestHandler.h"

#include "NativeWebContentHttpServer.h"
#include "Services/NativeWebContentImageCacheService.h"
#include "Services/NativeWebContentImageEncodeService.h"

bool UNativeWebContentDefaultImageRequestHandler::HandleImageRequest_Implementation(const FNativeWebContentHttpImageRequestContext& InRequestContext, FNativeWebContentHttpImageResponse& OutResponse, FString& OutError)
{
	if (InRequestContext.AssetPath.IsEmpty())
	{
		OutResponse.StatusCode = 400;
		OutResponse.ContentType = TEXT("application/json");
		OutError = TEXT("Missing required 'asset' parameter");
		return false;
	}

	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		OutResponse.StatusCode = 500;
		OutResponse.ContentType = TEXT("application/json");
		OutError = TEXT("NativeWebContentHttpServer module is not available");
		return false;
	}

	FNativeWebContentImageCacheService* const imageCacher = FNativeWebContentHttpServerModule::Get().GetImageCacher();
	FNativeWebContentImageEncodeService* const imageEncoder = FNativeWebContentHttpServerModule::Get().GetImageEncoder();
	if (!imageCacher || !imageEncoder)
	{
		OutResponse.StatusCode = 500;
		OutResponse.ContentType = TEXT("application/json");
		OutError = TEXT("Module services are not initialized");
		return false;
	}

	UTexture2D* image = nullptr;
	if (!imageCacher->GetImageByPackagePath(InRequestContext.AssetPath, image) || !image)
	{
		OutResponse.StatusCode = 404;
		OutResponse.ContentType = TEXT("application/json");
		OutError = FString::Printf(TEXT("Image asset not found: '%s'"), *InRequestContext.AssetPath);
		return false;
	}

	TArray<uint8> pngBytes;
	if (!imageEncoder->EncodeTextureToPngBytes(image, pngBytes, OutError))
	{
		OutResponse.StatusCode = 500;
		OutResponse.ContentType = TEXT("application/json");
		return false;
	}

	OutResponse.StatusCode = 200;
	OutResponse.ContentType = TEXT("image/png");
	OutResponse.Body = MoveTemp(pngBytes);
	return true;
}

void UNativeWebContentDefaultImageRequestHandler::HandleImageRequestAsync(const FNativeWebContentHttpImageRequestContext& InRequestContext, FOnImageRequestCompleted InOnCompleted)
{
	UE_LOG(LogNativeWebContentHttpServer, Verbose, TEXT("Default handler start. asset='%s'"), *InRequestContext.AssetPath);
	if (InRequestContext.AssetPath.IsEmpty())
	{
		FNativeWebContentHttpImageResponse response;
		response.StatusCode = 400;
		response.ContentType = TEXT("application/json");
		UE_LOG(LogNativeWebContentHttpServer, Warning, TEXT("Default handler reject: asset is empty"));
		InOnCompleted(false, response, TEXT("Missing required 'asset' parameter"));
		return;
	}

	if (!FNativeWebContentHttpServerModule::IsAvailable())
	{
		FNativeWebContentHttpImageResponse response;
		response.StatusCode = 500;
		response.ContentType = TEXT("application/json");
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Default handler failed: module is not available"));
		InOnCompleted(false, response, TEXT("NativeWebContentHttpServer module is not available"));
		return;
	}

	FNativeWebContentImageEncodeService* const imageEncoder = FNativeWebContentHttpServerModule::Get().GetImageEncoder();
	if (!imageEncoder)
	{
		FNativeWebContentHttpImageResponse response;
		response.StatusCode = 500;
		response.ContentType = TEXT("application/json");
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Default handler failed: encoder service is null"));
		InOnCompleted(false, response, TEXT("Image encode service is not initialized"));
		return;
	}

	imageEncoder->RequestPngBytesByAssetPathAsync(InRequestContext.AssetPath, [InOnCompleted](bool bInSuccess, const TArray<uint8>& InPngBytes, const FString& InError) {
		FNativeWebContentHttpImageResponse response;
		const bool bNotFound = InError.Contains(TEXT("not found"));
		response.StatusCode = bInSuccess ? 200 : (bNotFound ? 404 : 500);
		response.ContentType = bInSuccess ? TEXT("image/png") : TEXT("application/json");
		if (bInSuccess)
		{
			response.Body = InPngBytes;
		}
		UE_LOG(
			LogNativeWebContentHttpServer,
			Verbose,
			TEXT("Default handler complete. success=%s status=%d bytes=%d error='%s'"),
			bInSuccess ? TEXT("true") : TEXT("false"),
			response.StatusCode,
			response.Body.Num(),
			*InError);
		InOnCompleted(bInSuccess, response, InError);
	});
}
