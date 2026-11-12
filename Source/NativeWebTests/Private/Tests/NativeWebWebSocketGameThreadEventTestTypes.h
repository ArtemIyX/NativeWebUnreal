/**
 * @file NativeWebWebSocketServer\\Private\\Tests\\NativeWebWebSocketGameThreadEventTestTypes.h
 * @brief Declares test-only WebSocket server and client types for event queue automation tests.
 */
#pragma once

#include "CoreMinimal.h"
#include "Server/NativeWebWebSocketClientBase.h"
#include "Server/NativeWebWebSocketServerBase.h"
#include "NativeWebWebSocketGameThreadEventTestTypes.generated.h"

class UNativeWebWebSocketGameThreadEventTestServer;

/** @brief Test client that records downstream byte and text callbacks. */
UCLASS()
class UNativeWebWebSocketGameThreadEventTestClient : public UNativeWebWebSocketClientBase
{
	GENERATED_BODY()

public:
	static UNativeWebWebSocketGameThreadEventTestServer* ActiveServer;

	virtual void HandleBytesFromClient(const TArray<uint8>& InData) override;
	virtual void HandleStringFromClient(const FString& InMessage) override;
};

/** @brief Test server that records event order, payloads, and callback thread affinity. */
UCLASS()
class UNativeWebWebSocketGameThreadEventTestServer : public UNativeWebWebSocketServerBase
{
	GENERATED_BODY()

public:
	TArray<FString> Records;
	TArray<uint32> CallbackThreadIds;
	TArray<int64> MessageIds;
	TArray<TArray<uint8>> MessagePayloads;
	TArray<bool> MessageBinaryFlags;
	TArray<FString> MessageTexts;
	TArray<bool> ConnectedClientVisibleInCallbacks;
	TArray<bool> DisconnectedClientVisibleInCallbacks;
	bool bStopOnConnected = false;

	void ResetRecords();
	void RecordClientBytes();
	void RecordClientString(const FString& InMessage);

	UFUNCTION()
	void RecordConnectedDelegate(const FNativeWebWebSocketClientInfo& InClientInfo);
	UFUNCTION()
	void RecordDisconnectedDelegate(int64 InClientId, ENativeWebWebSocketCloseReason InReason);
	UFUNCTION()
	void RecordServerErrorDelegate(FName InServerName, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
	UFUNCTION()
	void RecordClientErrorDelegate(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);

protected:
	virtual void HandleClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo) override;
	virtual void HandleClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason) override;
	virtual void HandleServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage) override;
	virtual void HandleClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage) override;
	virtual void HandleClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary) override;
	virtual void HandleClientBytes(UNativeWebWebSocketClientBase* InClient, const TArray<uint8>& InData) override;
	virtual void HandleClientString(UNativeWebWebSocketClientBase* InClient, const FString& InMessage) override;
	virtual void ServerInitialized(class UNativeWebWebSocketSubsystem* InNativeWebWebSocketSubsystem) override;
};
