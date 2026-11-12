#pragma once

#include "CoreMinimal.h"
#include "NativeWebInputType.generated.h"

UENUM(BlueprintType)
enum class ENativeWebInputType : uint8
{
	INPUT_POINTER_MOVE = 0 UMETA(DisplayName="Pointer Move"),
	INPUT_POINTER_DOWN = 1 UMETA(DisplayName="Pointer Down"),
	INPUT_POINTER_UP = 2 UMETA(DisplayName="Pointer Up"),
	INPUT_WHEEL = 3 UMETA(DisplayName="Wheel"),
	INPUT_KEY_DOWN = 4 UMETA(DisplayName="Key Down"),
	INPUT_KEY_UP = 5 UMETA(DisplayName="Key Up"),
	INPUT_FOCUS_GAINED = 6 UMETA(DisplayName="Focus Gained"),
	INPUT_FOCUS_LOST = 7 UMETA(DisplayName="Focus Lost"),
};
