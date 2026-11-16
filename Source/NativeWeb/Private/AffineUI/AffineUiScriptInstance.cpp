#include "AffineUiScriptInstance.h"
#include "NativeWeb.h"

#if PLATFORM_WINDOWS
#include "quickjs.h"

namespace
{
	bool HasEmbeddedNull(const FString& Value)
	{
		return FCString::Strlen(*Value) != Value.Len();
	}
}

struct FAffineUiScriptInstance::FImageLeaseState
{
	struct FLease
	{
		uint64 ProviderLease = 0;
		uint64 Epoch = 0;
		FString Url;
		FString CacheKey;
		bool bReservedTerminalEvent = true;
	};
	struct FEvent
	{
		uint64 Lease = 0;
		bool bReady = false;
		FString Url;
	FString Error;
		FIntPoint Size = FIntPoint::ZeroValue;
	};
	TWeakObjectPtr<UNativeWebImageSubsystem> Subsystem;
	TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TMap<uint64, FLease> Active;
	TArray<FEvent> Events;
	uint64 NextLease = 1;
	uint64 Epoch = 0;
	bool bClosing = false;
	int32 ReservedEvents = 0;
};

void FAffineUiScriptInstance::CloseImageLeases()
{
	check(IsInGameThread());
	if (!ImageLeases) return;
	ImageLeases->bClosing = true;
	ImageLeases->Events.Reset();
	ImageLeases->ReservedEvents = 0;
	for (const auto& Pair : ImageLeases->Active)
	{
		if (UNativeWebImageSubsystem* Subsystem = ImageLeases->Subsystem.Get())
			Subsystem->ReleaseImage(Pair.Value.ProviderLease);
		if (TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> PinnedSurface = ImageLeases->Surface.Pin())
			PinnedSurface->RemoveImage(Pair.Value.Epoch, Pair.Key, Pair.Value.Url);
	}
	ImageLeases->Active.Reset();
}

TUniquePtr<FAffineUiScriptInstance> FAffineUiScriptInstance::Create(const FAffineUiSurfaceDesc& Desc,
	UNativeWebImageSubsystem* ImageSubsystem, FString& OutError)
{
	check(IsInGameThread());
	UE_LOG(LogNativeWebScript, Log, TEXT("Creating QuickJS bridge (surface=%dx%d)."), Desc.PixelSize.X,
	       Desc.PixelSize.Y);
	OutError.Reset();
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> NewSurface = FAffineUiSurface::Create(Desc, OutError);
	if (!NewSurface)
	{
		UE_LOG(LogNativeWebScript, Error, TEXT("Could not create AffineUI surface for QuickJS bridge: %s."), *OutError);
		return nullptr;
	}
	TUniquePtr<FQuickJSRuntime> NewRuntime = FQuickJSRuntime::Create(OutError);
	if (!NewRuntime)
	{
		UE_LOG(LogNativeWebScript, Error, TEXT("Could not create QuickJS runtime: %s."), *OutError);
		NewSurface->Close();
		return nullptr;
	}


	TUniquePtr<FAffineUiScriptInstance> Instance(new FAffineUiScriptInstance());
	Instance->Surface = MoveTemp(NewSurface);
	Instance->DocumentEpoch = Instance->Surface->GetDocumentEpoch();
	Instance->ImageLeases = MakeShared<FImageLeaseState, ESPMode::ThreadSafe>();
	Instance->ImageLeases->Subsystem = ImageSubsystem;
	Instance->ImageLeases->Surface = Instance->Surface;
	Instance->ImageLeases->Epoch = Instance->DocumentEpoch;
	Instance->Runtime = MoveTemp(NewRuntime);
	JSContext* Context = Instance->Runtime->GetContext();
	JS_SetContextOpaque(Context, Instance.Get());
	JSValue Global = JS_GetGlobalObject(Context);
	JSValue Api = JS_NewObject(Context);
	JS_SetPropertyStr(Context, Api, "setText",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::SetText, "setText", 2));
	JS_SetPropertyStr(Context, Api, "mountRoot",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::MountRoot, "mountRoot", 1));
	JS_SetPropertyStr(Context, Api, "createElement",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::CreateElement, "createElement", 2));
	JS_SetPropertyStr(Context, Api, "createText",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::CreateText, "createText", 1));
	JS_SetPropertyStr(Context, Api, "createComment",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::CreateComment, "createComment", 1));
	JS_SetPropertyStr(Context, Api, "insertBefore",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::InsertBefore, "insertBefore", 3));
	JS_SetPropertyStr(Context, Api, "remove",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::RemoveNode, "remove", 1));
	JS_SetPropertyStr(Context, Api, "setNodeText",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::SetNodeText, "setNodeText", 2));
	JS_SetPropertyStr(Context, Api, "setAttribute",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::SetAttribute, "setAttribute", 3));
	JS_SetPropertyStr(Context, Api, "removeAttribute",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::RemoveAttribute, "removeAttribute", 2));
	JS_SetPropertyStr(Context, Api, "setClickHandler",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::SetClickHandler, "setClickHandler", 1));
	JS_SetPropertyStr(Context, Api, "postString",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::PostString, "postString", 2));
	JS_SetPropertyStr(Context, Api, "postBytes",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::PostBytes, "postBytes", 2));
	JS_SetPropertyStr(Context, Api, "requestImage",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::RequestImage, "requestImage", 2));
	JS_SetPropertyStr(Context, Api, "releaseImage",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::ReleaseImage, "releaseImage", 1));
	JS_SetPropertyStr(Context, Global, "nativeWeb", Api);
	JS_FreeValue(Context, Global);
	FQuickJSError BootstrapError;
	JSValue BootstrapResult = JS_UNDEFINED;
	const FString Bootstrap = TEXT(
		"(()=>{const listeners=[];const imageListeners=[];const subscribe=(target,listener)=>{if(typeof listener!=='function')throw new TypeError('listener must be a function');target.push(listener);let active=true;return()=>{if(!active)return;active=false;const i=target.indexOf(listener);if(i>=0)target.splice(i,1)}};Object.defineProperty(nativeWeb,'addMessageListener',{value:function(listener){return subscribe(listeners,listener)}});Object.defineProperty(nativeWeb,'addImageListener',{value:function(listener){return subscribe(imageListeners,listener)}});Object.defineProperty(nativeWeb,'__dispatchMessages',{value:function(messages){const errors=[];for(const message of messages){for(const listener of listeners.slice()){try{listener(message)}catch(error){errors.push(String(error))}}}return errors}});Object.defineProperty(nativeWeb,'__dispatchImages',{value:function(events){const errors=[];for(const event of events){for(const listener of imageListeners.slice()){try{listener(event)}catch(error){errors.push(String(error))}}}return errors}})})();");
	if (!Instance->Runtime->Evaluate(Bootstrap, TEXT("<nativeweb-bootstrap>"), EQuickJSSourceType::Script,
	                                 BootstrapResult, BootstrapError))
	{
		OutError = BootstrapError.Message;
		UE_LOG(LogNativeWebScript, Error, TEXT("NativeWeb JavaScript bridge bootstrap failed: %s."), *OutError);
		Instance->Close();
		return nullptr;
	}
	JS_FreeValue(Context, BootstrapResult);
	UE_LOG(LogNativeWebScript, Display, TEXT("QuickJS bridge created successfully."));
	return Instance;
}

FAffineUiScriptInstance::~FAffineUiScriptInstance()
{
	if (!IsInGameThread())
	{
		UE_LOG(LogNativeWebScript, Fatal, TEXT("AffineUI script instances must be destroyed on the game thread."));
	}
	Close();
}

bool FAffineUiScriptInstance::Evaluate(const FString& Source, FQuickJSError& OutError,
                                       const FString& SourceName)
{
	check(IsInGameThread());
	if (!Runtime || !Runtime->IsOpen())
	{
		OutError.Message = TEXT("Script instance is closed.");
		UE_LOG(LogNativeWebScript, Warning, TEXT("Script evaluation rejected because the bridge is closed."));
		return false;
	}
	JSValue Result = JS_UNDEFINED;
	if (!Runtime->Evaluate(Source, SourceName, EQuickJSSourceType::Script, Result, OutError))
	{
		UE_LOG(LogNativeWebScript, Error, TEXT("JavaScript evaluation failed in '%s': %s."),
		       *SourceName, *OutError.Message);
		return false;
	}
	JS_FreeValue(Runtime->GetContext(), Result);
	UE_LOG(LogNativeWebScript, VeryVerbose, TEXT("JavaScript evaluated successfully in '%s' (%d chars)."),
	       *SourceName, Source.Len());
	return true;
}

bool FAffineUiScriptInstance::DispatchClick(uint64 NodeId, FQuickJSError& OutError)
{
	check(IsInGameThread());
	if (!Runtime || !Runtime->IsOpen() || !Surface || Surface->GetDocumentEpoch() != DocumentEpoch)
	{
		UE_LOG(LogNativeWebScript, VeryVerbose, TEXT("Ignored stale click callback for node %llu."),
		       static_cast<unsigned long long>(NodeId));
		return true;
	}
	JSContext* Context = Runtime->GetContext();
	JSValue Global = JS_GetGlobalObject(Context);
	JSValue Handler = JS_GetPropertyStr(Context, Global, "__nativeWebClick");
	JS_FreeValue(Context, Global);
	if (JS_IsException(Handler))
	{
		Runtime->TakeException(OutError);
		UE_LOG(LogNativeWebScript, Error, TEXT("Could not retrieve click handler: %s."), *OutError.Message);
		return false;
	}

	bool bSuccess = true;
	if (JS_IsFunction(Context, Handler))
	{
		JSValue Argument = JS_NewInt64(Context, static_cast<int64>(NodeId));
		JSValue Result = JS_UNDEFINED;
		bSuccess = Runtime->Call(Handler, JS_UNDEFINED, 1, &Argument, Result, OutError);
		JS_FreeValue(Context, Argument);
		JS_FreeValue(Context, Result);
	}
	JS_FreeValue(Context, Handler);
	if (!bSuccess)
	{
		UE_LOG(LogNativeWebScript, Error, TEXT("Click handler failed for node %llu: %s."),
		       static_cast<unsigned long long>(NodeId), *OutError.Message);
	}
	return bSuccess;
}

bool FAffineUiScriptInstance::PumpJobs(int32 MaxJobs, int32& OutExecuted, bool& bOutPending, FQuickJSError& OutError)
{
	check(IsInGameThread());
	if (!Runtime)
	{
		OutExecuted = 0;
		bOutPending = false;
		OutError.Message = TEXT("Script instance is closed.");
		UE_LOG(LogNativeWebScript, Warning, TEXT("Cannot pump jobs on a closed script instance."));
		return false;
	}
	const bool bSucceeded = Runtime->ExecutePendingJobs(MaxJobs, OutExecuted, bOutPending, OutError);
	if (bSucceeded)
	{
		UE_LOG(LogNativeWebTick, VeryVerbose,
		       TEXT("QuickJS job pump executed=%d max=%d pending=%s."), OutExecuted, MaxJobs,
		       bOutPending ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(LogNativeWebScript, Error, TEXT("QuickJS job pump failed after %d job(s): %s."),
		       OutExecuted, *OutError.Message);
	}
	return bSucceeded;
}

namespace
{
	int64 MessageSize(const FNativeWebMessage& Message)
	{
		if (Message.Type == ENativeWebPayloadType::PAYLOAD_BYTES) return Message.Bytes.Num();


		const FTCHARToUTF8 Utf8(*Message.String, Message.String.Len());
		return Utf8.Length();
	}
}

ENativeWebQueueResult FAffineUiScriptInstance::QueueToJavaScript(const FNativeWebMessage& Message)
{
	check(IsInGameThread());
	if (bClosing || !Runtime || !Runtime->IsOpen())
	{
		UE_LOG(LogNativeWebMessaging, VeryVerbose, TEXT("Rejected outbound message: script bridge is closed."));
		return ENativeWebQueueResult::RESULT_CLOSED;
	}
	const int64 Size = MessageSize(Message);


	FScopeLock Lock(&MessageMutex);
	if (Size > 1024 * 1024 || OutboundMessages.Num() >= 64 || OutboundBytes + Size > 4 * 1024 * 1024)
	{
		UE_LOG(LogNativeWebMessaging, Verbose,
		       TEXT("Outbound message queue full (channel='%s', bytes=%lld, count=%d, queuedBytes=%lld)."),
		       *Message.Channel.ToString(), Size, OutboundMessages.Num(), OutboundBytes);
		return ENativeWebQueueResult::RESULT_QUEUE_FULL;
	}
	OutboundMessages.Add(Message);
	OutboundBytes += Size;
	UE_LOG(LogNativeWebMessaging, VeryVerbose,
	       TEXT("Outbound message queued (channel='%s', type=%d, bytes=%lld, count=%d)."),
	       *Message.Channel.ToString(), static_cast<int32>(Message.Type), Size, OutboundMessages.Num());
	return ENativeWebQueueResult::RESULT_QUEUED;
}

int32 FAffineUiScriptInstance::DrainMessagesFromJavaScript(TArray<FNativeWebMessage>& OutMessages, int32 MaxMessages)
{
	check(IsInGameThread());


	FScopeLock Lock(&MessageMutex);
	const int32 Count = FMath::Min(MaxMessages, InboundMessages.Num());
	OutMessages.Reserve(OutMessages.Num() + Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FNativeWebMessage Message = MoveTemp(InboundMessages[0]);
		InboundMessages.RemoveAt(0, 1, EAllowShrinking::No);
		InboundBytes -= MessageSize(Message);
		OutMessages.Add(MoveTemp(Message));
	}
	if (Count > 0)
	{
		UE_LOG(LogNativeWebMessaging, VeryVerbose,
		       TEXT("Drained %d JS-to-Unreal message(s); inbound remains=%d bytes=%lld."),
		       Count, InboundMessages.Num(), InboundBytes);
	}
	return Count;
}

int32 FAffineUiScriptInstance::DrainMessagesToJavaScript(TArray<FNativeWebMessage>& OutMessages, int32 MaxMessages)
{
	check(IsInGameThread());


	FScopeLock Lock(&MessageMutex);
	const int32 Count = FMath::Min(MaxMessages, OutboundMessages.Num());
	OutMessages.Reserve(OutMessages.Num() + Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FNativeWebMessage Message = MoveTemp(OutboundMessages[0]);
		OutboundMessages.RemoveAt(0, 1, EAllowShrinking::No);
		OutboundBytes -= MessageSize(Message);
		OutMessages.Add(MoveTemp(Message));
	}
	if (Count > 0)
	{
		UE_LOG(LogNativeWebMessaging, VeryVerbose,
		       TEXT("Drained %d Unreal-to-JS message(s); outbound remains=%d bytes=%lld."),
		       Count, OutboundMessages.Num(), OutboundBytes);
	}
	return Count;
}

void FAffineUiScriptInstance::RefreshDocumentEpoch()
{
	check(IsInGameThread());
	if (Surface)
	{
		const uint64 NewEpoch = Surface->GetDocumentEpoch();
		if (NewEpoch != DocumentEpoch)
		{
			UNativeWebImageSubsystem* ImageSubsystem = ImageLeases ? ImageLeases->Subsystem.Get() : nullptr;
			CloseImageLeases();
			ImageLeases = MakeShared<FImageLeaseState, ESPMode::ThreadSafe>();
			ImageLeases->Subsystem = ImageSubsystem;
			ImageLeases->Surface = Surface;
			ImageLeases->Epoch = NewEpoch;
			DocumentEpoch = NewEpoch;
		}
		UE_LOG(LogNativeWebScript, VeryVerbose, TEXT("Script bridge document epoch refreshed to %llu."),
		       static_cast<unsigned long long>(DocumentEpoch));
	}
}

bool FAffineUiScriptInstance::DeliverPendingImageEvents(FQuickJSError& OutError)
{
	check(IsInGameThread());
	if (!Runtime || !Runtime->IsOpen() || !ImageLeases || ImageLeases->Events.IsEmpty()) return true;
	TArray<FImageLeaseState::FEvent> Events = MoveTemp(ImageLeases->Events);
	ImageLeases->Events.Reset();
	ImageLeases->ReservedEvents = FMath::Max(0, ImageLeases->ReservedEvents - Events.Num());
	for (const auto& Event : Events)
		if (FImageLeaseState::FLease* Lease = ImageLeases->Active.Find(Event.Lease))
			Lease->bReservedTerminalEvent = false;
	JSContext* Context = Runtime->GetContext();
	JSValue Global = JS_GetGlobalObject(Context);
	JSValue NativeWeb = JS_GetPropertyStr(Context, Global, "nativeWeb");
	JSValue Dispatch = JS_GetPropertyStr(Context, NativeWeb, "__dispatchImages");
	JS_FreeValue(Context, NativeWeb);
	JS_FreeValue(Context, Global);
	if (!JS_IsFunction(Context, Dispatch)) { JS_FreeValue(Context, Dispatch); return false; }
	JSValue Array = JS_NewArray(Context);
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		const auto& Event = Events[Index];
		JSValue Item = JS_NewObject(Context);
		JS_SetPropertyStr(Context, Item, "lease", JS_NewInt64(Context, static_cast<int64>(Event.Lease)));
		JS_SetPropertyStr(Context, Item, "status", JS_NewString(Context, Event.bReady ? "ready" : "error"));
		if (Event.bReady)
		{
			const FTCHARToUTF8 Url(*Event.Url);
			JS_SetPropertyStr(Context, Item, "url", JS_NewStringLen(Context, Url.Get(), Url.Length()));
			JS_SetPropertyStr(Context, Item, "width", JS_NewInt32(Context, Event.Size.X));
			JS_SetPropertyStr(Context, Item, "height", JS_NewInt32(Context, Event.Size.Y));
		}
		else
		{
			const FTCHARToUTF8 Error(*Event.Error);
			JS_SetPropertyStr(Context, Item, "error", JS_NewStringLen(Context, Error.Get(), Error.Length()));
		}
		JS_SetPropertyUint32(Context, Array, Index, Item);
	}
	JSValue Result = JS_UNDEFINED;
	FQuickJSError Error;
	const bool bSuccess = Runtime->Call(Dispatch, JS_UNDEFINED, 1, &Array, Result, Error);
	if (!bSuccess)
		UE_LOG(LogNativeWebScript, Warning, TEXT("Native image event dispatch failed: %s"), *Error.Message);
	OutError = MoveTemp(Error);
	JS_FreeValue(Context, Result);
	JS_FreeValue(Context, Array);
	JS_FreeValue(Context, Dispatch);
	return bSuccess;
}

bool FAffineUiScriptInstance::DeliverMessageToJavaScript(const FNativeWebMessage& Message, FQuickJSError& OutError)
{
	check(IsInGameThread());
	if (bClosing || !Runtime || !Runtime->IsOpen())
	{
		UE_LOG(LogNativeWebMessaging, VeryVerbose, TEXT("Skipped Unreal-to-JS delivery because the bridge is closed."));
		return false;
	}
	JSContext* Context = Runtime->GetContext();
	JSValue Global = JS_GetGlobalObject(Context);
	JSValue NativeWeb = JS_GetPropertyStr(Context, Global, "nativeWeb");
	JSValue Dispatch = JS_GetPropertyStr(Context, NativeWeb, "__dispatchMessages");
	JS_FreeValue(Context, NativeWeb);
	JS_FreeValue(Context, Global);
	if (!JS_IsFunction(Context, Dispatch))
	{
		JS_FreeValue(Context, Dispatch);
		UE_LOG(LogNativeWebMessaging, Error, TEXT("The JavaScript message dispatcher is missing."));
		return false;
	}
	JSValue Item = JS_NewObject(Context);
	if (!Message.Channel.IsNone())
	{
		const FTCHARToUTF8 Channel(*Message.Channel.ToString());
		JS_SetPropertyStr(Context, Item, "channel", JS_NewStringLen(Context, Channel.Get(), Channel.Length()));
	}
	if (Message.Type == ENativeWebPayloadType::PAYLOAD_STRING)
	{
		const FTCHARToUTF8 Value(*Message.String, Message.String.Len());
		JS_SetPropertyStr(Context, Item, "type", JS_NewString(Context, "string"));
		JS_SetPropertyStr(Context, Item, "value", JS_NewStringLen(Context, Value.Get(), Value.Length()));
	}
	else
	{
		JS_SetPropertyStr(Context, Item, "type", JS_NewString(Context, "bytes"));
		JS_SetPropertyStr(Context, Item, "value",
		                  JS_NewUint8ArrayCopy(Context, Message.Bytes.GetData(), Message.Bytes.Num()));
	}
	JSValue Items = JS_NewArray(Context);
	JS_SetPropertyUint32(Context, Items, 0, Item);
	JSValue Result = JS_UNDEFINED;
	const bool bSuccess = Runtime->Call(Dispatch, JS_UNDEFINED, 1, &Items, Result, OutError);
	bool bHadListenerError = false;
	if (bSuccess && JS_IsArray(Result))
	{
		JSValue LengthValue = JS_GetPropertyStr(Context, Result, "length");
		uint32 ErrorCount = 0;
		if (JS_ToUint32(Context, &ErrorCount, LengthValue) == 0 && ErrorCount > 0)
		{
			JSValue FirstError = JS_GetPropertyUint32(Context, Result, 0);
			bool bValid = false;
			OutError.Message = ReadString(Context, FirstError, bValid);
			JS_FreeValue(Context, FirstError);
			bHadListenerError = true;
		}
		JS_FreeValue(Context, LengthValue);
	}
	JS_FreeValue(Context, Result);
	JS_FreeValue(Context, Items);
	JS_FreeValue(Context, Dispatch);
	if (!bSuccess || bHadListenerError)
	{
		UE_LOG(LogNativeWebMessaging, Warning,
		       TEXT("Unreal-to-JS dispatch completed with listener/call errors (channel='%s', type=%d)."),
		       *Message.Channel.ToString(), static_cast<int32>(Message.Type));
	}
	else
	{
		UE_LOG(LogNativeWebMessaging, VeryVerbose,
		       TEXT("Unreal-to-JS dispatch completed (channel='%s', type=%d)."),
		       *Message.Channel.ToString(), static_cast<int32>(Message.Type));
	}
	return bSuccess && !bHadListenerError;
}

JSValue FAffineUiScriptInstance::PostString(JSContext* Context, JSValueConst, int32 ArgCount, JSValueConst* Args)
{
	if (ArgCount < 1 || ArgCount > 2 || !JS_IsString(Args[0]) ||
		(ArgCount == 2 && !JS_IsUndefined(Args[1]) && !JS_IsString(Args[1])))
		return JS_ThrowTypeError(Context, "postString expects a string value and an optional string channel");
	FAffineUiScriptInstance* Instance = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(Context));
	if (!Instance || Instance->bClosing) return JS_FALSE;
	size_t ValueLength = 0;
	const char* ValueUtf8 = JS_ToCStringLen(Context, &ValueLength, Args[0]);
	if (!ValueUtf8) return JS_EXCEPTION;
	if (ValueLength > 1024 * 1024)
	{
		JS_FreeCString(Context, ValueUtf8);
		return JS_ThrowRangeError(Context, "NativeWeb message exceeds 1 MiB");
	}
	FNativeWebMessage Message;
	Message.Type = ENativeWebPayloadType::PAYLOAD_STRING;


	const FUTF8ToTCHAR ValueText(ValueUtf8, static_cast<int32>(ValueLength));
	Message.String = FString(ValueText.Length(), ValueText.Get());
	JS_FreeCString(Context, ValueUtf8);
	if (ArgCount == 2 && !JS_IsUndefined(Args[1]))
	{
		bool bValid = false;
		const FString Channel = ReadString(Context, Args[1], bValid);
		if (JS_HasException(Context)) return JS_EXCEPTION;
		if (!bValid) return JS_ThrowTypeError(Context, "channel must be a valid string");
		Message.Channel = FName(*Channel);
	}


	FScopeLock Lock(&Instance->MessageMutex);
	const int64 Size = static_cast<int64>(ValueLength);
	if (Instance->InboundMessages.Num() >= 64 || Instance->InboundBytes + Size > 4 * 1024 * 1024)
	{
		UE_LOG(LogNativeWebMessaging, Verbose,
		       TEXT("JS string message rejected by inbound queue limit (channel='%s', bytes=%lld, count=%d)."),
		       *Message.Channel.ToString(), Size, Instance->InboundMessages.Num());
		return JS_FALSE;
	}
	UE_LOG(LogNativeWebMessaging, VeryVerbose,
	       TEXT("JS string message queued for Unreal (channel='%s', bytes=%lld, count=%d)."),
	       *Message.Channel.ToString(), Size, Instance->InboundMessages.Num() + 1);
	Instance->InboundMessages.Add(MoveTemp(Message));
	Instance->InboundBytes += Size;
	return JS_TRUE;
}

JSValue FAffineUiScriptInstance::PostBytes(JSContext* Context, JSValueConst, int32 ArgCount, JSValueConst* Args)
{
	if (ArgCount < 1 || ArgCount > 2 ||
		(ArgCount == 2 && !JS_IsUndefined(Args[1]) && !JS_IsString(Args[1])))
		return JS_ThrowTypeError(Context, "postBytes expects an ArrayBuffer or view and an optional string channel");
	FAffineUiScriptInstance* Instance = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(Context));
	if (!Instance || Instance->bClosing) return JS_FALSE;
	uint8* Data = nullptr;
	size_t Length = 0;
	if (JS_IsArrayBuffer(Args[0]))
	{
		Data = JS_GetArrayBuffer(Context, &Length, Args[0]);
		if (!Data && Length != 0) return JS_EXCEPTION;
	}
	else if (JS_IsDataView(Args[0]))
	{
		JSValue BufferValue = JS_GetPropertyStr(Context, Args[0], "buffer");
		JSValue OffsetValue = JS_GetPropertyStr(Context, Args[0], "byteOffset");
		JSValue LengthValue = JS_GetPropertyStr(Context, Args[0], "byteLength");
		uint64 Offset64 = 0, Length64 = 0;
		if (!JS_IsArrayBuffer(BufferValue) || JS_ToIndex(Context, &Offset64, OffsetValue) < 0 || JS_ToIndex(
			Context, &Length64, LengthValue) < 0)
		{
			JS_FreeValue(Context, BufferValue);
			JS_FreeValue(Context, OffsetValue);
			JS_FreeValue(Context, LengthValue);
			return JS_ThrowTypeError(Context, "invalid DataView");
		}
		size_t BufferLength = 0;
		uint8* BufferData = JS_GetArrayBuffer(Context, &BufferLength, BufferValue);
		JS_FreeValue(Context, BufferValue);
		JS_FreeValue(Context, OffsetValue);
		JS_FreeValue(Context, LengthValue);
		if ((!BufferData && BufferLength != 0) || Offset64 > BufferLength || Length64 > BufferLength - Offset64)
			return JS_ThrowTypeError(Context, "invalid DataView bounds");
		Data = BufferData ? BufferData + Offset64 : nullptr;
		Length = Length64;
	}
	else
	{
		JSValue ExactBuffer = JS_GetTypedArrayBuffer(Context, Args[0], nullptr, nullptr, nullptr);
		if (JS_IsException(ExactBuffer))
			return JS_ThrowTypeError(
				Context, "value must be an ArrayBuffer or ArrayBufferView");
		JSValue OffsetValue = JS_GetPropertyStr(Context, Args[0], "byteOffset");
		JSValue LengthValue = JS_GetPropertyStr(Context, Args[0], "byteLength");
		uint64 Offset = 0, ViewLength = 0;
		if (JS_ToIndex(Context, &Offset, OffsetValue) < 0 || JS_ToIndex(Context, &ViewLength, LengthValue) < 0)
		{
			JS_FreeValue(Context, ExactBuffer);
			JS_FreeValue(Context, OffsetValue);
			JS_FreeValue(Context, LengthValue);
			return JS_ThrowTypeError(Context, "invalid ArrayBufferView bounds");
		}
		JS_FreeValue(Context, OffsetValue);
		JS_FreeValue(Context, LengthValue);
		size_t BufferLength = 0;
		uint8* BufferData = JS_GetArrayBuffer(Context, &BufferLength, ExactBuffer);
		JS_FreeValue(Context, ExactBuffer);
		if ((!BufferData && BufferLength != 0) || Offset > BufferLength || ViewLength > BufferLength - Offset)
			return JS_ThrowTypeError(Context, "invalid ArrayBufferView");
		Data = BufferData ? BufferData + Offset : nullptr;
		Length = ViewLength;
	}
	if (Length > 1024 * 1024) return JS_ThrowRangeError(Context, "NativeWeb message exceeds 1 MiB");
	FNativeWebMessage Message;
	Message.Type = ENativeWebPayloadType::PAYLOAD_BYTES;
	if (Length) Message.Bytes.Append(Data, static_cast<int32>(Length));
	if (ArgCount == 2 && !JS_IsUndefined(Args[1]))
	{
		bool bValid = false;
		const FString Channel = ReadString(Context, Args[1], bValid);
		if (JS_HasException(Context)) return JS_EXCEPTION;
		if (!bValid) return JS_ThrowTypeError(Context, "channel must be a valid string");
		Message.Channel = FName(*Channel);
	}


	FScopeLock Lock(&Instance->MessageMutex);
	if (Instance->InboundMessages.Num() >= 64 || Instance->InboundBytes + static_cast<int64>(Length) > 4 * 1024 * 1024)
	{
		UE_LOG(LogNativeWebMessaging, Verbose,
		       TEXT("JS byte message rejected by inbound queue limit (channel='%s', bytes=%llu, count=%d)."),
		       *Message.Channel.ToString(), static_cast<unsigned long long>(Length), Instance->InboundMessages.Num());
		return JS_FALSE;
	}
	UE_LOG(LogNativeWebMessaging, VeryVerbose,
	       TEXT("JS byte message queued for Unreal (channel='%s', bytes=%llu, count=%d)."),
	       *Message.Channel.ToString(), static_cast<unsigned long long>(Length), Instance->InboundMessages.Num() + 1);
	Instance->InboundMessages.Add(MoveTemp(Message));
	Instance->InboundBytes += static_cast<int64>(Length);
	return JS_TRUE;
}

FString FAffineUiScriptInstance::ReadString(JSContext* Context, JSValueConst Value, bool& bOutValid)
{
	size_t Length = 0;
	const char* Utf8 = JS_ToCStringLen(Context, &Length, Value);
	if (!Utf8)
	{
		bOutValid = false;
		return FString();
	}
	const int32 SafeLength = static_cast<int32>(FMath::Min<size_t>(Length, MAX_int32));


	const FUTF8ToTCHAR Converted(Utf8, SafeLength);


	FString Result(Converted.Length(), Converted.Get());
	JS_FreeCString(Context, Utf8);
	bOutValid = !HasEmbeddedNull(Result);
	return Result;
}

JSValue FAffineUiScriptInstance::SetText(JSContext* Context, JSValueConst, int32 ArgCount, JSValueConst* Args)
{
	if (ArgCount != 2) { return JS_ThrowTypeError(Context, "nativeWeb.setText expects an element id and text"); }
	if (!JS_IsString(Args[0]) || !JS_IsString(Args[1]))
	{
		return JS_ThrowTypeError(Context, "element id and text must be strings");
	}
	bool bValidId = false, bValidText = false;
	const FString Id = ReadString(Context, Args[0], bValidId);
	if (JS_HasException(Context)) { return JS_EXCEPTION; }
	const FString Text = ReadString(Context, Args[1], bValidText);
	if (JS_HasException(Context)) { return JS_EXCEPTION; }
	if (!bValidId || Id.IsEmpty() || !bValidText)
	{
		return JS_ThrowTypeError(Context, "element id and text must be valid strings");
	}
	FAffineUiScriptInstance* Instance = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(Context));
	if (!Instance || !Instance->Surface || !Instance->Surface->SetTextById(Id, Text))
	{
		return JS_ThrowInternalError(Context, "text update could not be queued");
	}
	return JS_TRUE;
}

uint64 FAffineUiScriptInstance::AllocateNodeId() { return NextNodeId++; }

bool FAffineUiScriptInstance::Queue(const FAffineUiMutation& Mutation)
{
	check(IsInGameThread());
	return Surface && Mutation.DocumentEpoch == DocumentEpoch && Surface->QueueMutation(Mutation);
}

JSValue FAffineUiScriptInstance::CreateNode(JSContext* C, int32 N, JSValueConst* A, EAffineUiMutationKind Kind)
{
	const bool bElement = Kind == EAffineUiMutationKind::CreateElement;
	if (N < 1 || N > (bElement ? 2 : 1) || !JS_IsString(A[0]))
	{
		return JS_ThrowTypeError(C, "node creation expects a string and an optional SVG namespace for elements");
	}
	if (N == 2 && !JS_IsUndefined(A[1]))
	{
		bool bValidNamespace = false;
		if (!JS_IsString(A[1]) || ReadString(C, A[1], bValidNamespace) != TEXT("svg") || !bValidNamespace)
		{
			if (JS_HasException(C)) return JS_EXCEPTION;
			return JS_ThrowTypeError(C, "the supported element namespace is svg");
		}
		Kind = EAffineUiMutationKind::CreateSvgElement;
	}
	bool Valid = false;
	FString Value = ReadString(C, A[0], Valid);
	if (JS_HasException(C)) return JS_EXCEPTION;
	if (!Valid || (bElement && Value.IsEmpty()))
		return JS_ThrowTypeError(
			C, "node value is invalid");
	auto* I = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(C));
	if (!I) return JS_ThrowInternalError(C, "script instance is closed");
	const uint64 Id = I->AllocateNodeId();
	FAffineUiMutation M;
	M.DocumentEpoch = I->DocumentEpoch;
	M.NodeId = Id;
	M.Kind = Kind;
	if (bElement) M.Name = Value;
	else M.Value = Value;
	if (!I->Queue(M)) return JS_ThrowInternalError(C, "node creation could not be queued");
	return JS_NewInt64(C, static_cast<int64>(Id));
}

JSValue FAffineUiScriptInstance::RequestImage(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	if (N != 2 || !JS_IsString(A[0]) || !JS_IsString(A[1]))
		return JS_ThrowTypeError(C, "requestImage expects a provider name and key");
	bool bProviderValid = false;
	bool bKeyValid = false;
	const FString ProviderText = ReadString(C, A[0], bProviderValid);
	const FString Key = ReadString(C, A[1], bKeyValid);
	if (JS_HasException(C)) return JS_EXCEPTION;
	if (!bProviderValid || !bKeyValid || ProviderText.IsEmpty() || Key.IsEmpty() ||
		ProviderText.Len() > 128 || Key.Len() > 4096 || HasEmbeddedNull(ProviderText) || HasEmbeddedNull(Key))
		return JS_ThrowTypeError(C, "provider name or image key is invalid");
	FAffineUiScriptInstance* Instance = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(C));
	if (!Instance || !Instance->ImageLeases || Instance->bClosing)
		return JS_ThrowInternalError(C, "script instance is closed");
	const TSharedPtr<FImageLeaseState, ESPMode::ThreadSafe> State = Instance->ImageLeases;
	if (State->bClosing || State->Active.Num() >= 64 || State->ReservedEvents >= 64 || State->NextLease >= (1ull << 53))
		return JS_NewInt32(C, 0);
	UNativeWebImageSubsystem* Subsystem = State->Subsystem.Get();
	if (!Subsystem) return JS_NewInt32(C, 0);
	const uint64 LeaseId = State->NextLease++;
	const FString GpuBudgetNamespace = Subsystem->GetGpuBudgetNamespace();
	const uint64 MaxGpuBytes = Subsystem->GetMaxGpuBytes();
	FImageLeaseState::FLease Lease;
	Lease.Epoch = State->Epoch;
	Lease.Url = FString::Printf(TEXT("nativeweb-image://%llu/%llu"),
		static_cast<unsigned long long>(Lease.Epoch), static_cast<unsigned long long>(LeaseId));
	Lease.CacheKey = Subsystem->MakeCacheKey(FName(*ProviderText), Key);
	State->Active.Add(LeaseId, Lease);
	++State->ReservedEvents;
	const uint64 ProviderLease = Subsystem->RequestImage(FName(*ProviderText), Key,
		[State, LeaseId, GpuBudgetNamespace, MaxGpuBytes](uint64 ResolvedLease, UTexture2D* Texture, const FString& Error)
		{
			if (!State || State->bClosing || State->Epoch == 0 || State->Events.Num() >= State->ReservedEvents) return;
			FImageLeaseState::FLease* Active = State->Active.Find(LeaseId);
			if (!Active || ResolvedLease != Active->ProviderLease || Active->Epoch != State->Epoch) return;
			FImageLeaseState::FEvent Event;
			Event.Lease = LeaseId;
			Event.Url = Active->Url;
			if (!Texture)
			{
				Event.Error = Error.IsEmpty() ? TEXT("Image provider returned no texture.") : Error;
				State->Events.Add(MoveTemp(Event));
				return;
			}
			if (TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface = State->Surface.Pin())
			{
				const FString Url = Active->Url;
				const FString CacheKey = Active->CacheKey;
				const uint64 Epoch = Active->Epoch;
				if (Surface->InstallImage(Epoch, LeaseId, Url, CacheKey, GpuBudgetNamespace, MaxGpuBytes, Texture,
					[State, LeaseId, Epoch, Url](bool bInstalled, FIntPoint Size, const FString& InstallError)
					{
						if (State->bClosing || State->Epoch != Epoch) return;
						FImageLeaseState::FLease* Current = State->Active.Find(LeaseId);
						if (!Current || Current->Url != Url || State->Events.Num() >= State->ReservedEvents) return;
						FImageLeaseState::FEvent Terminal;
						Terminal.Lease = LeaseId;
						Terminal.bReady = bInstalled;
						Terminal.Url = Url;
						Terminal.Size = Size;
						Terminal.Error = InstallError;
						State->Events.Add(MoveTemp(Terminal));
					}))
					return;
			}
			Event.Error = TEXT("Image surface is no longer available.");
			State->Events.Add(MoveTemp(Event));
		});
	if (ProviderLease == 0)
	{
		State->Active.Remove(LeaseId);
		--State->ReservedEvents;
		return JS_NewInt32(C, 0);
	}
	if (FImageLeaseState::FLease* Active = State->Active.Find(LeaseId)) Active->ProviderLease = ProviderLease;
	return JS_NewFloat64(C, static_cast<double>(LeaseId));
}

JSValue FAffineUiScriptInstance::ReleaseImage(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	if (N != 1) return JS_ThrowTypeError(C, "releaseImage expects a lease ID");
	double Number = 0.0;
	if (JS_ToFloat64(C, &Number, A[0]) < 0 || !FMath::IsFinite(Number) || Number <= 0.0 ||
		Number >= 9007199254740992.0 || static_cast<double>(static_cast<uint64>(Number)) != Number)
		return JS_ThrowTypeError(C, "lease ID must be a positive safe integer");
	FAffineUiScriptInstance* Instance = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(C));
	if (!Instance || !Instance->ImageLeases) return JS_FALSE;
	const uint64 LeaseId = static_cast<uint64>(Number);
	FImageLeaseState::FLease Lease;
	if (!Instance->ImageLeases->Active.RemoveAndCopyValue(LeaseId, Lease)) return JS_FALSE;
	for (int32 Index = Instance->ImageLeases->Events.Num() - 1; Index >= 0; --Index)
		if (Instance->ImageLeases->Events[Index].Lease == LeaseId)
			Instance->ImageLeases->Events.RemoveAt(Index, 1, EAllowShrinking::No);
	if (Lease.bReservedTerminalEvent)
		Instance->ImageLeases->ReservedEvents = FMath::Max(0, Instance->ImageLeases->ReservedEvents - 1);
	if (Instance->ImageLeases->Subsystem.IsValid()) Instance->ImageLeases->Subsystem->ReleaseImage(Lease.ProviderLease);
	if (TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface = Instance->ImageLeases->Surface.Pin())
		Surface->RemoveImage(Lease.Epoch, LeaseId, Lease.Url);
	return JS_TRUE;
}

JSValue FAffineUiScriptInstance::MountRoot(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	if (N != 1 || !JS_IsString(A[0])) return JS_ThrowTypeError(C, "mountRoot expects an element ID");
	bool Valid = false;
	FString Name = ReadString(C, A[0], Valid);
	if (JS_HasException(C)) return JS_EXCEPTION;
	if (!Valid || Name.IsEmpty()) return JS_ThrowTypeError(C, "mount root ID is invalid");
	auto* I = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(C));
	if (!I) return JS_ThrowInternalError(C, "script instance is closed");
	const uint64 Id = I->AllocateNodeId();
	FAffineUiMutation M;
	M.DocumentEpoch = I->DocumentEpoch;
	M.NodeId = Id;
	M.Kind = EAffineUiMutationKind::MountRoot;
	M.Name = Name;
	if (!I->Queue(M)) return JS_ThrowInternalError(C, "mount root could not be queued");
	return JS_NewInt64(C, static_cast<int64>(Id));
}

JSValue FAffineUiScriptInstance::CreateElement(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return CreateNode(C, N, A, EAffineUiMutationKind::CreateElement);
}

JSValue FAffineUiScriptInstance::CreateText(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return CreateNode(C, N, A, EAffineUiMutationKind::CreateText);
}

JSValue FAffineUiScriptInstance::CreateComment(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return CreateNode(C, N, A, EAffineUiMutationKind::CreateComment);
}

JSValue FAffineUiScriptInstance::QueueCommand(JSContext* C, int32 N, JSValueConst* A, EAffineUiMutationKind Kind)
{
	auto* I = static_cast<FAffineUiScriptInstance*>(JS_GetContextOpaque(C));
	if (!I) return JS_ThrowInternalError(C, "script instance is closed");
	FAffineUiMutation M;
	M.DocumentEpoch = I->DocumentEpoch;
	M.Kind = Kind;
	int64 Id = 0;
	if (Kind == EAffineUiMutationKind::InsertBefore)
	{
		int64 Parent = 0, Before = 0;
		if (N != 3 || JS_ToInt64(C, &Parent, A[0]) < 0 || Parent <= 0 ||
			JS_ToInt64(C, &Id, A[1]) < 0 || Id <= 0)
		{
			return JS_ThrowTypeError(C, "insertBefore expects parent, node, anchor");
		}
		if (!JS_IsNull(A[2]) && !JS_IsUndefined(A[2]) &&
			(JS_ToInt64(C, &Before, A[2]) < 0 || Before <= 0))
		{
			return JS_ThrowTypeError(C, "anchor must be a node ID or null");
		}
		M.ParentId = static_cast<uint64>(Parent);
		M.NodeId = static_cast<uint64>(Id);
		M.BeforeId = static_cast<uint64>(Before);
	}
	else
	{
		if (N < 1 || JS_ToInt64(C, &Id, A[0]) < 0 || Id <= 0)
		{
			return JS_ThrowTypeError(C, "node ID must be a positive integer");
		}
		M.NodeId = static_cast<uint64>(Id);
	}
	if (Kind == EAffineUiMutationKind::RegisterClickHandler && N != 1)
	{
		return JS_ThrowTypeError(C, "setClickHandler expects one node ID");
	}
	if (Kind == EAffineUiMutationKind::Remove && N != 1)
	{
		return JS_ThrowTypeError(C, "remove expects one node ID");
	}
	if (Kind == EAffineUiMutationKind::SetText)
	{
		if (N != 2 || !JS_IsString(A[1]))
		{
			return JS_ThrowTypeError(C, "setNodeText expects node and text");
		}
		bool V = false;
		M.Value = ReadString(C, A[1], V);
		if (JS_HasException(C)) return JS_EXCEPTION;
		if (!V) return JS_ThrowTypeError(C, "text is invalid");
	}
	else if (Kind == EAffineUiMutationKind::SetAttribute || Kind == EAffineUiMutationKind::RemoveAttribute)
	{
		const int32 Expected = Kind == EAffineUiMutationKind::SetAttribute ? 3 : 2;
		if (N != Expected || !JS_IsString(A[1]))
		{
			return JS_ThrowTypeError(C, "attribute name is required");
		}
		bool V = false;
		M.Name = ReadString(C, A[1], V);
		if (JS_HasException(C)) return JS_EXCEPTION;
		if (!V || M.Name.IsEmpty()) return JS_ThrowTypeError(C, "attribute name is invalid");
		if (Expected == 3)
		{
			if (!JS_IsString(A[2])) return JS_ThrowTypeError(C, "attribute value must be a string");
			M.Value = ReadString(C, A[2], V);
			if (JS_HasException(C)) return JS_EXCEPTION;
			if (!V) return JS_ThrowTypeError(C, "attribute value is invalid");
		}
	}
	if (!I->Queue(M)) return JS_ThrowInternalError(C, "mutation could not be queued");
	return JS_TRUE;
}

JSValue FAffineUiScriptInstance::InsertBefore(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return QueueCommand(C, N, A, EAffineUiMutationKind::InsertBefore);
}

JSValue FAffineUiScriptInstance::RemoveNode(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return QueueCommand(C, N, A, EAffineUiMutationKind::Remove);
}

JSValue FAffineUiScriptInstance::SetNodeText(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return QueueCommand(C, N, A, EAffineUiMutationKind::SetText);
}

JSValue FAffineUiScriptInstance::SetAttribute(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return QueueCommand(C, N, A, EAffineUiMutationKind::SetAttribute);
}

JSValue FAffineUiScriptInstance::RemoveAttribute(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return QueueCommand(C, N, A, EAffineUiMutationKind::RemoveAttribute);
}

JSValue FAffineUiScriptInstance::SetClickHandler(JSContext* C, JSValueConst, int32 N, JSValueConst* A)
{
	return QueueCommand(C, N, A, EAffineUiMutationKind::RegisterClickHandler);
}

void FAffineUiScriptInstance::Close()
{
	if (!IsInGameThread())
	{
		UE_LOG(LogNativeWebScript, Error, TEXT("Refusing to close a QuickJS bridge off the game thread."));
		return;
	}
	if (bClosing) return;
	UE_LOG(LogNativeWebScript, Display, TEXT("Closing QuickJS bridge (inbound=%d outbound=%d)."),
	       InboundMessages.Num(), OutboundMessages.Num());
	bClosing = true;
	CloseImageLeases();
	{
		FScopeLock Lock(&MessageMutex);
		InboundMessages.Reset();
		OutboundMessages.Reset();
		InboundBytes = 0;
		OutboundBytes = 0;
	}
	if (Runtime)
	{
		if (JSContext* Context = Runtime->GetContext()) { JS_SetContextOpaque(Context, nullptr); }
		Runtime->Close();
		Runtime.Reset();
	}
	if (Surface)
	{
		Surface->Close();
		Surface.Reset();
	}
	UE_LOG(LogNativeWebScript, Verbose, TEXT("QuickJS bridge closed."));
}
#endif // PLATFORM_WINDOWS
