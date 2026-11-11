#pragma once

#include "CoreMinimal.h"
#include "AffineUiSurface.h"

class FTextureRenderTargetResource;

/** Private RHI-lane owner; Shutdown must drain queued work before destruction. */
class FAffineUiSurfaceRenderer
{
public:
	void Draw(FTextureRenderTargetResource* Target, FIntPoint Size, float DpiScale, FColor ClearColor,
		FAffineUiContent Content, FString FontDirectory, uint64 Revision,
		TArray<FAffineUiTextMutation> TextMutations,
		TArray<FAffineUiMutation> Mutations, uint64 DocumentEpoch,
		TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface);
	void DispatchInput(FAffineUiInput Input, FIntPoint TextureSize, uint64 DocumentEpoch,
		TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface);
	void Shutdown();

private:
	class FState;
	FState* State = nullptr;
};
