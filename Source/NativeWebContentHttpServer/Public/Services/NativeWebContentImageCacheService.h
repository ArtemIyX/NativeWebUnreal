/**
 * @file NativeWebContentHttpServer/Public/Services/NativeWebContentImageCacheService.h
 * @brief Image asset cache service.
 */
#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/** @brief Caches textures loaded from package/object paths. */
class NATIVEWEBCONTENTHTTPSERVER_API FNativeWebContentImageCacheService final
{
public:

 /**
 * @brief Loads or returns the cached texture identified by a package/object path.
 * @param InPackagePath Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param OutImage Output populated by this operation when applicable.
 * @return The result of the requested operation.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetImageByPackagePath(InPackagePath, OutImage);
 * @endcode
 */
	bool GetImageByPackagePath(const FString& InPackagePath, UTexture2D*& OutImage);
	/** @brief Returns cached image count.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetCachedImageCount();
 * @endcode
 */
	int32 GetCachedImageCount() const;
	/** @brief Clears cached images.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ClearCache();
 * @endcode
 */
	void ClearCache();

private:
	/** @brief Normalizes package path to object path format.
* @param InPackagePath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The resulting text or identifier.
 * @note The subsystem owns HTTP service and handler-object lifecycle.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NormalizeAssetPath(InPackagePath);
 * @endcode
 */
	FString NormalizeAssetPath(const FString& InPackagePath) const;

private:
	mutable FCriticalSection CacheMutex;
	TMap<FString, TStrongObjectPtr<UTexture2D>> CachedImagesByPath;
};
