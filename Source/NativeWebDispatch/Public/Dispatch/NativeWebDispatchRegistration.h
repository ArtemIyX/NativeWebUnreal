/**
 * @file NativeWebDispatch\Public\Dispatch\NativeWebDispatchRegistration.h
 * @brief Declares NativeWebDispatchRegistration for module NativeWebDispatch\Public\Dispatch\NativeWebDispatchRegistration.h.
 * @details Contains dispatch registry and value plumbing used by the plugin runtime and gameplay-facing systems.
 */
#pragma once

#include "CoreMinimal.h"
#include "Dispatch/NativeWebDispatchRegistry.h"

/** @brief Type declaration. */
class NATIVEWEBDISPATCH_API FNativeWebDispatchFactoryRegistrar
{
public:
	FNativeWebDispatchFactoryRegistrar(FNativeWebDispatchRouteKey InRouteKey, FNativeWebDispatchRegistry::FNativeWebDispatchFactory InFactory,
	                             /** @brief Function API. */
	                             bool bInAllowReplace = false);
};



#define NATIVEWEB_DISPATCH_CONCAT_INNER(InA, InB) InA##InB
#define NATIVEWEB_DISPATCH_CONCAT(InA, InB) NATIVEWEB_DISPATCH_CONCAT_INNER(InA, InB)

#define NATIVEWEB_DISPATCH_REGISTER_FACTORY(InMessageType, InFactory)                                                     \
	namespace                                                                                                      \
	{                                                                                                              \
	static FNativeWebDispatchFactoryRegistrar NATIVEWEB_DISPATCH_CONCAT(GNativeWebDispatchFactoryRegistrar_, __LINE__)(            \
		MakeNativeWebDispatchRouteKey(InMessageType), InFactory, false);                                                \
	}

#define NATIVEWEB_DISPATCH_REGISTER_FACTORY_REPLACE(InMessageType, InFactory)                                             \
	namespace                                                                                                      \
	{                                                                                                              \
	static FNativeWebDispatchFactoryRegistrar NATIVEWEB_DISPATCH_CONCAT(GNativeWebDispatchFactoryRegistrarReplace_, __LINE__)(     \
		MakeNativeWebDispatchRouteKey(InMessageType), InFactory, true);                                                 \
	}
