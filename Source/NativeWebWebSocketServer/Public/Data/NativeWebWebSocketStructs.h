/**
 * @file NativeWebWebSocketServer\Public\Data\NativeWebWebSocketStructs.h
 * @brief Defines configuration, result, client, and statistics structures for the NativeWebWebSocket WebSocket server.
 */
#pragma once

#include "CoreMinimal.h"
#include "Data/NativeWebWebSocketEnums.h"
#include "NativeWebWebSocketStructs.generated.h"


/**
 * @brief Defines the nativewebwebsocketserver api data structure.
 * @ingroup NativeWebWebSocketServer
 */
USTRUCT(BlueprintType)
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketPipelineConfig
{
	GENERATED_BODY()

	/** Maximum number of inbound messages waiting to be processed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NativeWebWebSocket|Pipeline")
	/** @brief Reflected data field `InInboundQueueMax`. */
	int32 InInboundQueueMax = 2048;

	/** Maximum number of messages waiting in the server send queue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NativeWebWebSocket|Pipeline")
	/** @brief Reflected data field `InSendQueueMax`. */
	int32 InSendQueueMax = 2048;

	/** Maximum number of pending writes for each client. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NativeWebWebSocket|Pipeline")
	/** @brief Reflected data field `InWriteQueueMaxPerClient`. */
	int32 InWriteQueueMaxPerClient = 1024;

	/** Encoding expected by the pipeline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NativeWebWebSocket|Pipeline")
	/** @brief Reflected data field `InPayloadFormat`. */
	ENativeWebWebSocketPayloadFormat InPayloadFormat = ENativeWebWebSocketPayloadFormat::Binary;
	/**
	 * @brief Defines the nativewebwebsocketserver api data structure.
	 * @ingroup NativeWebWebSocketServer
	 */
};

/** @brief Options used when creating a NativeWebWebSocket WebSocket server. */
USTRUCT(BlueprintType)
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketServerCreateOptions
{
	GENERATED_BODY()

	/** Logical identifier used to find the server instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NativeWebWebSocket")
	/** @brief Reflected data field `NameId`. */
	FName NameId = NAME_None;

	/** Requested listening port. Zero enables automatic port selection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NativeWebWebSocket")
	/** @brief Reflected data field `RequestedPort`. */
	int32 RequestedPort = 0;

	/** Pipeline queue and payload settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NativeWebWebSocket|Pipeline")
	/** @brief Reflected data field `InPipelineConfig`. */
	FNativeWebWebSocketPipelineConfig InPipelineConfig;
};

/** @brief Result returned after attempting to create a NativeWebWebSocket WebSocket server. */
USTRUCT(BlueprintType)
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketServerCreateResult
{
	GENERATED_BODY()

	/** Creation outcome. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket")
	/** @brief Reflected data field `Result`. */
	ENativeWebWebSocketCreateResult Result = ENativeWebWebSocketCreateResult::Failed;

	/** Actual listening port, when creation succeeds. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket")
	/** @brief Reflected data field `BoundPort`. */
	int32 BoundPort = 0;
};

/** @brief Information about a connected WebSocket client. */
USTRUCT(BlueprintType)
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketClientInfo
{
	GENERATED_BODY()

	/** Server-assigned client identifier. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket")
	/** @brief Reflected data field `ClientId`. */
	int64 ClientId = 0;

	/** Remote network address. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket")
	/** @brief Reflected data field `RemoteAddress`. */
	FString RemoteAddress;

	/** Time at which the client connected. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket")
	/** @brief Reflected data field `ConnectedAt`. */
	FDateTime ConnectedAt = FDateTime::MinValue();
};

/** @brief Runtime counters and queue metrics for the WebSocket server. */
USTRUCT(BlueprintType)
struct NATIVEWEBWEBSOCKETSERVER_API FNativeWebWebSocketServerStats
{
	GENERATED_BODY()

	/** Number of currently connected clients. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `ActiveClients`. */
	int32 ActiveClients = 0;

	/** Total bytes received. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `RxBytes`. */
	int64 RxBytes = 0;

	/** Total bytes transmitted. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `TxBytes`. */
	int64 TxBytes = 0;

	/** Number of messages dropped by the pipeline. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `DroppedMessages`. */
	int64 DroppedMessages = 0;

	/** Aggregate queue depth. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `QueueDepth`. */
	int64 QueueDepth = 0;

	/** Current receive throughput in bytes per second. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `RxBytesPerSec`. */
	float RxBytesPerSec = 0.0f;

	/** Current transmit throughput in bytes per second. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `TxBytesPerSec`. */
	float TxBytesPerSec = 0.0f;

	/** Average aggregate queue depth. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `AvgQueueDepth`. */
	float AvgQueueDepth = 0.0f;

	/** Current inbound queue depth. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `InInboundQueueDepth`. */
	int64 InInboundQueueDepth = 0;

	/** Current handler queue depth. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `InHandleQueueDepth`. */
	int64 InHandleQueueDepth = 0;

	/** Current server send queue depth. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `InSendQueueDepth`. */
	int64 InSendQueueDepth = 0;

	/** Current per-client write queue depth. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `InWriteQueueDepth`. */
	int64 InWriteQueueDepth = 0;

	/** Current game-thread event queue depth. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `GameThreadEventQueueDepth`. */
	int64 GameThreadEventQueueDepth = 0;

	/** Number of dropped game-thread events. */
	UPROPERTY(BlueprintReadOnly, Category = "NativeWebWebSocket|Stats")
	/** @brief Reflected data field `DroppedGameThreadEvents`. */
	int64 DroppedGameThreadEvents = 0;
};
