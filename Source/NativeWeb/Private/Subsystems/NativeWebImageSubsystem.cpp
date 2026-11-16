#include "Subsystems/NativeWebImageSubsystem.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "Images/NativeWebImageGpu.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

void UNativeWebImageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GpuBudgetNamespace = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	RegisterProvider(TEXT("asset"), NewObject<UNativeWebAssetImageProvider>(this));
	if (!EvictionTicker.IsValid())
	{
		EvictionTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UNativeWebImageSubsystem::EvictIdle), 1.0f);
	}
}

void UNativeWebImageSubsystem::SetCacheLimits(int32 InMaxEntries, int32 InMaxPending, float InIdleSeconds)
{
	check(IsInGameThread());
	MaxCacheEntries = FMath::Max(1, InMaxEntries);
	MaxPendingRequests = FMath::Max(1, InMaxPending);
	IdleSeconds = FMath::Max(0.0f, InIdleSeconds);
	if (!EvictionTicker.IsValid())
	{
		EvictionTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UNativeWebImageSubsystem::EvictIdle), 1.0f);
	}
}

int32 UNativeWebImageSubsystem::GetPendingLoadCount() const
{
	int32 Count = 0;
	for (const auto& Pair : Entries)
	{
		if (Pair.Value && (Pair.Value->bResolving || Pair.Value->LoadHandle.IsValid())) ++Count;
	}
	return Count;
}

bool UNativeWebImageSubsystem::EvictIdle(float)
{
	check(IsInGameThread());
	const double Now = FPlatformTime::Seconds();
	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		const TSharedPtr<FEntry>& Entry = It.Value();
		if (Entry && Entry->Subscribers.IsEmpty() && !Entry->bResolving && !Entry->LoadHandle.IsValid() &&
			Now - Entry->LastReleasedSeconds >= IdleSeconds)
		{
			if (Entry->Texture) LoadedTextures.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	const FString BudgetNamespace = GetGpuBudgetNamespace();
	const double GpuIdleSeconds = IdleSeconds;
	if (HasExpiredIdleImageGpuCache(BudgetNamespace, GpuIdleSeconds))
	{
		ENQUEUE_RENDER_COMMAND(NativeWebImageCacheIdleSweep)(
			[BudgetNamespace, GpuIdleSeconds](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.EnqueueLambda([BudgetNamespace, GpuIdleSeconds](FRHICommandListImmediate& Executing)
				{
					SweepImageGpuCache_RHI(Executing, BudgetNamespace, GpuIdleSeconds, false);
				});
			});
	}
	return true;
}

bool UNativeWebImageSubsystem::RegisterProvider(FName Namespace, UNativeWebImageProvider* Provider)
{
	check(IsInGameThread());
	if (Namespace.IsNone() || !IsValid(Provider)) return false;
	Providers.Add(Namespace, Provider);
	++ProviderGenerations.FindOrAdd(Namespace);
	return true;
}

void UNativeWebImageSubsystem::UnregisterProvider(FName Namespace)
{
	check(IsInGameThread());
	Providers.Remove(Namespace);
	++ProviderGenerations.FindOrAdd(Namespace);
}

FString UNativeWebImageSubsystem::MakeCacheKey(FName Namespace, const FString& Key) const
{
	if (Namespace.IsNone() || Key.IsEmpty()) return FString();
	return FString::Printf(TEXT("%s:%s:%llu:%s"), *GetGpuBudgetNamespace(), *Namespace.ToString(),
	                       static_cast<unsigned long long>(ProviderGenerations.FindRef(Namespace)), *Key);
}

FString UNativeWebImageSubsystem::GetGpuBudgetNamespace() const
{
	if (GpuBudgetNamespace.IsEmpty())
		const_cast<UNativeWebImageSubsystem*>(this)->GpuBudgetNamespace = FGuid::NewGuid().ToString(
			EGuidFormats::Digits);
	return GpuBudgetNamespace;
}

void UNativeWebImageSubsystem::SetGpuCacheBudgetBytes(uint64 InMaxGpuBytes)
{
	check(IsInGameThread());
	MaxGpuBytes = FMath::Clamp<uint64>(InMaxGpuBytes, 1ull, 1024ull * 1024 * 1024);
}

void UNativeWebImageSubsystem::GetGpuCacheUsage(uint64& OutActiveBytes, uint64& OutRetiringBytes,
                                                uint64& OutCachedBytes) const
{
	GetImageGpuCacheUsage(GetGpuBudgetNamespace(), OutActiveBytes, OutRetiringBytes, OutCachedBytes);
}

uint64 UNativeWebImageSubsystem::RequestImage(FName Namespace, const FString& Key, FImageCompletion Completion)
{
	check(IsInGameThread());
	if (Namespace.IsNone() || Key.IsEmpty() || !Completion || NextLeaseId == MAX_uint64 ||
		NextSubmissionGeneration == MAX_uint64)
		return 0;
	const TObjectPtr<UNativeWebImageProvider>* ProviderPtr = Providers.Find(Namespace);
	UNativeWebImageProvider* Provider = ProviderPtr ? ProviderPtr->Get() : nullptr;
	if (!IsValid(Provider)) return 0;

	const uint64 ProviderGeneration = ProviderGenerations.FindRef(Namespace);
	const FString CacheKey = MakeCacheKey(Namespace, Key);
	if (!Entries.Contains(CacheKey))
	{
		while (Entries.Num() >= MaxCacheEntries)
		{
			FString OldestKey;
			double OldestRelease = TNumericLimits<double>::Max();
			for (const auto& Pair : Entries)
			{
				const TSharedPtr<FEntry>& Candidate = Pair.Value;
				if (Candidate && Candidate->Subscribers.IsEmpty() && !Candidate->bResolving &&
					!Candidate->LoadHandle.IsValid() && Candidate->LastReleasedSeconds < OldestRelease)
				{
					OldestKey = Pair.Key;
					OldestRelease = Candidate->LastReleasedSeconds;
				}
			}
			if (OldestKey.IsEmpty()) break;
			if (TSharedPtr<FEntry>* Oldest = Entries.Find(OldestKey); Oldest && (*Oldest)->Texture)
			{
				LoadedTextures.Remove(OldestKey);
			}
			Entries.Remove(OldestKey);
		}
		if (Entries.Num() >= MaxCacheEntries || GetPendingLoadCount() >= MaxPendingRequests) return 0;
	}

	const uint64 Lease = NextLeaseId++;
	LeaseToKey.Add(Lease, CacheKey);
	if (TSharedPtr<FEntry>* Existing = Entries.Find(CacheKey))
	{
		(*Existing)->Subscribers.Add(Lease, MoveTemp(Completion));
		if ((*Existing)->bResolved && !(*Existing)->LoadHandle.IsValid())
		{
			const FImageCompletion Callback = (*Existing)->Subscribers.FindRef(Lease);
			UTexture2D* Texture = (*Existing)->Texture;
			const FString Error = (*Existing)->Error;
			TWeakObjectPtr<UNativeWebImageSubsystem> WeakThis(this);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Callback, Lease, Texture, Error]() mutable
			{
				if (!WeakThis.IsValid() || !WeakThis->LeaseToKey.Contains(Lease)) return;
				Callback(Lease, Texture, Error);
			});
		}
		return Lease;
	}

	TSharedPtr<FEntry> Entry = MakeShared<FEntry>();
	Entry->CacheKey = CacheKey;
	Entry->ProviderKey = Key;
	Entry->ProviderGeneration = ProviderGeneration;
	Entry->SubmissionGeneration = NextSubmissionGeneration++;
	Entry->Subscribers.Add(Lease, MoveTemp(Completion));
	Entry->bResolving = true;
	Entries.Add(CacheKey, Entry);
	TWeakObjectPtr<UNativeWebImageSubsystem> WeakThis(this);
	const uint64 SubmissionGeneration = Entry->SubmissionGeneration;
	Provider->ResolveImage(Key, [WeakThis, CacheKey, SubmissionGeneration](FNativeWebImageResolution Resolution) mutable
	{
		if (IsInGameThread())
		{
			if (WeakThis.IsValid()) WeakThis->OnResolved(CacheKey, SubmissionGeneration, MoveTemp(Resolution));
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, CacheKey, SubmissionGeneration,
				          Resolution = MoveTemp(Resolution)]() mutable
			          {
				          if (WeakThis.IsValid())
					          WeakThis->OnResolved(CacheKey, SubmissionGeneration, MoveTemp(Resolution));
			          });
		}
	});
	return Lease;
}

void UNativeWebImageSubsystem::OnResolved(FString CacheKey, uint64 SubmissionGeneration,
                                          FNativeWebImageResolution Resolution)
{
	check(IsInGameThread());
	TSharedPtr<FEntry>* Found = Entries.Find(CacheKey);
	if (!Found || !Found->IsValid() || (*Found)->SubmissionGeneration != SubmissionGeneration) return;
	TSharedPtr<FEntry> Entry = *Found;
	Entry->bResolving = false;
	Entry->bResolved = true;
	if (Resolution.LoadedTexture)
	{
		Entry->Texture = Resolution.LoadedTexture;
		Entry->AssetPath = FSoftObjectPath(Resolution.LoadedTexture->GetPathName());
		LoadedTextures.Add(CacheKey, Resolution.LoadedTexture);
	}
	else if (Resolution.SoftTexture.ToSoftObjectPath().IsValid() && Resolution.Error.IsEmpty())
	{
		Entry->AssetPath = Resolution.SoftTexture.ToSoftObjectPath();
		if (UTexture2D* Loaded = Resolution.SoftTexture.Get())
		{
			Entry->Texture = Loaded;
			LoadedTextures.Add(CacheKey, Loaded);
		}
		else
		{
			TWeakObjectPtr<UNativeWebImageSubsystem> WeakThis(this);
			const uint64 EntryGeneration = Entry->SubmissionGeneration;
			Entry->LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Entry->AssetPath,
				FStreamableDelegate::CreateLambda([WeakThis, CacheKey, EntryGeneration]()
				{
					if (WeakThis.IsValid()) WeakThis->OnLoaded(CacheKey, EntryGeneration);
				}));
			if (!Entry->LoadHandle.IsValid()) Resolution.Error = TEXT("Texture load request was rejected.");
		}
	}
	else if (Resolution.Error.IsEmpty())
	{
		Resolution.Error = TEXT("Provider returned neither a loaded texture nor a valid soft object path.");
	}
	Entry->Error = Resolution.Error;

	if (!Resolution.Error.IsEmpty())
	{
		for (auto& Pair : Entry->Subscribers)
		{
			const uint64 Lease = Pair.Key;
			FImageCompletion Callback = Pair.Value;
			const FString Error = Resolution.Error;
			TWeakObjectPtr<UNativeWebImageSubsystem> WeakThis(this);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Callback = MoveTemp(Callback), Lease, Error]() mutable
			{
				if (!WeakThis.IsValid() || !WeakThis->LeaseToKey.Contains(Lease)) return;
				Callback(Lease, nullptr, Error);
			});
		}
	}
	else if (Entry->Texture)
	{
		LoadedTextures.Add(CacheKey, Entry->Texture);
		for (auto& Pair : Entry->Subscribers)
		{
			const uint64 Lease = Pair.Key;
			FImageCompletion Callback = Pair.Value;
			UTexture2D* Texture = Entry->Texture;
			TWeakObjectPtr<UNativeWebImageSubsystem> WeakThis(this);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Callback = MoveTemp(Callback), Lease, Texture]() mutable
			{
				if (!WeakThis.IsValid() || !WeakThis->LeaseToKey.Contains(Lease)) return;
				Callback(Lease, Texture, FString());
			});
		}
	}
}

void UNativeWebImageSubsystem::OnLoaded(FString CacheKey, uint64 SubmissionGeneration)
{
	check(IsInGameThread());
	TSharedPtr<FEntry>* Found = Entries.Find(CacheKey);
	if (!Found || !Found->IsValid() || (*Found)->SubmissionGeneration != SubmissionGeneration) return;
	TSharedPtr<FEntry> Entry = *Found;
	Entry->Texture = Cast<UTexture2D>(Entry->AssetPath.ResolveObject());
	Entry->LoadHandle.Reset();
	if (Entry->Texture) LoadedTextures.Add(CacheKey, Entry->Texture);
	const FString Error = Entry->Texture ? FString() : TEXT("Texture asset failed to load or is not a Texture2D.");
	Entry->bResolved = true;
	Entry->Error = Error;
	for (auto& Pair : Entry->Subscribers)
	{
		const uint64 Lease = Pair.Key;
		FImageCompletion Callback = Pair.Value;
		UTexture2D* Texture = Entry->Texture;
		TWeakObjectPtr<UNativeWebImageSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Callback = MoveTemp(Callback), Lease, Texture, Error]() mutable
		{
			if (!WeakThis.IsValid() || !WeakThis->LeaseToKey.Contains(Lease)) return;
			Callback(Lease, Texture, Error);
		});
	}
}

void UNativeWebImageSubsystem::ReleaseImage(uint64 LeaseId)
{
	check(IsInGameThread());
	FString Key;
	if (!LeaseToKey.RemoveAndCopyValue(LeaseId, Key)) return;
	TSharedPtr<FEntry>* Found = Entries.Find(Key);
	if (!Found) return;
	TSharedPtr<FEntry> Entry = *Found;
	Entry->Subscribers.Remove(LeaseId);
	if (!Entry->Subscribers.IsEmpty()) return;
	Entry->LastReleasedSeconds = FPlatformTime::Seconds();
	if (Entry->LoadHandle.IsValid())
	{
		Entry->LoadHandle->CancelHandle();
		Entry->LoadHandle.Reset();
		LoadedTextures.Remove(Key);
		Entries.Remove(Key);
	}
	else if (Entry->bResolving)
	{
		Entries.Remove(Key);
	}
}

void UNativeWebImageSubsystem::Deinitialize()
{
	check(IsInGameThread());
	const FString BudgetNamespace = GetGpuBudgetNamespace();
	const double GpuIdleSeconds = IdleSeconds;
	if (EvictionTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(EvictionTicker);
	for (auto& Pair : Entries)
		if (Pair.Value && Pair.Value->LoadHandle.IsValid())
			Pair.Value->LoadHandle->
			     CancelHandle();
	Entries.Reset();
	LeaseToKey.Reset();
	LoadedTextures.Reset();
	Providers.Reset();
	ENQUEUE_RENDER_COMMAND(NativeWebImageCacheDeinitializeSweep)(
		[BudgetNamespace, GpuIdleSeconds](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.EnqueueLambda([BudgetNamespace, GpuIdleSeconds](FRHICommandListImmediate& Executing)
			{
				SweepImageGpuCache_RHI(Executing, BudgetNamespace, GpuIdleSeconds, true);
			});
		});
	Super::Deinitialize();
}
