/**
 * @file Source/NativeWebDispatch/Public/Dispatch/NativeWebDispatchMetadata.h
 * @brief Declares native web dispatch metadata for the NativeWebDispatch module.
 * @details Exact-key factory decoding and typed handler routing.
 * @ingroup NativeWebDispatch
 */

#pragma once

#include "CoreMinimal.h"
#include "Dispatch/NativeWebDispatchValue.h"
#include <type_traits>

/**
 * @brief Declares the nativewebdispatch api type and its operations.
 * @ingroup NativeWebDispatch
 */
class NATIVEWEBDISPATCH_API INativeWebDispatchMetadata
{
public:
/**
 * @brief Performs the ~i native web dispatch metadata operation.
 * @return The value produced by this operation.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ~INativeWebDispatchMetadata();
 * @endcode
 */

	virtual ~INativeWebDispatchMetadata() = default;

 /**
 * @brief Returns the stable type identity used to match metadata providers.
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

template <typename T>
class TNativeWebDispatchMetadata final : public INativeWebDispatchMetadata
{
public:
	explicit TNativeWebDispatchMetadata(T InValue)
		: Value(MoveTemp(InValue))
	{
	}

	FNativeWebDispatchTypeId GetTypeId() const override
	{
		return FNativeWebDispatchTypeId::Of<T>();
	}

	const T& GetValue() const
	{
		return Value;
	}

private:
	T Value;
};

/**
 * @brief Performs the make native web dispatch metadata operation.
* @param InValue Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return A shared result; it is invalid when the resource is unavailable.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * MakeNativeWebDispatchMetadata(InValue);
 * @endcode
 */

template <typename T>
TSharedPtr<const INativeWebDispatchMetadata> MakeNativeWebDispatchMetadata(T&& InValue)
{
	using FMetadataType = std::decay_t<T>;
	return MakeShared<TNativeWebDispatchMetadata<FMetadataType>>(Forward<T>(InValue));
}

/**
 * @brief Performs the native web dispatch try get metadata operation.
* @param InMetadata Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return The requested object/result, or null when unavailable.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NativeWebDispatchTryGetMetadata(InMetadata);
 * @endcode
 */

template <typename T>
const T* NativeWebDispatchTryGetMetadata(const INativeWebDispatchMetadata* InMetadata)
{
	if (!InMetadata || InMetadata->GetTypeId() != FNativeWebDispatchTypeId::Of<T>())
	{
		return nullptr;
	}

	return &static_cast<const TNativeWebDispatchMetadata<T>*>(InMetadata)->GetValue();
}
