

#include "Server/NativeWebWebSocketClientBase.h"

#include "Server/NativeWebWebSocketServerBase.h"

UNativeWebWebSocketClientBase::UNativeWebWebSocketClientBase(const FObjectInitializer& InInitializer) : Super(InInitializer)
{
	
}

void UNativeWebWebSocketClientBase::InitializeClient(TWeakObjectPtr<UNativeWebWebSocketServerBase> InOwnerServer, const FNativeWebWebSocketClientInfo& InInfo)
{
	OwnerServer = InOwnerServer;
	ClientInfo = InInfo;
}



ENativeWebWebSocketSendResult UNativeWebWebSocketClientBase::SendString(const FString& InMessage)
{
	if (!OwnerServer.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return OwnerServer->SendToClientString(ClientInfo.ClientId, InMessage);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketClientBase::SendBytes(const TArray<uint8>& InBytes)
{
	if (!OwnerServer.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return OwnerServer->SendToClientBytes(ClientInfo.ClientId, InBytes);
}

ENativeWebWebSocketSendResult UNativeWebWebSocketClientBase::Disconnect(ENativeWebWebSocketCloseReason InReason)
{
	if (!OwnerServer.IsValid())
	{
		return ENativeWebWebSocketSendResult::InvalidServer;
	}
	return OwnerServer->DisconnectClient(ClientInfo.ClientId, InReason);
}

void UNativeWebWebSocketClientBase::HandleBytesFromClient(const TArray<uint8>& InData)
{
}

void UNativeWebWebSocketClientBase::HandleStringFromClient(const FString& InMessage)
{
}
