/**
 * @file NativeWebContentHttpServer/Public/Libs/NativeWebContentHttpServerBPLibrary.h
 * @brief Blueprint helper accessors for NativeWebContentHttpServer.
 */
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NativeWebContentHttpServerBPLibrary.generated.h"

class UNativeWebContentHttpServerSubsystem;

/** @brief Blueprint helper library for subsystem lookup. */
UCLASS()
class NATIVEWEBCONTENTHTTPSERVER_API UNativeWebContentHttpServerBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Resolves game instance subsystem from world context object.
	 * @param InWorldContextObject Any UObject with world context.
	 * @return Subsystem instance or null.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer", meta = (WorldContext = "InWorldContextObject"))
	static UNativeWebContentHttpServerSubsystem* GetNativeWebContentHttpServerSubsystem(const UObject* InWorldContextObject);
};
