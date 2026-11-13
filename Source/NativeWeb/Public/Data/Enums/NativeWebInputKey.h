/**
 * @file Source/NativeWeb/Public/Data/Enums/NativeWebInputKey.h
 * @brief Declares native web input key for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebInputKey.generated.h"

/**
 * @brief Keyboard keys currently forwarded by the NativeWeb view input bridge.
 * @details This intentionally small enum is not a complete Unreal key-code mapping.
 * @ingroup NativeWeb
 */
UENUM(BlueprintType)
enum class ENativeWebInputKey : uint8
{
	/** No key is associated with the input event. */
	INPUT_KEY_NONE = 0 UMETA(DisplayName="None"),
	/** Tab key. */
	INPUT_KEY_TAB = 1 UMETA(DisplayName="Tab"),
	/** Enter/Return key. */
	INPUT_KEY_ENTER = 2 UMETA(DisplayName="Enter"),
	/** Space bar. */
	INPUT_KEY_SPACE = 3 UMETA(DisplayName="Space"),
};
