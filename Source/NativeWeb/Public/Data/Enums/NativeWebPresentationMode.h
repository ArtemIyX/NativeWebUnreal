/**
 * @file Source/NativeWeb/Public/Data/Enums/NativeWebPresentationMode.h
 * @brief Declares built-in NativeWeb viewport presentation modes.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebPresentationMode.generated.h"

/** @brief Built-in presentation selected for a NativeWeb view. */
UENUM(BlueprintType)
enum class ENativeWebPresentationMode : uint8
{
	/** @brief Present the opaque output texture in the game viewport through the UE render graph. */
	GpuViewport UMETA(DisplayName="GPU Viewport"),
	/** @brief Present and route input through the legacy Slate image widget. */
	Slate UMETA(DisplayName="Slate"),
	/** @brief Attach no built-in presenter; callers may use GetTexture and ForwardInput. */
	None UMETA(DisplayName="None")
};
