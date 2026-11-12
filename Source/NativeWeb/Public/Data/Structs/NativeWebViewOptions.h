/**
 * @file Source/NativeWeb/Public/Data/Structs/NativeWebViewOptions.h
 * @brief Declares native web view options for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebViewOptions.generated.h"

/**
 * @brief Configuration consumed when the subsystem creates a NativeWeb view.
 * @details Inline HTML/CSS and packaged asset paths are resolved by the game-thread runtime.
 * @ingroup NativeWeb
 */
USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebViewOptions
{
	GENERATED_BODY()

	/** @brief Initial output surface dimensions in pixels; both axes must be positive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FIntPoint PixelSize = FIntPoint(480, 240);

	/** @brief Slate presentation scale for the view; defaults to one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	float DpiScale = 1.0f;

	/** @brief Initial surface clear color used before document content is rendered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FColor ClearColor = FColor(0, 0, 0, 255);

	/** @brief Whether creation should attach the view widget to the local-player viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	bool bCreateViewportOverlay = true;

	/** @brief Z-order used when the initial viewport overlay is attached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	int32 ViewportZOrder = 0;

	/** @brief Inline HTML source used by CreateView; can be empty for later document loading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString Html;

	/** @brief Inline stylesheet source paired with Html. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString Css;

	/** @brief Optional project-Content-relative root used to resolve document assets and fonts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString PackagedAssetRoot;
};
