/**
 * @file Source/NativeWeb/Private/AffineUI/AffineUiSurfaceRenderer.h
 * @brief Declares affine ui surface renderer for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "AffineUiSurface.h"

class FTextureRenderTargetResource;

/**
 * @brief Owns AffineUI rendering and texture publication on the RHI lane.
 * @details Shutdown must drain queued work before this renderer is destroyed.
 * @ingroup NativeWeb
 */
class FAffineUiSurfaceRenderer
{
public:
	FAffineUiSurfaceRenderer();
	FAffineUiSurfaceRenderer(const FAffineUiSurfaceRenderer&) = delete;
	FAffineUiSurfaceRenderer& operator=(const FAffineUiSurfaceRenderer&) = delete;

	/**
	 * @brief Performs the draw operation.
	* @param Target Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Size Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param DpiScale Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param ClearColor Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Content Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param FontAssets Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Revision Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param TextMutations Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Mutations Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param DocumentEpoch Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Surface Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Draw(Target, Size, DpiScale, ClearColor, Content, FontAssets, Revision, TextMutations, Mutations, DocumentEpoch, Surface);
	 * @endcode
	 */

	void Draw(FTextureRenderTargetResource* Target, FIntPoint Size, float DpiScale, FColor ClearColor,
	          FAffineUiContent Content, TSharedRef<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe> FontAssets,
	          uint64 Revision,
	          TArray<FAffineUiTextMutation> TextMutations,
	          TArray<FAffineUiMutation> Mutations, uint64 DocumentEpoch,
	          TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface);
	/**
	 * @brief Dispatches input.
	* @param Input Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param TextureSize Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param DocumentEpoch Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Surface Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DispatchInput(Input, TextureSize, DocumentEpoch, Surface);
	 * @endcode
	 */

	void DispatchInput(FAffineUiInput Input, FIntPoint TextureSize, uint64 DocumentEpoch,
	                   TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface);
	void InstallImage(FString Url, FString CacheKey, FString GpuBudgetNamespace, uint64 MaxGpuBytes,
		uint64 DocumentEpoch, FTextureRHIRef Source,
		TFunction<void(bool, FIntPoint, const FString&)>&& Completion);
	void RemoveImage(FString Url, uint64 DocumentEpoch);
	/**
	 * @brief Performs the shutdown operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Shutdown();
	 * @endcode
	 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
	 */

	void Shutdown();
	static void ShutdownRetirementQueue();

private:
	class FState;
	static bool TickRetirementQueue(float DeltaSeconds);
	static TArray<TSharedPtr<FState, ESPMode::ThreadSafe>> RetiringStates;
	static FTSTicker::FDelegateHandle RetirementQueueTicker;
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
};
