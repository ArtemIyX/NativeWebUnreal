/**
 * @file Source/NativeWeb/Public/Data/Structs/NativeWebError.h
 * @brief Declares native web error for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "NativeWebError.generated.h"

/**
 * @brief Structured error details reported by NativeWeb view creation and runtime operations.
 * @details Fields may be empty when a particular failure source does not provide that detail.
 * @ingroup NativeWeb
 */
USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebError
{
	GENERATED_BODY()

	/** @brief Stable error category identifier where one is available. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FName Code = NAME_None;

	/** @brief Human-readable diagnostic message. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString Message;

	/** @brief Script or content source associated with the error. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString SourceName;

	/** @brief JavaScript stack trace when QuickJS supplied one. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FString ScriptStack;

	/** @brief Name of the view that reported the error. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NativeWeb")
	FName ViewName = NAME_None;
};
