

#include "Threads/NativeWebWebSocketReadRunnable.h"

#include "Config/NativeWebWebSocketCVars.h"
#include "Server/NativeWebWebSocketServerInstance.h"
#include "HAL/PlatformProcess.h"
#include "Math/UnrealMathUtility.h"

FNativeWebWebSocketReadRunnable::FNativeWebWebSocketReadRunnable(FNativeWebWebSocketServerInstance* InOwner)
	: Owner(InOwner)
{
}

uint32 FNativeWebWebSocketReadRunnable::Run()
{
	int32 idleSleepMs = FMath::Max(1, NativeWebWebSocketCVars::GetReadBusySleepMs());
	while (!bStopRequested.Load())
	{
		bool bHadActivity = false;
		if (Owner)
		{
			bHadActivity = Owner->TickBackendOnReadThread();
		}

		const int32 busySleepMs = FMath::Max(1, NativeWebWebSocketCVars::GetReadBusySleepMs());
		const int32 idleMaxSleepMs = FMath::Max(busySleepMs, NativeWebWebSocketCVars::GetReadIdleMaxSleepMs());
		if (bHadActivity)
		{
			idleSleepMs = busySleepMs;
		}
		else
		{
			idleSleepMs = FMath::Min(idleMaxSleepMs, idleSleepMs * 2);
		}

		FPlatformProcess::SleepNoStats(static_cast<float>(idleSleepMs) / 1000.0f);
	}
	return 0;
}

void FNativeWebWebSocketReadRunnable::Stop()
{
	bStopRequested.Store(true);
}
