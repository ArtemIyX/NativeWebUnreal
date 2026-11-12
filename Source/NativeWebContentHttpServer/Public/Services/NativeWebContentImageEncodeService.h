/**
 * @file NativeWebContentHttpServer/Public/Services/NativeWebContentImageEncodeService.h
 * @brief Texture encoding utilities.
 */
#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/** @brief Encodes textures into HTTP-friendly binary payloads. */
class NATIVEWEBCONTENTHTTPSERVER_API FNativeWebContentImageEncodeService final
{
public:
	/** @brief Async completion callback for encoded bytes request. */
	using FOnEncodedImageReady = TFunction<void(bool, const TArray<uint8>&, const FString&)>;


 /**
 * @brief Encodes the supplied texture into PNG bytes.
 * @param InTexture Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param OutPngBytes Output populated by this operation when applicable.
 * @param OutError Output populated by this operation when applicable.
 * @return The result of the requested operation.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EncodeTextureToPngBytes(InTexture, OutPngBytes, OutError);
 * @endcode
 */
	bool EncodeTextureToPngBytes(UTexture2D* InTexture, TArray<uint8>& OutPngBytes, FString& OutError) const;


 /**
 * @brief Loads and encodes an asset asynchronously, then invokes the completion callback on the documented completion lane.
 * @param InAssetPath Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InOnCompleted Input used by this operation; see the function declaration and owning type for its exact contract.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RequestPngBytesByAssetPathAsync(InAssetPath, InOnCompleted);
 * @endcode
 */
	void RequestPngBytesByAssetPathAsync(const FString& InAssetPath, FOnEncodedImageReady InOnCompleted);

	/** @brief Clears encoded-bytes cache and in-flight request state.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ClearEncodedCache();
 * @endcode
 */
	void ClearEncodedCache();

	/** @brief Returns number of encoded images cached in memory.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetEncodedCacheCount();
 * @endcode
 */
	int32 GetEncodedCacheCount() const;

private:
	struct FEncodedImageEntry
	{
		TArray<uint8> Bytes;
	};

/**
 * @brief Performs the complete pending requests operation.
* @param InAssetPath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInSuccess Boolean option controlling this operation.
* @param InBytes Opaque payload bytes; this operation does not impose an application schema.
* @param InError Receives diagnostic information when the operation fails.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * CompletePendingRequests(InAssetPath, bInSuccess, InBytes, InError);
 * @endcode
 */

	void CompletePendingRequests(const FString& InAssetPath, bool bInSuccess, const TArray<uint8>& InBytes, const FString& InError);

private:
	mutable FCriticalSection EncodedCacheMutex;
	TMap<FString, FEncodedImageEntry> EncodedImageCacheByPath;
	TMap<FString, TArray<FOnEncodedImageReady>> InFlightRequestsByPath;
};
