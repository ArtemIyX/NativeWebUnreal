/**
 * @file Source/NativeWeb/Private/Widgets/NativeWebViewSlate.h
 * @brief Declares native web view slate for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "AffineUiSurface.h"
#include "Widgets/SCompoundWidget.h"

/**
 * @brief Declares the u native web view type and its operations.
 * @ingroup NativeWeb
 */
class UNativeWebView;
class ULocalPlayer;
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
		SLATE_ARGUMENT(ULocalPlayer*, Player)
		SLATE_ARGUMENT(FSurfacePtr, Surface)
	SLATE_END_ARGS()

	/**
	 * @brief Performs the construct operation.
	* @param Args Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Construct(Args);
	 * @endcode
	 */

	void Construct(const FArguments& Args);
	/**
	 * @brief Sets texture.
	* @param Texture Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * SetTexture(Texture);
	 * @endcode
	 */

	void SetTexture(UTextureRenderTarget2D* Texture);
	/**
	 * @brief Sets visible.
	* @param bVisible Boolean option controlling this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * SetVisible(bVisible);
	 * @endcode
	 */

	void SetVisible(bool bVisible);
	/**
	 * @brief Performs the release input operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * ReleaseInput();
	 * @endcode
	 */

	void ReleaseInput(bool bPreserveKeyboardFocus = false);
	/**
	 * @brief Performs the supports keyboard focus operation.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * SupportsKeyboardFocus();
	 * @endcode
	 */

	virtual bool SupportsKeyboardFocus() const override { return true; }
	/**
	 * @brief Advances bounded work owned by this object.
	* @param Geometry Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param CurrentTime Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param DeltaTime Elapsed time supplied by the owning engine tick.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Tick(Geometry, CurrentTime, DeltaTime);
	 * @endcode
	 */

	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
	/**
	 * @brief Processes the mouse button down.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnMouseButtonDown(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	/**
	 * @brief Processes the mouse button double click.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnMouseButtonDoubleClick(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override;
	/**
	 * @brief Processes the mouse button up.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnMouseButtonUp(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	/**
	 * @brief Processes the mouse move.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnMouseMove(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
	/**
	 * @brief Processes the mouse wheel.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnMouseWheel(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	/**
	 * @brief Processes the mouse leave.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnMouseLeave(Parameter);
	 * @endcode
	 */

	virtual void OnMouseLeave(const FPointerEvent&) override;
	/**
	 * @brief Processes the mouse capture lost.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnMouseCaptureLost(Parameter);
	 * @endcode
	 */

	virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
	/**
	 * @brief Processes the focus received.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnFocusReceived(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override;
	/**
	 * @brief Processes the focus lost.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnFocusLost(Parameter);
	 * @endcode
	 */

	virtual void OnFocusLost(const FFocusEvent&) override;
	/**
	 * @brief Processes the key down.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnKeyDown(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
	/**
	 * @brief Processes the key up.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnKeyUp(Parameter, Parameter);
	 * @endcode
	 */

	virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override;
	/** Processes a character event for the active owning viewport console. */
	virtual FReply OnKeyChar(const FGeometry&, const FCharacterEvent&) override;

private:
	/**
	 * @brief Performs the forward pointer operation.
	* @param Kind Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * ForwardPointer(Kind, Parameter, Parameter);
	 * @endcode
	 */

	bool ForwardPointer(EAffineUiInputKind Kind, const FGeometry&, const FPointerEvent&);
	/**
	 * @brief Performs the forward key operation.
	* @param Kind Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * ForwardKey(Kind, Parameter);
	 * @endcode
	 */

	FReply ForwardKey(EAffineUiInputKind Kind, const FKeyEvent&);
	/**
	 * @brief Performs the forward operation.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Forward(Parameter);
	 * @endcode
	 */

	void Forward(const FAffineUiInput&) const;
	TWeakObjectPtr<UNativeWebView> View;
	TWeakObjectPtr<ULocalPlayer> Player;
	TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TStrongObjectPtr<UTextureRenderTarget2D> Target;
	TSharedPtr<SImage> Image;
	FSlateBrush Brush;
	bool bVisible = true;
	bool bPointerPressed = false;
};
