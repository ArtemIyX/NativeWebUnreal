#include "Dispatch/NativeWebDispatchRegistration.h"
#include "NativeWebDispatch.h"

FNativeWebDispatchFactoryRegistrar::FNativeWebDispatchFactoryRegistrar(FNativeWebDispatchRouteKey InRouteKey,
                                                           FNativeWebDispatchRegistry::FNativeWebDispatchFactory InFactory,
                                                           bool bInAllowReplace)
{
	FNativeWebDispatchModule::RegisterDeferredFactory(MoveTemp(InRouteKey), MoveTemp(InFactory), bInAllowReplace);
}
