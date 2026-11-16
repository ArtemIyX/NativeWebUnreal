#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NativeWebBuildAssetExampleCommandlet.generated.h"

UCLASS()
class UNativeWebBuildAssetExampleCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UNativeWebBuildAssetExampleCommandlet();
	virtual int32 Main(const FString& Params) override;
};
