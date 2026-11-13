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

/**
 * @brief Base UObject representing one connected NativeWebWebSocket client.
 * @details Client lifecycle and Blueprint events are surfaced on the game thread; the socket itself is owned by the server runtime.
 * @ingroup NativeWebWebSocketServer
 */
UCLASS(BlueprintType, Blueprintable)
class NATIVEWEBWEBSOCKETSERVER_API UNativeWebWebSocketClientBase : public UObject
{
	GENERATED_BODY()

public:
	/** @brief Constructs the client wrapper. @param InInitializer Unreal object initializer. */
	UNativeWebWebSocketClientBase(const FObjectInitializer& InInitializer);
public:
#pragma region PublicApi
	/** @brief Returns the server-assigned client identifier.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetClientId();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	int64 GetClientId() const { return ClientInfo.ClientId; }

	/** @brief Returns the client's remote network address.
 * @return The resulting text or identifier.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetRemoteAddress();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FString GetRemoteAddress() const { return ClientInfo.RemoteAddress; }

	/** @brief Returns the time at which the client connected.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetConnectedAt();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FDateTime GetConnectedAt() const { return ClientInfo.ConnectedAt; }

	/** @brief Sends a UTF-8 string to this client. @param InMessage Message text. @return Result of the send operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendString(InMessage);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendString(const FString& InMessage);

	/** @brief Sends binary data to this client. @param InBytes Payload bytes. @return Result of the send operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendBytes(InBytes);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendBytes(const TArray<uint8>& InBytes);

	/** @brief Disconnects this client. @param InReason Close reason. @return Result of the disconnect operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Disconnect(InReason);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult Disconnect(ENativeWebWebSocketCloseReason InReason = ENativeWebWebSocketCloseReason::Kicked);

	/** @brief Handles binary data received from this client. @param InData Received bytes.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance WebSocket servers and staged network pipeline.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleBytesFromClient(InData);
@endcode
 */
	virtual void HandleBytesFromClient(const TArray<uint8>& InData);
	/** @brief Handles text data received from this client. @param InMessage Received text.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @note Game-instance WebSocket servers and staged network pipeline.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleStringFromClient(InMessage);
@endcode
 */
	virtual void HandleStringFromClient(const FString& InMessage);
#pragma endregion

private:
#pragma region Internal
	/** Grants the owning server access to internal client state. */
	friend class UNativeWebWebSocketServerBase;
	/** Initializes this wrapper with its owning server and connection information.
* @param InOwnerServer Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InInfo Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * InitializeClient(InOwnerServer, InInfo);
 * @endcode
 */
	void InitializeClient(TWeakObjectPtr<UNativeWebWebSocketServerBase> InOwnerServer, const FNativeWebWebSocketClientInfo& InInfo);

	/** Server that owns this client wrapper. */
	TWeakObjectPtr<UNativeWebWebSocketServerBase> OwnerServer;
	/** Connection identity and timing information. */
	FNativeWebWebSocketClientInfo ClientInfo;
#pragma endregion
};

