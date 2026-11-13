/**
 * @file Source/NativeWeb/Public/Data/Enums/NativeWebInputType.h
 * @brief Declares native web input type for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebInputType.generated.h"

/**
 * @brief Event kinds accepted by @ref UNativeWebView::ForwardInput.
 * @details Pointer coordinates are expressed in view pixels; focus events carry no coordinate payload.
 * @ingroup NativeWeb
 */
UENUM(BlueprintType)
enum class ENativeWebInputType : uint8
{
	/** Pointer moved to the supplied view-space position. */
	INPUT_POINTER_MOVE = 0 UMETA(DisplayName="Pointer Move"),
	/** Pointer button was pressed at the supplied position. */
	INPUT_POINTER_DOWN = 1 UMETA(DisplayName="Pointer Down"),
	/** Pointer button was released at the supplied position. */
	INPUT_POINTER_UP = 2 UMETA(DisplayName="Pointer Up"),
	/** Mouse wheel delta was supplied. */
	INPUT_WHEEL = 3 UMETA(DisplayName="Wheel"),
	/** A supported key was pressed. */
	INPUT_KEY_DOWN = 4 UMETA(DisplayName="Key Down"),
	/** A supported key was released. */
	INPUT_KEY_UP = 5 UMETA(DisplayName="Key Up"),
	/** The host control gained focus. */
	INPUT_FOCUS_GAINED = 6 UMETA(DisplayName="Focus Gained"),
	/** The host control lost focus; held input should be cleared. */
	INPUT_FOCUS_LOST = 7 UMETA(DisplayName="Focus Lost"),
};
