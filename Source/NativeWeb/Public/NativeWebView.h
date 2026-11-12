/**
 * @file Source/NativeWeb/Public/NativeWebView.h
 * @brief Declares native web view for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"

#include "HAL/CriticalSection.h"
#include "UObject/Object.h"
#include "Data/Enums/NativeWebQueueResult.h"
#include "Data/Structs/NativeWebError.h"
#include "Data/Structs/NativeWebInputEvent.h"
#include "Data/Structs/NativeWebMessage.h"
#include "Data/Structs/NativeWebViewOptions.h"
#include "NativeWebDelegates.h"
#include "NativeWebView.generated.h"

class FAffineUiScriptInstance;
/**
 * @brief Slate presentation widget that displays a NativeWeb view and forwards viewport input.
 * @details Private implementation type; consumers should use UNativeWebView and its viewport attachment methods.
 * @ingroup NativeWeb
 */
class SNativeWebView;
class UNativeWebLocalPlayerSubsystem;

UCLASS(BlueprintType)
/**
 * @brief Reusable local-player view backed by the plugin's AffineUI surface.
 * @details Owns its QuickJS script instance, message queues, output texture, and optional Slate viewport attachment. The object is created and managed by UNativeWebLocalPlayerSubsystem.
 * @ingroup NativeWeb
 */
class NATIVEWEB_API UNativeWebView final : public UObject
{
	GENERATED_BODY()

public:
/**
 * @brief Performs UObject destruction-time cleanup.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * BeginDestroy();
 * @endcode
 */

	virtual void BeginDestroy() override;
/**
 * @brief Shows the view and attaches its Slate widget to the owning player viewport when needed.
 * @return True if the view is open and can be shown; false if it is closed or attachment fails.
 * @details Safe to call repeatedly. Visibility does not change whether the view owns a live AffineUI surface.
 * @note Game-thread only. Call @ref AttachToPlayerViewport explicitly when custom z-order is required.
 * @par Example
 * Show the view after creation.
 * @code{.cpp}
 * Show();
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool Show();
/**
 * @brief Hides the viewport widget and releases any pointer capture owned by this view.
 * @details The view and render surface remain alive; call @ref Close to release runtime resources. PlayerController input settings are not modified.
 * @note Game-thread only. The same view may be shown again later.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Hide();
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	void Hide();
/**
 * @brief Reports whether this view is currently visible in its Slate attachment.
 * @return True if shown; false if hidden or not attached.
 * @note Game-thread only. This does not report whether the underlying AffineUI surface is initialized.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * IsVisible();
 * @endcode
 */

	UFUNCTION(BlueprintPure, Category="NativeWeb")
	bool IsVisible() const { return bVisible; }

/**
 * @brief Resizes the render surface and attached viewport widget.
 * @param PixelSize Requested surface dimensions in pixels; both axes must be positive.
 * @param DpiScale Slate presentation scale for the view; defaults to 1.
 * @return True if the resize request is valid and accepted; otherwise false.
 * @note Game-thread API; rendering resource changes are handed to the RHI lane. Invalid dimensions are rejected.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Resize(PixelSize, DpiScale);
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool Resize(FIntPoint PixelSize, float DpiScale = 1.0f);

/**
 * @brief Adds the reusable Slate view widget to its owning local player's viewport.
 * @param ZOrder Slate viewport z-order; higher values are drawn above lower values.
 * @return True if attached or already attached; false if the owner, viewport, or widget is unavailable.
 * @details Repeated calls retain one attachment rather than adding duplicate widgets.
 * @note Game-thread only. Use @ref DetachFromPlayerViewport to remove the widget without closing the view.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * AttachToPlayerViewport(ZOrder);
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool AttachToPlayerViewport(int32 ZOrder = 0);

/**
 * @brief Removes the Slate widget from the viewport and releases view-owned focus and pointer capture.
 * @details The render surface and view remain alive. The widget can be attached again with @ref AttachToPlayerViewport.
 * @note Game-thread only; this does not modify PlayerController input mode or mapping settings.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * DetachFromPlayerViewport();
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	void DetachFromPlayerViewport();

/**
 * @brief Returns the render target used to display this view.
 * @return The view's output texture, or nullptr after close or before initialization.
 * @note The texture is owned by the view runtime. Do not destroy it or access its RHI resource from arbitrary threads.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * GetTexture();
 * @endcode
 */

	UFUNCTION(BlueprintPure, Category="NativeWeb")
	UTextureRenderTarget2D* GetTexture() const;

/**
 * @brief Forwards one pointer, keyboard, wheel, or focus event to the embedded surface.
 * @param Event Event type and coordinates/key data; pointer coordinates use the view's pixel space.
 * @return True if the view is open and accepted the event; false if closed or unsupported.
 * @details Input may also be forwarded by the attached Slate widget. Custom hosts can use this method directly.
 * @note Game-thread only. The API does not change PlayerController input mode or capture settings.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * ForwardInput(Event);
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool ForwardInput(const FNativeWebInputEvent& Event);

/**
 * @brief Replaces the current document HTML and stylesheet source.
 * @param Html HTML source to parse as the document.
 * @param Css CSS source associated with that document.
 * @param OutError Receives a structured error when document creation fails.
 * @return True if the document was accepted; false for a closed view or surface/runtime error.
 * @details Scripts are supplied separately through @ref EvaluateScript; module loading is not supported.
 * @note Game-thread only; surface work is dispatched to AffineUI's RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetDocument(Html, Css, OutError);
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool SetDocument(const FString& Html, const FString& Css, FNativeWebError& OutError);

/**
 * @brief Evaluates JavaScript source in this view's QuickJS context.
 * @param Source JavaScript source text.
 * @param SourceName Diagnostic name attached to syntax/runtime errors.
 * @param OutError Receives QuickJS error details when evaluation fails.
 * @return True if evaluation completes without a JavaScript exception; false if closed or evaluation fails.
 * @note Game-thread only. This executes ordinary script text; ES module loading is not supported.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EvaluateScript(Source, SourceName, OutError);
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool EvaluateScript(const FString& Source, const FString& SourceName, FNativeWebError& OutError);

/**
 * @brief Loads packaged document.
* @param HtmlRelativePath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param CssRelativePath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * LoadPackagedDocument(HtmlRelativePath, CssRelativePath, OutError);
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool LoadPackagedDocument(const FString& HtmlRelativePath, const FString& CssRelativePath,
	                          FNativeWebError& OutError);
/**
 * @brief Evaluates packaged script.
* @param ScriptRelativePath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EvaluatePackagedScript(ScriptRelativePath, OutError);
 * @endcode
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool EvaluatePackagedScript(const FString& ScriptRelativePath, FNativeWebError& OutError);

/**
 * @brief Copies a string message into the bounded Unreal-to-JavaScript queue.
 * @param Channel Optional channel label; NAME_None is allowed.
 * @param Value String payload copied into queue-owned storage.
 * @return @ref ENativeWebQueueResult indicating queued, closed, or full.
 * @details Delivery occurs on a later controlled game-thread tick and never synchronously enters JavaScript.
 * @note Queue capacity applies; a full queue returns QueueFull without accepting the payload.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EnqueueStringToJavaScript(Channel, Value);
 * @endcode
 */

	ENativeWebQueueResult EnqueueStringToJavaScript(FName Channel, FString Value);
/**
 * @brief Moves an owned byte payload into the bounded Unreal-to-JavaScript queue.
 * @param Channel Optional channel label; NAME_None is allowed.
 * @param Value Byte array moved into queue-owned storage.
 * @return @ref ENativeWebQueueResult indicating queued, closed, or full.
 * @details JavaScript receives a copied byte value on a later controlled game-thread tick.
 * @note No borrowed/caller-owned byte pointer is retained. Queue capacity applies.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EnqueueBytesToJavaScript(Channel, Value);
 * @endcode
 */

	ENativeWebQueueResult EnqueueBytesToJavaScript(FName Channel, TArray<uint8>&& Value);
/**
 * @brief Copies a string payload into the Unreal-to-JavaScript message queue.
 * @param Channel Optional message channel.
 * @param Value String view copied before this function returns.
 * @return True if accepted; false if the view is closed or the queue is full.
 * @details Reports queue acceptance only; it does not guarantee JavaScript listener delivery.
 * @note Delivery occurs asynchronously during the subsystem/view tick.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendStringToJavaScript(Channel, Value);
 * @endcode
 */

	bool SendStringToJavaScript(FName Channel, FStringView Value);
/**
 * @brief Copies a byte payload into the Unreal-to-JavaScript message queue.
 * @param Channel Optional message channel.
 * @param Value Byte span copied before this function returns; caller storage may be reused afterward.
 * @return True if accepted; false if the view is closed or the queue is full.
 * @details Reports queue acceptance only; it does not guarantee JavaScript listener delivery.
 * @note Delivery occurs asynchronously during the subsystem/view tick.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SendBytesToJavaScript(Channel, Value);
 * @endcode
 */

	bool SendBytesToJavaScript(FName Channel, TConstArrayView<uint8> Value);
/**
 * @brief Gets the multicast delegate for messages received from JavaScript.
 * @return Mutable delegate reference; bind handlers on the game thread.
 * @details Messages are delivered after the QuickJS host callback returns, so handlers are not called reentrantly from JavaScript.
 * @note Unbind delegates during owner teardown. Pending deliveries are discarded once the view closes.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * OnMessage();
 * @endcode
 */

	FNativeWebMessageReceived& OnMessage() { return MessageReceived; }
/**
 * @brief Gets the multicast delegate for view creation, script, queue, and runtime errors.
 * @return Mutable delegate reference; bind handlers on the game thread.
 * @details Error payloads include code, message, source, script stack, and view name where available.
 * @note Unbind delegates during owner teardown.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * OnError();
 * @endcode
 */

	FNativeWebViewError& OnError() { return Error; }
	
/**
 * @brief Idempotently closes the view and releases its runtime and Slate resources.
 * @details Stops message delivery, detaches the viewport widget, releases view-owned focus/capture, invalidates callbacks, and tears down QuickJS and AffineUI state on their owning lanes.
 * @note Game-thread only. Do not use the view for new operations after close; repeated calls are safe.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Close();
 * @endcode
 * @warning Outstanding references to the UObject may remain, but its runtime is no longer usable.
 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	void Close();
/**
 * @brief Reports whether @ref Close has been called or teardown has begun.
 * @return True once closed; false while the view accepts operations.
 * @note Game-thread only.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * IsClosed();
 * @endcode
 */

	UFUNCTION(BlueprintPure, Category="NativeWeb")
	bool IsClosed() const { return bClosed; }

private:
	friend class UNativeWebLocalPlayerSubsystem;
	friend class SNativeWebView;
/**
 * @brief Initializes the owning subsystem or runtime object.
* @param InName Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Options Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Player Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Initialize(InName, Options, Player, OutError);
 * @endcode
 */

	bool Initialize(FName InName, const FNativeWebViewOptions& Options, ULocalPlayer* Player,
	                FNativeWebError& OutError);
/**
 * @brief Advances bounded work owned by this object.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Tick();
 * @endcode
 */

	void Tick();
/**
 * @brief Sets output texture.
* @param Texture Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetOutputTexture(Texture);
 * @endcode
 */

	void SetOutputTexture(UTextureRenderTarget2D* Texture);
/**
 * @brief Loads document files.
* @param HtmlPath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param CssPath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * LoadDocumentFiles(HtmlPath, CssPath, OutError);
 * @endcode
 */

	bool LoadDocumentFiles(const FString& HtmlPath, const FString& CssPath, FNativeWebError& OutError);
/**
 * @brief Evaluates script file.
* @param ScriptPath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * EvaluateScriptFile(ScriptPath, OutError);
 * @endcode
 */

	bool EvaluateScriptFile(const FString& ScriptPath, FNativeWebError& OutError);
	FName ViewName = NAME_None;
	FString PackagedAssetRoot;
#if PLATFORM_WINDOWS
	TUniquePtr<FAffineUiScriptInstance> Script;
#endif
	TSharedPtr<class FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	TSharedPtr<SNativeWebView> Attachment;
	TWeakObjectPtr<ULocalPlayer> OwningPlayer;
	FNativeWebMessageReceived MessageReceived;
	FNativeWebViewError Error;
	FCriticalSection ProducerMutex;
	TArray<FNativeWebMessage> ProducerMessages;
	int64 ProducerBytes = 0;
	bool bVisible = false;
	bool bClosed = false;
	bool bInitialized = false;
	uint64 Generation = 1;
};
