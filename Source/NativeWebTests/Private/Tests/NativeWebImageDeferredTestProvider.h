#pragma once

#include "Images/NativeWebImageProvider.h"
#include "NativeWebImageDeferredTestProvider.generated.h"

/** Test provider that lets automation complete resolutions in a chosen order. */
UCLASS()
class NATIVEWEBTESTS_API UNativeWebImageDeferredTestProvider final : public UNativeWebImageProvider
{
	GENERATED_BODY()

public:
	virtual void ResolveImage(const FString& Key, FResolveCompletion Completion) override
	{
		PendingKeys.Add(Key);
		PendingCompletions.Add(MoveTemp(Completion));
	}

	int32 GetPendingCount() const { return PendingCompletions.Num(); }

	bool CompleteAt(int32 Index, FNativeWebImageResolution Resolution)
	{
		if (!PendingCompletions.IsValidIndex(Index)) return false;
		FResolveCompletion Completion = MoveTemp(PendingCompletions[Index]);
		PendingCompletions.RemoveAt(Index);
		PendingKeys.RemoveAt(Index);
		Completion(MoveTemp(Resolution));
		return true;
	}

private:
	TArray<FString> PendingKeys;
	TArray<FResolveCompletion> PendingCompletions;
};
