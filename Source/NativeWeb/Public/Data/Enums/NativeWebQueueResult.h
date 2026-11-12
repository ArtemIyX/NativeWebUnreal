#pragma once

#include "CoreMinimal.h"
#include "NativeWebQueueResult.generated.h"

UENUM(BlueprintType)
enum class ENativeWebQueueResult : uint8
{
	RESULT_QUEUED = 0 UMETA(DisplayName="Queued"),
	RESULT_CLOSED = 1 UMETA(DisplayName="Closed"),
	RESULT_QUEUE_FULL = 2 UMETA(DisplayName="Queue Full"),
};
