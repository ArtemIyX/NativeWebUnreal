#pragma once

#include "Data/Structs/NativeWebError.h"
#include "Data/Structs/NativeWebMessage.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebMessageReceived, const FNativeWebMessage&);
DECLARE_MULTICAST_DELEGATE_OneParam(FNativeWebViewError, const FNativeWebError&);
