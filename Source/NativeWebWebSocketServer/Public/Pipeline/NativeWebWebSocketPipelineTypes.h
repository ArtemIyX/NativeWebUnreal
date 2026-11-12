/**
 * @file NativeWebWebSocketServer\Public\Pipeline\NativeWebWebSocketPipelineTypes.h
 * @brief Defines packet structures exchanged by the WebSocket pipeline.
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/NativeWebWebSocketEnums.h"

/** @brief A packet received from a connected client. */
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketInboundPacket
{
	/** Identifier of the client that sent the packet. */
	int64 ClientId = 0;
	/** Packet payload bytes. */
	TArray<uint8> Payload;
	/** Whether the original packet used the binary wire format. */
	bool bBinary = true;
	/** UTC time at which the packet was received. */
	FDateTime ReceivedAtUtc = FDateTime::UtcNow();
};

/** @brief A request to encode and send data to one or more clients. */
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketSendRequest
{
	/** Explicit destination client identifiers. */
	TArray<int64> TargetClientIds;
	/** Whether the request targets all clients. */
	bool bBroadcast = false;
	/** Client excluded from broadcast delivery. */
	int64 ExcludedClientId = 0;
	/** Encoding to use for the outgoing payload. */
	ENativeWebWebSocketPayloadFormat PayloadFormat = ENativeWebWebSocketPayloadFormat::Binary;
	/** Binary payload, used for binary-compatible formats. */
	TArray<uint8> BytesPayload;
	/** Text payload, used for string-compatible formats. */
	FString TextPayload;
};

/** @brief A payload ready for writing to a specific client connection. */
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketWritePacket
{
	/** Destination client identifier. */
	int64 ClientId = 0;
	/** Encoded payload bytes. */
	TArray<uint8> Payload;
	/** Whether the payload must be written as binary data. */
	bool bBinary = true;
};

