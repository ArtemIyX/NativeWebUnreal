#include "NativeWebWebSocketServer.h"

#include "Debug/NativeWebWebSocketDebugCommands.h"
#include "Logging/NativeWebWebSocketLog.h"

void FNativeWebWebSocketServerModule::StartupModule()
{
	DebugCommands = MakeUnique<FNativeWebWebSocketDebugCommands>();
	DebugCommands->Startup();
	UE_LOG(LogNativeWebWebSocket, Log, TEXT("NativeWebWebSocketServer module started"));
}

void FNativeWebWebSocketServerModule::ShutdownModule()
{
	if (DebugCommands)
	{
		DebugCommands->Shutdown();
		DebugCommands.Reset();
	}
	UE_LOG(LogNativeWebWebSocket, Log, TEXT("NativeWebWebSocketServer module shutdown"));
}

IMPLEMENT_MODULE(FNativeWebWebSocketServerModule, NativeWebWebSocketServer)
