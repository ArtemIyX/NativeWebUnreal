/**
 * @file NativeWebWebSocketServer\\Private\\Server\\NativeWebWebSocketGameThreadEvent.h
 * @brief Defines owned event payloads transferred to the WebSocket game-thread dispatcher.
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "Data/NativeWebWebSocketStructs.h"
#include "Misc/TVariant.h"

/** @brief Payload for a newly connected client. */
struct FNativeWebWebSocketClientConnectedEvent
{
	FNativeWebWebSocketClientInfo ClientInfo;
};

/** @brief Payload for a disconnected client. */
struct FNativeWebWebSocketClientDisconnectedEvent
{
	int64 ClientId = 0;
	ENativeWebWebSocketCloseReason Reason = ENativeWebWebSocketCloseReason::ClientClosed;
};

/** @brief Payload for a server-level error. */
struct FNativeWebWebSocketServerErrorEvent
{
	ENativeWebWebSocketErrorCode ErrorCode = ENativeWebWebSocketErrorCode::None;
	FString Message;
};

/** @brief Payload for a client-level error. */
struct FNativeWebWebSocketClientErrorEvent
{
	int64 ClientId = 0;
	ENativeWebWebSocketErrorCode ErrorCode = ENativeWebWebSocketErrorCode::None;
	FString Message;
};

/** @brief Payload for bytes received from a client. */
struct FNativeWebWebSocketClientMessageEvent
{
	int64 ClientId = 0;
	TArray<uint8> Payload;
	bool bBinary = false;
};

/** @brief Variant containing one owned WebSocket game-thread event payload. */
using FNativeWebWebSocketGameThreadEventPayload = TVariant<FNativeWebWebSocketClientConnectedEvent, FNativeWebWebSocketClientDisconnectedEvent,
	FNativeWebWebSocketServerErrorEvent, FNativeWebWebSocketClientErrorEvent, FNativeWebWebSocketClientMessageEvent>;

/** @brief A server-generation-stamped event queued for game-thread dispatch. */
struct FNativeWebWebSocketGameThreadEvent
{
	uint64 Generation = 0;
	FNativeWebWebSocketGameThreadEventPayload Payload;
	FNativeWebWebSocketGameThreadEvent() = default;

	FNativeWebWebSocketGameThreadEvent(uint64 InGeneration, FNativeWebWebSocketClientConnectedEvent&& InPayload)
		: Generation(InGeneration)
	{
		Payload.Set<FNativeWebWebSocketClientConnectedEvent>(MoveTemp(InPayload));
	}

	FNativeWebWebSocketGameThreadEvent(uint64 InGeneration, FNativeWebWebSocketClientDisconnectedEvent&& InPayload)
		: Generation(InGeneration)
	{
		Payload.Set<FNativeWebWebSocketClientDisconnectedEvent>(MoveTemp(InPayload));
	}

	FNativeWebWebSocketGameThreadEvent(uint64 InGeneration, FNativeWebWebSocketServerErrorEvent&& InPayload)
		: Generation(InGeneration)
	{
		Payload.Set<FNativeWebWebSocketServerErrorEvent>(MoveTemp(InPayload));
	}

	FNativeWebWebSocketGameThreadEvent(uint64 InGeneration, FNativeWebWebSocketClientErrorEvent&& InPayload)
		: Generation(InGeneration)
	{
		Payload.Set<FNativeWebWebSocketClientErrorEvent>(MoveTemp(InPayload));
	}

	FNativeWebWebSocketGameThreadEvent(uint64 InGeneration, FNativeWebWebSocketClientMessageEvent&& InPayload)
		: Generation(InGeneration)
	{
		Payload.Set<FNativeWebWebSocketClientMessageEvent>(MoveTemp(InPayload));
	}
};

/** @brief Owns the cross-thread event queue and its atomic lifecycle counters. */
class FNativeWebWebSocketGameThreadEventState
{
public:
	/** @brief MPSC queue written by runtime threads and read by the game thread. */
	TQueue<FNativeWebWebSocketGameThreadEvent, EQueueMode::Mpsc> Events;
	/** @brief Queue depth including an in-flight producer reservation. */
	TAtomic<int32> EventCount{0};
	/** @brief Producer gate closed during stop and failed startup. */
	TAtomic<bool> bAcceptEvents{false};
	/** @brief Monotonic server-run identifier. */
	TAtomic<uint64> Generation{0};
	/** @brief Total rejected game-thread events. */
	TAtomic<int64> DroppedEvents{0};
	/** @brief Rejected message events included in DroppedMessages. */
	TAtomic<int64> DroppedMessages{0};
	/** @brief Requests game-thread shutdown after a control event overflow. */
	TAtomic<bool> bControlOverflowStopRequested{false};
};
