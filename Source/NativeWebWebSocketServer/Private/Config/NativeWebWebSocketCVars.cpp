#include "Config/NativeWebWebSocketCVars.h"

#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxMessageBytes(TEXT("cefws.max_message_bytes"), 1024 * 1024, TEXT("Maximum accepted websocket message size in bytes."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxTextMessageBytes(TEXT("cefws.max_text_message_bytes"), 512 * 1024, TEXT("Maximum accepted text websocket message size in bytes."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxOutboundMessageBytes(TEXT("cefws.max_outbound_message_bytes"), 1024 * 1024, TEXT("Maximum outbound websocket payload size in bytes."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxRxBytesPerSecPerClient(TEXT("cefws.max_rx_bytes_per_sec_per_client"), 4 * 1024 * 1024, TEXT("Per-client inbound bandwidth cap (bytes/sec). <=0 disables."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxTxBytesPerSecPerClient(TEXT("cefws.max_tx_bytes_per_sec_per_client"), 4 * 1024 * 1024, TEXT("Per-client outbound bandwidth cap (bytes/sec). <=0 disables."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxQueueMessagesPerClient(TEXT("cefws.max_queue_messages_per_client"), 1024, TEXT("Maximum queued outbound messages per client."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxQueueBytesPerClient(TEXT("cefws.max_queue_bytes_per_client"), 8 * 1024 * 1024, TEXT("Maximum queued outbound bytes per client."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketQueueDropPolicy(TEXT("cefws.queue_drop_policy"), 0, TEXT("Queue overflow policy: 0=drop_oldest, 1=reject_new."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketWriteBatchMaxMessages(TEXT("cefws.write_batch_max_messages"), 16, TEXT("Maximum messages drained per client on one write pass."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketWriteBatchMaxBytes(TEXT("cefws.write_batch_max_bytes"), 128 * 1024, TEXT("Maximum bytes drained per client on one write pass."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketReadBusySleepMs(TEXT("cefws.read_busy_sleep_ms"), 1, TEXT("Read thread sleep in milliseconds when activity is detected."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketReadIdleMaxSleepMs(TEXT("cefws.read_idle_max_sleep_ms"), 16, TEXT("Read thread max backoff sleep in milliseconds during idle periods."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketShutdownDrainMs(TEXT("cefws.shutdown_drain_ms"), 100, TEXT("Drain window before stopping websocket threads (milliseconds)."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketWriteIdleSleepMs(TEXT("cefws.write_idle_sleep_ms"), 2, TEXT("Write thread idle wait in milliseconds."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketShutdownTimeoutMs(TEXT("cefws.shutdown_timeout_ms"), 1000, TEXT("Server shutdown grace timeout in milliseconds."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketMaxPortScan(TEXT("cefws.max_port_scan"), 32, TEXT("Maximum ports to probe for auto-next-port behavior."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketGameThreadEventsPerTick(TEXT("cefws.game_thread_events_per_tick"), 512, TEXT("Maximum game-thread websocket events dispatched per frame."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketGameThreadEventQueueMax(TEXT("cefws.game_thread_event_queue_max"), 4096, TEXT("Maximum queued game-thread websocket events per server."));
	TAutoConsoleVariable<float> CVarNativeWebWebSocketHeartbeatIntervalSec(TEXT("cefws.heartbeat_interval_sec"), 0.0f, TEXT("Heartbeat send interval in seconds. <=0 disables heartbeat."));
	TAutoConsoleVariable<float> CVarNativeWebWebSocketIdleTimeoutSec(TEXT("cefws.idle_timeout_sec"), 60.0f, TEXT("Client idle timeout in seconds. <=0 disables timeout."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketValidateUtf8(TEXT("cefws.validate_utf8"), 1, TEXT("Validate UTF-8 payloads when processing text frames."));
	TAutoConsoleVariable<int32> CVarNativeWebWebSocketLogTraffic(TEXT("cefws.log_traffic"), 0, TEXT("Enable traffic logging for websocket payload flow."));
}

namespace NativeWebWebSocketCVars
{
	int32 GetMaxMessageBytes() { return CVarNativeWebWebSocketMaxMessageBytes.GetValueOnAnyThread(); }
	int32 GetMaxTextMessageBytes() { return CVarNativeWebWebSocketMaxTextMessageBytes.GetValueOnAnyThread(); }
	int32 GetMaxOutboundMessageBytes() { return CVarNativeWebWebSocketMaxOutboundMessageBytes.GetValueOnAnyThread(); }
	int32 GetMaxRxBytesPerSecPerClient() { return CVarNativeWebWebSocketMaxRxBytesPerSecPerClient.GetValueOnAnyThread(); }
	int32 GetMaxTxBytesPerSecPerClient() { return CVarNativeWebWebSocketMaxTxBytesPerSecPerClient.GetValueOnAnyThread(); }
	int32 GetMaxQueueMessagesPerClient() { return CVarNativeWebWebSocketMaxQueueMessagesPerClient.GetValueOnAnyThread(); }
	int32 GetMaxQueueBytesPerClient() { return CVarNativeWebWebSocketMaxQueueBytesPerClient.GetValueOnAnyThread(); }
	int32 GetQueueDropPolicy() { return CVarNativeWebWebSocketQueueDropPolicy.GetValueOnAnyThread(); }
	int32 GetWriteBatchMaxMessages() { return CVarNativeWebWebSocketWriteBatchMaxMessages.GetValueOnAnyThread(); }
	int32 GetWriteBatchMaxBytes() { return CVarNativeWebWebSocketWriteBatchMaxBytes.GetValueOnAnyThread(); }
	int32 GetReadBusySleepMs() { return CVarNativeWebWebSocketReadBusySleepMs.GetValueOnAnyThread(); }
	int32 GetReadIdleMaxSleepMs() { return CVarNativeWebWebSocketReadIdleMaxSleepMs.GetValueOnAnyThread(); }
	int32 GetShutdownDrainMs() { return CVarNativeWebWebSocketShutdownDrainMs.GetValueOnAnyThread(); }
	int32 GetWriteIdleSleepMs() { return CVarNativeWebWebSocketWriteIdleSleepMs.GetValueOnAnyThread(); }
	int32 GetShutdownTimeoutMs() { return CVarNativeWebWebSocketShutdownTimeoutMs.GetValueOnAnyThread(); }
	int32 GetMaxPortScan() { return CVarNativeWebWebSocketMaxPortScan.GetValueOnAnyThread(); }
	int32 GetGameThreadEventsPerTick() { return CVarNativeWebWebSocketGameThreadEventsPerTick.GetValueOnAnyThread(); }
	int32 GetGameThreadEventQueueMax() { return CVarNativeWebWebSocketGameThreadEventQueueMax.GetValueOnAnyThread(); }
	float GetHeartbeatIntervalSec() { return CVarNativeWebWebSocketHeartbeatIntervalSec.GetValueOnAnyThread(); }
	float GetIdleTimeoutSec() { return CVarNativeWebWebSocketIdleTimeoutSec.GetValueOnAnyThread(); }
	bool GetValidateUtf8() { return CVarNativeWebWebSocketValidateUtf8.GetValueOnAnyThread() != 0; }
	bool GetLogTraffic() { return CVarNativeWebWebSocketLogTraffic.GetValueOnAnyThread() != 0; }
}
