/**
 * @file Source/NativeWeb/Public/Data/Structs/NativeWebInputEvent.h
 * @brief Declares native web input event for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/Enums/NativeWebInputKey.h"
#include "Data/Enums/NativeWebInputType.h"
#include "NativeWebInputEvent.generated.h"

/**
 * @brief Input event forwarded to the embedded NativeWeb surface.
 * @details NormalizedPosition is in normalized view coordinates (0..1 across the displayed view); fields unrelated to Type are ignored. This is a deliberately small input contract, not a full Unreal key mapping.
 * @ingroup NativeWeb
 */
USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebInputEvent
{
	GENERATED_BODY()

	/** @brief Reflected data field `Type`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	ENativeWebInputType Type = ENativeWebInputType::INPUT_POINTER_MOVE;

	/** @brief Reflected data field `NormalizedPosition`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FVector2D NormalizedPosition = FVector2D::ZeroVector;

	/** @brief Reflected data field `Key`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	ENativeWebInputKey Key = ENativeWebInputKey::INPUT_KEY_NONE;

	/** @brief Reflected data field `WheelDelta`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	float WheelDelta = 0.0f;

	/** @brief Reflected data field `bShift`. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	bool bShift = false;
};
