#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Assets/NativeWebAppAsset.h"
#include "Data/Structs/NativeWebError.h"
#include "Data/Structs/NativeWebPresentationConfig.h"
#include "Data/Structs/NativeWebSurfaceConfig.h"
#include "NativeWebCreateAppViewAsyncAction.generated.h"

class UNativeWebLocalPlayerSubsystem;
class UNativeWebView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNativeWebAppViewResult, UNativeWebView*, View, const FNativeWebError&, Error);

UCLASS()
class NATIVEWEB_API UNativeWebCreateAppViewAsyncAction final : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FNativeWebAppViewResult Success;

	UPROPERTY(BlueprintAssignable)
	FNativeWebAppViewResult Failure;

	UFUNCTION(BlueprintCallable, Category="NativeWeb|App", meta=(BlueprintInternalUseOnly="true", ToolTip="Loads a NativeWeb app and creates its local-player view."))
	static UNativeWebCreateAppViewAsyncAction* CreateAppViewAsync(
		UNativeWebLocalPlayerSubsystem* Subsystem, TSoftObjectPtr<UNativeWebAppAsset> App, FName Name,
		FNativeWebSurfaceConfig SurfaceConfig, FNativeWebPresentationConfig PresentationConfig);

	virtual void Activate() override;

private:
	void Complete(UNativeWebView* View, const FNativeWebError& Error);

	TWeakObjectPtr<UNativeWebLocalPlayerSubsystem> Subsystem;
	TSoftObjectPtr<UNativeWebAppAsset> App;
	FName Name = NAME_None;
	FNativeWebSurfaceConfig SurfaceConfig;
	FNativeWebPresentationConfig PresentationConfig;
	bool bActivated = false;
	bool bCompleted = false;
};
