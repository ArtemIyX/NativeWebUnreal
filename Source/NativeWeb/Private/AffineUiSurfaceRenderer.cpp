#include "AffineUiSurfaceRenderer.h"

#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderCommandFence.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogAffineUiSurfaceRender, Log, All);

#if PLATFORM_WINDOWS
#include "ID3D11DynamicRHI.h"
#pragma push_macro("check")
#pragma push_macro("small")
#pragma push_macro("FW_THIN")
#pragma push_macro("FW_LIGHT")
#pragma push_macro("FW_NORMAL")
#pragma push_macro("FW_MEDIUM")
#pragma push_macro("FW_SEMI_BOLD")
#pragma push_macro("FW_BOLD")
#pragma push_macro("FW_BLACK")
#undef check
#undef small
#undef FW_THIN
#undef FW_LIGHT
#undef FW_NORMAL
#undef FW_MEDIUM
#undef FW_SEMI_BOLD
#undef FW_BOLD
#undef FW_BLACK
#include "affineui.h"
#pragma pop_macro("FW_BLACK")
#pragma pop_macro("FW_BOLD")
#pragma pop_macro("FW_SEMI_BOLD")
#pragma pop_macro("FW_MEDIUM")
#pragma pop_macro("FW_NORMAL")
#pragma pop_macro("FW_LIGHT")
#pragma pop_macro("FW_THIN")
#pragma pop_macro("small")
#pragma pop_macro("check")

namespace
{
// AffineUI's C log sink is process-global; this surface serializes all use on the RHI lane.
bool bAffineError = false;
void AffineLog(affineui_log_level Level, const char* Message, void*)
{
	if (Level == AFFINEUI_LOG_ERROR)
	{
		bAffineError = true;
		UE_LOG(LogAffineUiSurfaceRender, Error, TEXT("AffineUI: %s"), UTF8_TO_TCHAR(Message ? Message : ""));
	}
	else if (Level == AFFINEUI_LOG_WARN)
	{
		UE_LOG(LogAffineUiSurfaceRender, Warning, TEXT("AffineUI: %s"), UTF8_TO_TCHAR(Message ? Message : ""));
	}
}

int LoadFontResource(void* User, const char* Url, affineui_resource_blob* Out)
{
	if (!User || !Url || !Out) { return 0; }
	FString Name = UTF8_TO_TCHAR(Url);
	if (Name.StartsWith(TEXT("/"))) { Name.RightChopInline(1); }
	if (!Name.StartsWith(TEXT("fonts/"), ESearchCase::CaseSensitive)) { return 0; }
	const FString Leaf = Name.RightChop(6);
	const FString Extension = FPaths::GetExtension(Leaf);
	if (Leaf.IsEmpty() || Leaf.Contains(TEXT("/")) || Leaf.Contains(TEXT("\\")) || Leaf.Contains(TEXT(":")) ||
		(!Extension.Equals(TEXT("ttf")) && !Extension.Equals(TEXT("otf"))))
	{
		return 0;
	}

	const TMap<FString, TArray<uint8>>& Assets = *static_cast<const TMap<FString, TArray<uint8>>*>(User);
	const TArray<uint8>* Bytes = Assets.Find(Name);
	if (!Bytes || Bytes->IsEmpty()) { return 0; }
	TUniquePtr<TArray<uint8>> Copy = MakeUnique<TArray<uint8>>(*Bytes);
	Out->data = Copy->GetData();
	Out->size = static_cast<size_t>(Copy->Num());
	Out->release = [](void* Resource) { delete static_cast<TArray<uint8>*>(Resource); };
	Out->user = Copy.Release();
	return 1;
}

int ToAffineKey(EAffineUiInputKey Key)
{
	switch (Key)
	{
	case EAffineUiInputKey::Tab: return AFFINEUI_KEY_TAB;
	case EAffineUiInputKey::Enter: return AFFINEUI_KEY_ENTER;
	case EAffineUiInputKey::Space: return AFFINEUI_KEY_SPACE;
	default: return AFFINEUI_KEY_UNKNOWN;
	}
}
}

class FAffineUiSurfaceRenderer::FState
{
public:
	~FState()
	{
		if (Ui) { affineui_ui_destroy(Ui); }
	}

	bool Render(FRHITexture* Output, FIntPoint Size, float DpiScale, FColor ClearColor, const FAffineUiContent& Content,
		const TArray<FAffineUiTextMutation>& TextMutations,
		const TArray<FAffineUiMutation>& Mutations, uint64 InDocumentEpoch,
		TSharedRef<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe> InFontAssets)
	{
		if (!Output || !IsRHID3D11()) { return Fail(TEXT("D3D11 output is unavailable")); }
		ID3D11DynamicRHI* Rhi = GetID3D11DynamicRHI();
		ID3D11Device* Device = Rhi->RHIGetDevice();
		ID3D11DeviceContext* Immediate = Rhi->RHIGetDeviceContext();
		ID3D11Resource* Destination = Rhi->RHIGetResource(Output);
		if (!Device || !Immediate || !Destination) { return Fail(TEXT("D3D11 device or output is unavailable")); }

		TRefCountPtr<ID3D11Texture2D> DestinationTexture;
		HRESULT Result = Destination->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(DestinationTexture.GetInitReference()));
		if (FAILED(Result)) { return Fail(TEXT("QueryInterface output"), Result); }
		D3D11_TEXTURE2D_DESC OutputDesc{};
		DestinationTexture->GetDesc(&OutputDesc);
		if (OutputDesc.Width != static_cast<uint32>(Size.X) || OutputDesc.Height != static_cast<uint32>(Size.Y) ||
			OutputDesc.MipLevels != 1 || OutputDesc.ArraySize != 1 || OutputDesc.SampleDesc.Count != 1 ||
			(OutputDesc.Format != DXGI_FORMAT_B8G8R8A8_TYPELESS && OutputDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
			 OutputDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))
		{
			return Fail(TEXT("Output texture is not a compatible single-sample BGRA8 texture"));
		}

		if (!Context)
		{
			Result = Device->CreateDeferredContext(0, Context.GetInitReference());
			if (FAILED(Result)) { return Fail(TEXT("CreateDeferredContext"), Result); }
		}
		D3D11_TEXTURE2D_DESC ColorDesc{};
		ColorDesc.Width = Size.X; ColorDesc.Height = Size.Y;
		ColorDesc.MipLevels = 1; ColorDesc.ArraySize = 1;
		ColorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		ColorDesc.SampleDesc.Count = 1;
		ColorDesc.Usage = D3D11_USAGE_DEFAULT;
		ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		TRefCountPtr<ID3D11Texture2D> Color;
		TRefCountPtr<ID3D11RenderTargetView> ColorView;
		TRefCountPtr<ID3D11Texture2D> Depth;
		TRefCountPtr<ID3D11DepthStencilView> DepthView;
		Result = Device->CreateTexture2D(&ColorDesc, nullptr, Color.GetInitReference());
		if (FAILED(Result)) { return Fail(TEXT("CreateTexture2D color"), Result); }
		Result = Device->CreateRenderTargetView(Color.GetReference(), nullptr, ColorView.GetInitReference());
		if (FAILED(Result)) { return Fail(TEXT("CreateRenderTargetView"), Result); }
		D3D11_TEXTURE2D_DESC DepthDesc = ColorDesc;
		DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		Result = Device->CreateTexture2D(&DepthDesc, nullptr, Depth.GetInitReference());
		if (FAILED(Result)) { return Fail(TEXT("CreateTexture2D depth"), Result); }
		Result = Device->CreateDepthStencilView(Depth.GetReference(), nullptr, DepthView.GetInitReference());
		if (FAILED(Result)) { return Fail(TEXT("CreateDepthStencilView"), Result); }

		bAffineError = false;
		if (!Ui)
		{
			Ui = affineui_ui_create();
			if (!Ui) { return Fail(TEXT("AffineUI allocation failed")); }
			affineui_gpu_context Gpu{};
			Gpu.backend = AFFINEUI_GPU_D3D11; Gpu.d3d11_device = Device;
			Gpu.d3d11_device_context = Context.GetReference();
			Gpu.color_format = AFFINEUI_FORMAT_BGRA8; Gpu.depth_format = AFFINEUI_FORMAT_DEPTH_STENCIL;
			Gpu.sample_count = 1;
			affineui_init_desc Init{}; Init.gpu = &Gpu; Init.log = &AffineLog;
			RegisteredClickNodes.Reset();
			affineui_ui_init(Ui, &Init);
			if (bAffineError)
			{
				affineui_ui_destroy(Ui); Ui = nullptr;
				bHasContent = false; AppliedText.Reset();
				DiscardRecordedCommands();
				return Fail(TEXT("AffineUI initialization failed"));
			}
			FontAssets = InFontAssets;
			if (!FontAssets->IsEmpty())
			{
				affineui_ui_set_resource_loader(Ui, &LoadFontResource, const_cast<TMap<FString, TArray<uint8>>*>(FontAssets.Get()));
			}
			UE_LOG(LogAffineUiSurfaceRender, Display, TEXT("AffineUI %s initialized on Unreal D3D11"), UTF8_TO_TCHAR(affineui_version()));
		}
		const bool bHtmlChanged = !bHasContent || LastHtml != Content.Html;
		const bool bCssChanged = !bHasContent || LastCss != Content.Css;
		const FTCHARToUTF8 Html(*Content.Html);
		const FTCHARToUTF8 Css(*Content.Css);
		if (bCssChanged) { affineui_ui_set_css(Ui, Css.Get()); LastCss = Content.Css; }
		if (bHtmlChanged)
		{
			affineui_ui_set_html(Ui, Html.Get());
			LastHtml = Content.Html;
			AppliedText.Reset();
			NodeHandles.Reset();
		}
		if (DocumentEpoch != InDocumentEpoch)
		{
			NodeHandles.Reset();
			DocumentEpoch = InDocumentEpoch;
		}
		bHasContent = true;
		if (!Mutations.IsEmpty())
		{
			affineui_ui_begin_node_mutations(Ui);
			bool bMutationSuccess = true;
			for (const FAffineUiMutation& Mutation : Mutations)
			{
				if (Mutation.DocumentEpoch != DocumentEpoch) { bMutationSuccess = false; break; }
				affineui_dom_handle Node = Resolve(Mutation.NodeId);
				affineui_dom_handle Parent = Resolve(Mutation.ParentId);
				affineui_dom_handle Before = Resolve(Mutation.BeforeId);
				const FTCHARToUTF8 Name(*Mutation.Name);
				const FTCHARToUTF8 Value(*Mutation.Value);
				switch (Mutation.Kind)
				{
				case EAffineUiMutationKind::MountRoot:
					Node = affineui_ui_mount_root(Ui, Name.Get());
					bMutationSuccess = IsValidHandle(Node);
					if (bMutationSuccess) NodeHandles.Add(Mutation.NodeId, Node);
					break;
				case EAffineUiMutationKind::CreateElement:
				case EAffineUiMutationKind::CreateSvgElement:
					Node = affineui_ui_create_node(Ui, Mutation.Kind == EAffineUiMutationKind::CreateSvgElement ?
						AFFINEUI_DOM_SVG_ELEMENT : AFFINEUI_DOM_ELEMENT, Name.Get());
					bMutationSuccess = IsValidHandle(Node);
					if (bMutationSuccess) NodeHandles.Add(Mutation.NodeId, Node);
					break;
				case EAffineUiMutationKind::CreateText:
					Node = affineui_ui_create_node(Ui, AFFINEUI_DOM_TEXT, Value.Get());
					bMutationSuccess = IsValidHandle(Node);
					if (bMutationSuccess) NodeHandles.Add(Mutation.NodeId, Node);
					break;
				case EAffineUiMutationKind::CreateComment:
					Node = affineui_ui_create_node(Ui, AFFINEUI_DOM_COMMENT, Value.Get());
					bMutationSuccess = IsValidHandle(Node);
					if (bMutationSuccess) NodeHandles.Add(Mutation.NodeId, Node);
					break;
				case EAffineUiMutationKind::InsertBefore:
					bMutationSuccess = affineui_ui_insert_node(Ui, Parent, Node, Before) != 0;
					break;
				case EAffineUiMutationKind::Remove:
					bMutationSuccess = affineui_ui_remove_node(Ui, Node) != 0;
					if (bMutationSuccess) PruneInvalidHandles();
					break;
				case EAffineUiMutationKind::SetText:
					bMutationSuccess = affineui_ui_node_set_text(Ui, Node, Value.Get()) != 0;
					break;
				case EAffineUiMutationKind::SetAttribute:
					bMutationSuccess = affineui_ui_node_set_attr(Ui, Node, Name.Get(), Value.Get()) != 0;
					break;
				case EAffineUiMutationKind::RemoveAttribute:
					bMutationSuccess = affineui_ui_node_remove_attr(Ui, Node, Name.Get()) != 0;
					break;
				case EAffineUiMutationKind::RegisterClickHandler:
					bMutationSuccess = IsValidHandle(Node);
					if (bMutationSuccess) { RegisterClickHandler(Mutation.NodeId); }
					break;
				}
				if (!bMutationSuccess) break;
			}
			affineui_ui_end_node_mutations(Ui);
			if (!bMutationSuccess)
			{
				// A partial batch cannot safely be replayed; discard its document and identity map.
				affineui_ui_destroy(Ui); Ui = nullptr;
				bHasContent = false; AppliedText.Reset(); NodeHandles.Reset();
				DiscardRecordedCommands();
				return Fail(TEXT("AffineUI mutation batch failed; surface state was reset"));
			}
		}
		for (const FAffineUiTextMutation& Mutation : TextMutations)
		{
			if (const FString* PreviousText = AppliedText.Find(Mutation.ElementId); PreviousText && *PreviousText == Mutation.Text) { continue; }
			const FTCHARToUTF8 ElementId(*Mutation.ElementId);
			const FTCHARToUTF8 Text(*Mutation.Text);
			(void)affineui_ui_set_text(Ui, ElementId.Get(), Text.Get());
			// The C API returns false both for an unknown id and for an unchanged value.
			// Remember attempts so a stale id does not generate work on every redraw.
			AppliedText.Add(Mutation.ElementId, Mutation.Text);
		}
		affineui_ui_set_clear_color(Ui, ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A);
		affineui_frame_target Target{};
		Target.width = Size.X; Target.height = Size.Y; Target.dpi_scale = DpiScale;
		Target.sample_count = 1; Target.clear = 1;
		Target.d3d11_render_view = ColorView.GetReference();
		Target.d3d11_depth_stencil_view = DepthView.GetReference();
		affineui_ui_render(Ui, &Target);
		if (bAffineError)
		{
			// Discarded uploads can leave AffineUI's internal cache inconsistent.
			affineui_ui_destroy(Ui); Ui = nullptr;
			bHasContent = false; AppliedText.Reset();
			DiscardRecordedCommands();
			return Fail(TEXT("AffineUI draw reported an error"));
		}

		Context->ClearState();
		Context->CopyResource(Destination, Color.GetReference());
		TRefCountPtr<ID3D11CommandList> Commands;
		Result = Context->FinishCommandList(false, Commands.GetInitReference());
		if (FAILED(Result) || !Commands)
		{
			// The deferred context is no longer trusted after a failed finish.
			if (Ui) { affineui_ui_destroy(Ui); Ui = nullptr; }
			bHasContent = false; AppliedText.Reset();
			Context.SafeRelease();
			return Fail(TEXT("FinishCommandList"), Result);
		}
		Immediate->ExecuteCommandList(Commands.GetReference(), true);
		RenderedSize = Size;
		RenderedDpiScale = DpiScale;
		return true;
	}

	bool DispatchInput(const FAffineUiInput& Input, FIntPoint TextureSize, uint64 InDocumentEpoch, TArray<uint64>& OutClicks)
	{
		if (!Ui || DocumentEpoch != InDocumentEpoch) { return false; }

		affineui_event Event{};
		Event.button = AFFINEUI_MOUSE_LEFT;
		Event.shift = Input.bShift;
		const bool bPointer = Input.Kind == EAffineUiInputKind::MouseMove ||
			Input.Kind == EAffineUiInputKind::MouseDown || Input.Kind == EAffineUiInputKind::MouseUp ||
			Input.Kind == EAffineUiInputKind::MouseWheel;
		if (bPointer)
		{
			// A captured release must still clear AffineUI's pressed state if a
			// resize changed the output between press and release. Coordinates
			// are normalized by Slate, so map them to the renderer's current size.
			if (TextureSize != RenderedSize && Input.Kind != EAffineUiInputKind::MouseUp) { return false; }
			Event.x = FMath::FloorToInt(Input.Position.X * RenderedSize.X / RenderedDpiScale);
			Event.y = FMath::FloorToInt(Input.Position.Y * RenderedSize.Y / RenderedDpiScale);
		}
		switch (Input.Kind)
		{
		case EAffineUiInputKind::MouseMove: Event.type = AFFINEUI_EVENT_MOUSE_MOVE; break;
		case EAffineUiInputKind::MouseDown: Event.type = AFFINEUI_EVENT_MOUSE_DOWN; break;
		case EAffineUiInputKind::MouseUp: Event.type = AFFINEUI_EVENT_MOUSE_UP; break;
		case EAffineUiInputKind::MouseWheel:
			Event.type = AFFINEUI_EVENT_MOUSE_WHEEL;
			Event.wheel_dy = Input.WheelDelta;
			break;
		case EAffineUiInputKind::KeyDown:
		case EAffineUiInputKind::KeyUp:
			Event.type = Input.Kind == EAffineUiInputKind::KeyDown ? AFFINEUI_EVENT_KEY_DOWN : AFFINEUI_EVENT_KEY_UP;
			Event.key = ToAffineKey(Input.Key);
			break;
		case EAffineUiInputKind::FocusGained: Event.type = AFFINEUI_EVENT_FOCUS_GAINED; break;
		case EAffineUiInputKind::FocusLost: Event.type = AFFINEUI_EVENT_FOCUS_LOST; break;
		}
		// Some control handlers use the most recently hit hover chain on release.
		if (Input.Kind == EAffineUiInputKind::MouseUp)
		{
			affineui_event Move = Event;
			Move.type = AFFINEUI_EVENT_MOUSE_MOVE;
			affineui_ui_dispatch(Ui, &Move);
		}
		affineui_ui_dispatch(Ui, &Event);
		OutClicks = MoveTemp(ClickedNodes);
		return affineui_ui_needs_update(Ui) != 0;
	}

private:
	void RegisterClickHandler(uint64 NodeId)
	{
		if (RegisteredClickNodes.Contains(NodeId)) { return; }
		RegisteredClickNodes.Add(NodeId);
		struct FClickBinding
		{
			FState* State;
			uint64 Epoch;
			uint64 NodeId;
		};
		FClickBinding* Binding = new FClickBinding{this, DocumentEpoch, NodeId};
		const FString Selector = FString::Printf(TEXT(".nativeweb-click-%llu"), static_cast<unsigned long long>(NodeId));
		const FTCHARToUTF8 SelectorUtf8(*Selector);
		affineui_ui_on_click(Ui, SelectorUtf8.Get(), [](void* User)
		{
			const FClickBinding& Click = *static_cast<FClickBinding*>(User);
			if (Click.Epoch == Click.State->DocumentEpoch && IsValidHandle(Click.State->Resolve(Click.NodeId)))
			{
				Click.State->ClickedNodes.Add(Click.NodeId);
			}
		}, Binding, [](void* User) { delete static_cast<FClickBinding*>(User); });
	}

	affineui_dom_handle Resolve(uint64 Id) const
	{
		if (Id == 0) return {};
		if (const affineui_dom_handle* Handle = NodeHandles.Find(Id)) return *Handle;
		return {};
	}
	static bool IsValidHandle(affineui_dom_handle Handle) { return Handle.document_id != 0 && Handle.generation != 0; }
	void PruneInvalidHandles()
	{
		for (auto It = NodeHandles.CreateIterator(); It; ++It)
			if (!affineui_ui_node_valid(Ui, It.Value())) It.RemoveCurrent();
	}

	void DiscardRecordedCommands()
	{
		if (!Context) { return; }
		Context->ClearState();
		TRefCountPtr<ID3D11CommandList> Discarded;
		if (FAILED(Context->FinishCommandList(false, Discarded.GetInitReference())))
		{
			if (Ui) { affineui_ui_destroy(Ui); Ui = nullptr; }
			Context.SafeRelease();
		}
	}

	static bool Fail(const TCHAR* Operation, HRESULT Result = S_OK)
	{
		UE_LOG(LogAffineUiSurfaceRender, Error, TEXT("%s (HRESULT 0x%08x)"), Operation, static_cast<uint32>(Result));
		return false;
	}
	TRefCountPtr<ID3D11DeviceContext> Context;
	affineui_ui* Ui = nullptr;
	FString LastHtml;
	FString LastCss;
	TMap<FString, FString> AppliedText;
	TMap<uint64, affineui_dom_handle> NodeHandles;
	TSet<uint64> RegisteredClickNodes;
	TArray<uint64> ClickedNodes;
	TSharedPtr<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe> FontAssets;
	FIntPoint RenderedSize = FIntPoint::ZeroValue;
	float RenderedDpiScale = 1.0f;
	uint64 DocumentEpoch = 0;
	bool bHasContent = false;
};
#else
class FAffineUiSurfaceRenderer::FState {};
#endif

void FAffineUiSurfaceRenderer::Draw(FTextureRenderTargetResource* Target, FIntPoint Size, float DpiScale, FColor ClearColor,
	FAffineUiContent Content, TSharedRef<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe> FontAssets,
	uint64 Revision, TArray<FAffineUiTextMutation> TextMutations,
	TArray<FAffineUiMutation> Mutations, uint64 DocumentEpoch,
	TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface)
{
	ENQUEUE_RENDER_COMMAND(AffineSurfaceDraw)([this, Target, Size, DpiScale, ClearColor, Content = MoveTemp(Content), FontAssets = MoveTemp(FontAssets),
		TextMutations = MoveTemp(TextMutations), Mutations = MoveTemp(Mutations), DocumentEpoch, Revision, Surface](FRHICommandListImmediate& RHICmdList) mutable
	{
		FTextureRHIRef Output;
		if (Target) { Output = Target->GetRenderTargetTexture(); }
		RHICmdList.EnqueueLambda([this, Output, Size, DpiScale, ClearColor, Content = MoveTemp(Content), FontAssets = MoveTemp(FontAssets),
			TextMutations = MoveTemp(TextMutations), Mutations = MoveTemp(Mutations), DocumentEpoch, Revision, Surface](FRHICommandListBase&)
		{
			bool bSuccess = false;
#if PLATFORM_WINDOWS
			if (!State) { State = new FState(); }
			bSuccess = State->Render(Output.GetReference(), Size, DpiScale, ClearColor, Content, TextMutations, Mutations, DocumentEpoch, FontAssets);
#endif
			AsyncTask(ENamedThreads::GameThread, [Surface, Revision, bSuccess]()
			{
				if (TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Pinned = Surface.Pin())
				{
					Pinned->OnRenderComplete(Revision, bSuccess);
				}
			});
		});
	});
}

void FAffineUiSurfaceRenderer::DispatchInput(FAffineUiInput Input, FIntPoint TextureSize, uint64 DocumentEpoch,
	TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface)
{
#if PLATFORM_WINDOWS
	ENQUEUE_RENDER_COMMAND(AffineSurfaceInput)([this, Input, TextureSize, DocumentEpoch, Surface](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.EnqueueLambda([this, Input, TextureSize, DocumentEpoch, Surface](FRHICommandListBase&)
		{
			TArray<uint64> Clicks;
			const bool bNeedsRedraw = State && State->DispatchInput(Input, TextureSize, DocumentEpoch, Clicks);
			if (Clicks.IsEmpty() && !bNeedsRedraw) { return; }
			AsyncTask(ENamedThreads::GameThread, [Surface, DocumentEpoch, Clicks = MoveTemp(Clicks), bNeedsRedraw]() mutable
			{
				if (TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Pinned = Surface.Pin())
				{
					Pinned->OnInputComplete(DocumentEpoch, MoveTemp(Clicks), bNeedsRedraw);
				}
			});
		});
	});
#endif
}

void FAffineUiSurfaceRenderer::Shutdown()
{
	ENQUEUE_RENDER_COMMAND(AffineSurfaceShutdown)([this](FRHICommandListImmediate& RHICmdList)
	{
		// Close is infrequent: retire submitted GPU work before releasing AffineUI's resources.
		RHICmdList.SubmitAndBlockUntilGPUIdle();
		RHICmdList.EnqueueLambda([this](FRHICommandListBase&)
		{
			delete State;
			State = nullptr;
		});
	});
	FRenderCommandFence Fence;
	Fence.BeginFence(FRenderCommandFence::ESyncDepth::RHIThread);
	Fence.Wait();
}
