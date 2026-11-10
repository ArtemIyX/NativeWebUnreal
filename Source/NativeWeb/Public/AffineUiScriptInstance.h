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

	bool Evaluate(const FString& Source, FQuickJSError& OutError);
	bool PumpJobs(int32 MaxJobs, int32& OutExecuted, bool& bOutPending, FQuickJSError& OutError);
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> GetSurface() const { return Surface; }
	void Close();

private:
	FAffineUiScriptInstance() = default;
	static JSValue SetText(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	static FString ReadString(JSContext* Context, JSValueConst Value, bool& bOutValid);
	TUniquePtr<FQuickJSRuntime> Runtime;
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
};
#endif // PLATFORM_WINDOWS
