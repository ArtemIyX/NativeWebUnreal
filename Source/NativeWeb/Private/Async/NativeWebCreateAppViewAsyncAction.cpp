#include "Async/NativeWebCreateAppViewAsyncAction.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"
#include "NativeWebView.h"

UNativeWebCreateAppViewAsyncAction* UNativeWebCreateAppViewAsyncAction::CreateAppViewAsync(
	UNativeWebLocalPlayerSubsystem* InSubsystem, TSoftObjectPtr<UNativeWebAppAsset> InApp, FName InName,
	FNativeWebSurfaceConfig InSurfaceConfig, FNativeWebPresentationConfig InPresentationConfig)
{
	UNativeWebCreateAppViewAsyncAction* Action = NewObject<UNativeWebCreateAppViewAsyncAction>();
	Action->Subsystem = InSubsystem;
	Action->App = MoveTemp(InApp);
	Action->Name = InName;
	Action->SurfaceConfig = MoveTemp(InSurfaceConfig);
	Action->PresentationConfig = MoveTemp(InPresentationConfig);
	if (IsValid(InSubsystem))
	{
		if (ULocalPlayer* Player = InSubsystem->GetLocalPlayer())
		{
			if (UGameInstance* GameInstance = Player->GetGameInstance()) Action->RegisterWithGameInstance(GameInstance);
		}
	}
	return Action;
}

void UNativeWebCreateAppViewAsyncAction::Activate()
{
	check(IsInGameThread());
	if (bActivated || bCompleted) return;
	bActivated = true;
	UNativeWebLocalPlayerSubsystem* ActiveSubsystem = Subsystem.Get();
	if (!IsValid(ActiveSubsystem))
	{
		FNativeWebError Error;
		Error.Code = TEXT("MissingSubsystem");
		Error.Message = TEXT("The requested NativeWeb local-player subsystem is unavailable.");
		Error.ViewName = Name;
		Complete(nullptr, Error);
		return;
	}
	TWeakObjectPtr<UNativeWebCreateAppViewAsyncAction> WeakThis(this);
	ActiveSubsystem->CreateAppViewAsync(App, Name, SurfaceConfig, PresentationConfig,
		FNativeWebCreateAppViewComplete::CreateLambda([WeakThis](UNativeWebView* View, const FNativeWebError& Error)
		{
			if (UNativeWebCreateAppViewAsyncAction* Action = WeakThis.Get()) Action->Complete(View, Error);
		}));
}

void UNativeWebCreateAppViewAsyncAction::Complete(UNativeWebView* View, const FNativeWebError& Error)
{
	if (bCompleted) return;
	bCompleted = true;
	if (View) Success.Broadcast(View, Error);
	else Failure.Broadcast(nullptr, Error);
	SetReadyToDestroy();
}
