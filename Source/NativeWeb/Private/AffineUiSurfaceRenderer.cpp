#include "AffineUiSurfaceRenderer.h"

#include "Async/Async.h"
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
		const TArray<FAffineUiMutation>& Mutations, uint64 InDocumentEpoch)
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
			affineui_ui_init(Ui, &Init);
			if (bAffineError)
			{
				affineui_ui_destroy(Ui); Ui = nullptr;
				bHasContent = false; AppliedText.Reset();
				DiscardRecordedCommands();
				return Fail(TEXT("AffineUI initialization failed"));
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
					Node = affineui_ui_create_node(Ui, AFFINEUI_DOM_ELEMENT, Name.Get());
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
		return true;
	}

private:
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
	uint64 DocumentEpoch = 0;
	bool bHasContent = false;
};
#else
class FAffineUiSurfaceRenderer::FState {};
#endif

void FAffineUiSurfaceRenderer::Draw(FTextureRenderTargetResource* Target, FIntPoint Size, float DpiScale, FColor ClearColor,
	FAffineUiContent Content, uint64 Revision, TArray<FAffineUiTextMutation> TextMutations,
	TArray<FAffineUiMutation> Mutations, uint64 DocumentEpoch,
	TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface)
{
	ENQUEUE_RENDER_COMMAND(AffineSurfaceDraw)([this, Target, Size, DpiScale, ClearColor, Content = MoveTemp(Content),
		TextMutations = MoveTemp(TextMutations), Mutations = MoveTemp(Mutations), DocumentEpoch, Revision, Surface](FRHICommandListImmediate& RHICmdList) mutable
	{
		FTextureRHIRef Output;
		if (Target) { Output = Target->GetRenderTargetTexture(); }
		RHICmdList.EnqueueLambda([this, Output, Size, DpiScale, ClearColor, Content = MoveTemp(Content),
			TextMutations = MoveTemp(TextMutations), Mutations = MoveTemp(Mutations), DocumentEpoch, Revision, Surface](FRHICommandListBase&)
		{
			bool bSuccess = false;
#if PLATFORM_WINDOWS
			if (!State) { State = new FState(); }
			bSuccess = State->Render(Output.GetReference(), Size, DpiScale, ClearColor, Content, TextMutations, Mutations, DocumentEpoch);
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
