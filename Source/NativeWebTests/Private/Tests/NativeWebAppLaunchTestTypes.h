#pragma once

#include "CoreMinimal.h"
#include "Data/Structs/NativeWebError.h"
#include "NativeWebView.h"
#include "NativeWebAppLaunchTestTypes.generated.h"

UCLASS()
class UNativeWebAppLaunchTestReceiver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void ReceiveSuccess(UNativeWebView* View, const FNativeWebError& Error);

	UFUNCTION()
	void ReceiveFailure(UNativeWebView* View, const FNativeWebError& Error);

	int32 SuccessCount = 0;
	int32 FailureCount = 0;
	TWeakObjectPtr<UNativeWebView> LastView;
	FNativeWebError LastError;
};
