#include "Images/NativeWebImageProvider.h"

void UNativeWebAssetImageProvider::ResolveImage(const FString& Key, FResolveCompletion Completion)
{
	check(IsInGameThread());
	FNativeWebImageResolution Result;
	const FSoftObjectPath Path(Key);
	if (!Path.IsValid())
	{
		Result.Error = TEXT("Expected a valid Unreal soft object path.");
	}
	else
	{
		Result.SoftTexture = TSoftObjectPtr<UTexture2D>(Path);
	}
	Completion(MoveTemp(Result));
}
