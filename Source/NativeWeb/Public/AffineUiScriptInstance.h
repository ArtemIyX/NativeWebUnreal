#pragma once

#include "CoreMinimal.h"
#include "AffineUiSurface.h"
#if PLATFORM_WINDOWS
#include "QuickJSRuntime.h"

/** One game-thread QuickJS host paired with one NativeWeb AffineUI surface. */
class NATIVEWEB_API FAffineUiScriptInstance final
{
public:
	static TUniquePtr<FAffineUiScriptInstance> Create(const FAffineUiSurfaceDesc& Desc, FString& OutError);
	~FAffineUiScriptInstance();
	FAffineUiScriptInstance(const FAffineUiScriptInstance&) = delete;
	FAffineUiScriptInstance& operator=(const FAffineUiScriptInstance&) = delete;

	bool Evaluate(const FString& Source, FQuickJSError& OutError,
	              const FString& SourceName = TEXT("<nativeweb-script>"));
	bool DispatchClick(uint64 NodeId, FQuickJSError& OutError);
	bool PumpJobs(int32 MaxJobs, int32& OutExecuted, bool& bOutPending, FQuickJSError& OutError);
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> GetSurface() const { return Surface; }
	void Close();

private:
	FAffineUiScriptInstance() = default;
	static JSValue SetText(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue MountRoot(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue CreateElement(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue CreateText(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue CreateComment(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue InsertBefore(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue RemoveNode(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue SetNodeText(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue SetAttribute(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue SetClickHandler(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue RemoveAttribute(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static JSValue CreateNode(JSContext* Context, int32 ArgCount, JSValueConst* Args, EAffineUiMutationKind Kind);
	static JSValue QueueCommand(JSContext* Context, int32 ArgCount, JSValueConst* Args, EAffineUiMutationKind Kind);
	uint64 AllocateNodeId();
	bool Queue(const FAffineUiMutation& Mutation);
	static FString ReadString(JSContext* Context, JSValueConst Value, bool& bOutValid);
	TUniquePtr<FQuickJSRuntime> Runtime;
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	uint64 DocumentEpoch = 0;
	uint64 NextNodeId = 1;
};
#endif // PLATFORM_WINDOWS
