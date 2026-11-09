#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AffineViewportDemoActor.generated.h"

class FAffineUiSurface;
class UGameViewportClient;
class SWidget;

/** Opt-in D3D11 viewport example for the AffineUI surface. */
UCLASS()
class NATIVEWEB_API AAffineViewportDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AAffineViewportDemoActor();

	/** Queues a complete replacement of the document and its stylesheet. */
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool SetDemoContent(const FString& Html, const FString& Css);

	/** Reads both files before replacing content; a failed read preserves the current panel. */
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool LoadDemoFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError);

	/** Requests a new pixel size and CSS DPI scale. */
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool ResizeDemoSurface(int32 Width, int32 Height, float DpiScale = 1.0f);

	/** Reports the most recent asynchronous render failure, if any. */
	UFUNCTION(BlueprintPure, Category="NativeWeb|Affine Demo")
	FString GetDemoLastError() const;

	/** Optionally replace the inline example with files when Play begins. */
	UPROPERTY(EditAnywhere, Category="NativeWeb|Affine Demo")
	bool bLoadFilesOnBeginPlay = false;

	UPROPERTY(EditAnywhere, Category="NativeWeb|Affine Demo", meta=(EditCondition="bLoadFilesOnBeginPlay"))
	FString HtmlFilePath;

	UPROPERTY(EditAnywhere, Category="NativeWeb|Affine Demo", meta=(EditCondition="bLoadFilesOnBeginPlay"))
	FString CssFilePath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TWeakObjectPtr<UGameViewportClient> ViewportClient;
	TSharedPtr<SWidget> ViewportWidget;
};
