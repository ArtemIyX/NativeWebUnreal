#include "Widgets/NativeWebViewSlate.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "NativeWeb.h"
#include "Input/NativeWebInputRouting.h"
#include "NativeWebView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"

void SNativeWebView::Construct(const FArguments& Args)
{
	View = Args._View;
	Player = Args._Player;
	Surface = Args._Surface;
	UNativeWebView* ViewObject = View.Get();
	UE_LOG(LogNativeWebViewport, Verbose, TEXT("Constructing Slate attachment (view=%s surface=%s)."),
	       ViewObject ? *ViewObject->GetName() : TEXT("<null>"), Surface.IsValid() ? TEXT("valid") : TEXT("null"));
	SetCanTick(true);
	ChildSlot[SAssignNew(Image, SImage).Image(&Brush).Visibility(EVisibility::HitTestInvisible)];
}

void SNativeWebView::SetTexture(UTextureRenderTarget2D* Texture)
{
	UE_LOG(LogNativeWebViewport, VeryVerbose, TEXT("Slate attachment texture changed to %s."),
	       Texture ? *Texture->GetName() : TEXT("<null>"));
	Target.Reset(Texture);
	Brush.SetResourceObject(Texture);
	if (Texture) Brush.ImageSize = FVector2D(Texture->SizeX, Texture->SizeY);
	if (Image) Image->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SNativeWebView::SetVisible(bool bInVisible)
{
	if (bVisible == bInVisible) return;
	UE_LOG(LogNativeWebViewport, Verbose, TEXT("Slate attachment visibility changed to %s."),
	       bInVisible ? TEXT("visible") : TEXT("collapsed"));
	if (!bVisible) ReleaseInput();
	bVisible = bInVisible;
	SetVisibility(bVisible ? EVisibility::Visible : EVisibility::Collapsed);
}

void SNativeWebView::ReleaseInput(const bool bPreserveKeyboardFocus)
{
	UE_LOG(LogNativeWebInput, Verbose,
	       TEXT("Releasing Slate input state (pointerPressed=%s mouseCapture=%s keyboardFocus=%s)."),
	       bPointerPressed ? TEXT("true") : TEXT("false"), HasMouseCapture() ? TEXT("true") : TEXT("false"),
	       HasKeyboardFocus() ? TEXT("true") : TEXT("false"));
	if (bPointerPressed)
	{
		bPointerPressed = false;
		Forward({EAffineUiInputKind::MouseUp, FVector2D(-1.0, -1.0)});
	}
	if (HasMouseCapture()) FSlateApplication::Get().ReleaseAllPointerCapture();
	if (bPreserveKeyboardFocus) return;
	if (HasKeyboardFocus()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
	else Forward({EAffineUiInputKind::FocusLost});
}

void SNativeWebView::Tick(const FGeometry& Geometry, double, float)
{
	if (!bVisible) return;
	if (NativeWebInputRouting::IsConsoleActive(Player.Get()))
	{
		if (bPointerPressed || HasKeyboardFocus()) ReleaseInput(true);
		return;
	}
	const FVector2D LocalSize = Geometry.GetLocalSize();
	if (LocalSize.X <= 0 || LocalSize.Y <= 0) return;
	const float Scale = Geometry.GetAccumulatedLayoutTransform().GetScale();
	const double Fit = FMath::Min(1.0, 4096.0 / FMath::Max(LocalSize.X * Scale, LocalSize.Y * Scale));


	const FIntPoint Pixels(FMath::Max(1, FMath::RoundToInt(LocalSize.X * Scale * Fit)),
	                       FMath::Max(1, FMath::RoundToInt(LocalSize.Y * Scale * Fit)));
	UE_LOG(LogNativeWebViewport, VeryVerbose,
	       TEXT("Slate attachment geometry local=%.1fx%.1f scale=%.3f output=%dx%d."),
	       LocalSize.X, LocalSize.Y, Scale, Pixels.X, Pixels.Y);
	if (UNativeWebView* Pinned = View.Get())
		Pinned->Resize(
			Pixels, FMath::Clamp(Scale * static_cast<float>(Fit), 0.01f, 8.0f));
}

void SNativeWebView::Forward(const FAffineUiInput& Input) const
{
	if (!bVisible) return;
	UNativeWebView* Pinned = View.Get();
	if (!Pinned) return;
	UE_LOG(LogNativeWebInput, VeryVerbose, TEXT("Slate forwarding input kind=%d position=(%.3f,%.3f)."),
	       static_cast<int32>(Input.Kind), Input.Position.X, Input.Position.Y);
	FNativeWebInputEvent Event;
	switch (Input.Kind)
	{
	case EAffineUiInputKind::MouseMove: Event.Type = ENativeWebInputType::INPUT_POINTER_MOVE;
		break;
	case EAffineUiInputKind::MouseDown: Event.Type = ENativeWebInputType::INPUT_POINTER_DOWN;
		break;
	case EAffineUiInputKind::MouseUp: Event.Type = ENativeWebInputType::INPUT_POINTER_UP;
		break;
	case EAffineUiInputKind::MouseWheel: Event.Type = ENativeWebInputType::INPUT_WHEEL;
		break;
	case EAffineUiInputKind::KeyDown: Event.Type = ENativeWebInputType::INPUT_KEY_DOWN;
		break;
	case EAffineUiInputKind::KeyUp: Event.Type = ENativeWebInputType::INPUT_KEY_UP;
		break;
	case EAffineUiInputKind::FocusGained: Event.Type = ENativeWebInputType::INPUT_FOCUS_GAINED;
		break;
	case EAffineUiInputKind::FocusLost: Event.Type = ENativeWebInputType::INPUT_FOCUS_LOST;
		break;
	}
	Event.NormalizedPosition = Input.Position;
	Event.WheelDelta = Input.WheelDelta;
	Event.bShift = Input.bShift;
	if (Input.Key == EAffineUiInputKey::Tab) Event.Key = ENativeWebInputKey::INPUT_KEY_TAB;
	else if (Input.Key == EAffineUiInputKey::Enter) Event.Key = ENativeWebInputKey::INPUT_KEY_ENTER;
	else if (Input.Key == EAffineUiInputKey::Space) Event.Key = ENativeWebInputKey::INPUT_KEY_SPACE;
	Pinned->ForwardInput(Event);
}

bool SNativeWebView::ForwardPointer(EAffineUiInputKind Kind, const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bVisible || NativeWebInputRouting::IsConsoleActive(Player.Get())) return false;
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0 || Size.Y <= 0) return false;
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	FAffineUiInput Input{Kind, FVector2D(Local.X / Size.X, Local.Y / Size.Y)};
	UE_LOG(LogNativeWebInput, VeryVerbose,
	       TEXT("Slate pointer kind=%d local=(%.1f,%.1f) normalized=(%.3f,%.3f)."), static_cast<int32>(Kind),
	       Local.X, Local.Y, Input.Position.X, Input.Position.Y);
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
	UE_LOG(LogNativeWebInput, Verbose, TEXT("Slate pointer press captured; requesting web view keyboard focus."));
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
	UE_LOG(LogNativeWebInput, Verbose, TEXT("Slate pointer released; releasing pointer capture."));
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
	if (!bPointerPressed && !NativeWebInputRouting::IsConsoleActive(Player.Get()))
		Forward({EAffineUiInputKind::MouseMove, FVector2D(-1.0, -1.0)});
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
	UE_LOG(LogNativeWebInput, Verbose, TEXT("Slate attachment received keyboard focus."));
	Forward({EAffineUiInputKind::FocusGained});
	return FReply::Handled();
}

void SNativeWebView::OnFocusLost(const FFocusEvent&)
{
	UE_LOG(LogNativeWebInput, Verbose, TEXT("Slate attachment lost keyboard focus."));
	Forward({EAffineUiInputKind::FocusLost});
}

FReply SNativeWebView::OnKeyChar(const FGeometry&, const FCharacterEvent& Event)
{
	if (!bVisible || !NativeWebInputRouting::IsConsoleActive(Player.Get())) return FReply::Unhandled();
	return NativeWebInputRouting::RouteCharacterToConsole(Player.Get(), Event)
		       ? FReply::Handled()
		       : FReply::Unhandled();
}

FReply SNativeWebView::ForwardKey(EAffineUiInputKind Kind, const FKeyEvent& Event)
{
	if (!bVisible) return FReply::Unhandled();
	if (NativeWebInputRouting::IsConsoleActive(Player.Get()))
		return NativeWebInputRouting::RouteKeyToConsole(Player.Get(), Event,
		                                                Kind == EAffineUiInputKind::KeyDown)
			       ? FReply::Handled()
			       : FReply::Unhandled();
	FAffineUiInput Input{Kind};
	if (Event.GetKey() == EKeys::Tab) Input.Key = EAffineUiInputKey::Tab;
	else if (Event.GetKey() == EKeys::Enter) Input.Key = EAffineUiInputKey::Enter;
	else if (Event.GetKey() == EKeys::SpaceBar) Input.Key = EAffineUiInputKey::Space;
	else return FReply::Unhandled();
	Input.bShift = Event.IsShiftDown();
	UE_LOG(LogNativeWebInput, VeryVerbose, TEXT("Slate forwarding key event kind=%d key=%s shift=%s."),
	       static_cast<int32>(Kind), *Event.GetKey().ToString(), Input.bShift ? TEXT("true") : TEXT("false"));
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
