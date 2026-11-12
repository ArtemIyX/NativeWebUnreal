/**
 * @file NativeWebContentHttpServer/Public/Handlers/NativeWebContentHttpImageRequestHandler.h
 * @brief Base request handler types for /img route processing.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NativeWebContentHttpImageRequestHandler.generated.h"

/** @brief Input payload used by image request handlers. */
USTRUCT(BlueprintType)
struct NATIVEWEBCONTENTHTTPSERVER_API FNativeWebContentHttpImageRequestContext
{
	GENERATED_BODY()

	/** Asset path extracted from the request, typically used as the image lookup key. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebContentHttpServer")
	FString AssetPath;

	/** Raw HTTP request body forwarded to the handler when needed. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebContentHttpServer")
	FString RawBody;

	/** Parsed query-string parameters associated with the image request. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebContentHttpServer")
	TMap<FString, FString> QueryParams;
};

/** @brief Output payload produced by image request handlers. */
USTRUCT(BlueprintType)
struct NATIVEWEBCONTENTHTTPSERVER_API FNativeWebContentHttpImageResponse
{
	GENERATED_BODY()

	/** HTTP-style status code returned by the handler. */
	UPROPERTY(BlueprintReadWrite, Category = "NativeWebContentHttpServer")
	int32 StatusCode = 200;

	/** MIME type of the response body bytes. */
	UPROPERTY(BlueprintReadWrite, Category = "NativeWebContentHttpServer")
	FString ContentType = TEXT("application/octet-stream");

	/** Binary body payload returned to the HTTP layer. */
	UPROPERTY(BlueprintReadWrite, Category = "NativeWebContentHttpServer")
	TArray<uint8> Body;
};

/** @brief Pluggable image request handler contract. */
UCLASS(Abstract, Blueprintable, BlueprintType, DisplayName = "Image Handler (Abstract)")
class NATIVEWEBCONTENTHTTPSERVER_API UNativeWebContentHttpImageRequestHandler : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Handles /img request.
	 * @param InRequestContext Parsed request data.
	 * @param OutResponse Response payload.
	 * @param OutError Error text for failed handling.
	 * @return True when request is handled successfully.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool HandleImageRequest(const FNativeWebContentHttpImageRequestContext& InRequestContext, FNativeWebContentHttpImageResponse& OutResponse, FString& OutError);

	/** @brief Async completion callback type for image handlers. */
	using FOnImageRequestCompleted = TFunction<void(bool, const FNativeWebContentHttpImageResponse&, const FString&)>;

	/**
	 * @brief Handles /img request asynchronously.
	 * @param InRequestContext Parsed request data.
	 * @param InOnCompleted Completion callback.
	 */
	virtual void HandleImageRequestAsync(const FNativeWebContentHttpImageRequestContext& InRequestContext, FOnImageRequestCompleted InOnCompleted);
};
