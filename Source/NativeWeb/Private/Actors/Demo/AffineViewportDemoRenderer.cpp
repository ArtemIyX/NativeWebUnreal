#include "Actors/Demo/AffineViewportDemoRenderer.h"

#include "CoreMinimal.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderCommandFence.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"

static TAutoConsoleVariable<int32> CVarAffineViewportReadback(
	TEXT("NativeWeb.AffineViewportReadback"), 1,
	TEXT("Enable diagnostic GPU readbacks for the Affine viewport demo (1 on, 0 off). Set before starting Play."),
	ECVF_Default);

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

DEFINE_LOG_CATEGORY_STATIC(LogAffineViewportRender, Log, All);

namespace
{
	// AffineUI's C log sink is process-global. Access is confined to the RHI lane.
	bool bAffineError = false;

	void AffineLog(affineui_log_level Level, const char* Message, void*)
	{
		if (Level == AFFINEUI_LOG_ERROR)
		{
			bAffineError = true;
			UE_LOG(LogAffineViewportRender, Error, TEXT("AffineUI: %s"), UTF8_TO_TCHAR(Message ? Message : ""));
		}
		else if (Level == AFFINEUI_LOG_WARN)
		{
			UE_LOG(LogAffineViewportRender, Warning, TEXT("AffineUI: %s"), UTF8_TO_TCHAR(Message ? Message : ""));
		}
		else
		{
			UE_LOG(LogAffineViewportRender, Log, TEXT("AffineUI: %s"), UTF8_TO_TCHAR(Message ? Message : ""));
		}
	}

	constexpr const char* Html = R"HTML(
<html><body><div class="stripe"></div><div class="content">
<h1>AffineUI viewport</h1><button>Count 0</button>
</div></body></html>)HTML";
	constexpr const char* Css = R"CSS(
body { margin: 0; background-color: #081629; color: #effaff; font-family: sans-serif; }
.stripe { background-color: #19dbe9; width: 90px; height: 14px; }
.content { padding: 24px; }
h1 { font-size: 27px; margin: 0 0 24px 0; }
button { background-color: #087489; color: #ffffff; padding: 10px 22px; font-size: 18px; }
)CSS";
}

class FAffineViewportDemoRenderer::FState
{
public:
	explicit FState(bool bInDiagnostics) : bDiagnostics(bInDiagnostics) {}
	~FState()
	{
		if (Ui)
		{
			affineui_ui_destroy(Ui);
		}
	}

	void Render(FRHITexture* Output, FRHITexture* Sampled)
	{
		UE_LOG(LogAffineViewportRender, Display, TEXT("RHI textures: render=%p slate-resource=%p same=%d"), Output, Sampled, Output == Sampled ? 1 : 0);
		if (!Output || !IsRHID3D11())
		{
			UE_LOG(LogAffineViewportRender, Error, TEXT("D3D11 output texture is unavailable."));
			return;
		}
		ID3D11DynamicRHI* Rhi = GetID3D11DynamicRHI();
		ID3D11Device* Device = Rhi->RHIGetDevice();
		ID3D11DeviceContext* Immediate = Rhi->RHIGetDeviceContext();
		ID3D11Resource* Destination = Rhi->RHIGetResource(Output);
		if (!Device || !Immediate || !Destination)
		{
			UE_LOG(LogAffineViewportRender, Error,
			       TEXT("D3D11 device, immediate context, or output resource is unavailable."));
			return;
		}
		TRefCountPtr<ID3D11Texture2D> DestinationTexture;
		HRESULT Result = Destination->QueryInterface(__uuidof(ID3D11Texture2D),
		                                             reinterpret_cast<void**>(DestinationTexture.GetInitReference()));
		if (FAILED(Result))
		{
			LogFailure(TEXT("QueryInterface output texture"), Result);
			return;
		}
		D3D11_TEXTURE2D_DESC OutputDesc{};
		DestinationTexture->GetDesc(&OutputDesc);
		UE_LOG(LogAffineViewportRender, Display, TEXT("Output: %ux%u format=%u mips=%u samples=%u bind=0x%x; render texture=%p"),
			OutputDesc.Width, OutputDesc.Height, static_cast<uint32>(OutputDesc.Format), OutputDesc.MipLevels,
			OutputDesc.SampleDesc.Count, OutputDesc.BindFlags, Destination);
		if (OutputDesc.Width != 480 || OutputDesc.Height != 240 || OutputDesc.MipLevels != 1 ||
			OutputDesc.ArraySize != 1 || OutputDesc.SampleDesc.Count != 1 ||
			(OutputDesc.Format != DXGI_FORMAT_B8G8R8A8_TYPELESS && OutputDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
				OutputDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))
		{
			UE_LOG(LogAffineViewportRender, Error,
			       TEXT("Output texture is incompatible with the BGRA8 demo copy; retaining the fallback."));
			return;
		}

		Result = Device->CreateDeferredContext(0, Context.GetInitReference());
		if (FAILED(Result))
		{
			LogFailure(TEXT("CreateDeferredContext"), Result);
			return;
		}

		D3D11_TEXTURE2D_DESC ColorDesc{};
		ColorDesc.Width = 480;
		ColorDesc.Height = 240;
		ColorDesc.MipLevels = 1;
		ColorDesc.ArraySize = 1;
		ColorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		ColorDesc.SampleDesc.Count = 1;
		ColorDesc.Usage = D3D11_USAGE_DEFAULT;
		ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		Result = Device->CreateTexture2D(&ColorDesc, nullptr, Color.GetInitReference());
		if (FAILED(Result))
		{
			LogFailure(TEXT("CreateTexture2D color"), Result);
			return;
		}
		Result = Device->CreateRenderTargetView(Color.GetReference(), nullptr, ColorView.GetInitReference());
		if (FAILED(Result))
		{
			LogFailure(TEXT("CreateRenderTargetView"), Result);
			return;
		}

		D3D11_TEXTURE2D_DESC DepthDesc = ColorDesc;
		DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		Result = Device->CreateTexture2D(&DepthDesc, nullptr, Depth.GetInitReference());
		if (FAILED(Result))
		{
			LogFailure(TEXT("CreateTexture2D depth/stencil"), Result);
			return;
		}
		Result = Device->CreateDepthStencilView(Depth.GetReference(), nullptr, DepthView.GetInitReference());
		if (FAILED(Result))
		{
			LogFailure(TEXT("CreateDepthStencilView"), Result);
			return;
		}

		bAffineError = false;
		UE_LOG(LogAffineViewportRender, Display, TEXT("AffineUI version: %s"), UTF8_TO_TCHAR(affineui_version()));
		Ui = affineui_ui_create();
		if (!Ui)
		{
			UE_LOG(LogAffineViewportRender, Error, TEXT("AffineUI allocation failed."));
			return;
		}
		affineui_gpu_context Gpu{};
		Gpu.backend = AFFINEUI_GPU_D3D11;
		Gpu.d3d11_device = Device;
		Gpu.d3d11_device_context = Context.GetReference();
		Gpu.color_format = AFFINEUI_FORMAT_BGRA8;
		Gpu.depth_format = AFFINEUI_FORMAT_DEPTH_STENCIL;
		Gpu.sample_count = 1;
		affineui_init_desc Init{};
		Init.gpu = &Gpu;
		Init.log = &AffineLog;
		affineui_ui_init(Ui, &Init);
		if (bAffineError)
		{
			UE_LOG(LogAffineViewportRender, Error, TEXT("AffineUI initialization failed; retaining the fallback."));
			return;
		}
		affineui_ui_set_css(Ui, Css);
		affineui_ui_set_html(Ui, Html);
		affineui_ui_set_clear_color(Ui, 8, 22, 41, 255);
		affineui_frame_target Target{};
		Target.width = 480;
		Target.height = 240;
		Target.dpi_scale = 1.0f;
		Target.sample_count = 1;
		Target.clear = 1;
		Target.d3d11_render_view = ColorView.GetReference();
		Target.d3d11_depth_stencil_view = DepthView.GetReference();
		affineui_ui_render(Ui, &Target);
		int ContentWidth = 0, ContentHeight = 0;
		affineui_ui_content_size(Ui, &ContentWidth, &ContentHeight);
		UE_LOG(LogAffineViewportRender, Display, TEXT("AffineUI render returned: content=%dx%d needs_update=%d reported_error=%d"),
			ContentWidth, ContentHeight, affineui_ui_needs_update(Ui), bAffineError ? 1 : 0);
		if (bAffineError)
		{
			UE_LOG(LogAffineViewportRender, Error, TEXT("AffineUI draw failed; retaining the fallback."));
			return;
		}

		// Unbind the producer before copying to the UE-owned Slate texture.
		Context->ClearState();
		Context->CopyResource(Destination, Color.GetReference());
		TRefCountPtr<ID3D11CommandList> Commands;
		Result = Context->FinishCommandList(false, Commands.GetInitReference());
		if (FAILED(Result) || !Commands)
		{
			LogFailure(TEXT("FinishCommandList"), Result);
			return;
		}
		Immediate->ExecuteCommandList(Commands.GetReference(), true);
		UE_LOG(LogAffineViewportRender, Display,
		       TEXT("AffineUI D3D11 command list submitted; GPU output not yet verified."));
		if (bDiagnostics)
		{
			Sample(Output, Sampled, TEXT("after-submit"));
		}
	}

	void Sample(FRHITexture* Output, FRHITexture* Sampled, const TCHAR* Phase)
	{
		if (!bDiagnostics || !Output || !Color || !IsRHID3D11()) { return; }
		ID3D11DynamicRHI* Rhi = GetID3D11DynamicRHI();
		ID3D11Device* Device = Rhi->RHIGetDevice();
		ID3D11DeviceContext* Immediate = Rhi->RHIGetDeviceContext();
		ID3D11Resource* OutputResource = Rhi->RHIGetResource(Output);
		if (!Device || !Immediate || !OutputResource)
		{
			UE_LOG(LogAffineViewportRender, Warning, TEXT("Pixel diagnostic %s: D3D11 resources unavailable."), Phase);
			return;
		}
		ReadPixels(Device, Immediate, Color.GetReference(), TEXT("producer"), Phase);
		ReadPixels(Device, Immediate, OutputResource, TEXT("viewport-target"), Phase);
		if (Sampled && Sampled != Output)
		{
			if (ID3D11Resource* SampledResource = Rhi->RHIGetResource(Sampled))
			{
				ReadPixels(Device, Immediate, SampledResource, TEXT("slate-resource"), Phase);
			}
		}
	}

private:
	static void ReadPixels(ID3D11Device* Device, ID3D11DeviceContext* Immediate, ID3D11Resource* Resource, const TCHAR* Label, const TCHAR* Phase)
	{
		TRefCountPtr<ID3D11Texture2D> Source;
		HRESULT Result = Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(Source.GetInitReference()));
		if (FAILED(Result)) { LogFailure(TEXT("diagnostic QueryInterface"), Result); return; }
		D3D11_TEXTURE2D_DESC Desc{};
		Source->GetDesc(&Desc);
		if (Desc.Width != 480 || Desc.Height != 240 || Desc.SampleDesc.Count != 1 ||
			(Desc.Format != DXGI_FORMAT_B8G8R8A8_TYPELESS && Desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && Desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))
		{
			UE_LOG(LogAffineViewportRender, Warning, TEXT("Pixel diagnostic %s %s: unsupported texture %ux%u format=%u samples=%u."),
				Phase, Label, Desc.Width, Desc.Height, static_cast<uint32>(Desc.Format), Desc.SampleDesc.Count);
			return;
		}
		Desc.Usage = D3D11_USAGE_STAGING;
		Desc.BindFlags = 0;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		Desc.MiscFlags = 0;
		TRefCountPtr<ID3D11Texture2D> Staging;
		Result = Device->CreateTexture2D(&Desc, nullptr, Staging.GetInitReference());
		if (FAILED(Result)) { LogFailure(TEXT("diagnostic staging texture"), Result); return; }
		Immediate->CopyResource(Staging.GetReference(), Source.GetReference());
		D3D11_MAPPED_SUBRESOURCE Mapped{};
		Result = Immediate->Map(Staging.GetReference(), 0, D3D11_MAP_READ, 0, &Mapped);
		if (FAILED(Result)) { LogFailure(TEXT("diagnostic Map"), Result); return; }
		uint64 SumR = 0, SumG = 0, SumB = 0;
		uint32 Cyan = 0, Light = 0;
		for (uint32 Y = 0; Y < Desc.Height; ++Y)
		{
			const uint8* Row = static_cast<const uint8*>(Mapped.pData) + Y * Mapped.RowPitch;
			for (uint32 X = 0; X < Desc.Width; ++X)
			{
				const uint8* Pixel = Row + X * 4;
				const uint8 B = Pixel[0], G = Pixel[1], R = Pixel[2];
				SumR += R; SumG += G; SumB += B;
				Cyan += R < 110 && G > 120 && B > 120;
				Light += R > 175 && G > 175 && B > 175;
			}
		}
		const uint8* Stripe = static_cast<const uint8*>(Mapped.pData) + 5 * Mapped.RowPitch + 10 * 4;
		const uint8* Corner = static_cast<const uint8*>(Mapped.pData) + (Desc.Height - 1) * Mapped.RowPitch + (Desc.Width - 1) * 4;
		const uint64 Count = static_cast<uint64>(Desc.Width) * Desc.Height;
		UE_LOG(LogAffineViewportRender, Display, TEXT("Pixel diagnostic %s %s: avg RGB=(%llu,%llu,%llu), stripe(10,5)=(%u,%u,%u,%u), corner=(%u,%u,%u,%u), cyan=%u light=%u"),
			Phase, Label, SumR / Count, SumG / Count, SumB / Count,
			Stripe[2], Stripe[1], Stripe[0], Stripe[3], Corner[2], Corner[1], Corner[0], Corner[3], Cyan, Light);
		const bool bSaveProducer = FCString::Strcmp(Phase, TEXT("after-submit")) == 0 && FCString::Strcmp(Label, TEXT("producer")) == 0;
		const bool bSaveTarget = FCString::Strcmp(Phase, TEXT("after-one-second")) == 0 && FCString::Strcmp(Label, TEXT("viewport-target")) == 0;
		if (bSaveProducer || bSaveTarget)
		{
			// Copy the diagnostic snapshot while mapped; write it off the RHI thread.
			const uint32 RowBytes = Desc.Width * 4;
			const uint32 FileSize = 54 + RowBytes * Desc.Height;
			TArray<uint8> Bitmap;
			Bitmap.SetNumZeroed(FileSize);
			auto Write32 = [&Bitmap](int32 Offset, uint32 Value) { FMemory::Memcpy(Bitmap.GetData() + Offset, &Value, sizeof(Value)); };
			auto Write16 = [&Bitmap](int32 Offset, uint16 Value) { FMemory::Memcpy(Bitmap.GetData() + Offset, &Value, sizeof(Value)); };
			Bitmap[0] = 'B'; Bitmap[1] = 'M';
			Write32(2, FileSize); Write32(10, 54); Write32(14, 40);
			Write32(18, Desc.Width); Write32(22, static_cast<uint32>(-static_cast<int32>(Desc.Height)));
			Write16(26, 1); Write16(28, 32); Write32(34, RowBytes * Desc.Height);
			for (uint32 Y = 0; Y < Desc.Height; ++Y)
			{
				FMemory::Memcpy(Bitmap.GetData() + 54 + Y * RowBytes,
					static_cast<const uint8*>(Mapped.pData) + Y * Mapped.RowPitch, RowBytes);
			}
			const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AffineViewportDiagnostics"));
			const FString Path = FPaths::Combine(Directory, bSaveProducer ? TEXT("producer.bmp") : TEXT("viewport-target.bmp"));
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Directory, Path, Bitmap = MoveTemp(Bitmap)]()
			{
				IFileManager::Get().MakeDirectory(*Directory, true);
				if (FFileHelper::SaveArrayToFile(Bitmap, *Path))
				{
					UE_LOG(LogAffineViewportRender, Display, TEXT("Saved pixel diagnostic: %s"), *FPaths::ConvertRelativePathToFull(Path));
				}
				else
				{
					UE_LOG(LogAffineViewportRender, Warning, TEXT("Could not save pixel diagnostic: %s"), *Path);
				}
			});
		}
		Immediate->Unmap(Staging.GetReference(), 0);
	}

	static void LogFailure(const TCHAR* Operation, HRESULT Result)
	{
		UE_LOG(LogAffineViewportRender, Error, TEXT("%s failed (HRESULT 0x%08x); retaining the fallback."), Operation,
		       static_cast<uint32>(Result));
	}

	TRefCountPtr<ID3D11DeviceContext> Context;
	TRefCountPtr<ID3D11Texture2D> Color;
	TRefCountPtr<ID3D11RenderTargetView> ColorView;
	TRefCountPtr<ID3D11Texture2D> Depth;
	TRefCountPtr<ID3D11DepthStencilView> DepthView;
	affineui_ui* Ui = nullptr;
	bool bDiagnostics = false;
};
#else
class FAffineViewportDemoRenderer::FState
{
};
#endif

FAffineViewportDemoRenderer* FAffineViewportDemoRenderer::Start(FTextureRenderTargetResource* Target)
{
#if PLATFORM_WINDOWS
	if (!Target) { return nullptr; }
	FAffineViewportDemoRenderer* Renderer = new FAffineViewportDemoRenderer();
	const bool bDiagnostics = DiagnosticsEnabled();
	UE_LOG(LogAffineViewportRender, Display, TEXT("GPU pixel diagnostic: %s"), bDiagnostics ? TEXT("enabled; synchronous diagnostic readback") : TEXT("disabled via NativeWeb.AffineViewportReadback"));
	ENQUEUE_RENDER_COMMAND(AffineDemoDraw)([Renderer, Target, bDiagnostics](FRHICommandListImmediate& RHICmdList)
	{
		FTextureRHIRef Output = Target->GetRenderTargetTexture();
		FTextureRHIRef Sampled = Target->GetTextureRHI();
		RHICmdList.EnqueueLambda([Renderer, Output, Sampled, bDiagnostics](FRHICommandListBase&)
		{
			Renderer->State = new FState(bDiagnostics);
			Renderer->State->Render(Output.GetReference(), Sampled.GetReference());
		});
	});
	return Renderer;
#else
	return nullptr;
#endif
}

bool FAffineViewportDemoRenderer::DiagnosticsEnabled()
{
	return CVarAffineViewportReadback.GetValueOnAnyThread() != 0;
}

void FAffineViewportDemoRenderer::SampleLater(FAffineViewportDemoRenderer* Renderer, FTextureRenderTargetResource* Target)
{
#if PLATFORM_WINDOWS
	if (!Renderer || !Target) { return; }
	ENQUEUE_RENDER_COMMAND(AffineDemoDelayedSample)([Renderer, Target](FRHICommandListImmediate& RHICmdList)
	{
		FTextureRHIRef Output = Target->GetRenderTargetTexture();
		FTextureRHIRef Sampled = Target->GetTextureRHI();
		RHICmdList.EnqueueLambda([Renderer, Output, Sampled](FRHICommandListBase&)
		{
			if (Renderer->State) { Renderer->State->Sample(Output.GetReference(), Sampled.GetReference(), TEXT("after-one-second")); }
		});
	});
#endif
}

void FAffineViewportDemoRenderer::Stop(FAffineViewportDemoRenderer* Renderer)
{
	if (!Renderer) { return; }
	ENQUEUE_RENDER_COMMAND(AffineDemoShutdown)([Renderer](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.EnqueueLambda([Renderer](FRHICommandListBase&)
		{
			delete Renderer->State;
			delete Renderer;
		});
	});
	FRenderCommandFence Fence;
	Fence.BeginFence(FRenderCommandFence::ESyncDepth::RHIThread);
	Fence.Wait();
}
