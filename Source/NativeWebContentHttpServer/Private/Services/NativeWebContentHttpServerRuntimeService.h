/**
 * @file NativeWebContentHttpServer/Private/Services/NativeWebContentHttpServerRuntimeService.h
 * @brief Internal HTTP runtime service for binding and serving /img route.
 */
#pragma once

#include "CoreMinimal.h"
#include "HttpRouteHandle.h"

/**
 * @brief Declares the i http router type and its operations.
 * @ingroup NativeWebContentHttpServer
 */
class IHttpRouter;
class UNativeWebContentHttpImageRequestHandler;

/** @brief Internal wrapper over Unreal HttpServer route lifecycle. */
class FNativeWebContentHttpServerRuntimeService final
{
public:

 /**
 * @brief Implements Start for this module.
 * @param InPort TCP port requested for the listener; zero selects the configured default.
 * @param InHandler Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Start(InPort, InHandler);
 * @endcode
 */
	bool Start(int32 InPort, TWeakObjectPtr<UNativeWebContentHttpImageRequestHandler> InHandler);
	/** @brief Unbinds route and stops listeners.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Stop();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	void Stop();

	/** @brief Returns true when listener and route are active.
 * @return True when the named condition holds.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * IsRunning();
 * @endcode
 */
	bool IsRunning() const;
	/** @brief Returns currently active listen port or 0.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetPort();
 * @endcode
 */
	int32 GetPort() const;

private:
	/** @brief Handles incoming /img requests.
* @param InRequest Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InOnComplete Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HandleImageRoute(InRequest, InOnComplete);
 * @endcode
 */
	bool HandleImageRoute(const FHttpServerRequest& InRequest, const FHttpResultCallback& InOnComplete);

	/** @brief Resolves asset path from query or body payload.
* @param InRequest Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InRawBody Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The resulting text or identifier.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ResolveAssetPath(InRequest, InRawBody);
 * @endcode
 */
	FString ResolveAssetPath(const FHttpServerRequest& InRequest, const FString& InRawBody) const;
	/** @brief Decodes raw request body as UTF-8 string.
* @param InRequest Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The resulting text or identifier.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ParseBodyAsUtf8(InRequest);
 * @endcode
 */
	FString ParseBodyAsUtf8(const FHttpServerRequest& InRequest) const;

	/** @brief Completes request with JSON error body.
 * @param InOnComplete Callback/delegate supplied by the caller; it is invoked according to the owning type contract.
 * @param InStatusCode Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
 * @param InMessage Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance content HTTP and image request services.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
CompleteJson(InOnComplete, InStatusCode, InMessage);
@endcode
 */
	static void CompleteJson(const FHttpResultCallback& InOnComplete, int32 InStatusCode, const FString& InMessage);
	/** @brief Completes request with binary body.
 * @param InOnComplete Callback/delegate supplied by the caller; it is invoked according to the owning type contract.
 * @param InStatusCode Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
 * @param InContentType Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
 * @param InBody Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance content HTTP and image request services.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
CompleteBinary(InOnComplete, InStatusCode, InContentType, InBody);
@endcode
 */
	static void CompleteBinary(const FHttpResultCallback& InOnComplete, int32 InStatusCode, const FString& InContentType, const TArray<uint8>& InBody);

private:
	int32 ListenPort = 0;
	FHttpRouteHandle ImgRouteHandle;
	TSharedPtr<IHttpRouter> HttpRouter;
	TWeakObjectPtr<UNativeWebContentHttpImageRequestHandler> RequestHandler;
};
