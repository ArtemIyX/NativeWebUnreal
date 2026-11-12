/**
 * @file NativeWebWebSocketServer\Public\Pipeline\NativeWebWebSocketDefaultCodecs.h
 * @brief Declares the built-in WebSocket payload codecs.
 */
#pragma once

#include "CoreMinimal.h"
#include "Pipeline/INativeWebWebSocketPacketCodec.h"

/** @brief Codec that forwards binary payloads without transformation. */
class NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketBinaryPassthroughCodec : public INativeWebWebSocketPacketCodec
{
public:
	virtual bool DecodeInbound(const FNativeWebWebSocketInboundPacket& InPacket, FNativeWebWebSocketInboundPacket& OutDecodedPacket, FString& OutError) override;
	virtual bool EncodeSendRequest(const FNativeWebWebSocketSendRequest& InRequest, TArray<FNativeWebWebSocketWritePacket>& OutWritePackets, FString& OutError) override;
	virtual ENativeWebWebSocketPayloadFormat GetPayloadFormat() const override { return ENativeWebWebSocketPayloadFormat::Binary; }
};

/** @brief Codec that converts payloads to and from UTF-8 text. */
class NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketUtf8StringCodec : public INativeWebWebSocketPacketCodec
{
public:
	explicit FNativeWebWebSocketUtf8StringCodec(ENativeWebWebSocketPayloadFormat InPayloadFormat = ENativeWebWebSocketPayloadFormat::Utf8String)
		: PayloadFormat(InPayloadFormat)
	{
	}

	virtual bool DecodeInbound(const FNativeWebWebSocketInboundPacket& InPacket, FNativeWebWebSocketInboundPacket& OutDecodedPacket, FString& OutError) override;
	virtual bool EncodeSendRequest(const FNativeWebWebSocketSendRequest& InRequest, TArray<FNativeWebWebSocketWritePacket>& OutWritePackets, FString& OutError) override;
	virtual ENativeWebWebSocketPayloadFormat GetPayloadFormat() const override { return PayloadFormat; }

private:
	/** Payload format reported by this codec. */
	ENativeWebWebSocketPayloadFormat PayloadFormat = ENativeWebWebSocketPayloadFormat::Utf8String;
};

