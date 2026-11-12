#pragma once

#include "CoreMinimal.h"
#include "Data/Enums/NativeWebInputKey.h"
#include "Data/Enums/NativeWebInputType.h"
#include "NativeWebInputEvent.generated.h"

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebInputEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	ENativeWebInputType Type = ENativeWebInputType::INPUT_POINTER_MOVE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FVector2D NormalizedPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	ENativeWebInputKey Key = ENativeWebInputKey::INPUT_KEY_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	float WheelDelta = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	bool bShift = false;
};
