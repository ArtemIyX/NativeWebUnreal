/**
 * @file NativeWebContentHttpServer/Public/Handlers/NativeWebContentDefaultImageRequestHandler.h
 * @brief Default /img handler implementation.
 */
#pragma once

#include "CoreMinimal.h"
#include "Handlers/NativeWebContentHttpImageRequestHandler.h"
#include "NativeWebContentDefaultImageRequestHandler.generated.h"

/**
 * @brief Default handler that resolves cached image content and converts it into an HTTP response payload.
 * @ingroup NativeWebContentHttpServer
 */
UCLASS(BlueprintType, DisplayName = "Default Image Handler")
class NATIVEWEBCONTENTHTTPSERVER_API
	UNativeWebContentDefaultImageRequestHandler : public UNativeWebContentHttpImageRequestHandler
{
	GENERATED_BODY()

public:
	/**
	* @brief Builds the default image response from the parsed request context.
	* @param InRequestContext Input used by this operation; see the function declaration and owning type for its exact contract.
	* @param OutResponse Output populated by this operation when applicable.
	* @param OutError Output populated by this operation when applicable.
	* @return The result of the requested operation.
	* @note The subsystem owns HTTP service and handler-object lifecycle.
	* @par Example
	* Invocation example; substitute arguments that satisfy the function contract.
	* @code{.cpp}
	* HandleImageRequest_Implementation(InRequestContext, OutResponse, OutError);
	* @endcode
	*/
	virtual bool HandleImageRequest_Implementation(const FNativeWebContentHttpImageRequestContext& InRequestContext,
	                                               FNativeWebContentHttpImageResponse& OutResponse,
	                                               FString& OutError) override;


	/**
	* @brief Handles an image request asynchronously and invokes the completion callback when processing finishes.
	* @param InRequestContext Input used by this operation; see the function declaration and owning type for its exact contract.
	* @param InOnCompleted Input used by this operation; see the function declaration and owning type for its exact contract.
	* @note The subsystem owns HTTP service and handler-object lifecycle.
	* @par Example
	* Invocation example; substitute arguments that satisfy the function contract.
	* @code{.cpp}
	* HandleImageRequestAsync(InRequestContext, InOnCompleted);
	* @endcode
	*/
	virtual void HandleImageRequestAsync(const FNativeWebContentHttpImageRequestContext& InRequestContext,
	                                     FOnImageRequestCompleted InOnCompleted) override;
};
