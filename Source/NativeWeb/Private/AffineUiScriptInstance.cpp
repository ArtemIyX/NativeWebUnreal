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

bool FAffineUiScriptInstance::Evaluate(const FString& Source, FQuickJSError& OutError,
	                                   const FString& SourceName)
{
	check(IsInGameThread());
	if (!Runtime || !Runtime->IsOpen())
	{
		OutError.Message = TEXT("Script instance is closed.");
		return false;
	}
	JSValue Result = JS_UNDEFINED;
	if (!Runtime->Evaluate(Source, SourceName, EQuickJSSourceType::Script, Result, OutError))
	{
		return false;
	}
	JS_FreeValue(Runtime->GetContext(), Result);
	return true;
}

bool FAffineUiScriptInstance::DispatchClick(uint64 NodeId, FQuickJSError& OutError)
{
	check(IsInGameThread());
	if (!Runtime || !Runtime->IsOpen() || !Surface || Surface->GetDocumentEpoch() != DocumentEpoch)
	{
		return true;
	}
	JSContext* Context = Runtime->GetContext();
	JSValue Global = JS_GetGlobalObject(Context);
	JSValue Handler = JS_GetPropertyStr(Context, Global, "__nativeWebClick");
	JS_FreeValue(Context, Global);
	if (JS_IsException(Handler))
	{
		Runtime->TakeException(OutError);
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
	if (!Valid || (bElement && Value.IsEmpty())) return JS_ThrowTypeError(
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
