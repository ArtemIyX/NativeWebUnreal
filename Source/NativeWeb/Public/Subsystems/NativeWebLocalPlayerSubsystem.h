/**
 * @file Source/NativeWeb/Public/Subsystems/NativeWebLocalPlayerSubsystem.h
 * @brief Declares native web local player subsystem for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "NativeWebView.h"
#include "Data/Structs/NativeWebPresentationConfig.h"
#include "Data/Structs/NativeWebSurfaceConfig.h"
#include "NativeWebLocalPlayerSubsystem.generated.h"

/**
 * @brief Local-player owner and factory for reusable NativeWeb views.
 * @details Tracks named view UObjects, advances their bounded message/QuickJS work, and enforces the current one-live-AffineUI-surface process limit.
 * @ingroup NativeWeb
 */
UCLASS()
class NATIVEWEB_API UNativeWebLocalPlayerSubsystem final : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes local-player view ownership and registers the subsystem tick.
	 * @param Collection Engine subsystem collection used during initialization.
	 * @note Game-thread lifecycle callback. Tick removal is paired in @ref Deinitialize.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Initialize(Collection);
	 * @endcode
	 */

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/**
	 * @brief Closes all owned views and unregisters the subsystem tick.
	 * @details Each view performs its normal idempotent teardown before the subsystem releases its references.
	 * @note Game-thread lifecycle callback; no QuickJS context is allowed to outlive subsystem teardown.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Deinitialize();
	 * @endcode
	 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
	 */

	virtual void Deinitialize() override;

public:
	/**
	 * @brief Creates and registers a view from a packaged project Content directory.
	 * @param Name Unique view name within this local-player subsystem; duplicate live names are rejected.
	 * @param ContentRelativeDirectory Project-Content-relative package directory containing ui.css and ui.js.
	 * @param SurfaceConfig Initial AffineUI surface dimensions, DPI, and clear color.
	 * @param PresentationConfig Built-in presenter, fallback, and Slate z-order settings.
	 * @param OutError Receives a structured failure reason, including platform or occupied surface capacity.
	 * @return The initialized view, or nullptr if validation, file loading, runtime creation, or capacity checks fail.
	 * @details Generates a small HTML shell containing #app; the package does not need index.html. Optional fonts may be placed in a fonts subdirectory.
	 * @note Game-thread only. Packaged file resolution occurs before render-lane work.
	 * @see CreateInlineView, FNativeWebSurfaceConfig, FNativeWebPresentationConfig
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * CreatePackagedView(Name, ContentRelativeDirectory, SurfaceConfig, PresentationConfig, OutError);
	 * @endcode
	 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	UNativeWebView* CreatePackagedView(
		FName Name,
		const FString& ContentRelativeDirectory,
		const FNativeWebSurfaceConfig& SurfaceConfig,
		const FNativeWebPresentationConfig& PresentationConfig,
		FNativeWebError& OutError);

	/**
	 * @brief Creates and registers a view from caller-provided inline HTML and CSS.
	 * @param Name Unique view name within this local-player subsystem.
	 * @param Html HTML source for the document.
	 * @param Css CSS source paired with the document.
	 * @param SurfaceConfig Initial AffineUI surface dimensions, DPI, and clear color.
	 * @param PresentationConfig Built-in presenter, fallback, and Slate z-order settings.
	 * @param OutError Receives a structured failure reason.
	 * @return The initialized view, or nullptr if validation, runtime creation, or surface capacity checks fail.
	 * @details Inline startup JavaScript remains explicit through UNativeWebView::EvaluateScript.
	 * @note Game-thread only. The subsystem owns and ticks the returned UObject.
	 */
	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	UNativeWebView* CreateInlineView(
		FName Name,
		const FString& Html,
		const FString& Css,
		const FNativeWebSurfaceConfig& SurfaceConfig,
		const FNativeWebPresentationConfig& PresentationConfig,
		FNativeWebError& OutError);

	/**
	 * @brief Looks up an open view by its subsystem-local name.
	 * @param Name Name supplied when the view was created.
	 * @return The registered view, or nullptr if no open view has that name.
	 * @note Game-thread only. Closed views are not returned.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * FindView(Name);
	 * @endcode
	 */

	UFUNCTION(BlueprintPure, Category="NativeWeb")
	UNativeWebView* FindView(FName Name) const;

	/**
	 * @brief Closes and unregisters the named view.
	 * @param Name Name supplied when the view was created.
	 * @return True if a registered view was found and closed; false if the name is unknown.
	 * @details Closing invalidates pending callbacks/messages and releases the process-wide surface capacity.
	 * @note Game-thread only. @ref UNativeWebView::Close is also idempotent.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DestroyView(Name);
	 * @endcode
	 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
	 */

	UFUNCTION(BlueprintCallable, Category="NativeWeb")
	bool DestroyView(FName Name);

private:
	UNativeWebView* CreateConfiguredView(FName Name, const FString& Html, const FString& Css,
	                                     const FString& PackagedAssetRoot,
	                                     const FNativeWebSurfaceConfig& SurfaceConfig,
	                                     const FNativeWebPresentationConfig& PresentationConfig,
	                                     FNativeWebError& OutError);

	UPROPERTY(Transient)
	/** @brief Reflected data field `Views`. */
	TMap<FName, TObjectPtr<UNativeWebView>> Views;
	FTSTicker::FDelegateHandle TickHandle;
	bool Tick(float DeltaTime);
};
