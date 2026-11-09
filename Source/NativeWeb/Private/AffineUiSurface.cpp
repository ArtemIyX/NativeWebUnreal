#include "AffineUiSurface.h"

#include "AffineUiSurfaceRenderer.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/FileHelper.h"
#include "RenderCommandFence.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogAffineUiSurface, Log, All);

namespace
{
TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> ActiveSurface;

bool IsValidDescriptor(FIntPoint Size, float DpiScale)
{
	return Size.X > 0 && Size.Y > 0 && Size.X <= 4096 && Size.Y <= 4096 &&
		FMath::IsFinite(DpiScale) && DpiScale > 0.0f && DpiScale <= 8.0f;
}

bool ContainsEmbeddedNull(const FString& Text)
{
	return FCString::Strlen(*Text) != Text.Len();
}
}

class FAffineUiSurface::FImpl
{
public:
	struct FRetiredTexture
	{
		explicit FRetiredTexture(UTextureRenderTarget2D* InTexture) : Texture(InTexture)
		{
			GpuFence = RHICreateGPUFence(TEXT("AffineSurfaceRetire"));
			if (GpuFence)
			{
				FGPUFenceRHIRef FenceToWrite = GpuFence;
				ENQUEUE_RENDER_COMMAND(AffineSurfaceRetire)([FenceToWrite](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.WriteGPUFence(FenceToWrite);
				});
			}
			else
			{
				UE_LOG(LogAffineUiSurface, Warning, TEXT("Could not allocate a GPU retirement fence; keeping the old texture until Close."));
			}
			Fence.BeginFence(FRenderCommandFence::ESyncDepth::RHIThread);
		}
		TStrongObjectPtr<UTextureRenderTarget2D> Texture;
		FGPUFenceRHIRef GpuFence;
		FRenderCommandFence Fence;
	};

	void CollectRetired()
	{
		Retired.RemoveAll([](const TUniquePtr<FRetiredTexture>& Entry)
		{
			return Entry->Fence.IsFenceComplete() && Entry->GpuFence && Entry->GpuFence->Poll();
		});
	}

	FAffineUiSurfaceDesc Desired;
	FAffineUiTextureReady OnTextureReady;
	FAffineUiSurfaceError OnError;
	FAffineUiSurfaceRenderer Renderer;
	TStrongObjectPtr<UTextureRenderTarget2D> Output;
	TStrongObjectPtr<UTextureRenderTarget2D> Pending;
	TArray<TUniquePtr<FRetiredTexture>> Retired;
	FTSTicker::FDelegateHandle RetirementTicker;
	FString LastError;
	uint64 DesiredRevision = 1;
	uint64 PendingRevision = 0;
	bool bInFlight = false;
	bool bDirty = true;
	bool bClosed = false;
};

FAffineUiSurface::FAffineUiSurface() : Impl(MakeUnique<FImpl>()) {}

TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> FAffineUiSurface::Create(const FAffineUiSurfaceDesc& Desc, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
#if !PLATFORM_WINDOWS
	OutError = TEXT("AffineUI surfaces require Win64 D3D11.");
	return nullptr;
#else
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11)
	{
		OutError = TEXT("AffineUI surfaces require D3D11; launch with -dx11.");
		return nullptr;
	}
	if (ActiveSurface.IsValid())
	{
		OutError = TEXT("Only one AffineUI surface can be active in this process.");
		return nullptr;
	}
	if (!IsValidDescriptor(Desc.PixelSize, Desc.DpiScale) || Desc.ClearColor.A != 255 ||
		ContainsEmbeddedNull(Desc.Content.Html) || ContainsEmbeddedNull(Desc.Content.Css))
	{
		OutError = TEXT("Invalid surface size, DPI scale, clear color alpha, or embedded null in content.");
		return nullptr;
	}
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface = MakeShareable(new FAffineUiSurface());
	Surface->Impl->Desired = Desc;
	ActiveSurface = Surface;
	Surface->StartPending();
	return Surface;
#endif
}

FAffineUiSurface::~FAffineUiSurface()
{
	check(IsInGameThread());
	Close();
}

bool FAffineUiSurface::SetContent(const FAffineUiContent& Content)
{
	check(IsInGameThread());
	if (Impl->bClosed || ContainsEmbeddedNull(Content.Html) || ContainsEmbeddedNull(Content.Css)) { return false; }
	Impl->Desired.Content = Content;
	++Impl->DesiredRevision;
	Impl->bDirty = true;
	StartPending();
	return true;
}

bool FAffineUiSurface::LoadContentFromFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (Impl->bClosed) { OutError = TEXT("Surface is closed."); return false; }
	FAffineUiContent NewContent;
	if (!FFileHelper::LoadFileToString(NewContent.Html, *HtmlPath))
	{
		OutError = FString::Printf(TEXT("Could not read HTML file: %s"), *HtmlPath);
		return false;
	}
	if (!FFileHelper::LoadFileToString(NewContent.Css, *CssPath))
	{
		OutError = FString::Printf(TEXT("Could not read CSS file: %s"), *CssPath);
		return false;
	}
	if (!SetContent(NewContent))
	{
		OutError = TEXT("File content contains an embedded null or the surface is closed.");
		return false;
	}
	return true;
}

bool FAffineUiSurface::Resize(FIntPoint PixelSize, float DpiScale)
{
	check(IsInGameThread());
	if (Impl->bClosed || !IsValidDescriptor(PixelSize, DpiScale)) { return false; }
	if (Impl->Desired.PixelSize == PixelSize && Impl->Desired.DpiScale == DpiScale) { return true; }
	Impl->Desired.PixelSize = PixelSize;
	Impl->Desired.DpiScale = DpiScale;
	++Impl->DesiredRevision;
	Impl->bDirty = true;
	StartPending();
	return true;
}

bool FAffineUiSurface::RequestRedraw()
{
	check(IsInGameThread());
	if (Impl->bClosed) { return false; }
	++Impl->DesiredRevision;
	Impl->bDirty = true;
	StartPending();
	return true;
}

void FAffineUiSurface::SetOnTextureReady(FAffineUiTextureReady Callback)
{
	check(IsInGameThread());
	if (Impl->bClosed) { return; }
	Impl->OnTextureReady = MoveTemp(Callback);
	if (Impl->Output.IsValid())
	{
		TSharedRef<FAffineUiSurface, ESPMode::ThreadSafe> KeepAlive = AsShared();
		(void)KeepAlive;
		FAffineUiTextureReady SafeCallback = Impl->OnTextureReady;
		SafeCallback.ExecuteIfBound(Impl->Output.Get());
	}
}

void FAffineUiSurface::SetOnError(FAffineUiSurfaceError Callback)
{
	check(IsInGameThread());
	if (Impl->bClosed) { return; }
	Impl->OnError = MoveTemp(Callback);
	if (!Impl->LastError.IsEmpty())
	{
		TSharedRef<FAffineUiSurface, ESPMode::ThreadSafe> KeepAlive = AsShared();
		(void)KeepAlive;
		FAffineUiSurfaceError SafeCallback = Impl->OnError;
		const FString Error = Impl->LastError;
		SafeCallback.ExecuteIfBound(Error);
	}
}

UTextureRenderTarget2D* FAffineUiSurface::GetOutputTexture() const
{
	check(IsInGameThread());
	return Impl->Output.Get();
}

FString FAffineUiSurface::GetLastError() const
{
	check(IsInGameThread());
	return Impl->LastError;
}

void FAffineUiSurface::StartPending()
{
	check(IsInGameThread());
	if (Impl->bClosed || Impl->bInFlight || !Impl->bDirty) { return; }
	Impl->CollectRetired();
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	if (!Target)
	{
		Impl->LastError = TEXT("Could not allocate the output render target.");
		Impl->bDirty = false;
		TSharedRef<FAffineUiSurface, ESPMode::ThreadSafe> KeepAlive = AsShared();
		(void)KeepAlive;
		FAffineUiSurfaceError Callback = Impl->OnError;
		const FString Error = Impl->LastError;
		Callback.ExecuteIfBound(Error);
		return;
	}
	Impl->Pending.Reset(Target);
	Target->bNeedsTwoCopies = false;
	Target->bAutoGenerateMips = false;
	Target->ClearColor = FLinearColor::FromSRGBColor(Impl->Desired.ClearColor);
	Target->InitCustomFormat(Impl->Desired.PixelSize.X, Impl->Desired.PixelSize.Y, PF_B8G8R8A8, false);
	Target->UpdateResourceImmediate(true);
	Impl->PendingRevision = Impl->DesiredRevision;
	Impl->bInFlight = true;
	Impl->bDirty = false;
	Impl->Renderer.Draw(Target->GameThread_GetRenderTargetResource(), Impl->Desired.PixelSize,
		Impl->Desired.DpiScale, Impl->Desired.ClearColor, Impl->Desired.Content, Impl->PendingRevision, AsShared());
}

void FAffineUiSurface::OnRenderComplete(uint64 Revision, bool bSuccess)
{
	check(IsInGameThread());
	if (Impl->bClosed || !Impl->bInFlight || Revision != Impl->PendingRevision) { return; }
	TSharedRef<FAffineUiSurface, ESPMode::ThreadSafe> KeepAlive = AsShared();
	(void)KeepAlive;
	Impl->bInFlight = false;
	if (bSuccess && Revision == Impl->DesiredRevision)
	{
		if (Impl->Output.IsValid())
		{
			Impl->Retired.Add(MakeUnique<FImpl::FRetiredTexture>(Impl->Output.Get()));
			if (!Impl->RetirementTicker.IsValid())
			{
				TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> WeakSurface = AsShared();
				Impl->RetirementTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSurface](float)
				{
					TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Pinned = WeakSurface.Pin();
					if (!Pinned || Pinned->Impl->bClosed) { return false; }
					Pinned->Impl->CollectRetired();
					const bool bHasPollableFence = Pinned->Impl->Retired.ContainsByPredicate(
						[](const TUniquePtr<FImpl::FRetiredTexture>& Entry) { return Entry->GpuFence.IsValid(); });
					if (!bHasPollableFence)
					{
						Pinned->Impl->RetirementTicker.Reset();
						return false;
					}
					return true;
				}), 0.25f);
			}
		}
		Impl->Output.Reset(Impl->Pending.Get());
		Impl->Pending.Reset();
		Impl->LastError.Reset();
		// Hold the surface through callbacks that can close it or drop their last reference.
		FAffineUiTextureReady Callback = Impl->OnTextureReady;
		Callback.ExecuteIfBound(Impl->Output.Get());
	}
	else
	{
		Impl->Pending.Reset();
		if (!bSuccess && Revision == Impl->DesiredRevision)
		{
			Impl->LastError = TEXT("AffineUI render submission failed; the previous texture was retained. See LogAffineUiSurfaceRender.");
			UE_LOG(LogAffineUiSurface, Error, TEXT("%s"), *Impl->LastError);
			FAffineUiSurfaceError Callback = Impl->OnError;
			const FString Error = Impl->LastError;
			Callback.ExecuteIfBound(Error);
		}
	}
	StartPending();
}

void FAffineUiSurface::Close()
{
	check(IsInGameThread());
	if (Impl->bClosed) { return; }
	Impl->bClosed = true;
	Impl->OnTextureReady.Unbind();
	Impl->OnError.Unbind();
	if (Impl->RetirementTicker.IsValid())
	{
		FTSTicker::RemoveTicker(Impl->RetirementTicker);
		Impl->RetirementTicker.Reset();
	}
	Impl->Renderer.Shutdown();
	Impl->Pending.Reset();
	Impl->Output.Reset();
	Impl->Retired.Reset();
	if (ActiveSurface.Pin().Get() == this) { ActiveSurface.Reset(); }
}
