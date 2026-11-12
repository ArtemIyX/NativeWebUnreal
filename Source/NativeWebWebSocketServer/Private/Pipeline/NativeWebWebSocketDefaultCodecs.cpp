

#include "Pipeline/NativeWebWebSocketDefaultCodecs.h"
#include "Config/NativeWebWebSocketCVars.h"

bool FNativeWebWebSocketBinaryPassthroughCodec::DecodeInbound(const FNativeWebWebSocketInboundPacket& InPacket,
                                                        FNativeWebWebSocketInboundPacket& OutDecodedPacket,
                                                        FString& OutError)
{
	OutError.Reset();
	OutDecodedPacket = InPacket;
	return true;
}

bool FNativeWebWebSocketBinaryPassthroughCodec::EncodeSendRequest(const FNativeWebWebSocketSendRequest& InRequest,
                                                            TArray<FNativeWebWebSocketWritePacket>& OutWritePackets,
                                                            FString& OutError)
{
	OutError.Reset();
	OutWritePackets.Reset();
	if (InRequest.BytesPayload.Num() == 0)
	{
		OutError = TEXT("Empty binary payload");
		return false;
	}

	if (InRequest.bBroadcast)
	{
		for (const int64 clientId : InRequest.TargetClientIds)
		{
			if (clientId == InRequest.ExcludedClientId)
			{
				continue;
			}

			FNativeWebWebSocketWritePacket& writePacket = OutWritePackets.AddDefaulted_GetRef();
			writePacket.ClientId = clientId;
			writePacket.Payload = InRequest.BytesPayload;
			writePacket.bBinary = true;
		}
		return OutWritePackets.Num() > 0;
	}

	if (InRequest.TargetClientIds.Num() <= 0)
	{
		OutError = TEXT("No target clients for binary payload");
		return false;
	}

	for (const int64 clientId : InRequest.TargetClientIds)
	{
		FNativeWebWebSocketWritePacket& writePacket = OutWritePackets.AddDefaulted_GetRef();
		writePacket.ClientId = clientId;
		writePacket.Payload = InRequest.BytesPayload;
		writePacket.bBinary = true;
	}
	return true;
}

bool FNativeWebWebSocketUtf8StringCodec::DecodeInbound(const FNativeWebWebSocketInboundPacket& InPacket,
                                                 FNativeWebWebSocketInboundPacket& OutDecodedPacket,
                                                 FString& OutError)
{
	OutError.Reset();
	OutDecodedPacket = InPacket;
	if (InPacket.bBinary)
	{
		return true;
	}
	if (!NativeWebWebSocketCVars::GetValidateUtf8())
	{
		return true;
	}

	if (InPacket.Payload.Num() <= 0)
	{
		OutError = TEXT("Empty text payload");
		return false;
	}



	FUTF8ToTCHAR converter(reinterpret_cast<const ANSICHAR*>(InPacket.Payload.GetData()), InPacket.Payload.Num());
	if (converter.Length() <= 0)
	{
		OutError = TEXT("Invalid UTF-8 payload");
		return false;
	}

	return true;
}

bool FNativeWebWebSocketUtf8StringCodec::EncodeSendRequest(const FNativeWebWebSocketSendRequest& InRequest,
                                                     TArray<FNativeWebWebSocketWritePacket>& OutWritePackets,
                                                     FString& OutError)
{
	OutError.Reset();
	OutWritePackets.Reset();
	if (InRequest.TextPayload.IsEmpty())
	{
		OutError = TEXT("Empty text payload");
		return false;
	}



	FTCHARToUTF8 utf8(*InRequest.TextPayload);
	if (utf8.Length() <= 0)
	{
		OutError = TEXT("Invalid UTF-8 source payload");
		return false;
	}

	TArray<uint8> encoded;
	encoded.Append(reinterpret_cast<const uint8*>(utf8.Get()), utf8.Length());

	if (InRequest.bBroadcast)
	{
		for (const int64 clientId : InRequest.TargetClientIds)
		{
			if (clientId == InRequest.ExcludedClientId)
			{
				continue;
			}
			FNativeWebWebSocketWritePacket& writePacket = OutWritePackets.AddDefaulted_GetRef();
			writePacket.ClientId = clientId;
			writePacket.Payload = encoded;
			writePacket.bBinary = false;
		}
		return OutWritePackets.Num() > 0;
	}

	if (InRequest.TargetClientIds.Num() <= 0)
	{
		OutError = TEXT("No target clients for text payload");
		return false;
	}

	for (const int64 clientId : InRequest.TargetClientIds)
	{
		FNativeWebWebSocketWritePacket& writePacket = OutWritePackets.AddDefaulted_GetRef();
		writePacket.ClientId = clientId;
		writePacket.Payload = encoded;
		writePacket.bBinary = false;
	}
	return true;
}
