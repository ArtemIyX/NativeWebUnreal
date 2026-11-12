

#include "Threads/NativeWebWebSocketWriteRunnable.h"

#include "Config/NativeWebWebSocketCVars.h"
#include "Server/NativeWebWebSocketServerInstance.h"
#include "HAL/Event.h"
#include "Math/UnrealMathUtility.h"

FNativeWebWebSocketWriteRunnable::FNativeWebWebSocketWriteRunnable(FNativeWebWebSocketServerInstance* InOwner, FEvent* InWakeEvent)
	: Owner(InOwner)
	, WakeEvent(InWakeEvent)
{
}

uint32 FNativeWebWebSocketWriteRunnable::Run()
{
	while (!bStopRequested.Load())
	{
		const bool bDidWork = Owner ? Owner->PumpOutgoingOnWriteThread() : false;
		if (!bDidWork && WakeEvent)
		{
			const uint32 waitMs = static_cast<uint32>(FMath::Max(1, NativeWebWebSocketCVars::GetWriteIdleSleepMs()));
			WakeEvent->Wait(waitMs);
		}
	}
	return 0;
}

void FNativeWebWebSocketWriteRunnable::Stop()
{
	bStopRequested.Store(true);
	if (WakeEvent)
	{
		WakeEvent->Trigger();
	}
}
