#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FRHICommandListImmediate;

struct FNativeWebNormalizedImage
{
	FTextureRHIRef Source;
	FTextureRHIRef Output;
	FUnorderedAccessViewRHIRef OutputUav;
	FIntPoint Size = FIntPoint::ZeroValue;
	uint64 AllocationBytes = 0;
};

/** Normalizes a supported 2D color source on the executing graphics command stream. */
bool NormalizeImage_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIRef Source,
	FIntPoint MaxSize, const FString& CacheKey, const FString& BudgetNamespace, uint64 MaxGpuBytes,
	FNativeWebNormalizedImage& OutImage, FString& OutError);

/** Releases one view's cache pin after its final GPU sampling marker has completed. */
void MarkNormalizedImageRetiring_RHI(const FString& CacheKey);
void ReleaseNormalizedImage_RHI(const FString& CacheKey, bool bWasRetiring = false);
bool ReserveImageGpuBytes_RHI(const FString& CacheKey, uint64 Bytes, uint64 MaxGpuBytes);
void ReleaseImageGpuBytes_RHI(const FString& CacheKey, uint64 Bytes);
bool HasExpiredIdleImageGpuCache(const FString& BudgetNamespace, double IdleSeconds);
void SweepImageGpuCache_RHI(FRHICommandListImmediate& RHICmdList, const FString& BudgetNamespace,
	double IdleSeconds, bool bDeinitializing);
void GetImageGpuCacheUsage(const FString& BudgetNamespace, uint64& OutActiveBytes, uint64& OutRetiringBytes,
	uint64& OutCachedBytes);

#if WITH_DEV_AUTOMATION_TESTS
/** Holds retirement cleanup for one cache entry after its real GPU marker completes. */
NATIVEWEB_API bool SetImageGpuRetirementTestHold(const FString& CacheKey, bool bHold);
/** RHI-lane query for the automation-only retirement hold. */
NATIVEWEB_API bool IsImageGpuRetirementTestHeld_RHI(const FString& CacheKey);
/** Game-thread query used to prove the production marker reached the held cleanup path. */
NATIVEWEB_API bool DidImageGpuRetirementTestObserveReadyMarker(const FString& CacheKey);
#endif
