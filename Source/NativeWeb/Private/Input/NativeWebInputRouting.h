/**
 * @file Source/NativeWeb/Private/Input/NativeWebInputRouting.h
 * @brief Declares shared input-routing checks for NativeWeb presenters.
 */

#pragma once

class ULocalPlayer;
struct FCharacterEvent;
struct FKeyEvent;

namespace NativeWebInputRouting
{
	/** Returns whether the owning viewport's Unreal console currently owns input. */
	NATIVEWEB_API bool IsConsoleActive(const ULocalPlayer* Player);

	/** Routes a key event directly to the active owning viewport console. */
	NATIVEWEB_API bool RouteKeyToConsole(const ULocalPlayer* Player, const FKeyEvent& Event, bool bPressed);

	/** Routes a character event directly to the active owning viewport console. */
	NATIVEWEB_API bool RouteCharacterToConsole(const ULocalPlayer* Player, const FCharacterEvent& Event);

	/** Keeps the presenter policy independently testable from engine viewport state. */
	NATIVEWEB_API bool ShouldSuppressInput(bool bConsoleActive);
}
