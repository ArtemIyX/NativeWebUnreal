#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NativeWebImageProvider.generated.h"

struct FNativeWebImageResolution
{
	TSoftObjectPtr<class UTexture2D> SoftTexture;
	class UTexture2D* LoadedTexture = nullptr;
	FString Error;
};

/** Resolves application-defined image keys. Resolution and completion are game-thread owned. */
UCLASS(Abstract)
class NATIVEWEB_API UNativeWebImageProvider : public UObject
{
	GENERATED_BODY()
public:
	using FResolveCompletion = TFunction<void(FNativeWebImageResolution)>;
	/** Providers may complete asynchronously, but must invoke completion on the game thread. */
	virtual void ResolveImage(const FString& Key, FResolveCompletion Completion)
	{
		(void)Key;
		if (!Completion) return;
		FNativeWebImageResolution Result;
		Result.Error = TEXT("This image provider does not implement ResolveImage.");
		Completion(MoveTemp(Result));
	}
};

/** Built-in provider resolving a validated Unreal soft object path. */
UCLASS()
class NATIVEWEB_API UNativeWebAssetImageProvider final : public UNativeWebImageProvider
{
	GENERATED_BODY()
public:
	virtual void ResolveImage(const FString& Key, FResolveCompletion Completion) override;
};
