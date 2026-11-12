/**
 * @file NativeWebContentHttpServer/Public/Handlers/NativeWebContentDefaultImageRequestHandler.h
 * @brief Default /img handler implementation.
 */
#pragma once

#include "CoreMinimal.h"
#include "Handlers/NativeWebContentHttpImageRequestHandler.h"
#include "NativeWebContentDefaultImageRequestHandler.generated.h"

/** @brief Default handler that resolves cached image content and converts it into an HTTP response payload. */
UCLASS(BlueprintType, DisplayName = "Default Image Handler")
class NATIVEWEBCONTENTHTTPSERVER_API UNativeWebContentDefaultImageRequestHandler : public UNativeWebContentHttpImageRequestHandler
{
	GENERATED_BODY()

public:
	/**
	 * @brief Handles request using module cache and encoder services.
	 * @param InRequestContext Parsed request data.
	 * @param OutResponse Output response payload.
	 * @param OutError Error text on failure.
	 * @return True on success.
	 */
	virtual bool HandleImageRequest_Implementation(const FNativeWebContentHttpImageRequestContext& InRequestContext, FNativeWebContentHttpImageResponse& OutResponse, FString& OutError) override;

	/**
	 * @brief Handles request asynchronously with encoded-bytes dedupe/cache.
	 * @param InRequestContext Parsed request data.
	 * @param InOnCompleted Completion callback.
	 */
	virtual void HandleImageRequestAsync(const FNativeWebContentHttpImageRequestContext& InRequestContext, FOnImageRequestCompleted InOnCompleted) override;
};
