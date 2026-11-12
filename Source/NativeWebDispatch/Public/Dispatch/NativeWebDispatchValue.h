/**
 * @file NativeWebDispatch\Public\Dispatch\NativeWebDispatchValue.h
 * @brief Declares NativeWebDispatchValue for module NativeWebDispatch\Public\Dispatch\NativeWebDispatchValue.h.
 * @details Contains dispatch registry and value plumbing used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include <type_traits>

/** @brief Type declaration. */
struct FNativeWebDispatchTypeId
{
	/** @brief Value state. */
	const void* Value = nullptr;

	bool IsValid() const
	{
		return Value != nullptr;
	}

	bool operator==(const FNativeWebDispatchTypeId& InOther) const
	{
		return Value == InOther.Value;
	}

	bool operator!=(const FNativeWebDispatchTypeId& InOther) const
	{
		return !(*this == InOther);
	}

	template <typename T>
	static FNativeWebDispatchTypeId Of()
	{
		/** @brief token state. */
		static const uint8 token = 0;
		/** @brief outTypeId state. */
		FNativeWebDispatchTypeId outTypeId;
		/** @brief Value state. */
		outTypeId.Value = &token;
		return outTypeId;
	}
};

/** @brief Type declaration. */
class NATIVEWEBDISPATCH_API INativeWebDispatchValue
{
public:
	virtual ~INativeWebDispatchValue() = default;
	/** @brief GetTypeId API. */
	virtual FNativeWebDispatchTypeId GetTypeId() const = 0;
};

template <typename T>
/** @brief Type declaration. */
class TNativeWebDispatchValue final : public INativeWebDispatchValue
{
public:
	explicit TNativeWebDispatchValue(const T& InValue)
		: Value(InValue)
	{
	}

	explicit TNativeWebDispatchValue(T&& InValue)
		: Value(MoveTemp(InValue))
	{
	}

	virtual FNativeWebDispatchTypeId GetTypeId() const override
	{
		return FNativeWebDispatchTypeId::Of<T>();
	}

	const T& GetValue() const
	{
		return Value;
	}

	T& GetValue()
	{
		return Value;
	}

private:
	/** @brief Value state. */
	T Value;
};

template <typename T>
const TNativeWebDispatchValue<T>* NativeWebDispatchTryGetValue(const INativeWebDispatchValue& InValue)
{
	if (InValue.GetTypeId() != FNativeWebDispatchTypeId::Of<T>())
	{
		return nullptr;
	}
	return static_cast<const TNativeWebDispatchValue<T>*>(&InValue);
}

template <typename T>
TNativeWebDispatchValue<T>* NativeWebDispatchTryGetValue(INativeWebDispatchValue& InValue)
{
	if (InValue.GetTypeId() != FNativeWebDispatchTypeId::Of<T>())
	{
		return nullptr;
	}
	return static_cast<TNativeWebDispatchValue<T>*>(&InValue);
}

template <typename T>
TUniquePtr<INativeWebDispatchValue> MakeNativeWebDispatchValue(T&& InValue)
{
	using FValueType = std::decay_t<T>;
	return MakeUnique<TNativeWebDispatchValue<FValueType>>(Forward<T>(InValue));
}

