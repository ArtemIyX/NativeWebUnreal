/**
 * @file NativeWebWebSocketServer\Public\Server\NativeWebWebSocketClientBase.h
 * @brief Declares the UObject wrapper for a connected WebSocket client.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/NativeWebWebSocketStructs.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "NativeWebWebSocketClientBase.generated.h"

class UNativeWebWebSocketServerBase;

/** @brief UObject representation of one connected WebSocket client. */
UCLASS(BlueprintType, Blueprintable)
class NATIVEWEBWEBSOCKETSERVER_API UNativeWebWebSocketClientBase : public UObject
{
	GENERATED_BODY()

public:
	/** @brief Constructs the client wrapper. @param InInitializer Unreal object initializer. */
	UNativeWebWebSocketClientBase(const FObjectInitializer& InInitializer);
public:
#pragma region PublicApi
	/** @brief Returns the server-assigned client identifier. */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	int64 GetClientId() const { return ClientInfo.ClientId; }

	/** @brief Returns the client's remote network address. */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FString GetRemoteAddress() const { return ClientInfo.RemoteAddress; }

	/** @brief Returns the time at which the client connected. */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FDateTime GetConnectedAt() const { return ClientInfo.ConnectedAt; }

	/** @brief Sends a UTF-8 string to this client. @param InMessage Message text. @return Result of the send operation. */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendString(const FString& InMessage);

	/** @brief Sends binary data to this client. @param InBytes Payload bytes. @return Result of the send operation. */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendBytes(const TArray<uint8>& InBytes);

	/** @brief Disconnects this client. @param InReason Close reason. @return Result of the disconnect operation. */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult Disconnect(ENativeWebWebSocketCloseReason InReason = ENativeWebWebSocketCloseReason::Kicked);

	/** @brief Handles binary data received from this client. @param InData Received bytes. */
	virtual void HandleBytesFromClient(const TArray<uint8>& InData);
	/** @brief Handles text data received from this client. @param InMessage Received text. */
	virtual void HandleStringFromClient(const FString& InMessage);
#pragma endregion

private:
#pragma region Internal
	/** Grants the owning server access to internal client state. */
	friend class UNativeWebWebSocketServerBase;
	/** Initializes this wrapper with its owning server and connection information. */
	void InitializeClient(TWeakObjectPtr<UNativeWebWebSocketServerBase> InOwnerServer, const FNativeWebWebSocketClientInfo& InInfo);

	/** Server that owns this client wrapper. */
	TWeakObjectPtr<UNativeWebWebSocketServerBase> OwnerServer;
	/** Connection identity and timing information. */
	FNativeWebWebSocketClientInfo ClientInfo;
#pragma endregion
};

