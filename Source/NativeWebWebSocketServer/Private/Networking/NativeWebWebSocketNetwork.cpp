#include "Networking/NativeWebWebSocketNetwork.h"

#include "Logging/NativeWebWebSocketLog.h"

#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI

FNativeWebWebSocketNetwork::FNativeWebWebSocketNetwork(NativeWebWebSocketInternalContext* InContext, NativeWebWebSocketInternal* InWsi)
	: Context(InContext)
	, Wsi(InWsi)
{
	const int32 sock = static_cast<int32>(lws_get_socket_fd(Wsi));
	struct sockaddr_in remoteAddr;
	FMemory::Memzero(&remoteAddr, sizeof(remoteAddr));
	socklen_t remoteAddrLen = sizeof(remoteAddr);
	getpeername(sock, reinterpret_cast<struct sockaddr*>(&remoteAddr), &remoteAddrLen);

	ANSICHAR ipBuffer[INET_ADDRSTRLEN];
	RemoteIp = ANSI_TO_TCHAR(inet_ntop(AF_INET, &remoteAddr.sin_addr, ipBuffer, INET_ADDRSTRLEN));
	RemotePort = static_cast<int32>(ntohs(remoteAddr.sin_port));
}

FNativeWebWebSocketNetwork::~FNativeWebWebSocketNetwork()
{
	FlushInternal();
	ReceivedCallback.Unbind();
}

void FNativeWebWebSocketNetwork::SetConnectedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack)
{
	ConnectedCallBack = InCallBack;
}

void FNativeWebWebSocketNetwork::SetConnectionErrorCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack)
{
	ErrorCallBack = InCallBack;
}

void FNativeWebWebSocketNetwork::SetReceiveCallBack(FNativeWebWebSocketNetworkPacketReceivedCallback InCallBack)
{
	ReceivedCallback = InCallBack;
}

void FNativeWebWebSocketNetwork::SetSocketClosedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack)
{
	SocketClosedCallback = InCallBack;
}

bool FNativeWebWebSocketNetwork::Send(const uint8* InData, uint32 InSize, bool bInPrependSize)
{
	if (!InData || InSize == 0)
	{
		return false;
	}

	TArray<uint8> buffer;
	buffer.AddDefaulted(LWS_PRE);
	buffer.Append(InData, InSize);

	{
		FScopeLock lock(&OutgoingLock);
		OutgoingBuffer.Add(MoveTemp(buffer));
	}

	if (Wsi)
	{
		lws_callback_on_writable(Wsi);
	}

	return true;
}

void FNativeWebWebSocketNetwork::Tick()
{
}

void FNativeWebWebSocketNetwork::Flush()
{
	FlushInternal();
}

void FNativeWebWebSocketNetwork::FlushInternal()
{
	if (!Wsi)
	{
		return;
	}

	for (;;)
	{
		int32 messageCount = 0;
		{
			FScopeLock lock(&OutgoingLock);
			messageCount = OutgoingBuffer.Num();
		}
		if (messageCount == 0)
		{
			break;
		}
		lws_callback_on_writable(Wsi);
		break;
	}
}

void FNativeWebWebSocketNetwork::Close(uint16 InStatusCode, const FString& InReason)
{
	if (!Wsi)
	{
		return;
	}

	TArray<uint8> reasonBytes;
	if (!InReason.IsEmpty())
	{
		FTCHARToUTF8 utf8Reason(*InReason);
		reasonBytes.Append(reinterpret_cast<const uint8*>(utf8Reason.Get()), utf8Reason.Length());
	}

	uint8* reasonData = reasonBytes.Num() > 0 ? reasonBytes.GetData() : nullptr;
	const size_t reasonLen = static_cast<size_t>(reasonBytes.Num());
	lws_close_reason(Wsi, static_cast<lws_close_status>(InStatusCode), reasonData, reasonLen);
	lws_callback_on_writable(Wsi);
}

FString FNativeWebWebSocketNetwork::RemoteEndPoint(bool bInAppendPort)
{
	FString remote = RemoteIp;
	if (bInAppendPort)
	{
		remote += FString::Printf(TEXT(":%i"), RemotePort);
	}
	return remote;
}

FString FNativeWebWebSocketNetwork::LocalEndPoint(bool bInAppendPort)
{
	const int32 sock = static_cast<int32>(lws_get_socket_fd(Wsi));
	struct sockaddr_in localAddr;
	socklen_t localAddrLen = sizeof(localAddr);
	getsockname(sock, reinterpret_cast<struct sockaddr*>(&localAddr), &localAddrLen);

	ANSICHAR ipBuffer[INET_ADDRSTRLEN];
	FString local(ANSI_TO_TCHAR(inet_ntop(AF_INET, &localAddr.sin_addr, ipBuffer, INET_ADDRSTRLEN)));
	if (bInAppendPort)
	{
		local += FString::Printf(TEXT(":%i"), ntohs(localAddr.sin_port));
	}
	return local;
}

void FNativeWebWebSocketNetwork::OnReceive(void* InData, uint32 InSize, bool bInIsBinary)
{
	ReceivedCallback.ExecuteIfBound(InData, static_cast<int32>(InSize), bInIsBinary);
}

void FNativeWebWebSocketNetwork::OnRawWebSocketWritable(NativeWebWebSocketInternal* InWsi)
{
	TArray<uint8> packet;
	{
		FScopeLock lock(&OutgoingLock);
		if (OutgoingBuffer.Num() == 0)
		{
			return;
		}
		packet = MoveTemp(OutgoingBuffer[0]);
		OutgoingBuffer.RemoveAt(0);
	}

	uint32 bytesToSend = packet.Num() - LWS_PRE;
	uint32 sentOffset = 0;
	while (bytesToSend > 0)
	{
		const int32 sent = static_cast<int32>(lws_write(InWsi, packet.GetData() + LWS_PRE + sentOffset, bytesToSend, LWS_WRITE_BINARY));
		if (sent < 0)
		{
			ErrorCallBack.ExecuteIfBound();
			return;
		}
		sentOffset += static_cast<uint32>(sent);
		bytesToSend -= static_cast<uint32>(sent);
	}
}

void FNativeWebWebSocketNetwork::OnClose()
{
	SocketClosedCallback.ExecuteIfBound();
}
