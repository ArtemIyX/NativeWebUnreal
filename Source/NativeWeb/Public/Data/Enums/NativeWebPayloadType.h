/**
 * @file Source/NativeWeb/Public/Data/Enums/NativeWebPayloadType.h
 * @brief Declares native web payload type for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebPayloadType.generated.h"

/**
 * @brief Payload representation used by @ref FNativeWebMessage.
 * @details Values remain opaque to the bridge consumer beyond this string-versus-byte distinction.
 * @ingroup NativeWeb
 */
UENUM(BlueprintType)
enum class ENativeWebPayloadType : uint8
{
	/** Payload is stored in the message's String field. */
	PAYLOAD_STRING = 0 UMETA(DisplayName="String"),
	/** Payload is stored in the message's Bytes field without text decoding. */
	PAYLOAD_BYTES = 1 UMETA(DisplayName="Bytes"),
};
