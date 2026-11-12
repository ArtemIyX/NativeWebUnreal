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
	Ok,
	DecodeRegistryUnavailable,
	DecodeRouteNotFound,
	DecodeFailed,
	HandlerNotFound,
	HandlerTypeMismatch,
	HandlerFailed,
	InvalidHandler,
	InvalidRouteKey
};

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

/** @brief Type declaration. */
class NATIVEWEBDISPATCH_API FNativeWebDispatchHandlerRegistry
{
public:
	using FNativeWebDispatchHandler = TFunction<bool(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue, FString& OutError)>;
	using FNativeWebDispatchMetadataHandler = TFunction<bool(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue, const INativeWebDispatchMetadata* InMetadata, FString& OutError)>;

	FNativeWebDispatchHandlerRegistry() = default;

	explicit FNativeWebDispatchHandlerRegistry(TSharedPtr<FNativeWebDispatchRegistry> InDecodeRegistry)
		: DecodeRegistry(MoveTemp(InDecodeRegistry)) {}

	void SetDecodeRegistry(TSharedPtr<FNativeWebDispatchRegistry> InDecodeRegistry)
	{
		FWriteScopeLock writeLock(DecodeRegistryLock);
		DecodeRegistry = MoveTemp(InDecodeRegistry);
	}

	TSharedPtr<FNativeWebDispatchRegistry> GetDecodeRegistry() const
	{
		FReadScopeLock readLock(DecodeRegistryLock);
		return DecodeRegistry;
	}

	bool RegisterHandler(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchHandler InHandler, bool bInAllowReplace = false);
	bool RegisterHandler(const FNativeWebDispatchRouteKey& InRouteKey, FNativeWebDispatchMetadataHandler InHandler, bool bInAllowReplace = false);

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterHandler(TKey&& InKey, FNativeWebDispatchHandler InHandler, bool bInAllowReplace = false) { return RegisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MoveTemp(InHandler), bInAllowReplace); }

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterHandler(TKey&& InKey, FNativeWebDispatchMetadataHandler InHandler, bool bInAllowReplace = false) { return RegisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MoveTemp(InHandler), bInAllowReplace); }

	bool UnregisterHandler(const FNativeWebDispatchRouteKey& InRouteKey);

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool UnregisterHandler(TKey&& InKey) { return UnregisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey))); }

	bool HasHandler(const FNativeWebDispatchRouteKey& InRouteKey) const;

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool HasHandler(TKey&& InKey) const { return HasHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey))); }

	int32 GetHandlerCount() const;

	ENativeWebDispatchHandlerResult Handle(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue, FString& OutError) const;
	ENativeWebDispatchHandlerResult Handle(const FNativeWebDispatchRouteKey& InRouteKey, const INativeWebDispatchValue& InValue,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const;

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Handle(TKey&& InKey, const INativeWebDispatchValue& InValue, FString& OutError) const { return Handle(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InValue, OutError); }

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Handle(TKey&& InKey, const INativeWebDispatchValue& InValue,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const { return Handle(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InValue, MoveTemp(InMetadata), OutError); }

	ENativeWebDispatchHandlerResult Dispatch(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload, FString& OutError) const;
	ENativeWebDispatchHandlerResult Dispatch(const FNativeWebDispatchRouteKey& InRouteKey, const TArray<uint8>& InPayload,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const;

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Dispatch(TKey&& InKey, const TArray<uint8>& InPayload, FString& OutError) const { return Dispatch(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InPayload, OutError); }

	template <typename TKey>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	ENativeWebDispatchHandlerResult Dispatch(TKey&& InKey, const TArray<uint8>& InPayload,
		TSharedPtr<const INativeWebDispatchMetadata> InMetadata, FString& OutError) const { return Dispatch(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), InPayload, MoveTemp(InMetadata), OutError); }

	template <typename T, typename CallableType>
	bool RegisterTypedHandler(const FNativeWebDispatchRouteKey& InRouteKey, CallableType&& InHandler, bool bInAllowReplace = false)
	{
		return RegisterHandler(InRouteKey, MakeTypedHandler<T>(Forward<CallableType>(InHandler)), bInAllowReplace);
	}

	template <typename T, typename TKey, typename CallableType>
		requires (!std::is_same_v<std::decay_t<TKey>, FNativeWebDispatchRouteKey>)
	bool RegisterTypedHandler(TKey&& InKey, CallableType&& InHandler, bool bInAllowReplace = false)
	{
		return RegisterHandler(MakeNativeWebDispatchRouteKey(Forward<TKey>(InKey)), MakeTypedHandler<T>(Forward<CallableType>(InHandler)), bInAllowReplace);
	}

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
