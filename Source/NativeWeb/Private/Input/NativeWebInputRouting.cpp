#include "NativeWebInputRouting.h"

#include "Engine/Console.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"

namespace NativeWebInputRouting
{
	bool IsConsoleActive(const ULocalPlayer* Player)
	{
		const UGameViewportClient* ViewportClient = Player ? Player->ViewportClient : nullptr;
		const UConsole* Console = ViewportClient ? ViewportClient->ViewportConsole.Get() : nullptr;
		return Console && ShouldSuppressInput(Console->ConsoleActive());
	}

	bool ShouldSuppressInput(const bool bConsoleActive)
	{
		return bConsoleActive;
	}

	bool RouteKeyToConsole(const ULocalPlayer* Player, const FKeyEvent& Event, const bool bPressed)
	{
		const UGameViewportClient* ViewportClient = Player ? Player->ViewportClient : nullptr;
		UConsole* Console = ViewportClient ? ViewportClient->ViewportConsole.Get() : nullptr;
		if (!Console || !ShouldSuppressInput(Console->ConsoleActive())) return false;

		const EInputEvent InputEvent = bPressed ? (Event.IsRepeat() ? IE_Repeat : IE_Pressed) : IE_Released;
		return Console->InputKey(Event.GetInputDeviceId(), Event.GetKey(), InputEvent, 1.0f,
		                         Event.GetKey().IsGamepadKey());
	}

	bool RouteCharacterToConsole(const ULocalPlayer* Player, const FCharacterEvent& Event)
	{
		const UGameViewportClient* ViewportClient = Player ? Player->ViewportClient : nullptr;
		UConsole* Console = ViewportClient ? ViewportClient->ViewportConsole.Get() : nullptr;
		if (!Console || !ShouldSuppressInput(Console->ConsoleActive())) return false;

		return Console->InputChar(Event.GetInputDeviceId(), FString::Chr(Event.GetCharacter()));
	}
}
