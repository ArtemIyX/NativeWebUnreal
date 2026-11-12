

#include "Dispatch/NativeWebDispatchRegistry.h"

bool FNativeWebDispatchRegistry::RegisterFactory(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchFactory InFactory, bool bInAllowReplace)
{
	if (!InRouteKey.IsValid() || !InFactory)
	{
		return false;
	}

	FWriteScopeLock writeLock(RoutesLock);
	if (!bInAllowReplace && Routes.Contains(InRouteKey))
	{
		return false;
	}

	Routes.Add(InRouteKey, MoveTemp(InFactory));
	return true;
}

bool FNativeWebDispatchRegistry::UnregisterFactory(const FNativeWebDispatchRouteKey& InRouteKey)
{
	if (!InRouteKey.IsValid()) return false;
	FWriteScopeLock writeLock(RoutesLock);
	return Routes.Remove(InRouteKey) > 0;
}

bool FNativeWebDispatchRegistry::HasFactory(const FNativeWebDispatchRouteKey& InRouteKey) const
{
	if (!InRouteKey.IsValid()) return false;
	FReadScopeLock readLock(RoutesLock);
	return Routes.Contains(InRouteKey);
}

int32 FNativeWebDispatchRegistry::GetFactoryCount() const
{
	FReadScopeLock readLock(RoutesLock);
	return Routes.Num();
}

ENativeWebDispatchFactoryResult FNativeWebDispatchRegistry::Decode(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload,
                                                       TUniquePtr<INativeWebDispatchValue>& OutValue, FString& OutError) const
{
	if (!InRouteKey.IsValid())
	{
		OutValue.Reset();
		OutError = TEXT("Invalid dispatch route key");
		return ENativeWebDispatchFactoryResult::InvalidRouteKey;
	}
	FNativeWebDispatchFactory routeFactory;
	{
		FReadScopeLock readLock(RoutesLock);
		const FNativeWebDispatchFactory* foundFactory = Routes.Find(InRouteKey);
		if (!foundFactory)
		{
			OutValue.Reset();
			OutError = FString::Printf(TEXT("No dispatch factory for route %s"), *InRouteKey.GetDiagnosticText());
			return ENativeWebDispatchFactoryResult::RouteNotFound;
		}
		routeFactory = *foundFactory;
	}

	if (!routeFactory)
	{
		OutValue.Reset();
		OutError = FString::Printf(TEXT("Invalid dispatch factory for route %s"), *InRouteKey.GetDiagnosticText());
		return ENativeWebDispatchFactoryResult::InvalidFactory;
	}

	OutValue = routeFactory(InRouteKey, InPayload, OutError);
	if (!OutValue.IsValid())
	{
		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Factory returned null for route %s"), *InRouteKey.GetDiagnosticText());
		}
		return ENativeWebDispatchFactoryResult::FactoryFailed;
	}

	return ENativeWebDispatchFactoryResult::Ok;
}
