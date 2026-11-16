#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/StreamableManager.h"
#include "Images/NativeWebImageProvider.h"
#include "NativeWebImageSubsystem.generated.h"

/** Asynchronous provider registry and shared soft-texture request cache. */
UCLASS()
class NATIVEWEB_API UNativeWebImageSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	using FImageCompletion = TFunction<void(uint64 LeaseId, UTexture2D* Texture, const FString& Error)>;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="NativeWeb|Images")
	bool RegisterProvider(FName Namespace, UNativeWebImageProvider* Provider);
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Images")
	void UnregisterProvider(FName Namespace);

	/** Returns zero on admission failure. Completion is always queued on the game thread. */
	uint64 RequestImage(FName Namespace, const FString& Key, FImageCompletion Completion);
	FString MakeCacheKey(FName Namespace, const FString& Key) const;
	FString GetGpuBudgetNamespace() const;
	uint64 GetMaxGpuBytes() const { return MaxGpuBytes; }
	void SetGpuCacheBudgetBytes(uint64 InMaxGpuBytes);
	void GetGpuCacheUsage(uint64& OutActiveBytes, uint64& OutRetiringBytes, uint64& OutCachedBytes) const;
	/** Idempotent. Cancels the load after its final subscriber releases. */
	void ReleaseImage(uint64 LeaseId);
	/** Configures bounded metadata cache; active leases remain pinned. */
	void SetCacheLimits(int32 InMaxEntries, int32 InMaxPending, float InIdleSeconds);
	int32 GetActiveLeaseCount() const { return LeaseToKey.Num(); }
	int32 GetCachedEntryCount() const { return Entries.Num(); }
	int32 GetPendingLoadCount() const;
	virtual void Deinitialize() override;

private:
	struct FEntry
	{
		FString CacheKey;
		FString ProviderKey;
		FSoftObjectPath AssetPath;
		TObjectPtr<UTexture2D> Texture = nullptr;
		TSharedPtr<FStreamableHandle> LoadHandle;
		TMap<uint64, FImageCompletion> Subscribers;
		uint64 ProviderGeneration = 0;
		uint64 SubmissionGeneration = 0;
		double LastReleasedSeconds = 0.0;
		bool bResolving = false;
		bool bResolved = false;
		FString Error;
	};
	void OnResolved(FString CacheKey, uint64 SubmissionGeneration, FNativeWebImageResolution Resolution);
	void OnLoaded(FString CacheKey, uint64 SubmissionGeneration);
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UNativeWebImageProvider>> Providers;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>> LoadedTextures;
	TMap<FString, TSharedPtr<FEntry>> Entries;
	TMap<uint64, FString> LeaseToKey;
	TMap<FName, uint64> ProviderGenerations;
	uint64 NextLeaseId = 1;
	uint64 NextSubmissionGeneration = 1;
	int32 MaxPendingRequests = 32;
	int32 MaxCacheEntries = 128;
	uint64 MaxGpuBytes = 64ull * 1024 * 1024;
	mutable FString GpuBudgetNamespace;
	float IdleSeconds = 10.0f;
	FTSTicker::FDelegateHandle EvictionTicker;
	bool EvictIdle(float DeltaSeconds);
};
