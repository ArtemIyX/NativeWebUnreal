

#include "Threads/NativeWebWebSocketHandleRunnable.h"

#include "Config/NativeWebWebSocketCVars.h"
#include "Server/NativeWebWebSocketServerInstance.h"
#include "HAL/Event.h"
#include "Math/UnrealMathUtility.h"

FNativeWebWebSocketHandleRunnable::FNativeWebWebSocketHandleRunnable(FNativeWebWebSocketServerInstance* InOwner, FEvent* InWakeEvent)
	: Owner(InOwner)
	, WakeEvent(InWakeEvent)
{
}

uint32 FNativeWebWebSocketHandleRunnable::Run()
{
	while (!bStopRequested.Load())
	{
		const bool bDidWork = Owner ? Owner->PumpInboundOnHandleThread() : false;
		if (!bDidWork && WakeEvent)
		{
			const uint32 waitMs = static_cast<uint32>(FMath::Max(1, NativeWebWebSocketCVars::GetWriteIdleSleepMs()));
			WakeEvent->Wait(waitMs);
		}
	}
	return 0;
}

void FNativeWebWebSocketHandleRunnable::Stop()
{
	bStopRequested.Store(true);
	if (WakeEvent)
	{
		WakeEvent->Trigger();
	}
}
