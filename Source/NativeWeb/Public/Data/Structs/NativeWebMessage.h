#pragma once

#include "CoreMinimal.h"
#include "Data/Enums/NativeWebPayloadType.h"
#include "NativeWebMessage.generated.h"

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebMessage
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FName Channel = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	ENativeWebPayloadType Type = ENativeWebPayloadType::PAYLOAD_STRING;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString String;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	TArray<uint8> Bytes;
};
