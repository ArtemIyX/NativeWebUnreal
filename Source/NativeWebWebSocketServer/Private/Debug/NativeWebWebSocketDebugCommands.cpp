#include "Debug/NativeWebWebSocketDebugCommands.h"

#include "Config/NativeWebWebSocketCVars.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Logging/NativeWebWebSocketLog.h"
#include "Server/NativeWebWebSocketClientBase.h"
#include "Server/NativeWebWebSocketServerBase.h"
#include "Subsystems/NativeWebWebSocketSubsystem.h"

void FNativeWebWebSocketDebugCommands::Startup()
{
	IConsoleManager& consoleManager = IConsoleManager::Get();

	if (!CommandList)
	{
		CommandList = consoleManager.RegisterConsoleCommand(
			TEXT("ws.list"),
			TEXT("List websocket servers."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNativeWebWebSocketDebugCommands::HandleList));
	}

	if (!CommandStop)
	{
		CommandStop = consoleManager.RegisterConsoleCommand(
			TEXT("ws.stop"),
			TEXT("Stop websocket server by name: ws.stop <name>."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNativeWebWebSocketDebugCommands::HandleStop));
	}

	if (!CommandKick)
	{
		CommandKick = consoleManager.RegisterConsoleCommand(
			TEXT("ws.kick"),
			TEXT("Disconnect websocket client: ws.kick <server> <clientId>."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNativeWebWebSocketDebugCommands::HandleKick));
	}

	if (!CommandStats)
	{
		CommandStats = consoleManager.RegisterConsoleCommand(
			TEXT("ws.stats"),
			TEXT("Print websocket server stats: ws.stats <name>."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNativeWebWebSocketDebugCommands::HandleStats));
	}

	if (!CommandClients)
	{
		CommandClients = consoleManager.RegisterConsoleCommand(
			TEXT("ws.clients"),
			TEXT("Print websocket clients: ws.clients <server>."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNativeWebWebSocketDebugCommands::HandleClients));
	}

	if (!CommandCVars)
	{
		CommandCVars = consoleManager.RegisterConsoleCommand(
			TEXT("ws.cvars"),
			TEXT("Print websocket runtime cvar values."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNativeWebWebSocketDebugCommands::HandleCVars));
	}

	if (!CommandBenchSend)
	{
		CommandBenchSend = consoleManager.RegisterConsoleCommand(
			TEXT("ws.benchsend"),
			TEXT("Send synthetic load traffic: ws.benchsend <name> <count> <bytes>."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNativeWebWebSocketDebugCommands::HandleBenchSend));
	}
}

void FNativeWebWebSocketDebugCommands::Shutdown()
{
	IConsoleManager& consoleManager = IConsoleManager::Get();

	if (CommandList)
	{
		consoleManager.UnregisterConsoleObject(CommandList);
		CommandList = nullptr;
	}
	if (CommandStop)
	{
		consoleManager.UnregisterConsoleObject(CommandStop);
		CommandStop = nullptr;
	}
	if (CommandKick)
	{
		consoleManager.UnregisterConsoleObject(CommandKick);
		CommandKick = nullptr;
	}
	if (CommandStats)
	{
		consoleManager.UnregisterConsoleObject(CommandStats);
		CommandStats = nullptr;
	}
	if (CommandClients)
	{
		consoleManager.UnregisterConsoleObject(CommandClients);
		CommandClients = nullptr;
	}
	if (CommandCVars)
	{
		consoleManager.UnregisterConsoleObject(CommandCVars);
		CommandCVars = nullptr;
	}
	if (CommandBenchSend)
	{
		consoleManager.UnregisterConsoleObject(CommandBenchSend);
		CommandBenchSend = nullptr;
	}
}

UNativeWebWebSocketSubsystem* FNativeWebWebSocketDebugCommands::ResolveSubsystem() const
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& worldContext : GEngine->GetWorldContexts())
	{
		UGameInstance* gameInstance = worldContext.OwningGameInstance;
		if (!gameInstance)
		{
			continue;
		}

		if (UNativeWebWebSocketSubsystem* subsystem = gameInstance->GetSubsystem<UNativeWebWebSocketSubsystem>())
		{
			return subsystem;
		}
	}

	return nullptr;
}

void FNativeWebWebSocketDebugCommands::HandleList(const TArray<FString>& InArgs) const
{
	(void)InArgs;
	UNativeWebWebSocketSubsystem* subsystem = ResolveSubsystem();
	if (!subsystem)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.list: subsystem not available"));
		return;
	}

	const TArray<UNativeWebWebSocketServerBase*> servers = subsystem->GetAllServers();
	UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("ws.list: %d server(s)"), servers.Num());
	for (UNativeWebWebSocketServerBase* server : servers)
	{
		if (!server)
		{
			continue;
		}
		const FNativeWebWebSocketServerStats stats = server->GetStats();
		UE_LOG(
			LogNativeWebWebSocketServer,
			Log,
			TEXT("  name=%s port=%d running=%s format=%d clients=%d queue=%lld inbound=%lld send=%lld write=%lld game_events=%lld dropped_game_events=%lld"),
			*server->GetServerNameId().ToString(),
			server->GetBoundPort(),
			server->IsRunning() ? TEXT("true") : TEXT("false"),
			static_cast<int32>(server->GetPayloadFormat()),
			stats.ActiveClients,
			stats.QueueDepth,
			stats.InInboundQueueDepth,
			stats.InSendQueueDepth,
			stats.InWriteQueueDepth,
			stats.GameThreadEventQueueDepth,
			stats.DroppedGameThreadEvents);
	}
}

void FNativeWebWebSocketDebugCommands::HandleStop(const TArray<FString>& InArgs) const
{
	if (InArgs.Num() < 1)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.stop usage: ws.stop <name>"));
		return;
	}

	UNativeWebWebSocketSubsystem* subsystem = ResolveSubsystem();
	if (!subsystem)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.stop: subsystem not available"));
		return;
	}

	const FName nameId(*InArgs[0]);
	const bool bStopped = subsystem->StopServer(nameId);
	UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("ws.stop: name=%s stopped=%s"), *nameId.ToString(), bStopped ? TEXT("true") : TEXT("false"));
}

void FNativeWebWebSocketDebugCommands::HandleKick(const TArray<FString>& InArgs) const
{
	if (InArgs.Num() < 2)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.kick usage: ws.kick <server> <clientId>"));
		return;
	}

	UNativeWebWebSocketSubsystem* subsystem = ResolveSubsystem();
	if (!subsystem)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.kick: subsystem not available"));
		return;
	}

	const FName nameId(*InArgs[0]);
	UNativeWebWebSocketServerBase* server = subsystem->GetServer(nameId);
	if (!server)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.kick: server not found for '%s'"), *nameId.ToString());
		return;
	}

	int64 clientId = 0;
	if (!LexTryParseString(clientId, *InArgs[1]))
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.kick: invalid client id '%s'"), *InArgs[1]);
		return;
	}

	const ENativeWebWebSocketSendResult result = server->DisconnectClient(clientId, ENativeWebWebSocketCloseReason::Kicked);
	UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("ws.kick: server=%s client=%lld result=%d"), *nameId.ToString(), clientId, static_cast<int32>(result));
}

void FNativeWebWebSocketDebugCommands::HandleStats(const TArray<FString>& InArgs) const
{
	if (InArgs.Num() < 1)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.stats usage: ws.stats <name>"));
		return;
	}

	UNativeWebWebSocketSubsystem* subsystem = ResolveSubsystem();
	if (!subsystem)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.stats: subsystem not available"));
		return;
	}

	const FName nameId(*InArgs[0]);
	UNativeWebWebSocketServerBase* server = subsystem->GetServer(nameId);
	if (!server)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.stats: server not found for '%s'"), *nameId.ToString());
		return;
	}

	const FNativeWebWebSocketServerStats stats = server->GetStats();
	UE_LOG(
		LogNativeWebWebSocketServer,
		Log,
		TEXT("ws.stats: name=%s format=%d clients=%d rx=%lld tx=%lld rx_s=%.2f tx_s=%.2f dropped=%lld queue=%lld avg_queue=%.2f inbound=%lld handle=%lld send=%lld write=%lld game_events=%lld dropped_game_events=%lld"),
		*nameId.ToString(),
		static_cast<int32>(server->GetPayloadFormat()),
		stats.ActiveClients,
		stats.RxBytes,
		stats.TxBytes,
		stats.RxBytesPerSec,
		stats.TxBytesPerSec,
		stats.DroppedMessages,
		stats.QueueDepth,
		stats.AvgQueueDepth,
		stats.InInboundQueueDepth,
		stats.InHandleQueueDepth,
		stats.InSendQueueDepth,
		stats.InWriteQueueDepth,
		stats.GameThreadEventQueueDepth,
		stats.DroppedGameThreadEvents);
}

void FNativeWebWebSocketDebugCommands::HandleClients(const TArray<FString>& InArgs) const
{
	if (InArgs.Num() < 1)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.clients usage: ws.clients <name>"));
		return;
	}

	UNativeWebWebSocketSubsystem* subsystem = ResolveSubsystem();
	if (!subsystem)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.clients: subsystem not available"));
		return;
	}

	const FName nameId(*InArgs[0]);
	UNativeWebWebSocketServerBase* server = subsystem->GetServer(nameId);
	if (!server)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.clients: server not found for '%s'"), *nameId.ToString());
		return;
	}

	const TArray<UNativeWebWebSocketClientBase*> clients = server->GetClients();
	UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("ws.clients: server=%s count=%d"), *nameId.ToString(), clients.Num());
	for (const UNativeWebWebSocketClientBase* client : clients)
	{
		if (!client)
		{
			continue;
		}
		UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("  client=%lld remote=%s connected=%s"),
			client->GetClientId(), *client->GetRemoteAddress(), *client->GetConnectedAt().ToString());
	}
}

void FNativeWebWebSocketDebugCommands::HandleCVars(const TArray<FString>& InArgs) const
{
	(void)InArgs;
	UE_LOG(LogNativeWebWebSocketServer, Log,
		TEXT("ws.cvars: max_msg=%d max_text=%d max_out=%d q_drop=%d in_q=%d send_q=%d write_q=%d game_events_per_tick=%d game_event_queue_max=%d hb=%.1f idle=%.1f"),
		NativeWebWebSocketCVars::GetMaxMessageBytes(),
		NativeWebWebSocketCVars::GetMaxTextMessageBytes(),
		NativeWebWebSocketCVars::GetMaxOutboundMessageBytes(),
		NativeWebWebSocketCVars::GetQueueDropPolicy(),
		NativeWebWebSocketCVars::GetMaxQueueMessagesPerClient(),
		NativeWebWebSocketCVars::GetWriteBatchMaxMessages(),
		NativeWebWebSocketCVars::GetWriteBatchMaxBytes(),
		NativeWebWebSocketCVars::GetGameThreadEventsPerTick(),
		NativeWebWebSocketCVars::GetGameThreadEventQueueMax(),
		NativeWebWebSocketCVars::GetHeartbeatIntervalSec(),
		NativeWebWebSocketCVars::GetIdleTimeoutSec());
}

void FNativeWebWebSocketDebugCommands::HandleBenchSend(const TArray<FString>& InArgs) const
{
	if (InArgs.Num() < 3)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.benchsend usage: ws.benchsend <name> <count> <bytes>"));
		return;
	}

	UNativeWebWebSocketSubsystem* subsystem = ResolveSubsystem();
	if (!subsystem)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.benchsend: subsystem not available"));
		return;
	}

	const FName nameId(*InArgs[0]);
	UNativeWebWebSocketServerBase* server = subsystem->GetServer(nameId);
	if (!server)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.benchsend: server not found for '%s'"), *nameId.ToString());
		return;
	}

	int32 count = 0;
	int32 bytesPerMessage = 0;
	if (!LexTryParseString(count, *InArgs[1]) || !LexTryParseString(bytesPerMessage, *InArgs[2]) ||
		count <= 0 || bytesPerMessage <= 0)
	{
		UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.benchsend: invalid count/bytes"));
		return;
	}

	TArray<uint8> payload;
	payload.SetNumZeroed(bytesPerMessage);
	for (int32 i = 0; i < count; ++i)
	{
		payload[0] = static_cast<uint8>(i & 0xFF);
		const ENativeWebWebSocketSendResult result = server->BroadcastBytes(payload);
		if (result != ENativeWebWebSocketSendResult::Ok)
		{
			UE_LOG(LogNativeWebWebSocketServer, Warning, TEXT("ws.benchsend: send failed at i=%d result=%d"), i, static_cast<int32>(result));
			return;
		}
	}

	UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("ws.benchsend: server=%s sent=%d bytes_each=%d"),
		*nameId.ToString(), count, bytesPerMessage);
}
