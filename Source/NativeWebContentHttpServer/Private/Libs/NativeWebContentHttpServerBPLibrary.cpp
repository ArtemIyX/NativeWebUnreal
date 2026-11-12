
#include "Libs/NativeWebContentHttpServerBPLibrary.h"

#include "NativeWebContentHttpServer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Subsystems/NativeWebContentHttpServerSubsystem.h"

UNativeWebContentHttpServerSubsystem* UNativeWebContentHttpServerBPLibrary::GetNativeWebContentHttpServerSubsystem(const UObject* InWorldContextObject)
{
	if (!IsValid(InWorldContextObject))
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("GetNativeWebContentHttpServerSubsystem failed: world context object is invalid"));
		return nullptr;
	}

	UWorld* const world = InWorldContextObject->GetWorld();
	if (!world)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("GetNativeWebContentHttpServerSubsystem failed: world is null"));
		return nullptr;
	}

	UGameInstance* const gameInstance = world->GetGameInstance();
	if (!gameInstance)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("GetNativeWebContentHttpServerSubsystem failed: game instance is null"));
		return nullptr;
	}

	return gameInstance->GetSubsystem<UNativeWebContentHttpServerSubsystem>();
}
