
#include "Services/NativeWebContentHttpServerRuntimeService.h"

#include "NativeWebContentHttpServer.h"
#include "Dom/JsonObject.h"
#include "Handlers/NativeWebContentHttpImageRequestHandler.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FNativeWebContentHttpServerRuntimeService::Start(int32 InPort, TWeakObjectPtr<UNativeWebContentHttpImageRequestHandler> InHandler)
{
	if (IsRunning())
	{
		UE_LOG(LogNativeWebContentHttpServer, Warning, TEXT("Http server start skipped: already running on port %d"), ListenPort);
		return true;
	}

	if (InPort <= 0)
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Http server start failed: invalid port %d"), InPort);
		return false;
	}

	if (!InHandler.IsValid())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Http server start failed: request handler is invalid"));
		return false;
	}

	FHttpServerModule& httpServerModule = FHttpServerModule::Get();
	HttpRouter = httpServerModule.GetHttpRouter(static_cast<uint32>(InPort));
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Http server start failed: router is null for port %d"), InPort);
		return false;
	}

	RequestHandler = InHandler;
	ImgRouteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/img")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FNativeWebContentHttpServerRuntimeService::HandleImageRoute));

	if (!ImgRouteHandle.IsValid())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("Http server start failed: /img route bind failed"));
		RequestHandler.Reset();
		HttpRouter.Reset();
		return false;
	}

	httpServerModule.StartAllListeners();
	ListenPort = InPort;
	UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Http server started on port %d route /img"), ListenPort);
	return true;
}

void FNativeWebContentHttpServerRuntimeService::Stop()
{
	if (HttpRouter.IsValid() && ImgRouteHandle.IsValid())
	{
		HttpRouter->UnbindRoute(ImgRouteHandle);
		UE_LOG(LogNativeWebContentHttpServer, Log, TEXT("Http route /img unbound"));
	}

	ImgRouteHandle.Reset();
	RequestHandler.Reset();
	HttpRouter.Reset();
	ListenPort = 0;
}

bool FNativeWebContentHttpServerRuntimeService::IsRunning() const
{
	return ListenPort > 0 && ImgRouteHandle.IsValid() && HttpRouter.IsValid();
}

int32 FNativeWebContentHttpServerRuntimeService::GetPort() const
{
	return ListenPort;
}

bool FNativeWebContentHttpServerRuntimeService::HandleImageRoute(const FHttpServerRequest& InRequest, const FHttpResultCallback& InOnComplete)
{
	if (!RequestHandler.IsValid())
	{
		UE_LOG(LogNativeWebContentHttpServer, Error, TEXT("HandleImageRoute failed: request handler is invalid"));
		CompleteJson(InOnComplete, 500, TEXT("Request handler is not set"));
		return true;
	}

	const FString rawBody = ParseBodyAsUtf8(InRequest);
	FNativeWebContentHttpImageRequestContext requestContext;
	requestContext.RawBody = rawBody;
	requestContext.QueryParams = InRequest.QueryParams;
	requestContext.AssetPath = ResolveAssetPath(InRequest, rawBody);
	UE_LOG(
		LogNativeWebContentHttpServer,
		Log,
		TEXT("HTTP /img request received. asset='%s' queryCount=%d bodyBytes=%d"),
		*requestContext.AssetPath,
		requestContext.QueryParams.Num(),
		InRequest.Body.Num());

	RequestHandler->HandleImageRequestAsync(requestContext, [InOnComplete](bool bHandled, const FNativeWebContentHttpImageResponse& response, const FString& error) {
		if (!bHandled)
		{
			const int32 statusCode = response.StatusCode > 0 ? response.StatusCode : 500;
			UE_LOG(LogNativeWebContentHttpServer, Warning, TEXT("HTTP /img failed. status=%d error='%s'"), statusCode, *error);
			CompleteJson(InOnComplete, statusCode, error.IsEmpty() ? TEXT("Request handling failed") : error);
			return;
		}

		UE_LOG(
			LogNativeWebContentHttpServer,
			Verbose,
			TEXT("HTTP /img success. status=%d contentType='%s' bytes=%d"),
			response.StatusCode,
			*response.ContentType,
			response.Body.Num());
		CompleteBinary(InOnComplete, response.StatusCode, response.ContentType, response.Body);
	});

	return true;
}

FString FNativeWebContentHttpServerRuntimeService::ResolveAssetPath(const FHttpServerRequest& InRequest, const FString& InRawBody) const
{
	if (const FString* const queryValue = InRequest.QueryParams.Find(TEXT("asset")))
	{
		return *queryValue;
	}
	if (const FString* const queryValue = InRequest.QueryParams.Find(TEXT("aasset")))
	{
		return *queryValue;
	}

	FString assetPathFromBody;
	TSharedPtr<FJsonObject> jsonObject;
	const TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(InRawBody);
	if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject.IsValid())
	{
		if (jsonObject->TryGetStringField(TEXT("asset"), assetPathFromBody))
		{
			return assetPathFromBody;
		}
		if (jsonObject->TryGetStringField(TEXT("asset="), assetPathFromBody))
		{
			return assetPathFromBody;
		}
	}

	if (InRawBody.StartsWith(TEXT("asset=")))
	{
		return InRawBody.RightChop(6);
	}

	return FString();
}

FString FNativeWebContentHttpServerRuntimeService::ParseBodyAsUtf8(const FHttpServerRequest& InRequest) const
{
	if (InRequest.Body.Num() <= 0)
	{
		return FString();
	}

	const UTF8CHAR* const rawBodyData = reinterpret_cast<const UTF8CHAR*>(InRequest.Body.GetData());


	FUTF8ToTCHAR converter(rawBodyData, InRequest.Body.Num());
	return FString(converter.Length(), converter.Get());
}

void FNativeWebContentHttpServerRuntimeService::CompleteJson(const FHttpResultCallback& InOnComplete, int32 InStatusCode, const FString& InMessage)
{
	const FString escapedMessage = InMessage.Replace(TEXT("\""), TEXT("\\\""));
	const FString jsonBody = FString::Printf(TEXT("{\"error\":\"%s\"}"), *escapedMessage);


	FTCHARToUTF8 utf8(*jsonBody);
	TArray<uint8> bytes;
	bytes.Append(reinterpret_cast<const uint8*>(utf8.Get()), utf8.Length());
	CompleteBinary(InOnComplete, InStatusCode, TEXT("application/json"), bytes);
}

void FNativeWebContentHttpServerRuntimeService::CompleteBinary(const FHttpResultCallback& InOnComplete, int32 InStatusCode, const FString& InContentType, const TArray<uint8>& InBody)
{
	TArray<uint8> responseBody = InBody;
	TUniquePtr<FHttpServerResponse> response = FHttpServerResponse::Create(MoveTemp(responseBody), InContentType);
	response->Code = static_cast<EHttpServerResponseCodes>(InStatusCode);
	InOnComplete(MoveTemp(response));
}
