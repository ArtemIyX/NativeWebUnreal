#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AffineViewportDemoActor.generated.h"

class UGameViewportClient;
class UTextureRenderTarget2D;
class SWidget;
class FAffineViewportDemoRenderer;

/** Opt-in D3D11 viewport graphics probe. */
UCLASS()
class NATIVEWEB_API AAffineViewportDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AAffineViewportDemoActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SampleViewportDiagnostic();
	FTimerHandle DiagnosticTimer;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	TWeakObjectPtr<UGameViewportClient> ViewportClient;
	TSharedPtr<SWidget> ViewportWidget;
	FAffineViewportDemoRenderer* DemoRenderer = nullptr;
};
