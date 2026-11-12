#pragma once

#include "CoreMinimal.h"
#include "Dispatch/NativeWebDispatchValue.h"
#include <type_traits>

class NATIVEWEBDISPATCH_API INativeWebDispatchMetadata
{
public:
	virtual ~INativeWebDispatchMetadata() = default;
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

template <typename T>
TSharedPtr<const INativeWebDispatchMetadata> MakeNativeWebDispatchMetadata(T&& InValue)
{
	using FMetadataType = std::decay_t<T>;
	return MakeShared<TNativeWebDispatchMetadata<FMetadataType>>(Forward<T>(InValue));
}

template <typename T>
const T* NativeWebDispatchTryGetMetadata(const INativeWebDispatchMetadata* InMetadata)
{
	if (!InMetadata || InMetadata->GetTypeId() != FNativeWebDispatchTypeId::Of<T>())
	{
		return nullptr;
	}

	return &static_cast<const TNativeWebDispatchMetadata<T>*>(InMetadata)->GetValue();
}
