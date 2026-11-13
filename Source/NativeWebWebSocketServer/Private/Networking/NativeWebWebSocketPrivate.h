/**
 * @file NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketPrivate.h
 * @brief Declares NativeWebWebSocketPrivate for module NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketPrivate.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

class FNativeWebWebSocketNetwork;
class FNativeWebWebSocketServerBackend;

typedef struct lws_context NativeWebWebSocketInternalContext;
typedef struct lws NativeWebWebSocketInternal;
typedef struct lws_protocols NativeWebWebSocketInternalProtocol;

