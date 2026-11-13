/**
 * @file NativeWebWebSocketServer\Private\Stats\NativeWebWebSocketStats.h
 * @brief Declares NativeWebWebSocketStats for module NativeWebWebSocketServer\Private\Stats\NativeWebWebSocketStats.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"

DECLARE_STATS_GROUP(TEXT("NativeWebWebSocket"), STATGROUP_NativeWebWebSocket, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Clients"), STAT_NativeWebWebSocket_ActiveClients, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("RX Bytes Per Tick"), STAT_NativeWebWebSocket_RxBytes, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("TX Bytes Per Tick"), STAT_NativeWebWebSocket_TxBytes, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dropped Messages"), STAT_NativeWebWebSocket_DroppedMessages, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("Queue Depth"), STAT_NativeWebWebSocket_QueueDepth, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("Inbound Queue Depth"), STAT_NativeWebWebSocket_InboundQueueDepth, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("Send Queue Depth"), STAT_NativeWebWebSocket_SendQueueDepth, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("Write Queue Depth"), STAT_NativeWebWebSocket_WriteQueueDepth, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("Game Thread Event Queue Depth"), STAT_NativeWebWebSocket_GameThreadEventQueueDepth, STATGROUP_NativeWebWebSocket);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dropped Game Thread Events"), STAT_NativeWebWebSocket_DroppedGameThreadEvents, STATGROUP_NativeWebWebSocket);
DECLARE_CYCLE_STAT(TEXT("Read Pump"), STAT_NativeWebWebSocket_ReadPump, STATGROUP_NativeWebWebSocket);
DECLARE_CYCLE_STAT(TEXT("Handle Pump"), STAT_NativeWebWebSocket_HandlePump, STATGROUP_NativeWebWebSocket);
DECLARE_CYCLE_STAT(TEXT("Send Pump"), STAT_NativeWebWebSocket_SendPump, STATGROUP_NativeWebWebSocket);
DECLARE_CYCLE_STAT(TEXT("Write Pump"), STAT_NativeWebWebSocket_WritePump, STATGROUP_NativeWebWebSocket);

