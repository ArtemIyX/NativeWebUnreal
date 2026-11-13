/**
 * @file Source/NativeWeb/Public/Data/Structs/NativeWebMessage.h
 * @brief Declares native web message for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/Enums/NativeWebPayloadType.h"
#include "NativeWebMessage.generated.h"

/**
 * @brief Owned opaque message delivered across the JavaScript/Unreal bridge.
 * @details Exactly one payload field is selected by Type. Byte payloads are copied into this value.
 * @ingroup NativeWeb
 */
USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebMessage
{
	GENERATED_BODY()

public:
	/** @brief Optional routing label; NativeWeb does not interpret the channel schema. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FName Channel = NAME_None;

	/** @brief Selects whether String or Bytes contains the payload. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	ENativeWebPayloadType Type = ENativeWebPayloadType::PAYLOAD_STRING;

	/** @brief String payload when Type is PAYLOAD_STRING. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString String;

	/** @brief Opaque copied byte payload when Type is PAYLOAD_BYTES. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	TArray<uint8> Bytes;
};
