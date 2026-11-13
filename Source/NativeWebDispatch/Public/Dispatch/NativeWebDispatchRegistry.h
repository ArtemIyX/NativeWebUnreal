/**
 * @file NativeWebDispatch\Public\Dispatch\NativeWebDispatchRegistry.h
 * @brief Declares NativeWebDispatchRegistry for module NativeWebDispatch\Public\Dispatch\NativeWebDispatchRegistry.h.
 * @details Contains dispatch registry and value plumbing used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Dispatch/NativeWebDispatchRouteKey.h"
#include "Dispatch/NativeWebDispatchValue.h"

/** @brief Outcome of resolving and invoking a route's decode factory. @ingroup NativeWebDispatch */
enum class ENativeWebDispatchFactoryResult : uint8
{
	/** @brief Ok enumeration value. */
	Ok,
	/** @brief RouteNotFound enumeration value. */
	RouteNotFound,
	/** @brief FactoryFailed enumeration value. */
	FactoryFailed,
	/** @brief InvalidFactory enumeration value. */
	InvalidFactory,
	/** @brief InvalidRouteKey enumeration value. */
	InvalidRouteKey
};

/**
 * @brief Converts the value to a diagnostic string.
* @param InEnumValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The resulting text or identifier.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ToString(InEnumValue);
 * @endcode
 */

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

/**
 * @brief Thread-safe registry mapping exact route keys to value decoders.
 * @details The registry owns copied factories behind a read/write lock. Decoding returns a typed value or a structured failure result.
 * @ingroup NativeWebDispatch
 */
class NATIVEWEBDISPATCH_API FNativeWebDispatchRegistry
{
public:
	using FNativeWebDispatchFactory = TFunction<TUniquePtr<INativeWebDispatchValue>(
		const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload, FString& OutError)>;

	/** @brief RegisterFactory API.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InFactory Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInAllowReplace Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RegisterFactory(InRouteKey, InFactory, bInAllowReplace);
 * @endcode
 */
	bool RegisterFactory(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchFactory InFactory,
	                     bool bInAllowReplace = false);

	/**
	 * @brief Registers factory.
	* @param InKey Exact route identity used to look up the matching factory or handler.
	* @param InFactory Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param bInAllowReplace Boolean option controlling this operation.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * RegisterFactory(InKey, InFactory, bInAllowReplace);
	 * @endcode
	 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterFactory(TKey&& InKey, FNativeWebDispatchFactory InFactory, bool bInAllowReplace = false)
	{
		return RegisterFactory(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MoveTemp(InFactory),
		                       bInAllowReplace);
	}

	/** @brief UnregisterFactory API.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * UnregisterFactory(InRouteKey);
 * @endcode
 */
	bool UnregisterFactory(const FNativeWebDispatchRouteKey& InRouteKey);

	/**
	 * @brief Removes the registration for factory.
	* @param InKey Exact route identity used to look up the matching factory or handler.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * UnregisterFactory(InKey);
	 * @endcode
	 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool UnregisterFactory(TKey&& InKey)
	{
		return UnregisterFactory(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)));
	}

	/** @brief HasFactory API.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
 * @return True when the named condition holds.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HasFactory(InRouteKey);
 * @endcode
 */
	bool HasFactory(const FNativeWebDispatchRouteKey& InRouteKey) const;

	/**
	 * @brief Reports whether factory.
	* @param InKey Exact route identity used to look up the matching factory or handler.
	 * @return True when the named condition holds.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * HasFactory(InKey);
	 * @endcode
	 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool HasFactory(TKey&& InKey) const { return HasFactory(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey))); }

	/** @brief GetFactoryCount API.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetFactoryCount();
 * @endcode
 */
	int32 GetFactoryCount() const;

	/**
	 * @brief Performs the decode operation.
	* @param InRouteKey Exact route identity used to look up the matching factory or handler.
	* @param InPayload Opaque payload bytes; this operation does not impose an application schema.
	 * @param OutValue Output value populated by this operation when applicable.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return An enumerated status describing the outcome.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Decode(InRouteKey, InPayload, OutValue, OutError);
	 * @endcode
	 */

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
