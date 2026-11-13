/**
 * @file Source/NativeWeb/Private/AffineUI/AffineUiSurface.h
 * @brief Declares affine ui surface for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

/**
 * @brief Declares the u texture render target2 d type and its operations.
 * @ingroup NativeWeb
 */
class UTextureRenderTarget2D;
class FAffineUiSurfaceRenderer;

/** An owned HTML/CSS snapshot. Relative asset URLs are not resolved by this surface. */
struct NATIVEWEB_API FAffineUiContent
{
	FString Html;
	/**
	 * @brief Defines the nativeweb api data structure.
	 * @ingroup NativeWeb
	 */
	FString Css;
};

/**
 * @brief Defines the e affine ui mutation kind enumeration.
 * @ingroup NativeWeb
 */

/** Copied text update applied on the AffineUI renderer's owner lane. */
struct NATIVEWEB_API FAffineUiTextMutation
	/**
	 * @brief Defines the nativeweb api data structure.
	 * @ingroup NativeWeb
	 */
{
	FString ElementId;
	FString Text;
};

enum class EAffineUiMutationKind : uint8
{
	MountRoot, CreateElement, CreateSvgElement, CreateText, CreateComment, InsertBefore, Remove,
	SetText, SetAttribute, RemoveAttribute, RegisterClickHandler
};

/** Completion status for a submitted AffineUI frame. */
enum class EAffineUiRenderResult : uint8
{
	Success,
	Deferred,
	Failure,
	Pending
};

/** Copied game-thread command; strings and logical IDs cross to the RHI lane. */
struct NATIVEWEB_API FAffineUiMutation
{
	uint64 DocumentEpoch = 0;
	uint64 NodeId = 0;
	uint64 ParentId = 0;
	uint64 BeforeId = 0;
	EAffineUiMutationKind Kind = EAffineUiMutationKind::MountRoot;
	FString Name;
	FString Value;
};

/** Keeps an unconsumed draw batch ahead of newer mutations when a draw is deferred. */
struct NATIVEWEB_API FAffineUiDeferredMutationBatch
{
	void Begin(TArray<FAffineUiMutation>&& Mutations, uint64 InDocumentEpoch)
	{
		InFlight = MoveTemp(Mutations);
		DocumentEpoch = InDocumentEpoch;
	}

	void RestoreIfDeferred(uint64 CurrentDocumentEpoch, TArray<FAffineUiMutation>& Pending)
	{
		if (DocumentEpoch == CurrentDocumentEpoch && !InFlight.IsEmpty())
		{
			InFlight.Append(MoveTemp(Pending));
			Pending = MoveTemp(InFlight);
		}
		InFlight.Reset();
	}

	void MarkConsumed()
	{
		InFlight.Reset();
	}

	const TArray<FAffineUiMutation>& GetInFlight() const { return InFlight; }

private:
	TArray<FAffineUiMutation> InFlight;
	uint64 DocumentEpoch = 0;
};

enum class EAffineUiInputKind : uint8
	/**
	 * @brief Defines the nativeweb api data structure.
	 * @ingroup NativeWeb
	 */
{
	MouseMove, MouseDown, MouseUp, MouseWheel, KeyDown, KeyUp, FocusGained, FocusLost
};

enum class EAffineUiInputKey : uint8 { None, Tab, Enter, Space };

/** Input relative to the published texture, independent of Slate and CSS scale. */
struct FAffineUiInput
{
	EAffineUiInputKind Kind;
	FVector2D Position = FVector2D::ZeroVector;
	EAffineUiInputKey Key = EAffineUiInputKey::None;
	float WheelDelta = 0.0f;
	bool bShift = false;
};

struct NATIVEWEB_API FAffineUiSurfaceDesc
{
	FIntPoint PixelSize = FIntPoint(480, 240);
	float DpiScale = 1.0f;
	FColor ClearColor = FColor(0, 0, 0, 255);
	FAffineUiContent Content;
	FString FontDirectory;
};

DECLARE_DELEGATE_OneParam(FAffineUiTextureReady, UTextureRenderTarget2D*);
DECLARE_DELEGATE_OneParam(FAffineUiSurfaceError, const FString&);
DECLARE_DELEGATE_OneParam(FAffineUiClick, uint64);

/** One D3D11 surface. All public methods, including destruction, run on the game thread. */
class NATIVEWEB_API FAffineUiSurface : public TSharedFromThis<FAffineUiSurface, ESPMode::ThreadSafe>
{
public:
	/** Returns null and fills OutError when the RHI or descriptor is unsupported. */
	static TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe>
	Create(const FAffineUiSurfaceDesc& Desc, FString& OutError);
	~FAffineUiSurface();

	/** Queue an atomic HTML/CSS replacement. Success means accepted, not rendered.
* @param Content Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetContent(Content);
 * @endcode
 */
	bool SetContent(const FAffineUiContent& Content);
	/** Read both files through Unreal; on either failure the current content is retained.
* @param HtmlPath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param CssPath Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @param OutError Output value populated by this operation when applicable.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * LoadContentFromFiles(HtmlPath, CssPath, OutError);
 * @endcode
 */
	bool LoadContentFromFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError);
	/** Queue a new target; the previous output remains published until the replacement is ready.
* @param PixelSize Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param DpiScale Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * Resize(PixelSize, DpiScale);
 * @endcode
 */
	bool Resize(FIntPoint PixelSize, float DpiScale = 1.0f);
	/**
	 * @brief Performs the request redraw operation.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * RequestRedraw();
	 * @endcode
	 */

	bool RequestRedraw();
	/** Queue textContent replacement for an existing element ID without replacing HTML. True means queued, not yet rendered.
* @param ElementId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
* @param Text Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetTextById(ElementId, Text);
 * @endcode
 */
	bool SetTextById(const FString& ElementId, const FString& Text);
	/**
	 * @brief Queues mutation.
	* @param Mutation Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * QueueMutation(Mutation);
	 * @endcode
	 */

	bool QueueMutation(const FAffineUiMutation& Mutation);
	/**
	 * @brief Returns document epoch.
	 * @return The resulting count, size, index, hash, or numeric value.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetDocumentEpoch();
	 * @endcode
	 */

	uint64 GetDocumentEpoch() const;
	/**
	 * @brief Queues input.
	* @param Input Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * QueueInput(Input);
	 * @endcode
	 */

	bool QueueInput(const FAffineUiInput& Input);
	/**
	 * @brief Sets on click.
	 * @param Callback Caller-supplied callback invoked according to the owning type contract.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * SetOnClick(Callback);
	 * @endcode
	 */

	void SetOnClick(FAffineUiClick Callback);

	/** The callback runs on the game thread after each successful texture publication.
 * @param Callback Caller-supplied callback invoked according to the owning type contract.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetOnTextureReady(Callback);
 * @endcode
 */
	void SetOnTextureReady(FAffineUiTextureReady Callback);
	/** Reports asynchronous allocation and submission failures on the game thread.
 * @param Callback Caller-supplied callback invoked according to the owning type contract.
 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
 * @par Example
 * Invocation example; substitute arguments that satisfy the function contract.
 * @code{.cpp}
 * SetOnError(Callback);
 * @endcode
 */
	void SetOnError(FAffineUiSurfaceError Callback);
	/**
	 * @brief Returns output texture.
	 * @return The requested object/result, or null when unavailable.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetOutputTexture();
	 * @endcode
	 */

	UTextureRenderTarget2D* GetOutputTexture() const;
	/**
	 * @brief Returns last error.
	 * @return The resulting text or identifier.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetLastError();
	 * @endcode
	 */

	FString GetLastError() const;
	/**
	 * @brief Closes this resource and releases its owned state.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Close();
	 * @endcode
	 * @warning This call releases or replaces owned state; ensure dependent work has stopped before teardown.
	 */

	void Close();

private:
	friend class FAffineUiSurfaceRenderer;
	FAffineUiSurface();
	/**
	 * @brief Performs the start pending operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * StartPending();
	 * @endcode
	 */

	void StartPending();
	/**
	 * @brief Performs the request input redraw operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * RequestInputRedraw();
	 * @endcode
	 */

	void RequestInputRedraw();
	/**
	 * @brief Processes the render complete.
	* @param Revision Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param bSuccess Boolean option controlling this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnRenderComplete(Revision, Result);
	 * @endcode
	 */

	void OnRenderComplete(uint64 Revision, EAffineUiRenderResult Result);
	/**
	 * @brief Processes the input complete.
	* @param DocumentEpoch Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param Clicks Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	* @param bNeedsRedraw Boolean option controlling this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * OnInputComplete(DocumentEpoch, Clicks, bNeedsRedraw);
	 * @endcode
	 */

	void OnInputComplete(uint64 DocumentEpoch, TArray<uint64> Clicks, bool bNeedsRedraw);
	class FImpl;
	TUniquePtr<FImpl> Impl;
};
