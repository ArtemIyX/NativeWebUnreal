#pragma once

#include "CoreMinimal.h"
#include "NativeWebPayloadType.generated.h"

UENUM(BlueprintType)
enum class ENativeWebPayloadType : uint8
{
	PAYLOAD_STRING = 0 UMETA(DisplayName="String"),
	PAYLOAD_BYTES = 1 UMETA(DisplayName="Bytes"),
};
