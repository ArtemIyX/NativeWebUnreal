#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "NativeWebView.h"
#include "NativeWebLocalPlayerSubsystem.generated.h"

UCLASS()
class NATIVEWEB_API UNativeWebLocalPlayerSubsystem final : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UNativeWebView* CreateView(FName Name, const FNativeWebViewOptions& Options, FNativeWebError& OutError);
	
	UFUNCTION(BlueprintPure, Category="NativeWeb") 
	UNativeWebView* FindView(FName Name) const;
	
	UFUNCTION(BlueprintCallable, Category="NativeWeb") 
	bool DestroyView(FName Name);
private:
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UNativeWebView>> Views;
	FTSTicker::FDelegateHandle TickHandle;
	bool Tick(float DeltaTime);
};
