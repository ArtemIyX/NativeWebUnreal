/**
 * @file NativeWebContentHttpServer/Public/Libs/NativeWebContentHttpServerBPLibrary.h
 * @brief Blueprint helper accessors for NativeWebContentHttpServer.
 */
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NativeWebContentHttpServerBPLibrary.generated.h"

/**
 * @brief Declares the u native web content http server subsystem type and its operations.
 * @ingroup NativeWebContentHttpServer
 */
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
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance content HTTP and image request services.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
GetNativeWebContentHttpServerSubsystem(InWorldContextObject);
@endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebContentHttpServer", meta = (WorldContext = "InWorldContextObject"))
	static UNativeWebContentHttpServerSubsystem* GetNativeWebContentHttpServerSubsystem(const UObject* InWorldContextObject);
};
