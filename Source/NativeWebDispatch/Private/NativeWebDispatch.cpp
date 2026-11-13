

#include "NativeWebDispatch.h"
#include "Dispatch/NativeWebDispatchRegistry.h"

namespace
{

	struct FNativeWebDispatchDeferredFactoryEntry
	{
		FNativeWebDispatchRouteKey RouteKey;
		FNativeWebDispatchRegistry::FNativeWebDispatchFactory Factory;
		bool bAllowReplace = false;
	};

	FCriticalSection GDeferredFactoriesLock;
	TArray<FNativeWebDispatchDeferredFactoryEntry> GDeferredFactories;
}

#define LOCTEXT_NAMESPACE "FNativeWebDispatchModule"

FNativeWebDispatchModule& FNativeWebDispatchModule::Get()
{
	return FModuleManager::LoadModuleChecked<FNativeWebDispatchModule>("NativeWebDispatch");
}

bool FNativeWebDispatchModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("NativeWebDispatch");
}

void FNativeWebDispatchModule::RegisterDeferredFactory(FNativeWebDispatchRouteKey InRouteKey, FNativeWebDispatchRegistry::FNativeWebDispatchFactory InFactory,
	bool bInAllowReplace)
{
	if (IsAvailable())
	{
		FNativeWebDispatchModule& module = Get();
		if (module.DispatchRegistry.IsValid())
		{
			module.DispatchRegistry->RegisterFactory(InRouteKey, MoveTemp(InFactory), bInAllowReplace);
			return;
		}
	}

	FNativeWebDispatchDeferredFactoryEntry deferredEntry;
	deferredEntry.RouteKey = MoveTemp(InRouteKey);
	deferredEntry.Factory = MoveTemp(InFactory);
	deferredEntry.bAllowReplace = bInAllowReplace;



	FScopeLock lock(&GDeferredFactoriesLock);
	GDeferredFactories.Add(MoveTemp(deferredEntry));
}

void FNativeWebDispatchModule::StartupModule()
{
	DispatchRegistry = MakeShared<FNativeWebDispatchRegistry>();

	TArray<FNativeWebDispatchDeferredFactoryEntry> pendingEntries;
	{


		FScopeLock lock(&GDeferredFactoriesLock);
		pendingEntries = MoveTemp(GDeferredFactories);
		GDeferredFactories.Reset();
	}

	for (FNativeWebDispatchDeferredFactoryEntry& entry : pendingEntries)
	{
		DispatchRegistry->RegisterFactory(entry.RouteKey, MoveTemp(entry.Factory), entry.bAllowReplace);
	}

}

void FNativeWebDispatchModule::ShutdownModule()
{
	DispatchRegistry.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNativeWebDispatchModule, NativeWebDispatch)
