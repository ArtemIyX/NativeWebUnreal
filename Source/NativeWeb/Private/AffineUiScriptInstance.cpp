

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

TUniquePtr<FAffineUiScriptInstance> FAffineUiScriptInstance::Create(const FAffineUiSurfaceDesc& Desc, FString& OutError)
{
	check(IsInGameThread());
	UE_LOG(LogNativeWebScript, Log, TEXT("Creating QuickJS bridge (surface=%dx%d)."), Desc.PixelSize.X, Desc.PixelSize.Y);
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
	JS_SetPropertyStr(Context, Global, "nativeWeb", Api);
	JS_FreeValue(Context, Global);
	FQuickJSError BootstrapError;
	JSValue BootstrapResult = JS_UNDEFINED;
	const FString Bootstrap = TEXT(
		"(()=>{const listeners=[];Object.defineProperty(nativeWeb,'addMessageListener',{value:function(listener){if(typeof listener!=='function')throw new TypeError('listener must be a function');listeners.push(listener);let active=true;return()=>{if(!active)return;active=false;const i=listeners.indexOf(listener);if(i>=0)listeners.splice(i,1)}}});Object.defineProperty(nativeWeb,'__dispatchMessages',{value:function(messages){const errors=[];for(const message of messages){for(const listener of listeners.slice()){try{listener(message)}catch(error){errors.push(String(error))}}}return errors}})})();");
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
		UE_LOG(LogNativeWebMessaging, VeryVerbose, TEXT("Drained %d JS-to-Unreal message(s); inbound remains=%d bytes=%lld."),
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
		UE_LOG(LogNativeWebMessaging, VeryVerbose, TEXT("Drained %d Unreal-to-JS message(s); outbound remains=%d bytes=%lld."),
		       Count, OutboundMessages.Num(), OutboundBytes);
	}
	return Count;
}

void FAffineUiScriptInstance::RefreshDocumentEpoch()
{
	check(IsInGameThread());
	if (Surface)
	{
		DocumentEpoch = Surface->GetDocumentEpoch();
		UE_LOG(LogNativeWebScript, VeryVerbose, TEXT("Script bridge document epoch refreshed to %llu."),
		       static_cast<unsigned long long>(DocumentEpoch));
	}
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
		if (JS_IsException(ExactBuffer)) return JS_ThrowTypeError(
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
