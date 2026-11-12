
#include "Handlers/NativeWebContentHttpImageRequestHandler.h"

bool UNativeWebContentHttpImageRequestHandler::HandleImageRequest_Implementation(const FNativeWebContentHttpImageRequestContext& InRequestContext, FNativeWebContentHttpImageResponse& OutResponse, FString& OutError)
{
	(void)InRequestContext;
	OutResponse.StatusCode = 501;
	OutResponse.ContentType = TEXT("application/json");
	OutError = TEXT("Image request handler is not implemented");
	OutResponse.Body.Reset();
	return false;
}

void UNativeWebContentHttpImageRequestHandler::HandleImageRequestAsync(const FNativeWebContentHttpImageRequestContext& InRequestContext, FOnImageRequestCompleted InOnCompleted)
{
	FNativeWebContentHttpImageResponse response;
	FString error;
	const bool bHandled = HandleImageRequest(InRequestContext, response, error);
	InOnCompleted(bHandled, response, error);
}
