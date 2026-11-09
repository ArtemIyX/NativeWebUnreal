#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class UTextureRenderTarget2D;
class FAffineUiSurfaceRenderer;

/** An owned HTML/CSS snapshot. Relative asset URLs are not resolved by this surface. */
struct NATIVEWEB_API FAffineUiContent
{
	FString Html;
	FString Css;
};

struct NATIVEWEB_API FAffineUiSurfaceDesc
{
	FIntPoint PixelSize = FIntPoint(480, 240);
	float DpiScale = 1.0f;
	FColor ClearColor = FColor(0, 0, 0, 255);
	FAffineUiContent Content;
};

DECLARE_DELEGATE_OneParam(FAffineUiTextureReady, UTextureRenderTarget2D*);
DECLARE_DELEGATE_OneParam(FAffineUiSurfaceError, const FString&);

/** One D3D11 surface. All public methods, including destruction, run on the game thread. */
class NATIVEWEB_API FAffineUiSurface : public TSharedFromThis<FAffineUiSurface, ESPMode::ThreadSafe>
{
public:
	/** Returns null and fills OutError when the RHI or descriptor is unsupported. */
	static TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Create(const FAffineUiSurfaceDesc& Desc, FString& OutError);
	~FAffineUiSurface();

	/** Queue an atomic HTML/CSS replacement. Success means accepted, not rendered. */
	bool SetContent(const FAffineUiContent& Content);
	/** Read both files through Unreal; on either failure the current content is retained. */
	bool LoadContentFromFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError);
	/** Queue a new target; the previous output remains published until the replacement is ready. */
	bool Resize(FIntPoint PixelSize, float DpiScale = 1.0f);
	bool RequestRedraw();

	/** The callback runs on the game thread after each successful texture publication. */
	void SetOnTextureReady(FAffineUiTextureReady Callback);
	/** Reports asynchronous allocation and submission failures on the game thread. */
	void SetOnError(FAffineUiSurfaceError Callback);
	UTextureRenderTarget2D* GetOutputTexture() const;
	FString GetLastError() const;
	void Close();

private:
	friend class FAffineUiSurfaceRenderer;
	FAffineUiSurface();
	void StartPending();
	void OnRenderComplete(uint64 Revision, bool bSuccess);
	class FImpl;
	TUniquePtr<FImpl> Impl;
};
