/**
 * @file NativeWebWebSocketServer\Public\Pipeline\INativeWebWebSocketPacketCodec.h
 * @brief Declares the interface for transforming WebSocket packets at the pipeline boundary.
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "Pipeline/NativeWebWebSocketPipelineTypes.h"

/** @brief Encodes and decodes packets for a configured payload format. */
class NATIVEWEBWEBSOCKETSERVER_API INativeWebWebSocketPacketCodec
{
public:
	virtual ~INativeWebWebSocketPacketCodec() = default;

	/** @brief Decodes an inbound packet. @param InPacket Received packet. @param OutDecodedPacket Decoded packet. @param OutError Receives a failure description. @return true when decoding succeeds. */
	virtual bool DecodeInbound(const FNativeWebWebSocketInboundPacket& InPacket, FNativeWebWebSocketInboundPacket& OutDecodedPacket, FString& OutError) = 0;
	/** @brief Encodes an outgoing send request into client write packets. @param InRequest Outgoing request. @param OutWritePackets Receives encoded packets. @param OutError Receives a failure description. @return true when encoding succeeds. */
	virtual bool EncodeSendRequest(const FNativeWebWebSocketSendRequest& InRequest, TArray<FNativeWebWebSocketWritePacket>& OutWritePackets, FString& OutError) = 0;
	/** @brief Returns the payload format handled by this codec. */
	virtual ENativeWebWebSocketPayloadFormat GetPayloadFormat() const = 0;
};

