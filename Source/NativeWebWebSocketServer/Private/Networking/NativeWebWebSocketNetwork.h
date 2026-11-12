/**
 * @file NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketNetwork.h
 * @brief Declares NativeWebWebSocketNetwork for module NativeWebWebSocketServer\Private\Networking\NativeWebWebSocketNetwork.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "Networking/INativeWebWebSocketNetwork.h"
#include "Networking/NativeWebWebSocketPrivate.h"

/** @brief Type declaration. */
class FNativeWebWebSocketNetwork : public INativeWebWebSocketNetwork
{
public:
	/** @brief FNativeWebWebSocketNetwork API. */
	FNativeWebWebSocketNetwork(NativeWebWebSocketInternalContext* InContext, NativeWebWebSocketInternal* InWsi);
	virtual ~FNativeWebWebSocketNetwork() override;

	virtual void SetConnectedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) override;
	virtual void SetConnectionErrorCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) override;
	virtual void SetReceiveCallBack(FNativeWebWebSocketNetworkPacketReceivedCallback InCallBack) override;
	virtual void SetSocketClosedCallBack(FNativeWebWebSocketNetworkInfoCallback InCallBack) override;

	virtual bool Send(const uint8* InData, uint32 InSize, bool bInPrependSize = true) override;
	virtual void Tick() override;
	virtual void Flush() override;
	virtual void Close(uint16 InStatusCode = 1000, const FString& InReason = FString()) override;

	virtual FString RemoteEndPoint(bool bInAppendPort) override;
	virtual FString LocalEndPoint(bool bInAppendPort) override;

	/** @brief OnReceive API. */
	void OnReceive(void* InData, uint32 InSize, bool bInIsBinary);
	/** @brief OnRawWebSocketWritable API. */
	void OnRawWebSocketWritable(NativeWebWebSocketInternal* InWsi);
	/** @brief OnClose API. */
	void OnClose();

private:
	/** @brief FlushInternal API. */
	void FlushInternal();

public:
	/** @brief ReceivedCallback state. */
	FNativeWebWebSocketNetworkPacketReceivedCallback ReceivedCallback;
	/** @brief ConnectedCallBack state. */
	FNativeWebWebSocketNetworkInfoCallback ConnectedCallBack;
	/** @brief ErrorCallBack state. */
	FNativeWebWebSocketNetworkInfoCallback ErrorCallBack;
	/** @brief SocketClosedCallback state. */
	FNativeWebWebSocketNetworkInfoCallback SocketClosedCallback;

	/** @brief OutgoingBuffer state. */
	TArray<TArray<uint8>> OutgoingBuffer;
	/** @brief OutgoingLock state. */
	FCriticalSection OutgoingLock;
	/** @brief Context state. */
	NativeWebWebSocketInternalContext* Context = nullptr;
	/** @brief Wsi state. */
	NativeWebWebSocketInternal* Wsi = nullptr;
	/** @brief RemoteIp state. */
	FString RemoteIp;
	/** @brief RemotePort state. */
	int32 RemotePort = 0;
};

