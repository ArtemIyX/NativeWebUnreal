/**
 * @file Source/NativeWeb/Public/Data/Enums/NativeWebQueueResult.h
 * @brief Declares native web queue result for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebQueueResult.generated.h"

/**
 * @brief Result of attempting to enqueue a message for later delivery.
 * @details Queue operations copy or move payload ownership and do not invoke JavaScript synchronously.
 * @ingroup NativeWeb
 */
UENUM(BlueprintType)
enum class ENativeWebQueueResult : uint8
{
	/** The payload was accepted for asynchronous delivery. */
	RESULT_QUEUED = 0 UMETA(DisplayName="Queued"),
	/** The view has closed and will not accept more messages. */
	RESULT_CLOSED = 1 UMETA(DisplayName="Closed"),
	/** The bounded queue has no capacity for this payload. */
	RESULT_QUEUE_FULL = 2 UMETA(DisplayName="Queue Full"),
};
