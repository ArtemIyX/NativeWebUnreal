/**
 * @file Source/NativeWeb/Private/NativeWebInputRouting.h
 * @brief Declares shared input-routing checks for NativeWeb presenters.
 */

#pragma once

class ULocalPlayer;

namespace NativeWebInputRouting
{
	/** Returns whether the owning viewport's Unreal console currently owns input. */
	NATIVEWEB_API bool IsConsoleActive(const ULocalPlayer* Player);

	/** Keeps the presenter policy independently testable from engine viewport state. */
	NATIVEWEB_API bool ShouldSuppressInput(bool bConsoleActive);
}
