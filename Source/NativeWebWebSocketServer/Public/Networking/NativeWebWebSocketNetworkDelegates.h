/**
 * @file NativeWebWebSocketServer\Public\Networking\NativeWebWebSocketNetworkDelegates.h
 * @brief Declares NativeWebWebSocketNetworkDelegates for module NativeWebWebSocketServer\Public\Networking\NativeWebWebSocketNetworkDelegates.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "Delegates/Delegate.h"

DECLARE_DELEGATE(FNativeWebWebSocketNetworkInfoCallback)
DECLARE_DELEGATE_ThreeParams(FNativeWebWebSocketNetworkPacketReceivedCallback, void*, int32, bool)
DECLARE_DELEGATE_OneParam(FNativeWebWebSocketNetworkClientConnectedCallback, class INativeWebWebSocketNetwork*)
DECLARE_DELEGATE_OneParam(FNativeWebWebSocketNetworkClientDisconnectedCallback, class INativeWebWebSocketNetwork*)

