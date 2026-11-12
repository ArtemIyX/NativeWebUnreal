/**
 * @file NativeWebWebSocketServer\Public\Data\NativeWebWebSocketDelegates.h
 * @brief Declares NativeWebWebSocketDelegates for module NativeWebWebSocketServer\Public\Data\NativeWebWebSocketDelegates.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "Data/NativeWebWebSocketStructs.h"
#include "NativeWebWebSocketDelegates.generated.h"

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNativeWebWebSocketClientConnectedDynamic, 
	const FNativeWebWebSocketClientInfo&, Client);

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNativeWebWebSocketClientDisconnectedDynamic, 
	int64, ClientId, ENativeWebWebSocketCloseReason, Reason);

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNativeWebWebSocketServerErrorDynamic, 
	FName, ServerName, ENativeWebWebSocketErrorCode, ErrorCode, const FString&, Message);

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNativeWebWebSocketClientErrorDynamic,
	int64, ClientId, ENativeWebWebSocketErrorCode, ErrorCode, const FString&, Message);