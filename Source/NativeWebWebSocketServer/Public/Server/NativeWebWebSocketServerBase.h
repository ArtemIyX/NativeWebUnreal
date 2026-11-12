/**
 * @file NativeWebWebSocketServer\Public\Server\NativeWebWebSocketServerBase.h
 * @brief Declares NativeWebWebSocketServerBase for module NativeWebWebSocketServer\Public\Server\NativeWebWebSocketServerBase.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/NativeWebWebSocketDelegates.h"
#include "Data/NativeWebWebSocketStructs.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "Pipeline/NativeWebWebSocketPipelineTypes.h"
#include "NativeWebWebSocketServerBase.generated.h"

class UNativeWebWebSocketSubsystem;
class UNativeWebWebSocketClientBase;
class FNativeWebWebSocketServerInstance;
class INativeWebWebSocketPacketCodec;
struct FNativeWebWebSocketGameThreadEvent;
struct FNativeWebWebSocketClientConnectedEvent;
struct FNativeWebWebSocketClientDisconnectedEvent;
struct FNativeWebWebSocketServerErrorEvent;
struct FNativeWebWebSocketClientErrorEvent;
struct FNativeWebWebSocketClientMessageEvent;
class FNativeWebWebSocketGameThreadEventState;
#if WITH_DEV_AUTOMATION_TESTS
class FNativeWebWebSocketGameThreadEventTestAccess;
#endif

/** Deletes the private event state where its definition is available. */
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketGameThreadEventStateDeleter
{
	void operator()(FNativeWebWebSocketGameThreadEventState* InState) const;
};

/** @brief Type declaration. */
UCLASS(BlueprintType, Blueprintable)
class NATIVEWEBWEBSOCKETSERVER_API UNativeWebWebSocketServerBase : public UObject
{
	GENERATED_BODY()

public:

	UNativeWebWebSocketServerBase(const FObjectInitializer& ObjectInitializer);
	/** @brief Releases the opaque event queue state after producer shutdown. */
	virtual ~UNativeWebWebSocketServerBase() override;
public:
#pragma region Lifecycle

	virtual void BeginDestroy() override;

	#pragma endregion

public:
#pragma region BlueprintEvents
	/** Fired after a client connection has been accepted and its client object initialized. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	FNativeWebWebSocketClientConnectedDynamic OnClientConnected;

	/** Fired after a client has disconnected from the server. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	FNativeWebWebSocketClientDisconnectedDynamic OnClientDisconnected;

	/** Fired when the server reports a server-level error. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	FNativeWebWebSocketServerErrorDynamic OnServerError;

	/** Fired when the server reports an error associated with a specific client. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	FNativeWebWebSocketClientErrorDynamic OnClientError;
#pragma endregion

public:
#pragma region PublicApi
	/** @brief Returns the identifier assigned to this server instance. @return Server identifier, or NAME_None before initialization. */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FName GetServerNameId() const { return NameId; }

	/** @brief Returns the configured port. @return Requested or assigned TCP port, or zero before configuration. */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	int32 GetBoundPort() const { return BoundPort; }

	/**
	 * @brief Reports whether the underlying WebSocket server is currently running.
	 * @return true when the runtime server exists and is running; otherwise false.
	 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	bool IsRunning() const;

	/** @brief Returns the payload format applied to outgoing and incoming WebSocket messages. @return Current payload format. */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket|Pipeline")
	ENativeWebWebSocketPayloadFormat GetPayloadFormat() const { return PayloadFormat; }

	/**
	 * @brief Sends a UTF-8 string message to one connected client.
	 * @param InClientId Identifier of the destination client.
	 * @param InMessage Message text to send.
	 * @return The send result, including invalid-client, disconnected, queue, and transport failures.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendToClientString(int64 InClientId, const FString& InMessage);

	/**
	 * @brief Sends binary data to one connected client.
	 * @param InClientId Identifier of the destination client.
	 * @param InBytes Payload bytes to send.
	 * @return The send result. The payload may still be transformed by the configured packet codec or pipeline.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendToClientBytes(int64 InClientId, const TArray<uint8>& InBytes);

	/**
	 * @brief Serializes and sends a JSON object or value to one connected client.
	 * @param InClientId Identifier of the destination client.
	 * @param InJsonObject Object to serialize. May be null when InJsonValue is supplied.
	 * @param InJsonValue Value to serialize when InJsonObject is null.
	 * @return SerializeFailed when neither input is valid or serialization fails; otherwise the underlying send result.
	 */
	ENativeWebWebSocketSendResult SendToClientJson(int64 InClientId, const TSharedPtr<FJsonObject>& InJsonObject, const TSharedPtr<FJsonValue>& InJsonValue = nullptr);

	/**
	 * @brief Sends a UTF-8 string message to every connected client.
	 * @param InMessage Message text to broadcast.
	 * @return The aggregate broadcast send result, or InvalidServer when the server is not running.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastString(const FString& InMessage);

	/**
	 * @brief Sends binary data to every connected client.
	 * @param InBytes Payload bytes to broadcast.
	 * @return The aggregate broadcast send result, or InvalidServer when the server is not running.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastBytes(const TArray<uint8>& InBytes);

	/**
	 * @brief Serializes and broadcasts a JSON object or value to every connected client.
	 * @param InJsonObject Object to serialize. May be null when InJsonValue is supplied.
	 * @param InJsonValue Value to serialize when InJsonObject is null.
	 * @return SerializeFailed when neither input is valid or serialization fails; otherwise the underlying broadcast result.
	 */
	ENativeWebWebSocketSendResult BroadcastJson(const TSharedPtr<FJsonObject>& InJsonObject, const TSharedPtr<FJsonValue>& InJsonValue = nullptr);

	/**
	 * @brief Sends a UTF-8 string message to every connected client except one.
	 * @param InExcludedClientId Client identifier excluded from delivery.
	 * @param InMessage Message text to broadcast.
	 * @return The broadcast send result, or InvalidServer when the server is not running.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastStringExcept(int64 InExcludedClientId, const FString& InMessage);

	/**
	 * @brief Sends binary data to every connected client except one.
	 * @param InExcludedClientId Client identifier excluded from delivery.
	 * @param InBytes Payload bytes to broadcast.
	 * @return The broadcast send result, or InvalidServer when the server is not running.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastBytesExcept(int64 InExcludedClientId, const TArray<uint8>& InBytes);

	/**
	 * @brief Disconnects a connected client and reports the requested close reason.
	 * @param InClientId Identifier of the client to disconnect.
	 * @param InReason Close reason sent to the client and used by the server runtime.
	 * @return Ok when the disconnect was queued; otherwise the relevant server, client, or transport failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult DisconnectClient(int64 InClientId, ENativeWebWebSocketCloseReason InReason = ENativeWebWebSocketCloseReason::Kicked);

	/**
	 * @brief Stops the underlying server if it is running.
	 * @note This is safe to call when the server is already stopped.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	void StopServer();

	/**
	 * @brief Returns the currently known client objects.
	 * @return A snapshot array of connected or still-being-cleaned-up client objects. The caller does not own the objects.
	 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	TArray<UNativeWebWebSocketClientBase*> GetClients() const;

	/**
	 * @brief Looks up a client by identifier.
	 * @param InClientId Identifier returned by the client object and connection event.
	 * @return The matching client object, or nullptr when no client is registered with that identifier.
	 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	UNativeWebWebSocketClientBase* GetClient(int64 InClientId) const;

	/**
	 * @brief Returns runtime connection and traffic statistics for this server.
	 * @return A value snapshot of the server statistics.
	 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FNativeWebWebSocketServerStats GetStats() const;

	/**
	 * @brief Sets the wire payload format used by the server pipeline.
	 * @param InPayloadFormat New payload format.
	 * @note The setting is forwarded to the active runtime server and affects subsequent messages.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket|Pipeline")
	void SetPayloadFormat(ENativeWebWebSocketPayloadFormat InPayloadFormat);

	/**
	 * @brief Replaces the server pipeline configuration.
	 * @param InPipelineConfig Configuration controlling processing stages for WebSocket payloads.
	 * @note The configuration is applied to the active runtime server when one exists and is also retained for the next start.
	 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket|Pipeline")
	void SetPipelineConfig(const FNativeWebWebSocketPipelineConfig& InPipelineConfig);

	/**
	 * @brief Sets the optional packet codec used to transform WebSocket payloads.
	 * @param InCodec Codec shared by the server runtime. Passing an invalid pointer disables the codec.
	 * @note The codec is retained by shared ownership and is applied to subsequent server traffic.
	 */
	void SetPacketCodec(const TSharedPtr<INativeWebWebSocketPacketCodec>& InCodec);

protected:
	/**
	 * @brief Handles a binary message delivered by the server runtime.
	 * @param InClient Client that sent the message.
	 * @param InData Decoded binary payload.
	 * @note Override this to implement application-level binary message handling. The base implementation is a no-op.
	 * @note Called only on the game thread.
	 */
	virtual void HandleClientBytes(UNativeWebWebSocketClientBase* InClient, const TArray<uint8>& InData);
	/**
	 * @brief Handles a text message delivered by the server runtime.
	 * @param InClient Client that sent the message.
	 * @param InMessage Decoded text payload.
	 * @note Override this to implement application-level text message handling. The base implementation is a no-op.
	 * @note Called only on the game thread.
	 */
	virtual void HandleClientString(UNativeWebWebSocketClientBase* InClient, const FString& InMessage);
	
	/**
	 * @brief Notifies the derived server object that the runtime server has initialized.
	 * @param InNativeWebWebSocketSubsystem WebSocket subsystem that owns the initialized runtime.
	 * @note This is called by the subsystem before normal client traffic is processed. The base implementation is a no-op.
	 * @note Called only on the game thread.
	 */
	virtual void ServerInitialized(UNativeWebWebSocketSubsystem* InNativeWebWebSocketSubsystem);
#pragma endregion

protected:
#pragma region InternalApi
	/** @brief UNativeWebWebSocketSubsystem state. */
	friend class UNativeWebWebSocketSubsystem;
	/** @brief FNativeWebWebSocketServerInstance state. */
	friend class FNativeWebWebSocketServerInstance;
	/** @brief Test-only access to the event queue seam. */
#if WITH_DEV_AUTOMATION_TESTS
	friend class FNativeWebWebSocketGameThreadEventTestAccess;
#endif
	/**
	 * @brief Starts the internal server runtime with the supplied configuration.
	 * @param InNameId Stable identifier used to identify this server.
	 * @param InBoundPort Requested local TCP port.
	 * @param InClientClass UObject class used to create client instances.
	 * @param InPipelineConfig Payload pipeline configuration.
	 * @return true when startup succeeds; otherwise false and the runtime reports the startup error.
	 * @note This is an internal entry point. Callers should normally use the WebSocket subsystem rather than invoking it directly.
	 */
	bool StartServerInternal(FName InNameId, int32 InBoundPort, TSubclassOf<UNativeWebWebSocketClientBase> InClientClass,
	                         const FNativeWebWebSocketPipelineConfig& InPipelineConfig);
	/**
	 * @brief Attaches an already-created runtime server instance to this UObject.
	 * @param InInstance Runtime instance to associate with this server.
	 * @note This transfers the shared reference and is intended for internal subsystem coordination.
	 */
	void AttachInstance(const TSharedPtr<FNativeWebWebSocketServerInstance>& InInstance);
	/**
	 * @brief Updates server state and emits OnClientConnected for a newly connected client.
	 * @param InClientInfo Identity and connection metadata for the client.
	 */
	void NotifyClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo);
	/**
	 * @brief Updates server state and emits OnClientDisconnected for a disconnected client.
	 * @param InClientId Identifier of the disconnected client.
	 * @param InReason Reason reported for the disconnection.
	 */
	void NotifyClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason);
	/**
	 * @brief Emits OnServerError for an error reported by the runtime server.
	 * @param InErrorCode Runtime error code.
	 * @param InMessage Human-readable error description.
	 */
	void NotifyServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
	/**
	 * @brief Emits OnClientError for an error associated with a client.
	 * @param InClientId Identifier of the affected client.
	 * @param InErrorCode Runtime error code.
	 * @param InMessage Human-readable error description.
	 */
	void NotifyClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
	/**
	 * @brief Routes a runtime client message to the appropriate text or binary handler.
	 * @param InClientId Identifier of the sending client.
	 * @param InPayload Message bytes, regardless of the original wire representation.
	 * @param bInBinary true for binary data; false for text data encoded as bytes.
	 */
	void NotifyClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary);
	
	/**
	 * @brief Receives a newly connected client notification after base bookkeeping.
	 * @param InClientInfo Identity and connection metadata for the client.
	 * @note The base implementation does nothing. Override to add server-specific connection handling.
	 * @note Called only on the game thread.
	 */
	virtual void HandleClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo);
	/**
	 * @brief Receives a client disconnection notification after base bookkeeping.
	 * @param InClientId Identifier of the disconnected client.
	 * @param InReason Reason reported for the disconnection.
	 * @note The base implementation does nothing. Override to add server-specific cleanup.
	 * @note Called only on the game thread.
	 */
	virtual void HandleClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason);
	/**
	 * @brief Receives a server-level runtime error after the error delegate is notified.
	 * @param InErrorCode Runtime error code.
	 * @param InMessage Human-readable error description.
	 * @note The base implementation does nothing. Override to add application logging or recovery.
	 * @note Called only on the game thread.
	 */
	virtual void HandleServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
	/**
	 * @brief Receives a client-specific runtime error after the error delegate is notified.
	 * @param InClientId Identifier of the affected client.
	 * @param InErrorCode Runtime error code.
	 * @param InMessage Human-readable error description.
	 * @note The base implementation does nothing. Override to add application logging or recovery.
	 * @note Called only on the game thread.
	 */
	virtual void HandleClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
	/**
	 * @brief Receives a client message after the runtime has identified its wire type.
	 * @param InClientId Identifier of the sending client.
	 * @param InPayload Message bytes.
	 * @param bInBinary true for binary data; false for text data.
	 * @note The base implementation does nothing. Override to implement application protocol handling.
	 * @note Called only on the game thread.
	 */
	virtual void HandleClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary);
#pragma endregion

private:
#pragma region InternalState
	/** @brief Enqueues an owned event from a producer thread. */
	bool EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent&& InEvent);
	/** @brief Drains at most the requested number of events on the game thread. */
	int32 DrainGameThreadEvents(int32 InMaxEvents);
	/** @brief Dispatches one generation-validated event on the game thread. */
	void DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent&& InEvent);
	/** @brief Applies a client-connected event and flushes messages buffered before connection. */
	void DispatchClientConnectedEvent(const FNativeWebWebSocketClientConnectedEvent& InEvent);
	/** @brief Applies a client-disconnected event before broadcasting lifecycle callbacks. */
	void DispatchClientDisconnectedEvent(const FNativeWebWebSocketClientDisconnectedEvent& InEvent);
	/** @brief Broadcasts a server error and invokes the native server error hook. */
	void DispatchServerErrorEvent(const FNativeWebWebSocketServerErrorEvent& InEvent);
	/** @brief Broadcasts a client error and invokes the native client error hook. */
	void DispatchClientErrorEvent(const FNativeWebWebSocketClientErrorEvent& InEvent);
	/** @brief Routes a client message to the generic, byte, and text handlers. */
	void DispatchClientMessageEvent(const FNativeWebWebSocketClientMessageEvent& InEvent);
	/** @brief Removes all queued events after producer threads have stopped. */
	void ClearGameThreadEvents();
	/** @brief Returns the current queued event count. */
	int32 GetGameThreadEventQueueDepth() const;
	/** @brief Consumes a control-event overflow stop request. */
	bool ConsumeControlEventOverflowStopRequest();

	/** @brief NameId state. */
	FName NameId = NAME_None;
	/** @brief BoundPort state. */
	int32 BoundPort = 0;
	/** @brief ClientClass state. */
	TSubclassOf<UNativeWebWebSocketClientBase> ClientClass;
	/** @brief PayloadFormat state. */
	ENativeWebWebSocketPayloadFormat PayloadFormat = ENativeWebWebSocketPayloadFormat::Binary;
	/** @brief PipelineConfig state. */
	FNativeWebWebSocketPipelineConfig PipelineConfig;
	/** @brief PacketCodec state. */
	TSharedPtr<INativeWebWebSocketPacketCodec> PacketCodec;
	/** @brief ClientObjectsLock state. */
	mutable FCriticalSection ClientObjectsLock;
	/** @brief ClientObjects state. */
	UPROPERTY()
	TMap<int64, TObjectPtr<UNativeWebWebSocketClientBase>> ClientObjects;
	/** @brief Messages received before their game-thread client object was created. */
	TMap<int64, TArray<FNativeWebWebSocketInboundPacket>> PendingClientMessages;
	/** @brief Instance state. */
	TSharedPtr<FNativeWebWebSocketServerInstance> Instance;
	/** @brief Cross-thread event queue state kept opaque to public consumers. */
	TUniquePtr<FNativeWebWebSocketGameThreadEventState, FNativeWebWebSocketGameThreadEventStateDeleter> GameThreadEventState;
#pragma endregion
};

