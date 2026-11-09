#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AffineViewportDemoActor.generated.h"

class UGameViewportClient;
class UTextureRenderTarget2D;
class SWidget;

/** Opt-in, solid-color presentation probe; does not yet render AffineUI. */
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
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	TWeakObjectPtr<UGameViewportClient> ViewportClient;
	TSharedPtr<SWidget> ViewportWidget;
};
