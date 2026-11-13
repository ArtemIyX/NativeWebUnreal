/**
 * @file Source/NativeWeb/Private/NativeWebGpuPresenter.h
 * @brief Declares the private GPU viewport presenter.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"

class ULocalPlayer;
class UNativeWebView;
class UTextureRenderTarget2D;

/** @brief Game-thread owner for the GPU scene-view extension and Slate input preprocessor. */
class FNativeWebGpuPresenter final
{
public:
	/** @brief Converts viewport pixels into normalized coordinates for a nonempty view rectangle. */
	static bool ConvertViewportPoint(const FVector2D& ViewportPixel, const FIntRect& ViewRect,
	                                 FVector2D& OutNormalized, bool bAllowOutside = false)
	{
		if (ViewRect.IsEmpty()) return false;
		if (!bAllowOutside && (ViewportPixel.X < ViewRect.Min.X || ViewportPixel.Y < ViewRect.Min.Y ||
			ViewportPixel.X >= ViewRect.Max.X || ViewportPixel.Y >= ViewRect.Max.Y))
			return false;
		OutNormalized = FVector2D((ViewportPixel.X - ViewRect.Min.X) / ViewRect.Width(),
		                          (ViewportPixel.Y - ViewRect.Min.Y) / ViewRect.Height());
		return true;
	}

	FNativeWebGpuPresenter();
	~FNativeWebGpuPresenter();

	/** @brief Initializes render and input hooks without enabling drawing or consuming input. */
	bool Initialize(UNativeWebView* View, ULocalPlayer* Player, UTextureRenderTarget2D* Texture, FString& OutReason);
	/** @brief Publishes a new output texture reference to render-thread state. */
	void SetTexture(UTextureRenderTarget2D* Texture);
	/** @brief Enables drawing and input after the owner transactionally commits this presenter. */
	void Activate(bool bVisible);
	/** @brief Changes viewport draw and input enablement without unregistering the presenter. */
	void SetVisible(bool bVisible);
	/** @brief Disables drawing and input, releases held state, and unregisters owned hooks. */
	void Shutdown();
	/** @brief Returns whether a render callback validated the output and scene targets. */
	bool IsReady() const;
	/** @brief Returns a render-thread initialization failure, if one was reported. */
	bool GetFailure(FString& OutReason) const;

private:
	class FRenderExtension;
	class FInputAdapter;
	TSharedPtr<FRenderExtension, ESPMode::ThreadSafe> RenderExtension;
	TSharedPtr<FInputAdapter> InputAdapter;
};
