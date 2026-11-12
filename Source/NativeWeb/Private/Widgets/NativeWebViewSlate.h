#pragma once

#include "CoreMinimal.h"
#include "AffineUiSurface.h"
#include "Widgets/SCompoundWidget.h"

class UNativeWebView;
class UTextureRenderTarget2D;
class SImage;

class SNativeWebView final : public SCompoundWidget
{
public:
	using FSurfacePtr = TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe>;
	SLATE_BEGIN_ARGS(SNativeWebView)
		{
		}

		SLATE_ARGUMENT(UNativeWebView*, View)
		SLATE_ARGUMENT(FSurfacePtr, Surface)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void SetTexture(UTextureRenderTarget2D* Texture);
	void SetVisible(bool bVisible);
	void ReleaseInput();
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	virtual void OnMouseLeave(const FPointerEvent&) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
	virtual FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override;
	virtual void OnFocusLost(const FFocusEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override;

private:
	bool ForwardPointer(EAffineUiInputKind Kind, const FGeometry&, const FPointerEvent&);
	FReply ForwardKey(EAffineUiInputKind Kind, const FKeyEvent&);
	void Forward(const FAffineUiInput&) const;
	TWeakObjectPtr<UNativeWebView> View;
	TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	TSharedPtr<SImage> Image;
	FSlateBrush Brush;
	bool bVisible = true;
	bool bPointerPressed = false;
};
