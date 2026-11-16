#include "Images/NativeWebImageGpu.h"

#include "GlobalShader.h"
#include "DynamicRHI.h"
#include "PixelFormat.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"
#include "HAL/PlatformTime.h"
#include "HAL/CriticalSection.h"

class FNativeWebImageNormalizeCS final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNativeWebImageNormalizeCS);
	SHADER_USE_PARAMETER_STRUCT(FNativeWebImageNormalizeCS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FIntPoint, OutputSize)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FNativeWebImageNormalizeCS,
	"/Plugin/NativeWeb/Private/NativeWebImageNormalize.usf", "MainCS", SF_Compute);

namespace
{
	struct FImageCacheEntry
	{
		FNativeWebNormalizedImage Image;
		FString BudgetNamespace;
		uint32 ViewReferences = 0;
		uint32 RetiringReferences = 0;
		uint64 AdditionalBytes = 0;
		double LastReleasedSeconds = 0.0;
		bool bEvictOnLastRelease = false;
#if WITH_DEV_AUTOMATION_TESTS
		bool bAutomationHoldRetirement = false;
		bool bAutomationObservedReadyMarker = false;
#endif
	};
	TMap<FString, FImageCacheEntry> GImageCache;
	struct FImageBudgetUsage { uint64 ActiveBytes = 0; uint64 RetiringBytes = 0; uint64 CachedBytes = 0; };
	FCriticalSection GImageCacheMutex;
	TMap<FString, FImageBudgetUsage> GImageBudgetUsage;
	struct FImageReservation { FString BudgetNamespace; uint64 Bytes = 0; };
	TMap<FString, FImageReservation> GImageReservations;
	constexpr int32 MaxImageCacheEntries = 128;

	void AddBudgetBytes(const FString& Namespace, uint64 Bytes, bool bRetiring, bool bCached = false)
	{
		FImageBudgetUsage& Usage = GImageBudgetUsage.FindOrAdd(Namespace);
		if (bCached) Usage.CachedBytes += Bytes;
		else (bRetiring ? Usage.RetiringBytes : Usage.ActiveBytes) += Bytes;
	}

	void RemoveBudgetBytes(const FString& Namespace, uint64 Bytes, bool bRetiring, bool bCached = false)
	{
		if (FImageBudgetUsage* Usage = GImageBudgetUsage.Find(Namespace))
		{
			uint64& Counter = bCached ? Usage->CachedBytes : (bRetiring ? Usage->RetiringBytes : Usage->ActiveBytes);
			Counter -= FMath::Min(Counter, Bytes);
			if (Usage->ActiveBytes == 0 && Usage->RetiringBytes == 0 && Usage->CachedBytes == 0)
				GImageBudgetUsage.Remove(Namespace);
		}
	}

	int32 GetUsageCategory(uint32 ViewReferences, uint32 RetiringReferences)
	{
		if (ViewReferences > RetiringReferences) return 0;
		return RetiringReferences > 0 ? 1 : 2;
	}

	void MoveBudgetBytes(const FString& Namespace, uint64 Bytes, int32 From, int32 To)
	{
		RemoveBudgetBytes(Namespace, Bytes, From == 1, From == 2);
		AddBudgetBytes(Namespace, Bytes, To == 1, To == 2);
	}

	FIntPoint BoundedSize(FIntPoint Source, FIntPoint Bound)
	{
		const int32 LimitX = FMath::Clamp(Bound.X, 1, 4096);
		const int32 LimitY = FMath::Clamp(Bound.Y, 1, 4096);
		const double Scale = FMath::Min(1.0,
			FMath::Min(static_cast<double>(LimitX) / Source.X, static_cast<double>(LimitY) / Source.Y));
		return FIntPoint(FMath::Clamp(FMath::RoundToInt(Source.X * Scale), 1, LimitX),
			FMath::Clamp(FMath::RoundToInt(Source.Y * Scale), 1, LimitY));
	}
}

bool NormalizeImage_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIRef Source,
	FIntPoint MaxSize, const FString& CacheKey, const FString& BudgetNamespace, uint64 MaxGpuBytes,
	FNativeWebNormalizedImage& OutImage, FString& OutError)
{
	check(IsInRenderingThread());
	OutImage = {};
	if (!Source.IsValid())
	{
		OutError = TEXT("Texture resource is unavailable.");
		return false;
	}
	const FRHITextureDesc& SourceDesc = Source->GetDesc();
	if (SourceDesc.Dimension != ETextureDimension::Texture2D || SourceDesc.ArraySize != 1 ||
		SourceDesc.NumSamples != 1 || SourceDesc.Extent.X <= 0 || SourceDesc.Extent.Y <= 0 ||
		SourceDesc.Format == PF_Unknown || GPixelFormats[SourceDesc.Format].Capabilities == EPixelFormatCapabilities::None ||
		!EnumHasAnyFlags(GPixelFormats[SourceDesc.Format].Capabilities, EPixelFormatCapabilities::TextureSample))
	{
		OutError = TEXT("Only single-sample, single-slice sampled 2D color textures are supported.");
		return false;
	}
	const FPixelFormatInfo& OutputFormat = GPixelFormats[PF_R8G8B8A8];
	if (!EnumHasAnyFlags(OutputFormat.Capabilities, EPixelFormatCapabilities::TypedUAVStore))
	{
		OutError = TEXT("RGBA8 typed UAV stores are unavailable on this RHI.");
		return false;
	}

	const FIntPoint OutputSize = BoundedSize(SourceDesc.Extent, MaxSize);
	const double Now = FPlatformTime::Seconds();
	if (CacheKey.IsEmpty() || BudgetNamespace.IsEmpty() || MaxGpuBytes == 0)
	{
		OutError = TEXT("Normalized image cache key or GPU budget is invalid.");
		return false;
	}
	FRHITextureCreateDesc OutputDesc = FRHITextureCreateDesc::Create2D(
		TEXT("NativeWebImageNormalized"), OutputSize, PF_R8G8B8A8)
		.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV)
		.SetInitialState(ERHIAccess::UAVCompute);
	const uint64 RequiredBytes = RHICalcTexturePlatformSize(OutputDesc, 0).Size;
	if (RequiredBytes == 0 || RequiredBytes > MaxGpuBytes)
	{
		OutError = TEXT("Normalized image exceeds the GPU cache budget.");
		return false;
	}
	{
		FScopeLock Lock(&GImageCacheMutex);
		if (FImageCacheEntry* Existing = GImageCache.Find(CacheKey))
		{
			if (Existing->RetiringReferences > 0 || Existing->bEvictOnLastRelease)
			{
				OutError = TEXT("The normalized image cache entry is still retiring or shutting down.");
				return false;
			}
			if (Existing->Image.Source.GetReference() != Source.GetReference() || Existing->BudgetNamespace != BudgetNamespace)
			{
				if (Existing->ViewReferences != 0 || Existing->RetiringReferences != 0)
				{
					OutError = TEXT("The cache key is already pinned to another texture resource.");
					return false;
				}
				const int32 Category = GetUsageCategory(Existing->ViewReferences, Existing->RetiringReferences);
				RemoveBudgetBytes(Existing->BudgetNamespace, Existing->Image.AllocationBytes + Existing->AdditionalBytes,
					Category == 1, Category == 2);
				GImageCache.Remove(CacheKey);
				Existing = nullptr;
			}
			if (Existing)
			{
				const int32 Category = GetUsageCategory(Existing->ViewReferences, Existing->RetiringReferences);
				if (Category != 0) MoveBudgetBytes(BudgetNamespace,
					Existing->Image.AllocationBytes + Existing->AdditionalBytes, Category, 0);
				++Existing->ViewReferences;
				OutImage = Existing->Image;
				return true;
			}
		}
		if (GImageReservations.Contains(CacheKey))
		{
			OutError = TEXT("The normalized image allocation is already pending.");
			return false;
		}
		auto GetBudgetTotal = [&BudgetNamespace]()
		{
			uint64 Total = 0;
			for (const auto& Pair : GImageCache)
				if (Pair.Value.BudgetNamespace == BudgetNamespace)
					Total += Pair.Value.Image.AllocationBytes + Pair.Value.AdditionalBytes;
			for (const auto& Pair : GImageReservations)
				if (Pair.Value.BudgetNamespace == BudgetNamespace) Total += Pair.Value.Bytes;
			return Total;
		};
		while (GetBudgetTotal() > MaxGpuBytes - RequiredBytes || GImageCache.Num() + GImageReservations.Num() >= MaxImageCacheEntries)
		{
			FString OldestKey;
			double OldestReleased = TNumericLimits<double>::Max();
			for (const auto& Pair : GImageCache)
				if (Pair.Value.ViewReferences == 0 && Pair.Value.RetiringReferences == 0 &&
					(GetBudgetTotal() <= MaxGpuBytes - RequiredBytes || Pair.Value.BudgetNamespace == BudgetNamespace) &&
					Pair.Value.LastReleasedSeconds < OldestReleased)
				{
					OldestKey = Pair.Key;
					OldestReleased = Pair.Value.LastReleasedSeconds;
				}
			if (OldestKey.IsEmpty())
			{
				OutError = TEXT("Native image GPU cache is at its active or retiring allocation limit.");
				return false;
			}
			if (FImageCacheEntry* Oldest = GImageCache.Find(OldestKey))
			{
				const int32 Category = GetUsageCategory(Oldest->ViewReferences, Oldest->RetiringReferences);
				RemoveBudgetBytes(Oldest->BudgetNamespace, Oldest->Image.AllocationBytes + Oldest->AdditionalBytes,
					Category == 1, Category == 2);
			}
			GImageCache.Remove(OldestKey);
		}
		FImageReservation Reservation;
		Reservation.BudgetNamespace = BudgetNamespace;
		Reservation.Bytes = RequiredBytes;
		GImageReservations.Add(CacheKey, MoveTemp(Reservation));
		AddBudgetBytes(BudgetNamespace, RequiredBytes, false);
	}
	auto CancelReservation = [&CacheKey, &BudgetNamespace, RequiredBytes]()
	{
		FScopeLock Lock(&GImageCacheMutex);
		if (GImageReservations.Remove(CacheKey) > 0) RemoveBudgetBytes(BudgetNamespace, RequiredBytes, false);
	};
	FTextureRHIRef Output = RHICmdList.CreateTexture(OutputDesc);
	if (!Output.IsValid())
	{
		CancelReservation();
		OutError = TEXT("Could not allocate the normalized RGBA8 texture.");
		return false;
	}
	FUnorderedAccessViewRHIRef OutputUav = RHICmdList.CreateUnorderedAccessView(
		Output, FRHIViewDesc::CreateTextureUAV().SetDimensionFromTexture(Output));
	if (!OutputUav.IsValid())
	{
		CancelReservation();
		OutError = TEXT("Could not create a normalized texture UAV.");
		return false;
	}

	RHICmdList.Transition(FRHITransitionInfo(Source, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
	TShaderMapRef<FNativeWebImageNormalizeCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FNativeWebImageNormalizeCS::FParameters Parameters;
	Parameters.SourceTexture = Source;
	Parameters.SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters.OutputSize = OutputSize;
	Parameters.OutputTexture = OutputUav;
	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Parameters,
		FIntVector(FMath::DivideAndRoundUp(OutputSize.X, 8), FMath::DivideAndRoundUp(OutputSize.Y, 8), 1));
	RHICmdList.Transition(FRHITransitionInfo(Source, ERHIAccess::SRVCompute, ERHIAccess::SRVMask));
	RHICmdList.Transition(FRHITransitionInfo(Output, ERHIAccess::UAVCompute,
		RHIGetInterfaceType() == ERHIInterfaceType::D3D12 ? ERHIAccess::CopySrc : ERHIAccess::SRVMask));

	FImageCacheEntry Entry;
	Entry.Image.Source = MoveTemp(Source);
	Entry.Image.Output = MoveTemp(Output);
	Entry.Image.OutputUav = MoveTemp(OutputUav);
	Entry.Image.Size = OutputSize;
	Entry.Image.AllocationBytes = RequiredBytes;
	Entry.BudgetNamespace = BudgetNamespace;
	Entry.ViewReferences = 1;
	Entry.LastReleasedSeconds = Now;
	OutImage = Entry.Image;
	{
		FScopeLock Lock(&GImageCacheMutex);
		if (GImageReservations.Remove(CacheKey) == 0)
		{
			RemoveBudgetBytes(BudgetNamespace, RequiredBytes, false);
			OutImage = {};
			OutError = TEXT("Normalized image reservation expired before installation.");
			return false;
		}
		GImageCache.Add(CacheKey, MoveTemp(Entry));
	}
	return true;
}

void MarkNormalizedImageRetiring_RHI(const FString& CacheKey)
{
	FScopeLock Lock(&GImageCacheMutex);
	if (FImageCacheEntry* Entry = GImageCache.Find(CacheKey))
	{
		if (Entry->RetiringReferences < Entry->ViewReferences)
		{
			const int32 Category = GetUsageCategory(Entry->ViewReferences, Entry->RetiringReferences);
			++Entry->RetiringReferences;
			const int32 NewCategory = GetUsageCategory(Entry->ViewReferences, Entry->RetiringReferences);
			if (Category != NewCategory)
				MoveBudgetBytes(Entry->BudgetNamespace, Entry->Image.AllocationBytes + Entry->AdditionalBytes,
					Category, NewCategory);
		}
	}
}

void ReleaseNormalizedImage_RHI(const FString& CacheKey, bool bWasRetiring)
{
	FScopeLock Lock(&GImageCacheMutex);
	if (FImageCacheEntry* Entry = GImageCache.Find(CacheKey))
	{
		const int32 Category = GetUsageCategory(Entry->ViewReferences, Entry->RetiringReferences);
		if (Entry->ViewReferences > 0) --Entry->ViewReferences;
		if (bWasRetiring && Entry->RetiringReferences > 0) --Entry->RetiringReferences;
		const int32 NewCategory = GetUsageCategory(Entry->ViewReferences, Entry->RetiringReferences);
		if (Category != NewCategory)
			MoveBudgetBytes(Entry->BudgetNamespace, Entry->Image.AllocationBytes + Entry->AdditionalBytes,
				Category, NewCategory);
		if (Entry->ViewReferences == 0)
		{
			Entry->LastReleasedSeconds = FPlatformTime::Seconds();
			if (Entry->RetiringReferences == 0 && Entry->bEvictOnLastRelease)
			{
				RemoveBudgetBytes(Entry->BudgetNamespace, Entry->Image.AllocationBytes + Entry->AdditionalBytes,
					NewCategory == 1, NewCategory == 2);
				GImageCache.Remove(CacheKey);
			}
		}
	}
}

bool ReserveImageGpuBytes_RHI(const FString& CacheKey, uint64 Bytes, uint64 MaxGpuBytes)
{
	FScopeLock Lock(&GImageCacheMutex);
	FImageCacheEntry* Entry = GImageCache.Find(CacheKey);
	if (!Entry || Entry->ViewReferences == 0 || Bytes == 0 || Bytes > MaxGpuBytes ||
		Entry->Image.AllocationBytes + Entry->AdditionalBytes > MaxGpuBytes - Bytes)
		return false;
	const FString BudgetNamespace = Entry->BudgetNamespace;
	auto GetBudgetTotal = [&BudgetNamespace]()
	{
		uint64 Total = 0;
		for (const auto& Pair : GImageCache)
			if (Pair.Value.BudgetNamespace == BudgetNamespace)
				Total += Pair.Value.Image.AllocationBytes + Pair.Value.AdditionalBytes;
		for (const auto& Pair : GImageReservations)
			if (Pair.Value.BudgetNamespace == BudgetNamespace) Total += Pair.Value.Bytes;
		return Total;
	};
	while (GetBudgetTotal() > MaxGpuBytes - Bytes)
	{
		FString OldestKey;
		double OldestReleased = TNumericLimits<double>::Max();
		for (const auto& Pair : GImageCache)
			if (Pair.Key != CacheKey && Pair.Value.BudgetNamespace == BudgetNamespace && Pair.Value.ViewReferences == 0 &&
				Pair.Value.RetiringReferences == 0 &&
				Pair.Value.LastReleasedSeconds < OldestReleased)
			{
				OldestKey = Pair.Key;
				OldestReleased = Pair.Value.LastReleasedSeconds;
			}
		if (OldestKey.IsEmpty()) return false;
		if (FImageCacheEntry* Oldest = GImageCache.Find(OldestKey))
		{
			const int32 Category = GetUsageCategory(Oldest->ViewReferences, Oldest->RetiringReferences);
			RemoveBudgetBytes(Oldest->BudgetNamespace, Oldest->Image.AllocationBytes + Oldest->AdditionalBytes,
				Category == 1, Category == 2);
		}
		GImageCache.Remove(OldestKey);
	}
	Entry = GImageCache.Find(CacheKey);
	if (!Entry) return false;
	Entry->AdditionalBytes += Bytes;
	const int32 Category = GetUsageCategory(Entry->ViewReferences, Entry->RetiringReferences);
	AddBudgetBytes(Entry->BudgetNamespace, Bytes, Category == 1, Category == 2);
	return true;
}

void ReleaseImageGpuBytes_RHI(const FString& CacheKey, uint64 Bytes)
{
	FScopeLock Lock(&GImageCacheMutex);
	if (FImageCacheEntry* Entry = GImageCache.Find(CacheKey))
	{
		const uint64 ReleasedBytes = FMath::Min(Bytes, Entry->AdditionalBytes);
		Entry->AdditionalBytes -= ReleasedBytes;
		const int32 Category = GetUsageCategory(Entry->ViewReferences, Entry->RetiringReferences);
		RemoveBudgetBytes(Entry->BudgetNamespace, ReleasedBytes, Category == 1, Category == 2);
	}
}

bool HasExpiredIdleImageGpuCache(const FString& BudgetNamespace, double IdleSeconds)
{
	FScopeLock Lock(&GImageCacheMutex);
	const double Now = FPlatformTime::Seconds();
	for (const auto& Pair : GImageCache)
	{
		const FImageCacheEntry& Entry = Pair.Value;
		if (Entry.BudgetNamespace == BudgetNamespace && Entry.ViewReferences == 0 &&
			Entry.RetiringReferences == 0 && Now - Entry.LastReleasedSeconds >= IdleSeconds)
		{
			return true;
		}
	}
	return false;
}

void SweepImageGpuCache_RHI(FRHICommandListImmediate&, const FString& BudgetNamespace,
	double IdleSeconds, bool bDeinitializing)
{
	check(IsInRenderingThread() || IsInRHIThread());
	const double Now = FPlatformTime::Seconds();
	FScopeLock Lock(&GImageCacheMutex);
	for (auto It = GImageCache.CreateIterator(); It; ++It)
	{
		FImageCacheEntry& Entry = It.Value();
		if (Entry.BudgetNamespace != BudgetNamespace) continue;
		if (bDeinitializing) Entry.bEvictOnLastRelease = true;
		if (Entry.ViewReferences != 0 || Entry.RetiringReferences != 0) continue;
		if (!bDeinitializing && Now - Entry.LastReleasedSeconds < IdleSeconds) continue;
		const int32 Category = GetUsageCategory(Entry.ViewReferences, Entry.RetiringReferences);
		RemoveBudgetBytes(Entry.BudgetNamespace, Entry.Image.AllocationBytes + Entry.AdditionalBytes,
			Category == 1, Category == 2);
		It.RemoveCurrent();
	}
}

void GetImageGpuCacheUsage(const FString& BudgetNamespace, uint64& OutActiveBytes, uint64& OutRetiringBytes,
	uint64& OutCachedBytes)
{
	FScopeLock Lock(&GImageCacheMutex);
	const FImageBudgetUsage* Usage = GImageBudgetUsage.Find(BudgetNamespace);
	OutActiveBytes = Usage ? Usage->ActiveBytes : 0;
	OutRetiringBytes = Usage ? Usage->RetiringBytes : 0;
	OutCachedBytes = Usage ? Usage->CachedBytes : 0;
}

#if WITH_DEV_AUTOMATION_TESTS
bool SetImageGpuRetirementTestHold(const FString& CacheKey, bool bHold)
{
	FScopeLock Lock(&GImageCacheMutex);
	FImageCacheEntry* Entry = GImageCache.Find(CacheKey);
	if (!Entry) return false;
	if (bHold) Entry->bAutomationObservedReadyMarker = false;
	Entry->bAutomationHoldRetirement = bHold;
	return true;
}

bool IsImageGpuRetirementTestHeld_RHI(const FString& CacheKey)
{
	FScopeLock Lock(&GImageCacheMutex);
	FImageCacheEntry* Entry = GImageCache.Find(CacheKey);
	if (!Entry || !Entry->bAutomationHoldRetirement) return false;
	Entry->bAutomationObservedReadyMarker = true;
	return true;
}

bool DidImageGpuRetirementTestObserveReadyMarker(const FString& CacheKey)
{
	FScopeLock Lock(&GImageCacheMutex);
	const FImageCacheEntry* Entry = GImageCache.Find(CacheKey);
	return Entry && Entry->bAutomationObservedReadyMarker;
}
#endif
