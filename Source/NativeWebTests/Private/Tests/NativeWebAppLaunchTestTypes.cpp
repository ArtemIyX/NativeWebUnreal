#include "Tests/NativeWebAppLaunchTestTypes.h"

#include "NativeWebView.h"

void UNativeWebAppLaunchTestReceiver::ReceiveSuccess(UNativeWebView* View, const FNativeWebError& Error)
{
	++SuccessCount;
	LastView = View;
	LastError = Error;
}

void UNativeWebAppLaunchTestReceiver::ReceiveFailure(UNativeWebView* View, const FNativeWebError& Error)
{
	++FailureCount;
	LastView = View;
	LastError = Error;
}
