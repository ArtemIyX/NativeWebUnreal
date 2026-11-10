#include "AffineUiScriptInstance.h"

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
	OutError.Reset();
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> NewSurface = FAffineUiSurface::Create(Desc, OutError);
	if (!NewSurface) { return nullptr; }
	TUniquePtr<FQuickJSRuntime> NewRuntime = FQuickJSRuntime::Create(OutError);
	if (!NewRuntime)
	{
		NewSurface->Close();
		return nullptr;
	}
	TUniquePtr<FAffineUiScriptInstance> Instance(new FAffineUiScriptInstance());
	Instance->Surface = MoveTemp(NewSurface);
	Instance->Runtime = MoveTemp(NewRuntime);
	JSContext* Context = Instance->Runtime->GetContext();
	JS_SetContextOpaque(Context, Instance.Get());
	JSValue Global = JS_GetGlobalObject(Context);
	JSValue Api = JS_NewObject(Context);
	JS_SetPropertyStr(Context, Api, "setText",
	                  JS_NewCFunction(Context, &FAffineUiScriptInstance::SetText, "setText", 2));
	JS_SetPropertyStr(Context, Global, "nativeWeb", Api);
	JS_FreeValue(Context, Global);
	return Instance;
}

FAffineUiScriptInstance::~FAffineUiScriptInstance()
{
	if (!IsInGameThread())
	{
		UE_LOG(LogTemp, Fatal, TEXT("AffineUI script instances must be destroyed on the game thread."));
	}
	Close();
}

bool FAffineUiScriptInstance::Evaluate(const FString& Source, FQuickJSError& OutError)
{
	check(IsInGameThread());
	if (!Runtime || !Runtime->IsOpen())
	{
		OutError.Message = TEXT("Script instance is closed.");
		return false;
	}
	JSValue Result = JS_UNDEFINED;
	if (!Runtime->Evaluate(Source, TEXT("<nativeweb-fixture>"), EQuickJSSourceType::Script, Result, OutError))
	{
		return false;
	}
	JS_FreeValue(Runtime->GetContext(), Result);
	return true;
}

bool FAffineUiScriptInstance::PumpJobs(int32 MaxJobs, int32& OutExecuted, bool& bOutPending, FQuickJSError& OutError)
{
	check(IsInGameThread());
	if (!Runtime)
	{
		OutExecuted = 0;
		bOutPending = false;
		OutError.Message = TEXT("Script instance is closed.");
		return false;
	}
	return Runtime->ExecutePendingJobs(MaxJobs, OutExecuted, bOutPending, OutError);
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

void FAffineUiScriptInstance::Close()
{
	if (!IsInGameThread()) { return; }
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
}
#endif // PLATFORM_WINDOWS
