#include "Subsystems/NativeWebWebSocketSubsystem.h"

#include "Config/NativeWebWebSocketCVars.h"
#include "Logging/NativeWebWebSocketLog.h"
#include "Server/NativeWebWebSocketServerBase.h"

UNativeWebWebSocketSubsystem::UNativeWebWebSocketSubsystem()
	: FTickableGameObject(ETickableTickType::NewObject)
{
}

bool UNativeWebWebSocketSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UGameInstance* GI = Cast<UGameInstance>(Outer))
	{
		if (GI->IsDedicatedServerInstance())
		{
			return false;
		}
	}
	return true;
}

void UNativeWebWebSocketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bInitialized = true;
	SetTickableTickType(ETickableTickType::Conditional);
}

void UNativeWebWebSocketSubsystem::Deinitialize()
{
	bInitialized = false;
	SetTickableTickType(ETickableTickType::Never);
	StopAllServers();
	Super::Deinitialize();
}

FNativeWebWebSocketServerCreateResult UNativeWebWebSocketSubsystem::CreateOrGetServer(
	const FNativeWebWebSocketServerCreateOptions& InOptions,
	TSubclassOf<UNativeWebWebSocketServerBase> InServerClass,
	TSubclassOf<UNativeWebWebSocketClientBase> InClientClass,
	UNativeWebWebSocketServerBase*& OutServer)
{
	check(IsInGameThread());
	FNativeWebWebSocketServerCreateResult out;
	out.Result = ENativeWebWebSocketCreateResult::Failed;
	out.BoundPort = 0;
	OutServer = nullptr;

	if (InOptions.NameId.IsNone())
	{
		UE_LOG(LogNativeWebWebSocketServer, Error, TEXT("CreateOrGetServer: NameId is None"));
		return out;
	}

	if (UNativeWebWebSocketServerBase* Existing = GetServer(InOptions.NameId))
	{
		OutServer = Existing;
		out.Result = ENativeWebWebSocketCreateResult::ReturnedExisting;
		out.BoundPort = Existing->GetBoundPort();
		return out;
	}

	int32 basePort = 0;
	bool bInitialAdjusted = false;
	if (!TryResolvePort(InOptions.RequestedPort, basePort, bInitialAdjusted))
	{
		UE_LOG(LogNativeWebWebSocketServer, Error, TEXT("CreateOrGetServer: Failed to resolve base port for %s"), *InOptions.NameId.ToString());
		return out;
	}

	const int32 maxScan = FMath::Max(1, NativeWebWebSocketCVars::GetMaxPortScan());
	UClass* ServerUClass = InServerClass ? InServerClass.Get() : UNativeWebWebSocketServerBase::StaticClass();

	for (int32 Attempt = 0; Attempt <= maxScan; ++Attempt)
	{
		const int32 candidatePort = basePort + Attempt;
		UNativeWebWebSocketServerBase* NewServer = NewObject<UNativeWebWebSocketServerBase>(this, ServerUClass);
		if (!NewServer)
		{
			continue;
		}

		if (NewServer->StartServerInternal(InOptions.NameId, candidatePort, InClientClass,
		                                   InOptions.InPipelineConfig))
		{
			Servers.Add(InOptions.NameId, NewServer);
			OutServer = NewServer;
			out.BoundPort = NewServer->GetBoundPort();
			out.Result = (Attempt > 0 || bInitialAdjusted) ? ENativeWebWebSocketCreateResult::PortAutoAdjusted : ENativeWebWebSocketCreateResult::Created;
			UE_LOG(LogNativeWebWebSocketServer, Log, TEXT("CreateOrGetServer: Started '%s' on port %d"), *InOptions.NameId.ToString(), out.BoundPort);
			
			check(IsInGameThread());
			NewServer->ServerInitialized(this);
			return out;
		}
	}

	UE_LOG(LogNativeWebWebSocketServer, Error, TEXT("CreateOrGetServer: Failed to start '%s' after %d attempts"), *InOptions.NameId.ToString(), maxScan + 1);
	return out;
}

UNativeWebWebSocketServerBase* UNativeWebWebSocketSubsystem::GetServer(FName InNameId) const
{
	if (const TObjectPtr<UNativeWebWebSocketServerBase>* Found = Servers.Find(InNameId))
	{
		return *Found;
	}
	return nullptr;
}

bool UNativeWebWebSocketSubsystem::StopServer(FName InNameId)
{
	check(IsInGameThread());
	TObjectPtr<UNativeWebWebSocketServerBase>* Found = Servers.Find(InNameId);
	if (!Found || !*Found)
	{
		return false;
	}

	(*Found)->StopServer();
	Servers.Remove(InNameId);
	return true;
}

void UNativeWebWebSocketSubsystem::StopAllServers()
{
	check(IsInGameThread());
	for (const TPair<FName, TObjectPtr<UNativeWebWebSocketServerBase>>& Pair : Servers)
	{
		if (Pair.Value)
		{
			Pair.Value->StopServer();
		}
	}
	Servers.Empty();
	RoundRobinStartIndex = 0;
}

void UNativeWebWebSocketSubsystem::Tick(float InDeltaTime)
{
	check(IsInGameThread());
	(void)InDeltaTime;

	int32 remainingBudget = FMath::Max(0, NativeWebWebSocketCVars::GetGameThreadEventsPerTick());
	if (remainingBudget == 0)
	{
		return;
	}

	const TArray<UNativeWebWebSocketServerBase*> servers = GetAllServers();
	if (servers.IsEmpty())
	{
		RoundRobinStartIndex = 0;
		return;
	}

	const int32 serverCount = servers.Num();
	const int32 startIndex = RoundRobinStartIndex % serverCount;
	while (remainingBudget > 0)
	{
		bool bDispatchedAny = false;
		for (int32 offset = 0; offset < serverCount && remainingBudget > 0; ++offset)
		{
			UNativeWebWebSocketServerBase* server = servers[(startIndex + offset) % serverCount];
			if (!IsValid(server))
			{
				continue;
			}
			if (server->ConsumeControlEventOverflowStopRequest())
			{
				// A lost lifecycle event is more dangerous than dropping the affected server.
				server->StopServer();
				continue;
			}
			if (server->GetGameThreadEventQueueDepth() <= 0)
			{
				continue;
			}

			const int32 drainedCount = server->DrainGameThreadEvents(1);
			remainingBudget -= drainedCount;
			bDispatchedAny |= drainedCount > 0;
		}

		if (!bDispatchedAny)
		{
			break;
		}
	}

	RoundRobinStartIndex = (startIndex + 1) % serverCount;
}

TStatId UNativeWebWebSocketSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNativeWebWebSocketSubsystem, STATGROUP_Tickables);
}

bool UNativeWebWebSocketSubsystem::IsTickable() const
{
	return bInitialized && !IsTemplate() && GetGameInstance() != nullptr;
}

UWorld* UNativeWebWebSocketSubsystem::GetTickableGameObjectWorld() const
{
	// A game-instance subsystem owns one global dispatch budget across its worlds.
	return nullptr;
}

TArray<UNativeWebWebSocketServerBase*> UNativeWebWebSocketSubsystem::GetAllServers() const
{
	TArray<UNativeWebWebSocketServerBase*> out;
	out.Reserve(Servers.Num());
	for (const TPair<FName, TObjectPtr<UNativeWebWebSocketServerBase>>& Pair : Servers)
	{
		if (Pair.Value)
		{
			out.Add(Pair.Value);
		}
	}
	return out;
}

bool UNativeWebWebSocketSubsystem::TryResolvePort(int32 InRequestedPort, int32& OutResolvedPort, bool& bOutAdjusted) const
{
	OutResolvedPort = FMath::Max(1, InRequestedPort);
	bOutAdjusted = false;

	if (InRequestedPort <= 0)
	{
		OutResolvedPort = 7001;
		bOutAdjusted = true;
	}

	return true;
}
