#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Images/NativeWebImageProvider.h"
#include "Tests/NativeWebImageDeferredTestProvider.h"
#include "Subsystems/NativeWebImageSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FNativeWebImageSubsystemTestOwner
	{
		TStrongObjectPtr<UGameInstance> GameInstance{NewObject<UGameInstance>(GetTransientPackage())};
		TStrongObjectPtr<UNativeWebImageSubsystem> Service{NewObject<UNativeWebImageSubsystem>(GameInstance.Get())};
	};

	struct FNativeWebDeferredResolutionState
	{
		TStrongObjectPtr<UGameInstance> GameInstance{NewObject<UGameInstance>(GetTransientPackage())};
		TStrongObjectPtr<UNativeWebImageSubsystem> Service{NewObject<UNativeWebImageSubsystem>(GameInstance.Get())};
		TStrongObjectPtr<UNativeWebImageDeferredTestProvider> Provider{
			NewObject<UNativeWebImageDeferredTestProvider>(Service.Get())};
		FAutomationTestBase* Test = nullptr;
		uint64 ReplacementLease = 0;
		bool bReleasedCallbackDelivered = false;
		bool bReplacementCallbackDelivered = false;
		FString ReleasedCallbackError;
		FString ReplacementCallbackError;
	};

	class FNativeWebImageDeferredResolutionLatentCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FNativeWebImageDeferredResolutionLatentCommand(
			TSharedRef<FNativeWebDeferredResolutionState, ESPMode::ThreadSafe> InState)
			: State(MoveTemp(InState))
		{
		}

		virtual bool Update() override
		{
			if (!State->bReplacementCallbackDelivered && ++FramesWaited < 120) return false;
			State->Test->TestFalse(TEXT("released request never receives its deferred completion"),
				State->bReleasedCallbackDelivered);
			State->Test->TestTrue(TEXT("replacement request receives its own deferred completion"),
				State->bReplacementCallbackDelivered);
			State->Test->TestEqual(TEXT("replacement receives the new provider result"),
				State->ReplacementCallbackError, FString(TEXT("replacement result")));
			State->Service->ReleaseImage(State->ReplacementLease);
			State->Test->TestEqual(TEXT("replacement lease releases after delivery"),
				State->Service->GetActiveLeaseCount(), 0);
			return true;
		}

	private:
		TSharedRef<FNativeWebDeferredResolutionState, ESPMode::ThreadSafe> State;
		int32 FramesWaited = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebImageCacheLimitsTest, "NativeWeb.Images.CacheLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebImageCacheLimitsTest::RunTest(const FString&)
{
	FNativeWebImageSubsystemTestOwner Owner;
	UNativeWebImageSubsystem* Service = Owner.Service.Get();
	Service->SetCacheLimits(1, 1, 10.0f);
	TestEqual(TEXT("empty service has no active leases"), Service->GetActiveLeaseCount(), 0);
	TestEqual(TEXT("empty service has no cached entries"), Service->GetCachedEntryCount(), 0);
	TestEqual(TEXT("empty service has no pending loads"), Service->GetPendingLoadCount(), 0);
	UNativeWebAssetImageProvider* Provider = NewObject<UNativeWebAssetImageProvider>(Service);
	TestTrue(TEXT("the asset provider registers for admission checks"), Service->RegisterProvider(TEXT("asset"), Provider));
	const auto IgnoreCompletion = [](uint64, UTexture2D*, const FString&) {};
	const uint64 PinnedLease = Service->RequestImage(TEXT("asset"), TEXT("invalid first key"), IgnoreCompletion);
	TestTrue(TEXT("one cache entry is admitted within the configured capacity"), PinnedLease != 0);
	TestEqual(TEXT("the active lease pins the sole entry"), Service->GetCachedEntryCount(), 1);
	TestEqual(TEXT("a second key is rejected while the sole entry is pinned"),
		Service->RequestImage(TEXT("asset"), TEXT("invalid second key"), IgnoreCompletion), static_cast<uint64>(0));
	Service->ReleaseImage(PinnedLease);
	const uint64 ReplacementLease = Service->RequestImage(TEXT("asset"), TEXT("invalid second key"), IgnoreCompletion);
	TestTrue(TEXT("releasing a lease makes the old entry evictable under pressure"), ReplacementLease != 0);
	TestEqual(TEXT("pressure keeps metadata within the configured entry bound"), Service->GetCachedEntryCount(), 1);
	Service->ReleaseImage(ReplacementLease);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebImageRequestCoalescingTest, "NativeWeb.Images.RequestCoalescing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebImageRequestCoalescingTest::RunTest(const FString&)
{
	FNativeWebImageSubsystemTestOwner Owner;
	UNativeWebImageSubsystem* Service = Owner.Service.Get();
	UNativeWebAssetImageProvider* Provider = NewObject<UNativeWebAssetImageProvider>(Service);
	TestTrue(TEXT("asset provider registers"), Service->RegisterProvider(TEXT("asset"), Provider));
	const auto IgnoreCompletion = [](uint64, UTexture2D*, const FString&) {};
	const uint64 FirstLease = Service->RequestImage(TEXT("asset"), TEXT("invalid key"), IgnoreCompletion);
	const uint64 SecondLease = Service->RequestImage(TEXT("asset"), TEXT("invalid key"), IgnoreCompletion);
	TestTrue(TEXT("first request receives a lease"), FirstLease != 0);
	TestTrue(TEXT("coalesced request receives its own lease"), SecondLease != 0 && SecondLease != FirstLease);
	TestEqual(TEXT("repeated provider keys share one cache entry"), Service->GetCachedEntryCount(), 1);
	TestEqual(TEXT("both subscribers remain independently leased"), Service->GetActiveLeaseCount(), 2);
	Service->ReleaseImage(FirstLease);
	Service->ReleaseImage(SecondLease);
	TestEqual(TEXT("releasing both subscribers clears active leases"), Service->GetActiveLeaseCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAssetImageProviderTest, "NativeWeb.Images.AssetProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAssetImageProviderTest::RunTest(const FString&)
{
	UNativeWebAssetImageProvider* Provider = NewObject<UNativeWebAssetImageProvider>();
	bool bInvalidCompleted = false;
	Provider->ResolveImage(TEXT("not an object path"), [&bInvalidCompleted, this](FNativeWebImageResolution Result)
	{
		bInvalidCompleted = true;
		TestFalse(TEXT("invalid asset key has no soft path"), Result.SoftTexture.ToSoftObjectPath().IsValid());
		TestTrue(TEXT("invalid asset key reports a useful error"), !Result.Error.IsEmpty());
	});
	TestTrue(TEXT("asset provider completes the validation callback"), bInvalidCompleted);

	bool bValidCompleted = false;
	Provider->ResolveImage(TEXT("/Game/UI/T_Icon.T_Icon"), [&bValidCompleted, this](FNativeWebImageResolution Result)
	{
		bValidCompleted = true;
		TestTrue(TEXT("valid soft path is retained"), Result.SoftTexture.ToSoftObjectPath().IsValid());
		TestNull(TEXT("provider does not synchronously load the asset"), Result.LoadedTexture);
		TestTrue(TEXT("valid path has no resolution error"), Result.Error.IsEmpty());
	});
	TestTrue(TEXT("valid path resolution completes"), bValidCompleted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebImageLeaseReleaseAndGenerationTest,
	"NativeWeb.Images.ReleaseAndProviderGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebImageLeaseReleaseAndGenerationTest::RunTest(const FString&)
{
	FNativeWebImageSubsystemTestOwner Owner;
	FNativeWebImageSubsystemTestOwner OtherOwner;
	UNativeWebImageSubsystem* Service = Owner.Service.Get();
	UNativeWebImageSubsystem* OtherService = OtherOwner.Service.Get();
	Service->SetGpuCacheBudgetBytes(8ull * 1024 * 1024);
	TestEqual(TEXT("configured GPU budget is exposed"), Service->GetMaxGpuBytes(), static_cast<uint64>(8ull * 1024 * 1024));
	TestNotEqual(TEXT("per-game-instance cache identity is unique"),
		Service->MakeCacheKey(TEXT("asset"), TEXT("same key")), OtherService->MakeCacheKey(TEXT("asset"), TEXT("same key")));
	uint64 ActiveBytes = 0, RetiringBytes = 0, CachedBytes = 0;
	Service->GetGpuCacheUsage(ActiveBytes, RetiringBytes, CachedBytes);
	TestEqual(TEXT("new game-instance cache starts with no active allocations"), ActiveBytes, static_cast<uint64>(0));
	TestEqual(TEXT("new game-instance cache starts with no retiring allocations"), RetiringBytes, static_cast<uint64>(0));
	TestEqual(TEXT("new game-instance cache starts with no idle cached allocations"), CachedBytes, static_cast<uint64>(0));
	UNativeWebAssetImageProvider* Provider = NewObject<UNativeWebAssetImageProvider>(Service);
	TestTrue(TEXT("provider registers for the first generation"), Service->RegisterProvider(TEXT("asset"), Provider));
	const FString FirstGenerationKey = Service->MakeCacheKey(TEXT("asset"), TEXT("missing asset"));
	bool bCompletionRanInline = false;
	const uint64 Lease = Service->RequestImage(TEXT("asset"), TEXT("missing asset"),
		[&bCompletionRanInline](uint64, UTexture2D*, const FString&) { bCompletionRanInline = true; });
	TestTrue(TEXT("request receives a lease"), Lease != 0);
	TestFalse(TEXT("terminal callback is not delivered inline"), bCompletionRanInline);
	TestEqual(TEXT("pending request is tracked"), Service->GetActiveLeaseCount(), 1);
	Service->ReleaseImage(Lease);
	Service->ReleaseImage(Lease);
	TestEqual(TEXT("release is idempotent and removes the lease"), Service->GetActiveLeaseCount(), 0);
	Service->UnregisterProvider(TEXT("asset"));
	TestTrue(TEXT("provider can register for a replacement generation"), Service->RegisterProvider(TEXT("asset"), Provider));
	TestNotEqual(TEXT("replacement provider generation receives a distinct cache key"),
		Service->MakeCacheKey(TEXT("asset"), TEXT("missing asset")), FirstGenerationKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebImageDeferredResolutionGenerationTest,
	"NativeWeb.Images.DeferredResolutionGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebImageDeferredResolutionGenerationTest::RunTest(const FString&)
{
	TSharedRef<FNativeWebDeferredResolutionState, ESPMode::ThreadSafe> State =
		MakeShared<FNativeWebDeferredResolutionState, ESPMode::ThreadSafe>();
	State->Test = this;
	TestTrue(TEXT("deferred test provider registers"),
		State->Service->RegisterProvider(TEXT("deferred"), State->Provider.Get()));
	const uint64 ReleasedLease = State->Service->RequestImage(TEXT("deferred"), TEXT("same key"),
		[State](uint64, UTexture2D*, const FString& Error)
		{
			State->bReleasedCallbackDelivered = true;
			State->ReleasedCallbackError = Error;
		});
	TestTrue(TEXT("first deferred request receives a lease"), ReleasedLease != 0);
	TestEqual(TEXT("provider holds the first deferred completion"), State->Provider->GetPendingCount(), 1);
	State->Service->ReleaseImage(ReleasedLease);
	State->ReplacementLease = State->Service->RequestImage(TEXT("deferred"), TEXT("same key"),
		[State](uint64, UTexture2D*, const FString& Error)
		{
			State->bReplacementCallbackDelivered = true;
			State->ReplacementCallbackError = Error;
		});
	TestTrue(TEXT("same key can be requested again after final release"), State->ReplacementLease != 0);
	TestEqual(TEXT("replacement has its own deferred provider request"), State->Provider->GetPendingCount(), 2);
	FNativeWebImageResolution OldResolution;
	OldResolution.Error = TEXT("stale old result");
	TestTrue(TEXT("old request can complete after replacement exists"), State->Provider->CompleteAt(0, MoveTemp(OldResolution)));
	TestFalse(TEXT("stale completion does not resolve the replacement inline"),
		State->bReplacementCallbackDelivered);
	FNativeWebImageResolution NewResolution;
	NewResolution.Error = TEXT("replacement result");
	TestTrue(TEXT("replacement request completes independently"), State->Provider->CompleteAt(0, MoveTemp(NewResolution)));
	ADD_LATENT_AUTOMATION_COMMAND(FNativeWebImageDeferredResolutionLatentCommand(State));
	return true;
}
