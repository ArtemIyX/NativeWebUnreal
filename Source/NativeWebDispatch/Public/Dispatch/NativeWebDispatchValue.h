/**
 * @file NativeWebDispatch\Public\Dispatch\NativeWebDispatchValue.h
 * @brief Declares NativeWebDispatchValue for module NativeWebDispatch\Public\Dispatch\NativeWebDispatchValue.h.
 * @details Contains dispatch registry and value plumbing used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include <type_traits>

/**
 * @brief Type-erased identity token used to compare dispatch metadata providers.
 * @details Tokens are addresses of per-type static storage and are valid for the lifetime of the process.
 * @ingroup NativeWebDispatch
 */
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

	/**
	 * @brief Performs the of operation.
	 * @return The value produced by this operation.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Of();
	 * @endcode
	 */

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

/**
 * @brief Interface for decoded values passed to NativeWebDispatch handlers.
 * @details Implementations expose a stable type token so typed handlers can reject mismatched values.
 * @ingroup NativeWebDispatch
 */
class NATIVEWEBDISPATCH_API INativeWebDispatchValue
{
public:
	/**
	 * @brief Performs the ~i native web dispatch value operation.
	 * @return The value produced by this operation.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * ~INativeWebDispatchValue();
	 * @endcode
	 */

	virtual ~INativeWebDispatchValue() = default;
	/** @brief GetTypeId API.
 * @return The value produced by this operation.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetTypeId();
 * @endcode
 */
	virtual FNativeWebDispatchTypeId GetTypeId() const = 0;
};

/**
 * @brief Owning type-erased wrapper for one decoded value of type T.
 * @tparam T Concrete value type stored and exposed through the dispatch interface.
 * @ingroup NativeWebDispatch
 */
template <typename T>
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

	/**
	 * @brief Returns value.
	 * @return The value produced by this operation.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetValue();
	 * @endcode
	 */

	T& GetValue()
	{
		return Value;
	}

private:
	/** @brief Value state. */
	T Value;
};

/**
 * @brief Performs the native web dispatch try get value operation.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The requested object/result, or null when unavailable.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NativeWebDispatchTryGetValue(InValue);
 * @endcode
 */

template <typename T>
const TNativeWebDispatchValue<T>* NativeWebDispatchTryGetValue(const INativeWebDispatchValue& InValue)
{
	if (InValue.GetTypeId() != FNativeWebDispatchTypeId::Of<T>())
	{
		return nullptr;
	}
	return static_cast<const TNativeWebDispatchValue<T>*>(&InValue);
}

/**
 * @brief Performs the native web dispatch try get value operation.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The requested object/result, or null when unavailable.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NativeWebDispatchTryGetValue(InValue);
 * @endcode
 */

template <typename T>
TNativeWebDispatchValue<T>* NativeWebDispatchTryGetValue(INativeWebDispatchValue& InValue)
{
	if (InValue.GetTypeId() != FNativeWebDispatchTypeId::Of<T>())
	{
		return nullptr;
	}
	return static_cast<TNativeWebDispatchValue<T>*>(&InValue);
}

/**
 * @brief Performs the make native web dispatch value operation.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return An owned result; it is invalid when no value can be produced.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * MakeNativeWebDispatchValue(InValue);
 * @endcode
 */

template <typename T>
TUniquePtr<INativeWebDispatchValue> MakeNativeWebDispatchValue(T&& InValue)
{
	using FValueType = std::decay_t<T>;
	return MakeUnique<TNativeWebDispatchValue<FValueType>>(Forward<T>(InValue));
}
