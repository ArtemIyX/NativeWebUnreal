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
	/**
	 * @brief Decodes inbound.
	* @param InPacket Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutDecodedPacket Output value populated by this operation when applicable.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DecodeInbound(InPacket, OutDecodedPacket, OutError);
	 * @endcode
	 */

	virtual bool DecodeInbound(const FNativeWebWebSocketInboundPacket& InPacket,
	                           FNativeWebWebSocketInboundPacket& OutDecodedPacket, FString& OutError) override;
	/**
	 * @brief Encodes send request.
	* @param InRequest Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutWritePackets Output value populated by this operation when applicable.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * EncodeSendRequest(InRequest, OutWritePackets, OutError);
	 * @endcode
	 */

	virtual bool EncodeSendRequest(const FNativeWebWebSocketSendRequest& InRequest,
	                               TArray<FNativeWebWebSocketWritePacket>& OutWritePackets, FString& OutError) override;
	/**
	 * @brief Returns payload format.
	 * @return An enumerated status describing the outcome.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetPayloadFormat();
	 * @endcode
	 */

	virtual ENativeWebWebSocketPayloadFormat GetPayloadFormat() const override
	{
		return ENativeWebWebSocketPayloadFormat::Binary;
	}
};

/** @brief Codec that converts payloads to and from UTF-8 text. */
class NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketUtf8StringCodec : public INativeWebWebSocketPacketCodec
{
public:
	explicit FNativeWebWebSocketUtf8StringCodec(
		ENativeWebWebSocketPayloadFormat InPayloadFormat = ENativeWebWebSocketPayloadFormat::Utf8String)
		: PayloadFormat(InPayloadFormat)
	{
	}

	/**
	 * @brief Decodes inbound.
	* @param InPacket Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutDecodedPacket Output value populated by this operation when applicable.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DecodeInbound(InPacket, OutDecodedPacket, OutError);
	 * @endcode
	 */

	virtual bool DecodeInbound(const FNativeWebWebSocketInboundPacket& InPacket,
	                           FNativeWebWebSocketInboundPacket& OutDecodedPacket, FString& OutError) override;
	/**
	 * @brief Encodes send request.
	* @param InRequest Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutWritePackets Output value populated by this operation when applicable.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * EncodeSendRequest(InRequest, OutWritePackets, OutError);
	 * @endcode
	 */

	virtual bool EncodeSendRequest(const FNativeWebWebSocketSendRequest& InRequest,
	                               TArray<FNativeWebWebSocketWritePacket>& OutWritePackets, FString& OutError) override;
	/**
	 * @brief Returns payload format.
	 * @return An enumerated status describing the outcome.
	 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetPayloadFormat();
	 * @endcode
	 */

	virtual ENativeWebWebSocketPayloadFormat GetPayloadFormat() const override { return PayloadFormat; }

private:
	/** Payload format reported by this codec. */
	ENativeWebWebSocketPayloadFormat PayloadFormat = ENativeWebWebSocketPayloadFormat::Utf8String;
};
