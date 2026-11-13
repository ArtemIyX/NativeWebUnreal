/**
 * @file NativeWebWebSocketServer\Private\Config\NativeWebWebSocketCVars.h
 * @brief Declares NativeWebWebSocketCVars for module NativeWebWebSocketServer\Private\Config\NativeWebWebSocketCVars.h.
 * @details Contains websocket server components used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"

namespace NativeWebWebSocketCVars
{
	/** @brief GetMaxMessageBytes API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxMessageBytes();
 * @endcode
 */
	int32 GetMaxMessageBytes();
	/** @brief GetMaxTextMessageBytes API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxTextMessageBytes();
 * @endcode
 */
	int32 GetMaxTextMessageBytes();
	/** @brief GetMaxOutboundMessageBytes API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxOutboundMessageBytes();
 * @endcode
 */
	int32 GetMaxOutboundMessageBytes();
	/** @brief GetMaxRxBytesPerSecPerClient API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxRxBytesPerSecPerClient();
 * @endcode
 */
	int32 GetMaxRxBytesPerSecPerClient();
	/** @brief GetMaxTxBytesPerSecPerClient API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxTxBytesPerSecPerClient();
 * @endcode
 */
	int32 GetMaxTxBytesPerSecPerClient();
	/** @brief GetMaxQueueMessagesPerClient API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxQueueMessagesPerClient();
 * @endcode
 */
	int32 GetMaxQueueMessagesPerClient();
	/** @brief GetMaxQueueBytesPerClient API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxQueueBytesPerClient();
 * @endcode
 */
	int32 GetMaxQueueBytesPerClient();
	/** @brief GetQueueDropPolicy API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetQueueDropPolicy();
 * @endcode
 */
	int32 GetQueueDropPolicy();
	/** @brief GetWriteBatchMaxMessages API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetWriteBatchMaxMessages();
 * @endcode
 */
	int32 GetWriteBatchMaxMessages();
	/** @brief GetWriteBatchMaxBytes API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetWriteBatchMaxBytes();
 * @endcode
 */
	int32 GetWriteBatchMaxBytes();
	/** @brief GetReadBusySleepMs API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetReadBusySleepMs();
 * @endcode
 */
	int32 GetReadBusySleepMs();
	/** @brief GetReadIdleMaxSleepMs API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetReadIdleMaxSleepMs();
 * @endcode
 */
	int32 GetReadIdleMaxSleepMs();
	/** @brief GetShutdownDrainMs API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetShutdownDrainMs();
 * @endcode
 */
	int32 GetShutdownDrainMs();
	/** @brief GetWriteIdleSleepMs API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetWriteIdleSleepMs();
 * @endcode
 */
	int32 GetWriteIdleSleepMs();
	/** @brief GetShutdownTimeoutMs API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetShutdownTimeoutMs();
 * @endcode
 */
	int32 GetShutdownTimeoutMs();
	/** @brief GetMaxPortScan API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetMaxPortScan();
 * @endcode
 */
	int32 GetMaxPortScan();
	/** @brief Returns the maximum number of game-thread events dispatched per frame.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetGameThreadEventsPerTick();
 * @endcode
 */
	int32 GetGameThreadEventsPerTick();
	/** @brief Returns the maximum number of queued game-thread events per server.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetGameThreadEventQueueMax();
 * @endcode
 */
	int32 GetGameThreadEventQueueMax();
	/** @brief GetHeartbeatIntervalSec API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetHeartbeatIntervalSec();
 * @endcode
 */
	float GetHeartbeatIntervalSec();
	/** @brief GetIdleTimeoutSec API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetIdleTimeoutSec();
 * @endcode
 */
	float GetIdleTimeoutSec();
	/** @brief GetValidateUtf8 API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetValidateUtf8();
 * @endcode
 */
	bool GetValidateUtf8();
	/** @brief GetLogTraffic API.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Network stages run on worker threads; UObject events are marshalled to the game thread.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetLogTraffic();
 * @endcode
 */
	bool GetLogTraffic();
}

