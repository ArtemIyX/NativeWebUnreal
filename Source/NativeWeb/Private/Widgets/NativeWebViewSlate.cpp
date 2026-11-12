#include "Widgets/NativeWebViewSlate.h"

#include "Engine/TextureRenderTarget2D.h"
#include "InputCoreTypes.h"
#include "NativeWebView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"

void SNativeWebView::Construct(const FArguments& Args)
{
	View = Args._View;
	Surface = Args._Surface;
	SetCanTick(true);
	ChildSlot[SAssignNew(Image, SImage).Image(&Brush).Visibility(EVisibility::HitTestInvisible)];
}

void SNativeWebView::SetTexture(UTextureRenderTarget2D* Texture)
{
	Target.Reset(Texture);
	Brush.SetResourceObject(Texture);
	if (Texture) Brush.ImageSize = FVector2D(Texture->SizeX, Texture->SizeY);
	if (Image) Image->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SNativeWebView::SetVisible(bool bInVisible)
{
	if (bVisible == bInVisible) return;
	if (!bVisible) ReleaseInput();
	bVisible = bInVisible;
	SetVisibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed);
}

void SNativeWebView::ReleaseInput()
{
	if (bPointerPressed)
	{
		bPointerPressed = false;
		Forward({EAffineUiInputKind::MouseUp, FVector2D(-1.0, -1.0)});
	}
	if (HasMouseCapture()) FSlateApplication::Get().ReleaseAllPointerCapture();
	if (HasKeyboardFocus()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
	else Forward({EAffineUiInputKind::FocusLost});
}

void SNativeWebView::Tick(const FGeometry& Geometry, double, float)
{
	if (!bVisible) return;
	const FVector2D LocalSize = Geometry.GetLocalSize();
	if (LocalSize.X <= 0 || LocalSize.Y <= 0) return;
	const float Scale = Geometry.GetAccumulatedLayoutTransform().GetScale();
	const double Fit = FMath::Min(1.0, 4096.0 / FMath::Max(LocalSize.X * Scale, LocalSize.Y * Scale));
	const FIntPoint Pixels(FMath::Max(1, FMath::RoundToInt(LocalSize.X * Scale * Fit)),
	                       FMath::Max(1, FMath::RoundToInt(LocalSize.Y * Scale * Fit)));
	if (UNativeWebView* Pinned = View.Get())
		Pinned->Resize(
			Pixels, FMath::Clamp(Scale * static_cast<float>(Fit), 0.01f, 8.0f));
}

void SNativeWebView::Forward(const FAffineUiInput& Input) const
{
	if (!bVisible) return;
	UNativeWebView* Pinned = View.Get();
	if (!Pinned) return;
	FNativeWebInputEvent Event;
	switch (Input.Kind)
	{
	case EAffineUiInputKind::MouseMove: Event.Type = ENativeWebInputType::PointerMove;
		break;
	case EAffineUiInputKind::MouseDown: Event.Type = ENativeWebInputType::PointerDown;
		break;
	case EAffineUiInputKind::MouseUp: Event.Type = ENativeWebInputType::PointerUp;
		break;
	case EAffineUiInputKind::MouseWheel: Event.Type = ENativeWebInputType::Wheel;
		break;
	case EAffineUiInputKind::KeyDown: Event.Type = ENativeWebInputType::KeyDown;
		break;
	case EAffineUiInputKind::KeyUp: Event.Type = ENativeWebInputType::KeyUp;
		break;
	case EAffineUiInputKind::FocusGained: Event.Type = ENativeWebInputType::FocusGained;
		break;
	case EAffineUiInputKind::FocusLost: Event.Type = ENativeWebInputType::FocusLost;
		break;
	}
	Event.NormalizedPosition = Input.Position;
	Event.WheelDelta = Input.WheelDelta;
	Event.bShift = Input.bShift;
	if (Input.Key == EAffineUiInputKey::Tab) Event.Key = ENativeWebInputKey::Tab;
	else if (Input.Key == EAffineUiInputKey::Enter) Event.Key = ENativeWebInputKey::Enter;
	else if (Input.Key == EAffineUiInputKey::Space) Event.Key = ENativeWebInputKey::Space;
	Pinned->ForwardInput(Event);
}

bool SNativeWebView::ForwardPointer(EAffineUiInputKind Kind, const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bVisible) return false;
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0 || Size.Y <= 0) return false;
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	FAffineUiInput Input{Kind, FVector2D(Local.X / Size.X, Local.Y / Size.Y)};
	Input.WheelDelta = Event.GetWheelDelta();
	Input.bShift = Event.IsShiftDown();
	Forward(Input);
	return true;
}

FReply SNativeWebView::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !ForwardPointer(
		EAffineUiInputKind::MouseDown, Geometry, Event))
		return FReply::Unhandled();
	bPointerPressed = true;
	return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
}

FReply SNativeWebView::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return OnMouseButtonDown(Geometry, Event);
}

FReply SNativeWebView::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bPointerPressed || Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	bPointerPressed = false;
	ForwardPointer(EAffineUiInputKind::MouseUp, Geometry, Event);
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SNativeWebView::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return ForwardPointer(EAffineUiInputKind::MouseMove, Geometry, Event) ? FReply::Handled() : FReply::Unhandled();
}

FReply SNativeWebView::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return ForwardPointer(EAffineUiInputKind::MouseWheel, Geometry, Event) ? FReply::Handled() : FReply::Unhandled();
}

void SNativeWebView::OnMouseLeave(const FPointerEvent&)
{
	if (!bPointerPressed) Forward({EAffineUiInputKind::MouseMove, FVector2D(-1.0, -1.0)});
}

void SNativeWebView::OnMouseCaptureLost(const FCaptureLostEvent&)
{
	if (bPointerPressed)
	{
		bPointerPressed = false;
		Forward({EAffineUiInputKind::MouseUp, FVector2D(-1.0, -1.0)});
	}
}

FReply SNativeWebView::OnFocusReceived(const FGeometry&, const FFocusEvent&)
{
	Forward({EAffineUiInputKind::FocusGained});
	return FReply::Handled();
}

void SNativeWebView::OnFocusLost(const FFocusEvent&) { Forward({EAffineUiInputKind::FocusLost}); }

FReply SNativeWebView::ForwardKey(EAffineUiInputKind Kind, const FKeyEvent& Event)
{
	if (!bVisible) return FReply::Unhandled();
	FAffineUiInput Input{Kind};
	if (Event.GetKey() == EKeys::Tab) Input.Key = EAffineUiInputKey::Tab;
	else if (Event.GetKey() == EKeys::Enter) Input.Key = EAffineUiInputKey::Enter;
	else if (Event.GetKey() == EKeys::SpaceBar) Input.Key = EAffineUiInputKey::Space;
	else return FReply::Unhandled();
	Input.bShift = Event.IsShiftDown();
	Forward(Input);
	return FReply::Handled();
}

FReply SNativeWebView::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	return ForwardKey(EAffineUiInputKind::KeyDown, Event);
}

FReply SNativeWebView::OnKeyUp(const FGeometry&, const FKeyEvent& Event)
{
	return ForwardKey(EAffineUiInputKind::KeyUp, Event);
}
