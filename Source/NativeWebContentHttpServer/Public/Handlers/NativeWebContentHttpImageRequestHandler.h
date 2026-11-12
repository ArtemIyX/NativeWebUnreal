/**
 * @file NativeWebContentHttpServer/Public/Handlers/NativeWebContentHttpImageRequestHandler.h
 * @brief Base request handler types for /img route processing.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NativeWebContentHttpImageRequestHandler.generated.h"

/**
 * @brief Parsed input supplied to an image request handler.
 * @ingroup NativeWebContentHttpServer
 */
USTRUCT(BlueprintType)
struct NATIVEWEBCONTENTHTTPSERVER_API FNativeWebContentHttpImageRequestContext
{
	GENERATED_BODY()

	/** Asset path extracted from the request, typically used as the image lookup key. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `AssetPath`. */
	FString AssetPath;

	/** Raw HTTP request body forwarded to the handler when needed. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `RawBody`. */
	FString RawBody;

	/** Parsed query-string parameters associated with the image request. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `QueryParams`. */
	TMap<FString, FString> QueryParams;
};

/** @brief Output payload produced by image request handlers. */
USTRUCT(BlueprintType)
struct NATIVEWEBCONTENTHTTPSERVER_API FNativeWebContentHttpImageResponse
{
	GENERATED_BODY()

	/** HTTP-style status code returned by the handler. */
	UPROPERTY(BlueprintReadWrite, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `StatusCode`. */
	int32 StatusCode = 200;

	/** MIME type of the response body bytes. */
	UPROPERTY(BlueprintReadWrite, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `ContentType`. */
	FString ContentType = TEXT("application/octet-stream");

	/** Binary body payload returned to the HTTP layer. */
	UPROPERTY(BlueprintReadWrite, Category = "NativeWebContentHttpServer")
	/** @brief Reflected data field `Body`. */
	TArray<uint8> Body;
};

/** @brief Pluggable image request handler contract. */
UCLASS(Abstract, Blueprintable, BlueprintType, DisplayName = "Image Handler (Abstract)")
class NATIVEWEBCONTENTHTTPSERVER_API UNativeWebContentHttpImageRequestHandler : public UObject
{
	GENERATED_BODY()

public:
	/**
	* @brief Handles an image HTTP request and fills its response or error output.
	* @param InRequestContext Input used by this operation; see the function declaration and owning type for its exact contract.
	* @param OutResponse Output populated by this operation when applicable.
	* @param OutError Output populated by this operation when applicable.
	* @return The result of the requested operation.
	* @note The subsystem owns HTTP service and handler-object lifecycle.
	* @par Example
	* Invocation example; substitute arguments that satisfy the function contract.
	* @code{.cpp}
	* HandleImageRequest(InRequestContext, OutResponse, OutError);
	* @endcode
	*/
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool HandleImageRequest(const FNativeWebContentHttpImageRequestContext& InRequestContext,
	                        FNativeWebContentHttpImageResponse& OutResponse, FString& OutError);

	/** @brief Async completion callback type for image handlers. */
	using FOnImageRequestCompleted = TFunction<void(bool, const FNativeWebContentHttpImageResponse&, const FString&)>;

	/**
	 * @brief Handles /img request asynchronously.
	 * @param InRequestContext Parsed request data.
	 * @param InOnCompleted Completion callback.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance content HTTP and image request services.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleImageRequestAsync(InRequestContext, InOnCompleted);
@endcode
 */
	virtual void HandleImageRequestAsync(const FNativeWebContentHttpImageRequestContext& InRequestContext,
	                                     FOnImageRequestCompleted InOnCompleted);
};
