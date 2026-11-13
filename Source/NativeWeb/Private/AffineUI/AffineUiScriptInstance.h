/**
 * @file Source/NativeWeb/Private/AffineUI/AffineUiScriptInstance.h
 * @brief Declares affine ui script instance for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

#pragma once

#include "CoreMinimal.h"
#include "AffineUiSurface.h"
#include "NativeWebView.h"
#include "HAL/CriticalSection.h"
#if PLATFORM_WINDOWS
#include "QuickJSRuntime.h"

/** One game-thread QuickJS host paired with one NativeWeb AffineUI surface. */
class NATIVEWEB_API FAffineUiScriptInstance final
{
public:
	/**
	 * @brief Performs the create operation.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Desc Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param OutError Output parameter populated by this call, when applicable.
	 * @return An owned result; it is invalid when the operation cannot produce a value.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	Create(Desc, OutError);
	@endcode
	 */

	static TUniquePtr<FAffineUiScriptInstance> Create(const FAffineUiSurfaceDesc& Desc, FString& OutError);
	~FAffineUiScriptInstance();
	FAffineUiScriptInstance(const FAffineUiScriptInstance&) = delete;
	/**
	 * @brief Performs the operator= operation.
	* @param Parameter Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The value produced by this operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * operator=(Parameter);
	 * @endcode
	 */

	FAffineUiScriptInstance& operator=(const FAffineUiScriptInstance&) = delete;

	/**
	 * @brief Performs the evaluate operation.
	* @param Source Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutError Output value populated by this operation when applicable.
	* @param SourceName Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Evaluate(Source, OutError, SourceName);
	 * @endcode
	 */

	bool Evaluate(const FString& Source, FQuickJSError& OutError,
	              const FString& SourceName = TEXT("<nativeweb-script>"));
	/**
	 * @brief Dispatches click.
	* @param NodeId Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DispatchClick(NodeId, OutError);
	 * @endcode
	 */

	bool DispatchClick(uint64 NodeId, FQuickJSError& OutError);
	/**
	 * @brief Pumps jobs.
	* @param MaxJobs Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutExecuted Output value populated by this operation when applicable.
	 * @param bOutPending Output value populated by this operation when applicable.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * PumpJobs(MaxJobs, OutExecuted, bOutPending, OutError);
	 * @endcode
	 */

	bool PumpJobs(int32 MaxJobs, int32& OutExecuted, bool& bOutPending, FQuickJSError& OutError);
	/**
	 * @brief Queues to java script.
	* @param Message Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return An enumerated status describing the outcome.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * QueueToJavaScript(Message);
	 * @endcode
	 */

	ENativeWebQueueResult QueueToJavaScript(const FNativeWebMessage& Message);
	/**
	 * @brief Performs the drain messages from java script operation.
	 * @param OutMessages Output value populated by this operation when applicable.
	* @param MaxMessages Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The resulting count, size, index, hash, or numeric value.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DrainMessagesFromJavaScript(OutMessages, MaxMessages);
	 * @endcode
	 */

	int32 DrainMessagesFromJavaScript(TArray<FNativeWebMessage>& OutMessages, int32 MaxMessages);
	/**
	 * @brief Performs the drain messages to java script operation.
	 * @param OutMessages Output value populated by this operation when applicable.
	* @param MaxMessages Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return The resulting count, size, index, hash, or numeric value.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DrainMessagesToJavaScript(OutMessages, MaxMessages);
	 * @endcode
	 */

	int32 DrainMessagesToJavaScript(TArray<FNativeWebMessage>& OutMessages, int32 MaxMessages);
	/**
	 * @brief Performs the deliver message to java script operation.
	* @param Message Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @param OutError Output value populated by this operation when applicable.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * DeliverMessageToJavaScript(Message, OutError);
	 * @endcode
	 */

	bool DeliverMessageToJavaScript(const FNativeWebMessage& Message, FQuickJSError& OutError);
	/**
	 * @brief Performs the refresh document epoch operation.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * RefreshDocumentEpoch();
	 * @endcode
	 */

	void RefreshDocumentEpoch();
	/**
	 * @brief Returns surface.
	 * @return A shared result; it is invalid when the resource is unavailable.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * GetSurface();
	 * @endcode
	 */

	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> GetSurface() const { return Surface; }
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
	FAffineUiScriptInstance() = default;
	/**
	 * @brief Sets text.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	SetText(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue SetText(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Performs the mount root operation.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	MountRoot(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue MountRoot(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Creates element.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	CreateElement(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue CreateElement(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Creates text.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	CreateText(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue CreateText(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Creates comment.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	CreateComment(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue CreateComment(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Performs the insert before operation.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	InsertBefore(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue InsertBefore(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Removes node.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	RemoveNode(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue RemoveNode(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Sets node text.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	SetNodeText(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue SetNodeText(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Sets attribute.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	SetAttribute(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue SetAttribute(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Sets click handler.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	SetClickHandler(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue SetClickHandler(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Removes attribute.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	RemoveAttribute(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue RemoveAttribute(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Performs the post string operation.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	PostString(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue PostString(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Performs the post bytes operation.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ThisValue Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	PostBytes(Context, ThisValue, ArgCount, Args);
	@endcode
	 */

	static JSValue PostBytes(JSContext* Context, JSValueConst ThisValue, int32 ArgCount, JSValueConst* Args);
	/**
	 * @brief Creates node.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Kind Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	CreateNode(Context, ArgCount, Args, Kind);
	@endcode
	 */

	static JSValue CreateNode(JSContext* Context, int32 ArgCount, JSValueConst* Args, EAffineUiMutationKind Kind);
	/**
	 * @brief Queues command.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param ArgCount Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Args Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Kind Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @return The value produced by this operation.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	QueueCommand(Context, ArgCount, Args, Kind);
	@endcode
	 */

	static JSValue QueueCommand(JSContext* Context, int32 ArgCount, JSValueConst* Args, EAffineUiMutationKind Kind);
	/**
	 * @brief Performs the allocate node id operation.
	 * @return The resulting count, size, index, hash, or numeric value.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * AllocateNodeId();
	 * @endcode
	 */

	uint64 AllocateNodeId();
	/**
	 * @brief Performs the queue operation.
	* @param Mutation Input supplied by the caller; references are consumed only for the duration of this call unless the API states otherwise.
	 * @return True when the operation succeeds or accepts the request; false when it fails or rejects it.
	 * @note QuickJS/view APIs are game-thread owned; AffineUI D3D11 work runs on the RHI lane.
	 * @par Example
	 * Invocation example; substitute arguments that satisfy the function contract.
	 * @code{.cpp}
	 * Queue(Mutation);
	 * @endcode
	 */

	bool Queue(const FAffineUiMutation& Mutation);
	/**
	 * @brief Reads string.
	 * @details This entry point is part of the owning type/module contract. It uses the declared input and output parameters; consult the owning type documentation for lifecycle, ownership, threading, and failure context.
	 * @param Context Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param Value Value used by this operation; see the type contract for ownership and lifetime requirements.
	 * @param bOutValid Output parameter populated by this call, when applicable.
	 * @return The resulting text or identifier.
	 * @note The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
	 * @par Example
	 * The following call shape illustrates the invocation context; supply valid values and an owner appropriate to the documented lifecycle.
	 * @code{.cpp}
	ReadString(Context, Value, bOutValid);
	@endcode
	 */

	static FString ReadString(JSContext* Context, JSValueConst Value, bool& bOutValid);
	TUniquePtr<FQuickJSRuntime> Runtime;
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
	FCriticalSection MessageMutex;
	TArray<FNativeWebMessage> InboundMessages;
	TArray<FNativeWebMessage> OutboundMessages;
	int64 InboundBytes = 0;
	int64 OutboundBytes = 0;
	bool bClosing = false;
	uint64 DocumentEpoch = 0;
	uint64 NextNodeId = 1;
};
#endif // PLATFORM_WINDOWS
