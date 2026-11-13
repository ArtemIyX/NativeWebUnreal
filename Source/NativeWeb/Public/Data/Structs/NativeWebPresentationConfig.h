/**
 * @file Source/NativeWeb/Public/Data/Structs/NativeWebPresentationConfig.h
 * @brief Declares NativeWeb presentation configuration.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/Enums/NativeWebPresentationMode.h"
#include "NativeWebPresentationConfig.generated.h"

/** @brief Configuration for the built-in NativeWeb presenter. */
USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebPresentationConfig
{
	GENERATED_BODY()

	/** @brief Presenter selected and attached when the view is created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	ENativeWebPresentationMode PresentationMode = ENativeWebPresentationMode::GpuViewport;

	/** @brief Whether GPU presenter failure may activate the Slate fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	bool bFallbackToSlate = true;

	/** @brief Slate-only z-order used when the selected presenter is Slate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	int32 ViewportZOrder = 0;
};
