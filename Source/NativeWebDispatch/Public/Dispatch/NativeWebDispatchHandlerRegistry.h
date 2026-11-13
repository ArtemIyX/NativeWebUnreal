/**
 * @file NativeWebDispatch\Public\Dispatch\NativeWebDispatchHandlerRegistry.h
 * @brief Declares NativeWebDispatchHandlerRegistry for module NativeWebDispatch.
 * @details Contains typed handler routing built on top of FNativeWebDispatchRegistry decode results.
 */
#pragma once

#include "CoreMinimal.h"
#include "Dispatch/NativeWebDispatchMetadata.h"
#include "Dispatch/NativeWebDispatchRegistry.h"
#include "Dispatch/NativeWebDispatchValue.h"
#include <functional>
#include <type_traits>
#include <utility>

/** @brief Result of attempting to decode and invoke a handler. */
enum class ENativeWebDispatchHandlerResult : uint8
{
	/** @brief Ok enumeration value. */
	Ok,
	/** @brief DecodeRegistryUnavailable enumeration value. */
	DecodeRegistryUnavailable,
	/** @brief DecodeRouteNotFound enumeration value. */
	DecodeRouteNotFound,
	/** @brief DecodeFailed enumeration value. */
	DecodeFailed,
	/** @brief HandlerNotFound enumeration value. */
	HandlerNotFound,
	/** @brief HandlerTypeMismatch enumeration value. */
	HandlerTypeMismatch,
	/** @brief HandlerFailed enumeration value. */
	HandlerFailed,
	/** @brief InvalidHandler enumeration value. */
	InvalidHandler,
	/** @brief InvalidRouteKey enumeration value. */
	InvalidRouteKey
};

/**
 * @brief Converts the value to a diagnostic string.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The resulting text or identifier.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ToString(InValue);
 * @endcode
 */

inline FString ToString(ENativeWebDispatchHandlerResult InValue)
{
	switch (InValue)
	{
		case ENativeWebDispatchHandlerResult::Ok:
			return "Ok";
		case ENativeWebDispatchHandlerResult::DecodeRegistryUnavailable:
			return "DecodeRegistryUnavailable";
		case ENativeWebDispatchHandlerResult::DecodeRouteNotFound:
			return "DecodeRouteNotFound";
		case ENativeWebDispatchHandlerResult::DecodeFailed:
			return "DecodeFailed";
		case ENativeWebDispatchHandlerResult::HandlerNotFound:
			return "HandlerNotFound";
		case ENativeWebDispatchHandlerResult::HandlerTypeMismatch:
			return "HandlerTypeMismatch";
		case ENativeWebDispatchHandlerResult::HandlerFailed:
			return "HandlerFailed";
		case ENativeWebDispatchHandlerResult::InvalidHandler:
			return "InvalidHandler";
		case ENativeWebDispatchHandlerResult::InvalidRouteKey:
			return "InvalidRouteKey";
		default:
			return "INVALID";
	}
}

/**
 * @brief Thread-safe registry mapping exact route keys and decoded value types to handlers.
 * @details Each registered handler is invoked only after the decode registry returns a value with a matching type identity.
 * @ingroup NativeWebDispatch
 */
class NATIVEWEBDISPATCH_API FNativeWebDispatchHandlerRegistry
{
public:
	using FNativeWebDispatchHandler = TFunction<bool(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue, FString& OutError)>;
	using FNativeWebDispatchMetadataHandler = TFunction<bool(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue, const INativeWebDispatchMetadata* InMetadata, FString& OutError)>;

	FNativeWebDispatchHandlerRegistry() = default;

	explicit FNativeWebDispatchHandlerRegistry(TSharedPtr<FNativeWebDispatchRegistry> InDecodeRegistry)
		: DecodeRegistry(MoveTemp(InDecodeRegistry)) {}

/**
 * @brief Sets decode registry.
* @param InDecodeRegistry Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetDecodeRegistry(InDecodeRegistry);
 * @endcode
 */

	void SetDecodeRegistry(TSharedPtr<FNativeWebDispatchRegistry> InDecodeRegistry)
	{
/**
 * @brief Performs the write lock operation.
* @param DecodeRegistryLock Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * writeLock(DecodeRegistryLock);
 * @endcode
 */

		FWriteScopeLock writeLock(DecodeRegistryLock);
		DecodeRegistry = MoveTemp(InDecodeRegistry);
	}

	TSharedPtr<FNativeWebDispatchRegistry> GetDecodeRegistry() const
	{
/**
 * @brief Performs the read lock operation.
* @param DecodeRegistryLock Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * readLock(DecodeRegistryLock);
 * @endcode
 */

		FReadScopeLock readLock(DecodeRegistryLock);
		return DecodeRegistry;
	}

/**
 * @brief Registers handler.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InHandler Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInAllowReplace Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RegisterHandler(InRouteKey, InHandler, bInAllowReplace);
 * @endcode
 */

	bool RegisterHandler(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchHandler InHandler, bool bInAllowReplace = false);
/**
 * @brief Registers handler.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InHandler Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInAllowReplace Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RegisterHandler(InRouteKey, InHandler, bInAllowReplace);
 * @endcode
 */

	bool RegisterHandler(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchMetadataHandler InHandler, bool bInAllowReplace = false);

/**
 * @brief Registers handler.
* @param InKey Exact route identity used to look up the matching factory or handler.
* @param InHandler Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInAllowReplace Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RegisterHandler(InKey, InHandler, bInAllowReplace);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterHandler(TKey&& InKey, FNativeWebDispatchHandler InHandler, bool bInAllowReplace = false) { return RegisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MoveTemp(InHandler), bInAllowReplace); }

/**
 * @brief Registers handler.
* @param InKey Exact route identity used to look up the matching factory or handler.
* @param InHandler Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInAllowReplace Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RegisterHandler(InKey, InHandler, bInAllowReplace);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterHandler(TKey&& InKey, FNativeWebDispatchMetadataHandler InHandler, bool bInAllowReplace = false) { return RegisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MoveTemp(InHandler), bInAllowReplace); }

/**
 * @brief Removes the registration for handler.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * UnregisterHandler(InRouteKey);
 * @endcode
 */

	bool UnregisterHandler(const FNativeWebDispatchRouteKey& InRouteKey);

/**
 * @brief Removes the registration for handler.
* @param InKey Exact route identity used to look up the matching factory or handler.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * UnregisterHandler(InKey);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool UnregisterHandler(TKey&& InKey) { return UnregisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey))); }

/**
 * @brief Reports whether handler.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
 * @return True when the named condition holds.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HasHandler(InRouteKey);
 * @endcode
 */

	bool HasHandler(const FNativeWebDispatchRouteKey& InRouteKey) const;

/**
 * @brief Reports whether handler.
* @param InKey Exact route identity used to look up the matching factory or handler.
 * @return True when the named condition holds.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * HasHandler(InKey);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool HasHandler(TKey&& InKey) const { return HasHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey))); }

/**
 * @brief Returns handler count.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetHandlerCount();
 * @endcode
 */

	int32 GetHandlerCount() const;

/**
 * @brief Performs the handle operation.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Handle(InRouteKey, InValue, OutError);
 * @endcode
 */

	ENativeWebDispatchHandlerResult Handle(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue, FString& OutError) const;
/**
 * @brief Performs the handle operation.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMetadata Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Handle(InRouteKey, InValue, InMetadata, OutError);
 * @endcode
 */

	ENativeWebDispatchHandlerResult Handle(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const;

/**
 * @brief Performs the handle operation.
* @param InKey Exact route identity used to look up the matching factory or handler.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Handle(InKey, InValue, OutError);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Handle(TKey&& InKey, const INativeWebDispatchValue& InValue, FString& OutError) const { return Handle(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InValue, OutError); }

/**
 * @brief Performs the handle operation.
* @param InKey Exact route identity used to look up the matching factory or handler.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param InMetadata Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Handle(InKey, InValue, InMetadata, OutError);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Handle(TKey&& InKey, const INativeWebDispatchValue& InValue,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const { return Handle(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InValue, MoveTemp(InMetadata), OutError); }

/**
 * @brief Performs the dispatch operation.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InPayload Opaque payload bytes; this operation does not impose an application schema.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Dispatch(InRouteKey, InPayload, OutError);
 * @endcode
 */

	ENativeWebDispatchHandlerResult Dispatch(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload, FString& OutError) const;
/**
 * @brief Performs the dispatch operation.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InPayload Opaque payload bytes; this operation does not impose an application schema.
* @param InMetadata Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Dispatch(InRouteKey, InPayload, InMetadata, OutError);
 * @endcode
 */

	ENativeWebDispatchHandlerResult Dispatch(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const;

/**
 * @brief Performs the dispatch operation.
* @param InKey Exact route identity used to look up the matching factory or handler.
* @param InPayload Opaque payload bytes; this operation does not impose an application schema.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Dispatch(InKey, InPayload, OutError);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Dispatch(TKey&& InKey, const TArray<uint8>& InPayload, FString& OutError) const { return Dispatch(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InPayload, OutError); }

/**
 * @brief Performs the dispatch operation.
* @param InKey Exact route identity used to look up the matching factory or handler.
* @param InPayload Opaque payload bytes; this operation does not impose an application schema.
* @param InMetadata Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return An enumerated status describing the outcome.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Dispatch(InKey, InPayload, InMetadata, OutError);
 * @endcode
 */

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Dispatch(TKey&& InKey, const TArray<uint8>& InPayload,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const { return Dispatch(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InPayload, MoveTemp(InMetadata), OutError); }

/**
 * @brief Registers typed handler.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
* @param InHandler Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInAllowReplace Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RegisterTypedHandler(InRouteKey, InHandler, bInAllowReplace);
 * @endcode
 */

	template <typename T, typename CallableType>
	bool RegisterTypedHandler(const FNativeWebDispatchRouteKey& InRouteKey, CallableType&& InHandler, bool bInAllowReplace = false)
	{
		return RegisterHandler(InRouteKey, MakeTypedHandler<T>(Forward<CallableType>(InHandler)), bInAllowReplace);
	}

/**
 * @brief Registers typed handler.
* @param InKey Exact route identity used to look up the matching factory or handler.
* @param InHandler Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param bInAllowReplace Boolean option controlling this operation.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * RegisterTypedHandler(InKey, InHandler, bInAllowReplace);
 * @endcode
 */

	template <typename T, typename TKey, typename CallableType>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterTypedHandler(TKey&& InKey, CallableType&& InHandler, bool bInAllowReplace = false)
	{
		return RegisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MakeTypedHandler<T>(Forward<CallableType>(InHandler)), bInAllowReplace);
	}

/**
 * @brief Performs the make typed handler operation.
* @param InHandler Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The value produced by this operation.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * MakeTypedHandler(InHandler);
 * @endcode
 */

	template <typename T, typename CallableType>
	static FNativeWebDispatchMetadataHandler MakeTypedHandler(CallableType&& InHandler)
	{
		using FDecayedCallableType = std::decay_t<CallableType>;

		return [Handler = FDecayedCallableType(Forward<CallableType>(InHandler))](
			const FNativeWebDispatchRouteKey& InRouteKey,
			const INativeWebDispatchValue& InValue,
			const INativeWebDispatchMetadata* InMetadata,
			FString& OutError) mutable -> bool {
			const TNativeWebDispatchValue<T>* typedValue = NativeWebDispatchTryGetValue<T>(InValue);
			if (!typedValue)
			{
				OutError = TEXT("NativeWebDispatch.TypedHandler.TypeMismatch");
				return false;
			}

			const T& value = typedValue->GetValue();

			if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, const INativeWebDispatchMetadata*, FString&>)
			{
				return std::invoke(Handler, InRouteKey, value, InMetadata, OutError);
			}
			else if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const T&, const INativeWebDispatchMetadata*, FString&>)
			{
				return std::invoke(Handler, value, InMetadata, OutError);
			}
			else if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, const INativeWebDispatchMetadata*>)
			{
				return std::invoke(Handler, InRouteKey, value, InMetadata);
			}
			else if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const T&, const INativeWebDispatchMetadata*>)
			{
				return std::invoke(Handler, value, InMetadata);
			}
			else if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, FString&>)
			{
				return std::invoke(Handler, InRouteKey, value, OutError);
			}
			else if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const T&, FString&>)
			{
				return std::invoke(Handler, value, OutError);
			}
			else if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&>)
			{
				return std::invoke(Handler, InRouteKey, value);
			}
			else if constexpr (std::is_invocable_r_v<bool, FDecayedCallableType&, const T&>)
			{
				return std::invoke(Handler, value);
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, const INativeWebDispatchMetadata*, FString&>)
			{
				std::invoke(Handler, InRouteKey, value, InMetadata, OutError);
				return true;
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const T&, const INativeWebDispatchMetadata*, FString&>)
			{
				std::invoke(Handler, value, InMetadata, OutError);
				return true;
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, const INativeWebDispatchMetadata*>)
			{
				std::invoke(Handler, InRouteKey, value, InMetadata);
				return true;
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const T&, const INativeWebDispatchMetadata*>)
			{
				std::invoke(Handler, value, InMetadata);
				return true;
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, FString&>)
			{
				std::invoke(Handler, InRouteKey, value, OutError);
				return true;
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const T&, FString&>)
			{
				std::invoke(Handler, value, OutError);
				return true;
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&>)
			{
				std::invoke(Handler, InRouteKey, value);
				return true;
			}
			else if constexpr (std::is_invocable_v<FDecayedCallableType&, const T&>)
			{
				std::invoke(Handler, value);
				return true;
			}
			else
			{
				static_assert(
					std::is_invocable_v<FDecayedCallableType&, const T&> ||
					std::is_invocable_v<FDecayedCallableType&, const T&, const INativeWebDispatchMetadata*> ||
					std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&> ||
					std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, const INativeWebDispatchMetadata*> ||
					std::is_invocable_v<FDecayedCallableType&, const T&, FString&> ||
					std::is_invocable_v<FDecayedCallableType&, const T&, const INativeWebDispatchMetadata*, FString&> ||
					std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, FString&> ||
					std::is_invocable_v<FDecayedCallableType&, const FNativeWebDispatchRouteKey&, const T&, const INativeWebDispatchMetadata*, FString&>,
					"Typed dispatch handlers must accept one of: (const T&), (const FNativeWebDispatchRouteKey&, const T&), (const T&, FString&), or (const FNativeWebDispatchRouteKey&, const T&, FString&), optionally returning bool.");
				return false;
			}
		};
	}

private:
	mutable FRWLock DecodeRegistryLock;
	TSharedPtr<FNativeWebDispatchRegistry> DecodeRegistry;

	mutable FRWLock HandlersLock;
	TMap<FNativeWebDispatchRouteKey, FNativeWebDispatchMetadataHandler> Handlers;
};
