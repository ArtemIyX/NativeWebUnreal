/**
 * @file NativeWebDispatch\Public\Dispatch\NativeWebDispatchRouteKey.h
 * @brief Type-erased immutable keys used by NativeWebDispatch route registries.
 */
#pragma once

#include "CoreMinimal.h"
#include <concepts>
#include <type_traits>

class FNativeWebDispatchRouteKey;
/**
 * @brief Performs the native web dispatch try get route key operation.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
 * @return The requested object/result, or null when unavailable.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NativeWebDispatchTryGetRouteKey(InRouteKey);
 * @endcode
 */

template <typename TKey>
const std::decay_t<TKey>* NativeWebDispatchTryGetRouteKey(const FNativeWebDispatchRouteKey& InRouteKey);

template <typename TKey>
/**
 * @brief Defines the t native web dispatch route key traits data structure.
 * @ingroup NativeWebDispatch
 */
struct TNativeWebDispatchRouteKeyTraits
{
	using FKeyType = std::decay_t<TKey>;

	/**
	 * @brief Returns hash.
	* @param InKey Exact route identity used to look up the matching factory or handler.
	 * @return The resulting count, size, index, hash, or numeric value.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetHash(InKey);
	 * @endcode
	 */

	static uint32 GetHash(const FKeyType& InKey)
	{
		return GetTypeHash(InKey);
	}

	/**
	 * @brief Performs the equals operation.
	* @param InLeft Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param InRight Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Equals(InLeft, InRight);
	 * @endcode
	 */

	static bool Equals(const FKeyType& InLeft, const FKeyType& InRight)
	{
		return InLeft == InRight;
	}
};

namespace NativeWebDispatchRouteKeyPrivate
{
	/**
	 * @brief Returns type token.
	 * @return The requested object/result, or null when unavailable.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetTypeToken();
	 * @endcode
	 */

	template <typename TKey>
	const void* GetTypeToken()
	{
		/**
		 * @brief Declares the i storage type and its operations.
		 * @ingroup NativeWebDispatch
		 */
		static const uint8 Token = 0;
		return &Token;
	}

	class IStorage
	{
	public:
		/**
		 * @brief Performs the ~i storage operation.
		 * @return The value produced by this operation.
		 * @note Route keys match exact type and value; registry access is synchronized.
		 * @par Example
		 * Invocation example; substitute arguments that satisfy the function contract.
		 * @code{.cpp}
		 * ~IStorage();
		 * @endcode
		 */

		virtual ~IStorage() = default;
		/**
		 * @brief Returns type token.
		 * @return The requested object/result, or null when unavailable.
		 * @note Route keys match exact type and value; registry access is synchronized.
		 * @par Example
		 * Invocation example; substitute arguments that satisfy the function contract.
		 * @code{.cpp}
		 * GetTypeToken();
		 * @endcode
		 */

		virtual const void* GetTypeToken() const = 0;
		/**
		 * @brief Returns value hash.
		 * @return The resulting count, size, index, hash, or numeric value.
		 * @note Route keys match exact type and value; registry access is synchronized.
		 * @par Example
		 * Invocation example; substitute arguments that satisfy the function contract.
		 * @code{.cpp}
		 * GetValueHash();
		 * @endcode
		 */

		virtual uint32 GetValueHash() const = 0;
		/**
		 * @brief Performs the equals operation.
		* @param InOther Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
		 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
		 * @note Route keys match exact type and value; registry access is synchronized.
		 * @par Example
		 * Invocation example; substitute arguments that satisfy the function contract.
		 * @code{.cpp}
		 * Equals(InOther);
		 * @endcode
		 */

		virtual bool Equals(const IStorage& InOther) const = 0;
		/**
		 * @brief Returns description.
		 * @return The resulting text or identifier.
		 * @note Route keys match exact type and value; registry access is synchronized.
		 * @par Example
		 * Invocation example; substitute arguments that satisfy the function contract.
		 * @code{.cpp}
		 * GetDescription();
		 * @endcode
		 */

		virtual FString GetDescription() const = 0;
	};

	template <typename TKey>
	class TStorage final : public IStorage
	{
	public:
		using FKeyType = std::decay_t<TKey>;

		explicit TStorage(FKeyType InKey)
			: Key(MoveTemp(InKey))
		{
			ValueHash = TNativeWebDispatchRouteKeyTraits<FKeyType>::GetHash(Key);
		}

		virtual const void* GetTypeToken() const override
		{
			return NativeWebDispatchRouteKeyPrivate::GetTypeToken<FKeyType>();
		}

		virtual uint32 GetValueHash() const override
		{
			return ValueHash;
		}

		virtual bool Equals(const IStorage& InOther) const override
		{
			if (InOther.GetTypeToken() != GetTypeToken())
			{
				return false;
			}

			const TStorage& other = static_cast<const TStorage&>(InOther);
			return TNativeWebDispatchRouteKeyTraits<FKeyType>::Equals(Key, other.Key);
		}

		virtual FString GetDescription() const override
		{
			if constexpr (requires(const FKeyType& InValue)
			{
				TNativeWebDispatchRouteKeyTraits<FKeyType>::Describe(InValue);
			})
			{
				return TNativeWebDispatchRouteKeyTraits<FKeyType>::Describe(Key).Left(128);
			}
			return FString();
		}

		const FKeyType& GetKey() const
		{
			return Key;
		}

	private:
		const FKeyType Key;
		uint32 ValueHash = 0;
	};
}

class NATIVEWEBDISPATCH_API FNativeWebDispatchRouteKey
{
public:
	FNativeWebDispatchRouteKey() = default;

	explicit FNativeWebDispatchRouteKey(TSharedPtr<const NativeWebDispatchRouteKeyPrivate::IStorage> InStorage)
		: Storage(MoveTemp(InStorage))
	{
	}

	bool IsValid() const
	{
		return Storage.IsValid();
	}

	uint32 GetValueHash() const
	{
		return Storage.IsValid() ? Storage->GetValueHash() : 0;
	}

	const void* GetTypeIdentity() const
	{
		return Storage.IsValid() ? Storage->GetTypeToken() : nullptr;
	}

	FString GetDiagnosticDescription() const
	{
		return Storage.IsValid() ? Storage->GetDescription() : FString();
	}

	FString GetDiagnosticText() const
	{
		if (!IsValid())
		{
			return TEXT("invalid");
		}
		const FString description = GetDiagnosticDescription();
		const uint32 routeHash = HashCombine(GetValueHash(), PointerHash(GetTypeIdentity()));
		return description.IsEmpty()
			       ? FString::Printf(TEXT("hash=%u"), routeHash)
			       : FString::Printf(TEXT("hash=%u description=%s"), routeHash, *description);
	}

	/**
	 * @brief Attempts to get.
	 * @return The requested object/result, or null when unavailable.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * TryGet();
	 * @endcode
	 */

	template <typename TKey>
	const std::decay_t<TKey>* TryGet() const;

	bool operator==(const FNativeWebDispatchRouteKey& InOther) const
	{
		return Storage.IsValid() && InOther.Storage.IsValid() &&
			Storage->GetTypeToken() == InOther.Storage->GetTypeToken() && Storage->Equals(*InOther.Storage);
	}

private:
	/**
	 * @brief Performs the make native web dispatch route key operation.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param InKey Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
	 * @return The value produced by this operation.
	 * @note Exact-key factory decoding and typed handler routing.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	MakeNativeWebDispatchRouteKey(InKey);
	@endcode
	 */

	/**
	 * @brief Performs the make native web dispatch route key operation.
	* @param InKey Exact route identity used to look up the matching factory or handler.
	 * @return The value produced by this operation.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * MakeNativeWebDispatchRouteKey(InKey);
	 * @endcode
	 */

	template <typename TKey>
	friend FNativeWebDispatchRouteKey MakeNativeWebDispatchRouteKey(TKey&& InKey);
	/**
	 * @brief Performs the native web dispatch try get route key operation.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param InRouteKey Input value. Its lifetime and copy behavior are defined by the implementation and owning type.
	 * @return The requested object/result, or null when it is unavailable.
	 * @note Exact-key factory decoding and typed handler routing.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	NativeWebDispatchTryGetRouteKey(InRouteKey);
	@endcode
	 */

	/**
	 * @brief Performs the native web dispatch try get route key operation.
	* @param InRouteKey Exact route identity used to look up the matching factory or handler.
	 * @return The requested object/result, or null when unavailable.
	 * @note Route keys match exact type and value; registry access is synchronized.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * NativeWebDispatchTryGetRouteKey(InRouteKey);
	 * @endcode
	 */

	template <typename TKey>
	friend const std::decay_t<TKey>* NativeWebDispatchTryGetRouteKey(const FNativeWebDispatchRouteKey& InRouteKey);

	TSharedPtr<const NativeWebDispatchRouteKeyPrivate::IStorage> Storage;
};

/**
 * @brief Returns type hash.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
 * @return The resulting count, size, index, hash, or numeric value.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetTypeHash(InRouteKey);
 * @endcode
 */

inline uint32 GetTypeHash(const FNativeWebDispatchRouteKey& InRouteKey)
{
	return HashCombine(InRouteKey.GetValueHash(), PointerHash(InRouteKey.GetTypeIdentity()));
}

/**
 * @brief Performs the requires operation.
* @param InKey Exact route identity used to look up the matching factory or handler.
 * @return The value produced by this operation.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * requires(InKey);
 * @endcode
 */

template <typename TKey>
	requires requires(const std::decay_t<TKey>& InKey)
	{
		TNativeWebDispatchRouteKeyTraits<std::decay_t<TKey>>::GetHash(InKey);
		{ TNativeWebDispatchRouteKeyTraits<std::decay_t<TKey>>::Equals(InKey, InKey) } -> std::convertible_to<bool>;
	}
/**
 * @brief Performs the make native web dispatch route key operation.
* @param InKey Exact route identity used to look up the matching factory or handler.
 * @return The value produced by this operation.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * MakeNativeWebDispatchRouteKey(InKey);
 * @endcode
 */

FNativeWebDispatchRouteKey MakeNativeWebDispatchRouteKey(TKey&& InKey)
{
	using FKeyType = std::decay_t<TKey>;
	return FNativeWebDispatchRouteKey(
		MakeShared<const NativeWebDispatchRouteKeyPrivate::TStorage<FKeyType>>(Forward<TKey>(InKey)));
}

/**
 * @brief Performs the native web dispatch try get route key operation.
* @param InRouteKey Exact route identity used to look up the matching factory or handler.
 * @return The requested object/result, or null when unavailable.
 * @note Route keys match exact type and value; registry access is synchronized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * NativeWebDispatchTryGetRouteKey(InRouteKey);
 * @endcode
 */

template <typename TKey>
const std::decay_t<TKey>* NativeWebDispatchTryGetRouteKey(const FNativeWebDispatchRouteKey& InRouteKey)
{
	using FKeyType = std::decay_t<TKey>;
	if (!InRouteKey.IsValid() || InRouteKey.GetTypeIdentity() != NativeWebDispatchRouteKeyPrivate::GetTypeToken<
		FKeyType>())
	{
		return nullptr;
	}
	return &static_cast<const NativeWebDispatchRouteKeyPrivate::TStorage<FKeyType>*>(InRouteKey.Storage.Get())->
		GetKey();
}

template <typename TKey>
const std::decay_t<TKey>* FNativeWebDispatchRouteKey::TryGet() const
{
	return NativeWebDispatchTryGetRouteKey<TKey>(*this);
}
