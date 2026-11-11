#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "AffineViewportDemoActor.generated.h"

class APlayerController;
class FAffineUiSurface;
class FAffineUiScriptInstance;
class UGameViewportClient;
class SWidget;
enum class EMouseCaptureMode : uint8;
enum class EMouseLockMode : uint8;

/** Opt-in D3D11 viewport example for the AffineUI surface. */
UCLASS()
class NATIVEWEB_API AAffineViewportDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AAffineViewportDemoActor();
	virtual ~AAffineViewportDemoActor() override;

	/** Queues a complete replacement of the document and its stylesheet. */
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool SetDemoContent(const FString& Html, const FString& Css);

	/** Reads both files before replacing content; a failed read preserves the current panel. */
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool LoadDemoFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError);

	/** Sets a manual pixel size and CSS DPI scale in place of following the viewport. */
	UFUNCTION(BlueprintCallable, Category="NativeWeb|Affine Demo")
	bool ResizeDemoSurface(int32 Width, int32 Height, float DpiScale = 1.0f);

	/** Reports the most recent asynchronous render failure, if any. */
	UFUNCTION(BlueprintPure, Category="NativeWeb|Affine Demo")
	FString GetDemoLastError() const;

	/** Optionally replace the Vue mount shell with custom HTML and CSS at Play. */
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
#if PLATFORM_WINDOWS
	bool PumpVueJobs(float DeltaTime);
	void StartVueJobPump();
	void StopVueJobPump();

	TUniquePtr<FAffineUiScriptInstance> ScriptInstance;
	FTSTicker::FDelegateHandle VueJobTicker;
	TWeakObjectPtr<APlayerController> InputController;
	EMouseCaptureMode PreviousCaptureMode{};
	EMouseLockMode PreviousLockMode{};
	bool bPreviousShowMouseCursor = false;
	bool bPreviousIgnoreInput = false;
	bool bPreviousHideCursorDuringCapture = false;
#endif
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TWeakObjectPtr<UGameViewportClient> ViewportClient;
	TSharedPtr<SWidget> ViewportWidget;
	bool bFollowViewport = true;
};
