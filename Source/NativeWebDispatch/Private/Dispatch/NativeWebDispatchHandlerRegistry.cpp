

#include "Dispatch/NativeWebDispatchHandlerRegistry.h"

bool FNativeWebDispatchHandlerRegistry::RegisterHandler(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchHandler InHandler, bool bInAllowReplace)
{
	if (!InHandler)
	{
		return false;
	}

	return RegisterHandler(
		InRouteKey,
		[Handler = MoveTemp(InHandler)](const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue,
			const INativeWebDispatchMetadata*, FString& OutError) mutable
		{
			return Handler(InRouteKey, InValue, OutError);
		},
		bInAllowReplace);
}

bool FNativeWebDispatchHandlerRegistry::RegisterHandler(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchMetadataHandler InHandler, bool bInAllowReplace)
{
	if (!InRouteKey.IsValid() || !InHandler)
	{
		return false;
	}

	FWriteScopeLock writeLock(HandlersLock);
	if (!bInAllowReplace && Handlers.Contains(InRouteKey))
	{
		return false;
	}

	Handlers.Add(InRouteKey, MoveTemp(InHandler));
	return true;
}

bool FNativeWebDispatchHandlerRegistry::UnregisterHandler(const FNativeWebDispatchRouteKey& InRouteKey)
{
	if (!InRouteKey.IsValid()) return false;
	FWriteScopeLock writeLock(HandlersLock);
	return Handlers.Remove(InRouteKey) > 0;
}

bool FNativeWebDispatchHandlerRegistry::HasHandler(const FNativeWebDispatchRouteKey& InRouteKey) const
{
	if (!InRouteKey.IsValid()) return false;
	FReadScopeLock readLock(HandlersLock);
	return Handlers.Contains(InRouteKey);
}

int32 FNativeWebDispatchHandlerRegistry::GetHandlerCount() const
{
	FReadScopeLock readLock(HandlersLock);
	return Handlers.Num();
}

ENativeWebDispatchHandlerResult FNativeWebDispatchHandlerRegistry::Handle(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue, FString& OutError) const
{
	return Handle(InRouteKey, InValue, nullptr, OutError);
}

ENativeWebDispatchHandlerResult FNativeWebDispatchHandlerRegistry::Handle(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue,
	TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const
{
	OutError.Empty();
	if (!InRouteKey.IsValid()) return ENativeWebDispatchHandlerResult::InvalidRouteKey;
	FNativeWebDispatchMetadataHandler routeHandler;
	{
		FReadScopeLock readLock(HandlersLock);
		const FNativeWebDispatchMetadataHandler* foundHandler = Handlers.Find(InRouteKey);
		if (!foundHandler)
		{
			OutError = FString::Printf(TEXT("No dispatch handler for route %s"), *InRouteKey.GetDiagnosticText());
			return ENativeWebDispatchHandlerResult::HandlerNotFound;
		}
		routeHandler = *foundHandler;
	}

	if (!routeHandler)
	{
		OutError = FString::Printf(TEXT("Invalid dispatch handler for route %s"), *InRouteKey.GetDiagnosticText());
		return ENativeWebDispatchHandlerResult::InvalidHandler;
	}

	if (!routeHandler(InRouteKey, InValue, InMetadata.Get(), OutError))
	{
		if (OutError == TEXT("NativeWebDispatch.TypedHandler.TypeMismatch"))
		{
			return ENativeWebDispatchHandlerResult::HandlerTypeMismatch;
		}

		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Dispatch handler failed for route %s"), *InRouteKey.GetDiagnosticText());
		}
		return ENativeWebDispatchHandlerResult::HandlerFailed;
	}

	return ENativeWebDispatchHandlerResult::Ok;
}

ENativeWebDispatchHandlerResult FNativeWebDispatchHandlerRegistry::Dispatch(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload, FString& OutError) const
{
	return Dispatch(InRouteKey, InPayload, nullptr, OutError);
}

ENativeWebDispatchHandlerResult FNativeWebDispatchHandlerRegistry::Dispatch(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload,
	TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const
{
	OutError.Empty();
	if (!InRouteKey.IsValid()) return ENativeWebDispatchHandlerResult::InvalidRouteKey;
	TSharedPtr<FNativeWebDispatchRegistry> decodeRegistry = GetDecodeRegistry();
	if (!decodeRegistry.IsValid())
	{
		OutError = TEXT("Dispatch decode registry is unavailable");
		return ENativeWebDispatchHandlerResult::DecodeRegistryUnavailable;
	}

	TUniquePtr<INativeWebDispatchValue> decodedValue;
	switch (decodeRegistry->Decode(InRouteKey, InPayload, decodedValue, OutError))
	{
	case ENativeWebDispatchFactoryResult::Ok:
		break;
	case ENativeWebDispatchFactoryResult::RouteNotFound:
		return ENativeWebDispatchHandlerResult::DecodeRouteNotFound;
	case ENativeWebDispatchFactoryResult::FactoryFailed:
	case ENativeWebDispatchFactoryResult::InvalidFactory:
	default:
		return ENativeWebDispatchHandlerResult::DecodeFailed;
	}

	if (!decodedValue.IsValid())
	{
		if (OutError.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Dispatch decode returned null for route %s"), *InRouteKey.GetDiagnosticText());
		}
		return ENativeWebDispatchHandlerResult::DecodeFailed;
	}

	return Handle(InRouteKey, *decodedValue, MoveTemp(InMetadata), OutError);
}
