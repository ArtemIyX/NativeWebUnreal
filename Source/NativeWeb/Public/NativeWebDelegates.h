/**
 * @file Source/NativeWeb/Public/NativeWebDelegates.h
 * @brief Declares native web delegates for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "Data/Structs/NativeWebError.h"
#include "Data/Structs/NativeWebMessage.h"
#include "Data/Enums/NativeWebPresentationMode.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebMessageReceived, const FNativeWebMessage&);
DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebViewError, const FNativeWebError&);
/** @brief Broadcast on the game thread when a requested presenter switch completes or fails. */
DECLARE_MULTICAST_DELEGATE_FourParams(FNativeWebPresentationChanged, ENativeWebPresentationMode, ENativeWebPresentationMode, bool, const FString&);
