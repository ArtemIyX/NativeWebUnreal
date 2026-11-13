/**
 * @file Source/NativeWeb/Public/NativeWebDelegates.h
 * @brief Declares native web delegates for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "Data/Structs/NativeWebError.h"
#include "Data/Structs/NativeWebMessage.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebMessageReceived, const FNativeWebMessage&);
DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebViewError, const FNativeWebError&);
