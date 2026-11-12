#pragma once

#include "CoreMinimal.h"
#include "NativeWebError.generated.h"

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebError
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FName Code = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString Message;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString SourceName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString ScriptStack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FName ViewName = NAME_None;
};
