/**
 * @file Source/NativeWeb/Public/Data/Structs/NativeWebSurfaceConfig.h
 * @brief Declares NativeWeb surface configuration.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebSurfaceConfig.generated.h"

/** @brief Configuration for the AffineUI output surface. */
USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebSurfaceConfig
{
	GENERATED_BODY()

	/** @brief Initial output surface dimensions in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FIntPoint PixelSize = FIntPoint(480, 240);

	/** @brief Device/UI scale used by the AffineUI surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	float DpiScale = 1.0f;

	/** @brief Opaque clear color used before document content is rendered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FColor ClearColor = FColor(0, 0, 0, 255);
};
