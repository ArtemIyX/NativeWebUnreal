/**
 * @file NativeWebDispatch\Public\Dispatch\NativeWebDispatchRegistry.h
 * @brief Declares NativeWebDispatchRegistry for module NativeWebDispatch\Public\Dispatch\NativeWebDispatchRegistry.h.
 * @details Contains dispatch registry and value plumbing used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Dispatch/NativeWebDispatchRouteKey.h"
#include "Dispatch/NativeWebDispatchValue.h"

/** @brief Type declaration. */
enum class ENativeWebDispatchFactoryResult : uint8
{
	Ok,
	RouteNotFound,
	FactoryFailed,
	InvalidFactory,
	InvalidRouteKey
};

inline FString ToString(ENativeWebDispatchFactoryResult InEnumValue)
{
	switch (InEnumValue)
	{
		case ENativeWebDispatchFactoryResult::Ok:
			return "Ok";
		case ENativeWebDispatchFactoryResult::RouteNotFound:
			return "RouteNotFound";
		case ENativeWebDispatchFactoryResult::FactoryFailed:
			return "FactoryFailed";
		case ENativeWebDispatchFactoryResult::InvalidFactory:
			return "InvalidFactory";
		case ENativeWebDispatchFactoryResult::InvalidRouteKey:
			return "InvalidRouteKey";
		default: ;
	}
	return "INVALID";
}

/** @brief Type declaration. */
class NATIVEWEBDISPATCH_API FNativeWebDispatchRegistry
{
public:
	using FNativeWebDispatchFactory = TFunction<TUniquePtr<INativeWebDispatchValue>(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload, FString& OutError)>;

	/** @brief RegisterFactory API. */
	bool RegisterFactory(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchFactory InFactory, bool bInAllowReplace = false);

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterFactory(TKey&& InKey, FNativeWebDispatchFactory InFactory, bool bInAllowReplace = false)
	{
		return RegisterFactory(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MoveTemp(InFactory), bInAllowReplace);
	}

	/** @brief UnregisterFactory API. */
	bool UnregisterFactory(const FNativeWebDispatchRouteKey& InRouteKey);

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool UnregisterFactory(TKey&& InKey) { return UnregisterFactory(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey))); }

	/** @brief HasFactory API. */
	bool HasFactory(const FNativeWebDispatchRouteKey& InRouteKey) const;

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool HasFactory(TKey&& InKey) const { return HasFactory(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey))); }

	/** @brief GetFactoryCount API. */
	int32 GetFactoryCount() const;

	ENativeWebDispatchFactoryResult Decode(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload,
		TUniquePtr<INativeWebDispatchValue>& OutValue, FString& OutError) const;

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchFactoryResult Decode(TKey&& InKey, const TArray<uint8>& InPayload,
		TUniquePtr<INativeWebDispatchValue>& OutValue, FString& OutError) const
	{
		return Decode(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InPayload, OutValue, OutError);
	}

private:
	/** @brief RoutesLock state. */
	mutable FRWLock RoutesLock;
	/** @brief Routes state. */
	TMap<FNativeWebDispatchRouteKey, FNativeWebDispatchFactory> Routes;
};