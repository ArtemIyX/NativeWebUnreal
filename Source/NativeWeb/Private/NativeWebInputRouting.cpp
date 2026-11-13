#include "NativeWebInputRouting.h"

#include "Engine/Console.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

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
}
