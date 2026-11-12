#pragma once

#include "CoreMinimal.h"
#include "NativeWebInputKey.generated.h"

UENUM(BlueprintType)
enum class ENativeWebInputKey : uint8
{
	INPUT_KEY_NONE = 0 UMETA(DisplayName="None"),
	INPUT_KEY_TAB = 1 UMETA(DisplayName="Tab"),
	INPUT_KEY_ENTER = 2 UMETA(DisplayName="Enter"),
	INPUT_KEY_SPACE = 3 UMETA(DisplayName="Space"),
};
