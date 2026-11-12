#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AffineViewportDemoActor.generated.h"

class UNativeWebView;

/** Opt-in example client for the reusable NativeWeb local-player API. */
UCLASS()
class NATIVEWEB_API AAffineViewportDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AAffineViewportDemoActor();
	
protected:
	UPROPERTY(EditAnywhere, Category="NativeWeb|Affine Demo")
	bool bLoadFilesOnBeginPlay = false;
	UPROPERTY(EditAnywhere, Category="NativeWeb|Affine Demo", meta=(EditCondition="bLoadFilesOnBeginPlay"))
	FString HtmlFilePath;
	UPROPERTY(EditAnywhere, Category="NativeWeb|Affine Demo", meta=(EditCondition="bLoadFilesOnBeginPlay"))
	FString CssFilePath;

public:
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool SetDemoContent(const FString& Html, const FString& Css);
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool LoadDemoFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError);
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool ResizeDemoSurface(int32 Width, int32 Height, float DpiScale = 1.0f);
	UFUNCTION(BlueprintPure, Category="NativeWeb|Affine Demo")
	FString GetDemoLastError() const { return LastError; }


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNativeWebView> View;
	FName ViewName = TEXT("AffineViewportDemo");
	FString LastError;
};
