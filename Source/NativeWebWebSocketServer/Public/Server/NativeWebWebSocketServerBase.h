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
/**
 * @brief Declares the f native web web socket game thread event test access type and its operations.
 * @ingroup NativeWebWebSocketServer
 */
class FNativeWebWebSocketGameThreadEventTestAccess;
#endif

/** Deletes the private event state where its definition is available. */
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketGameThreadEventStateDeleter
{
	void operator()(FNativeWebWebSocketGameThreadEventState* InState) const;
};

/**
 * @brief UObject-facing base class for a managed NativeWebWebSocket server.
 * @details Public lifecycle and message APIs are game-thread owned; networking is performed by the private worker pipeline and events are marshalled back to the game thread.
 * @ingroup NativeWebWebSocketServer
 */
UCLASS(BlueprintType, Blueprintable)
class NATIVEWEBWEBSOCKETSERVER_API UNativeWebWebSocketServerBase : public UObject
{
	GENERATED_BODY()

public:

	UNativeWebWebSocketServerBase(const FObjectInitializer& ObjectInitializer);
	/** @brief Releases the opaque event queue state after producer shutdown.
 * @return The value produced by this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ~UNativeWebWebSocketServerBase();
 * @endcode
 */
	virtual ~UNativeWebWebSocketServerBase() override;
public:
#pragma region Lifecycle

/**
 * @brief Performs UObject destruction-time cleanup.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BeginDestroy();
 * @endcode
 */

	virtual void BeginDestroy() override;

	#pragma endregion

public:
#pragma region BlueprintEvents
	/** Fired after a client connection has been accepted and its client object initialized. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	/** @brief Reflected data field `OnClientConnected`. */
	FNativeWebWebSocketClientConnectedDynamic OnClientConnected;

	/** Fired after a client has disconnected from the server. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	/** @brief Reflected data field `OnClientDisconnected`. */
	FNativeWebWebSocketClientDisconnectedDynamic OnClientDisconnected;

	/** Fired when the server reports a server-level error. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	/** @brief Reflected data field `OnServerError`. */
	FNativeWebWebSocketServerErrorDynamic OnServerError;

	/** Fired when the server reports an error associated with a specific client. */
	UPROPERTY(BlueprintAssignable, Category = "NativeWebWebSocket|Events")
	/** @brief Reflected data field `OnClientError`. */
	FNativeWebWebSocketClientErrorDynamic OnClientError;
#pragma endregion

public:
#pragma region PublicApi
	/** @brief Returns the identifier assigned to this server instance. @return Server identifier, or NAME_None before initialization.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetServerNameId();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FName GetServerNameId() const { return NameId; }

	/** @brief Returns the configured port. @return Requested or assigned TCP port, or zero before configuration.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetBoundPort();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	int32 GetBoundPort() const { return BoundPort; }


 /**
 * @brief Reports whether the server instance is accepting connections.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * IsRunning();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	bool IsRunning() const;

	/** @brief Returns the payload format applied to outgoing and incoming WebSocket messages. @return Current payload format.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetPayloadFormat();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket|Pipeline")
	ENativeWebWebSocketPayloadFormat GetPayloadFormat() const { return PayloadFormat; }


 /**
 * @brief Queues a string payload for one connected client using the configured WebSocket payload format.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InMessage Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendToClientString(InClientId, InMessage);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendToClientString(int64 InClientId, const FString& InMessage);


 /**
 * @brief Queues opaque bytes for one connected client; the payload is copied before asynchronous network sending.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InBytes Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendToClientBytes(InClientId, InBytes);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult SendToClientBytes(int64 InClientId, const TArray<uint8>& InBytes);


 /**
 * @brief Serializes the supplied JSON value with the configured codec and queues it for one connected client.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InJsonObject Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InJsonValue Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendToClientJson(InClientId, InJsonObject, InJsonValue);
 * @endcode
 */
	ENativeWebWebSocketSendResult SendToClientJson(int64 InClientId, const TSharedPtr<FJsonObject>& InJsonObject, const TSharedPtr<FJsonValue>& InJsonValue = nullptr);


 /**
 * @brief Queues a string payload for every connected client.
 * @param InMessage Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastString(InMessage);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastString(const FString& InMessage);


 /**
 * @brief Queues opaque bytes for every connected client.
 * @param InBytes Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastBytes(InBytes);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastBytes(const TArray<uint8>& InBytes);


 /**
 * @brief Serializes the supplied JSON value and queues it for every connected client.
 * @param InJsonObject Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InJsonValue Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastJson(InJsonObject, InJsonValue);
 * @endcode
 */
	ENativeWebWebSocketSendResult BroadcastJson(const TSharedPtr<FJsonObject>& InJsonObject, const TSharedPtr<FJsonValue>& InJsonValue = nullptr);


 /**
 * @brief Queues a string payload for every connected client except the specified client.
 * @param InExcludedClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InMessage Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastStringExcept(InExcludedClientId, InMessage);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastStringExcept(int64 InExcludedClientId, const FString& InMessage);


 /**
 * @brief Queues opaque bytes for every connected client except the specified client.
 * @param InExcludedClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InBytes Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BroadcastBytesExcept(InExcludedClientId, InBytes);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult BroadcastBytesExcept(int64 InExcludedClientId, const TArray<uint8>& InBytes);


 /**
 * @brief Requests closure of one connected client with the supplied close reason.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InReason Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DisconnectClient(InClientId, InReason);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	ENativeWebWebSocketSendResult DisconnectClient(int64 InClientId, ENativeWebWebSocketCloseReason InReason = ENativeWebWebSocketCloseReason::Kicked);


 /**
 * @brief Stops the server and drains worker activity before releasing its runtime instance.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StopServer();
 * @endcode
 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket")
	void StopServer();


 /**
 * @brief Returns the clients currently owned by this server.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetClients();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	TArray<UNativeWebWebSocketClientBase*> GetClients() const;


 /**
 * @brief Looks up one connected client by its assigned identifier.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetClient(InClientId);
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	UNativeWebWebSocketClientBase* GetClient(int64 InClientId) const;


 /**
 * @brief Returns a snapshot of server counters and queue state.
 * @return The result of the requested operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetStats();
 * @endcode
 */
	UFUNCTION(BlueprintPure, Category = "NativeWebWebSocket")
	FNativeWebWebSocketServerStats GetStats() const;


 /**
 * @brief Selects the built-in payload format for future send and receive operations.
 * @param InPayloadFormat Input used by this operation; see the function declaration and owning type for its exact contract.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetPayloadFormat(InPayloadFormat);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket|Pipeline")
	void SetPayloadFormat(ENativeWebWebSocketPayloadFormat InPayloadFormat);


 /**
 * @brief Replaces the configuration used to build the message codec pipeline.
 * @param InPipelineConfig Input used by this operation; see the function declaration and owning type for its exact contract.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetPipelineConfig(InPipelineConfig);
 * @endcode
 */
	UFUNCTION(BlueprintCallable, Category = "NativeWebWebSocket|Pipeline")
	void SetPipelineConfig(const FNativeWebWebSocketPipelineConfig& InPipelineConfig);


 /**
 * @brief Installs a custom packet codec for the server pipeline.
 * @param InCodec Input used by this operation; see the function declaration and owning type for its exact contract.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetPacketCodec(InCodec);
 * @endcode
 */
	void SetPacketCodec(const TSharedPtr<INativeWebWebSocketPacketCodec>& InCodec);

protected:
	/**
	 * @brief Handles a binary message delivered by the server runtime.
	 * @param InClient Client that sent the message.
	 * @param InData Decoded binary payload.
	 * @note Override this to implement application-level binary message handling. The base implementation is a no-op.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleClientBytes(InClient, InData);
@endcode
 */
	virtual void HandleClientBytes(UNativeWebWebSocketClientBase* InClient, const TArray<uint8>& InData);
	/**
	 * @brief Handles a text message delivered by the server runtime.
	 * @param InClient Client that sent the message.
	 * @param InMessage Decoded text payload.
	 * @note Override this to implement application-level text message handling. The base implementation is a no-op.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleClientString(InClient, InMessage);
@endcode
 */
	virtual void HandleClientString(UNativeWebWebSocketClientBase* InClient, const FString& InMessage);
	
	/**
	 * @brief Notifies the derived server object that the runtime server has initialized.
	 * @param InNativeWebWebSocketSubsystem WebSocket subsystem that owns the initialized runtime.
	 * @note This is called by the subsystem before normal client traffic is processed. The base implementation is a no-op.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
ServerInitialized(InNativeWebWebSocketSubsystem);
@endcode
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
 * @brief Implements StartServerInternal for this module.
 * @param InNameId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InBoundPort Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InClientClass Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InPipelineConfig Input used by this operation; see the function declaration and owning type for its exact contract.
 * @return The result of the requested operation.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * StartServerInternal(InNameId, InBoundPort, InClientClass, InPipelineConfig);
 * @endcode
 */
	bool StartServerInternal(FName InNameId, int32 InBoundPort, TSubclassOf<UNativeWebWebSocketClientBase> InClientClass,
	                         const FNativeWebWebSocketPipelineConfig& InPipelineConfig);

 /**
 * @brief Implements AttachInstance for this module.
 * @param InInstance Input used by this operation; see the function declaration and owning type for its exact contract.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * AttachInstance(InInstance);
 * @endcode
 */
	void AttachInstance(const TSharedPtr<FNativeWebWebSocketServerInstance>& InInstance);

 /**
 * @brief Implements NotifyClientConnected for this module.
 * @param InClientInfo Input used by this operation; see the function declaration and owning type for its exact contract.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NotifyClientConnected(InClientInfo);
 * @endcode
 */
	void NotifyClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo);

 /**
 * @brief Implements NotifyClientDisconnected for this module.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InReason Input used by this operation; see the function declaration and owning type for its exact contract.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NotifyClientDisconnected(InClientId, InReason);
 * @endcode
 */
	void NotifyClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason);

 /**
 * @brief Implements NotifyServerError for this module.
 * @param InErrorCode Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InMessage Input used by this operation; see the function declaration and owning type for its exact contract.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NotifyServerError(InErrorCode, InMessage);
 * @endcode
 */
	void NotifyServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);

 /**
 * @brief Implements NotifyClientError for this module.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InErrorCode Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InMessage Input used by this operation; see the function declaration and owning type for its exact contract.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NotifyClientError(InClientId, InErrorCode, InMessage);
 * @endcode
 */
	void NotifyClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);

 /**
 * @brief Implements NotifyClientMessage for this module.
 * @param InClientId Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param InPayload Input used by this operation; see the function declaration and owning type for its exact contract.
 * @param bInBinary Boolean option controlling this operation.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NotifyClientMessage(InClientId, InPayload, bInBinary);
 * @endcode
 */
	void NotifyClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary);
	
	/**
	 * @brief Receives a newly connected client notification after base bookkeeping.
	 * @param InClientInfo Identity and connection metadata for the client.
	 * @note The base implementation does nothing. Override to add server-specific connection handling.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleClientConnected(InClientInfo);
@endcode
 */
	virtual void HandleClientConnected(const FNativeWebWebSocketClientInfo& InClientInfo);
	/**
	 * @brief Receives a client disconnection notification after base bookkeeping.
	 * @param InClientId Identifier of the disconnected client.
	 * @param InReason Reason reported for the disconnection.
	 * @note The base implementation does nothing. Override to add server-specific cleanup.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleClientDisconnected(InClientId, InReason);
@endcode
 */
	virtual void HandleClientDisconnected(int64 InClientId, ENativeWebWebSocketCloseReason InReason);
	/**
	 * @brief Receives a server-level runtime error after the error delegate is notified.
	 * @param InErrorCode Runtime error code.
	 * @param InMessage Human-readable error description.
	 * @note The base implementation does nothing. Override to add application logging or recovery.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleServerError(InErrorCode, InMessage);
@endcode
 */
	virtual void HandleServerError(ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
	/**
	 * @brief Receives a client-specific runtime error after the error delegate is notified.
	 * @param InClientId Identifier of the affected client.
	 * @param InErrorCode Runtime error code.
	 * @param InMessage Human-readable error description.
	 * @note The base implementation does nothing. Override to add application logging or recovery.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleClientError(InClientId, InErrorCode, InMessage);
@endcode
 */
	virtual void HandleClientError(int64 InClientId, ENativeWebWebSocketErrorCode InErrorCode, const FString& InMessage);
	/**
	 * @brief Receives a client message after the runtime has identified its wire type.
	 * @param InClientId Identifier of the sending client.
	 * @param InPayload Message bytes.
	 * @param bInBinary true for binary data; false for text data.
	 * @note The base implementation does nothing. Override to implement application protocol handling.
	 * @note Called only on the game thread.
 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
 * @par Example
 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
 * @code{.cpp}
HandleClientMessage(InClientId, InPayload, bInBinary);
@endcode
 */
	virtual void HandleClientMessage(int64 InClientId, const TArray<uint8>& InPayload, bool bInBinary);
#pragma endregion

private:
#pragma region InternalState
	/** @brief Enqueues an owned event from a producer thread.
* @param InEvent Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EnqueueGameThreadEvent(InEvent);
 * @endcode
 */
	bool EnqueueGameThreadEvent(FNativeWebWebSocketGameThreadEvent&& InEvent);
	/** @brief Drains at most the requested number of events on the game thread.
* @param InMaxEvents Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DrainGameThreadEvents(InMaxEvents);
 * @endcode
 */
	int32 DrainGameThreadEvents(int32 InMaxEvents);
	/** @brief Dispatches one generation-validated event on the game thread.
* @param InEvent Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DispatchGameThreadEvent(InEvent);
 * @endcode
 */
	void DispatchGameThreadEvent(FNativeWebWebSocketGameThreadEvent&& InEvent);
	/** @brief Applies a client-connected event and flushes messages buffered before connection.
* @param InEvent Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DispatchClientConnectedEvent(InEvent);
 * @endcode
 */
	void DispatchClientConnectedEvent(const FNativeWebWebSocketClientConnectedEvent& InEvent);
	/** @brief Applies a client-disconnected event before broadcasting lifecycle callbacks.
* @param InEvent Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DispatchClientDisconnectedEvent(InEvent);
 * @endcode
 */
	void DispatchClientDisconnectedEvent(const FNativeWebWebSocketClientDisconnectedEvent& InEvent);
	/** @brief Broadcasts a server error and invokes the native server error hook.
* @param InEvent Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DispatchServerErrorEvent(InEvent);
 * @endcode
 */
	void DispatchServerErrorEvent(const FNativeWebWebSocketServerErrorEvent& InEvent);
	/** @brief Broadcasts a client error and invokes the native client error hook.
* @param InEvent Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DispatchClientErrorEvent(InEvent);
 * @endcode
 */
	void DispatchClientErrorEvent(const FNativeWebWebSocketClientErrorEvent& InEvent);
	/** @brief Routes a client message to the generic, byte, and text handlers.
* @param InEvent Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DispatchClientMessageEvent(InEvent);
 * @endcode
 */
	void DispatchClientMessageEvent(const FNativeWebWebSocketClientMessageEvent& InEvent);
	/** @brief Removes all queued events after producer threads have stopped.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ClearGameThreadEvents();
 * @endcode
 */
	void ClearGameThreadEvents();
	/** @brief Returns the current queued event count.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetGameThreadEventQueueDepth();
 * @endcode
 */
	int32 GetGameThreadEventQueueDepth() const;
	/** @brief Consumes a control-event overflow stop request.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ConsumeControlEventOverflowStopRequest();
 * @endcode
 */
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
	/** @brief Reflected data field `ClientObjects`. */
	TMap<int64, TObjectPtr<UNativeWebWebSocketClientBase>> ClientObjects;
	/** @brief Messages received before their game-thread client object was created. */
	TMap<int64, TArray<FNativeWebWebSocketInboundPacket>> PendingClientMessages;
	/** @brief Instance state. */
	TSharedPtr<FNativeWebWebSocketServerInstance> Instance;
	/** @brief Cross-thread event queue state kept opaque to public consumers. */
	TUniquePtr<FNativeWebWebSocketGameThreadEventState, FNativeWebWebSocketGameThreadEventStateDeleter> GameThreadEventState;
#pragma endregion
};

