/**
 * @file NativeWebContentHttpServer/Private/Services/NativeWebContentImageCacheService.cpp
 * @brief Image cache load/lookup/clear implementation.
 */
#include "Services/NativeWebContentImageCacheService.h"

#include "NativeWebContentHttpServer.h"
#include "Misc/PackageName.h"
#include "Stats/NativeWebContentHttpServerStats.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/Texture2D.h"

FString FNativeWebContentImageCacheService::NormalizeAssetPath(const FString& InPackagePath) const
{
	FString assetPath = InPackagePath.TrimStartAndEnd();
	if (assetPath.IsEmpty())
	{
		return assetPath;
	}

	if (!assetPath.Contains(TEXT(".")))
	{
		const FString assetName = FPackageName::GetShortName(assetPath);
		if (!assetName.IsEmpty())
		{
			assetPath = FString::Printf(TEXT("%s.%s"), *assetPath, *assetName);
		}
	}

	return assetPath;
}

bool FNativeWebContentImageCacheService::GetImageByPackagePath(const FString& InPackagePath, UTexture2D*& OutImage)
{
	OutImage = nullptr;
	const FString normalizedPath = NormalizeAssetPath(InPackagePath);
	if (normalizedPath.IsEmpty())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("GetImageByPackagePath failed: input path is empty"));
		return false;
	}

	{
		FScopeLock lock(&CacheMutex);
		if (const TStrongObjectPtr<UTexture2D>* cachedImage = CachedImagesByPath.Find(normalizedPath))
		{
			if (cachedImage->IsValid())
			{
				OutImage = cachedImage->Get();
				UE_LOG(LogNativeWebContentHttpServer, Verbose, TEXT("Cache hit for '%s'"), *normalizedPath);
				return true;
			}

			UE_LOG(LogNativeWebContentHttpServer, Warning, TEXT("Stale cache entry for '%s', removing"), *normalizedPath);
			CachedImagesByPath.Remove(normalizedPath);
			SET_DWORD_STAT(STAT_NativeWebContentHttpServer_CachedImages, static_cast<uint32>(CachedImagesByPath.Num()));
		}
	}

	FSoftObjectPath softPath(normalizedPath);
	UE_LOG(LogNativeWebContentHttpServer, Verbose, TEXT("Cache miss for '%s'. Loading asset"), *normalizedPath);
	if (!softPath.IsValid())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Invalid image asset path '%s'"), *normalizedPath);
		return false;
	}

	UObject* const loadedObject = softPath.TryLoad();
	if (!loadedObject)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Failed to load image from path '%s'"), *normalizedPath);
		return false;
	}

	UTexture2D* const loadedTexture = Cast<UTexture2D>(loadedObject);
	if (!loadedTexture)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Loaded asset '%s' is not UTexture2D (actual class: %s)"), *normalizedPath, *loadedObject->GetClass()->GetName());
		return false;
	}

	{
		FScopeLock lock(&CacheMutex);
		CachedImagesByPath.Add(normalizedPath, TStrongObjectPtr<UTexture2D>(loadedTexture));
		SET_DWORD_STAT(STAT_NativeWebContentHttpServer_CachedImages, static_cast<uint32>(CachedImagesByPath.Num()));
		UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Cached image '%s'. Cached images: %d"), *normalizedPath, CachedImagesByPath.Num());
	}

	OutImage = loadedTexture;
	return true;
}

int32 FNativeWebContentImageCacheService::GetCachedImageCount() const
{
	FScopeLock lock(&CacheMutex);
	return CachedImagesByPath.Num();
}

void FNativeWebContentImageCacheService::ClearCache()
{
	FScopeLock lock(&CacheMutex);
	const int32 numCachedBeforeClear = CachedImagesByPath.Num();
	CachedImagesByPath.Reset();
	SET_DWORD_STAT(STAT_NativeWebContentHttpServer_CachedImages, 0);
	UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Image cache cleared. Removed %d cached images"), numCachedBeforeClear);
}
