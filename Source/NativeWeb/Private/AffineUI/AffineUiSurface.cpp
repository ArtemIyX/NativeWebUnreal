#include "AffineUiSurface.h"

#include "AffineUiSurfaceRenderer.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "NativeWeb.h"
#include "RenderCommandFence.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

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
		explicit FRetiredTexture(TStrongObjectPtr<UTextureRenderTarget2D>&& InTexture) : Texture(MoveTemp(InTexture))
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
				UE_LOG(LogNativeWebRender, Warning,
				       TEXT("Could not allocate a GPU retirement fence; keeping the old texture until Close."));
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
	FAffineUiClick OnClick;
	FAffineUiSurfaceRenderer Renderer;
	TSharedPtr<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe> FontAssets;
	TStrongObjectPtr<UTextureRenderTarget2D> Output;
	TStrongObjectPtr<UTextureRenderTarget2D> Pending;
	TArray<TUniquePtr<FRetiredTexture>> Retired;
	FTSTicker::FDelegateHandle RetirementTicker;
	FTSTicker::FDelegateHandle RetryTicker;
	FString LastError;
	uint64 DesiredRevision = 1;
	uint64 PendingRevision = 0;
	uint64 PendingDocumentEpoch = 0;
	uint64 OutputDocumentEpoch = 0;
	TMap<FString, FString> LiveTextUpdates;
	TArray<FAffineUiMutation> PendingMutations;
	uint64 DocumentEpoch = 1;
	bool bInFlight = false;
	bool bDirty = true;
	bool bClosed = false;
};

FAffineUiSurface::FAffineUiSurface() : Impl(MakeUnique<FImpl>())
{
}

TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> FAffineUiSurface::Create(
	const FAffineUiSurfaceDesc& Desc, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	UE_LOG(LogNativeWebAffineUi, Log, TEXT("Requesting surface %dx%d at DPI %.2f."),
	       Desc.PixelSize.X, Desc.PixelSize.Y, Desc.DpiScale);
#if !PLATFORM_WINDOWS
	OutError = TEXT("AffineUI surfaces require Win64 D3D11 or D3D12.");
	UE_LOG(LogNativeWebAffineUi, Error, TEXT("Surface creation rejected: %s"), *OutError);
	return nullptr;
#else
	if (!GDynamicRHI || (GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11 &&
		GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D12))
	{
		OutError = TEXT("AffineUI surfaces require the D3D11 or D3D12 RHI; launch with -dx11 or -dx12.");
		UE_LOG(LogNativeWebAffineUi, Error, TEXT("Surface creation rejected: %s"), *OutError);
		return nullptr;
	}
	if (ActiveSurface.IsValid())
	{
		OutError = TEXT("Only one AffineUI surface can be active in this process.");
		UE_LOG(LogNativeWebAffineUi, Error, TEXT("Surface capacity rejected creation: %s"), *OutError);
		return nullptr;
	}
	if (!IsValidDescriptor(Desc.PixelSize, Desc.DpiScale) || Desc.ClearColor.A != 255 ||
		ContainsEmbeddedNull(Desc.Content.Html) || ContainsEmbeddedNull(Desc.Content.Css))
	{
		OutError = TEXT("Invalid surface size, DPI scale, clear color alpha, or embedded null in content.");
		UE_LOG(LogNativeWebAffineUi, Error, TEXT("Surface creation rejected: %s"), *OutError);
		return nullptr;
	}
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface = MakeShareable(new FAffineUiSurface());
	Surface->Impl->Desired = Desc;
	TMap<FString, TArray<uint8>> PreloadedFonts;
	if (!Desc.FontDirectory.IsEmpty())
	{
		TArray<FString> FontFiles;
		IFileManager::Get().FindFiles(FontFiles, *FPaths::Combine(Desc.FontDirectory, TEXT("*.ttf")), true, false);
		TArray<FString> OpenTypeFiles;
		IFileManager::Get().FindFiles(OpenTypeFiles, *FPaths::Combine(Desc.FontDirectory, TEXT("*.otf")), true, false);
		FontFiles.Append(OpenTypeFiles);
		for (const FString& File : FontFiles)
		{
			TArray<uint8> Bytes;
			if (FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(Desc.FontDirectory, File)) && !Bytes.IsEmpty())
			{
				PreloadedFonts.Add(FString(TEXT("fonts/")) + File, MoveTemp(Bytes));
				UE_LOG(LogNativeWebAssets, VeryVerbose, TEXT("Preloaded font asset '%s'."), *File);
			}
			else
			{
				UE_LOG(LogNativeWebAssets, Warning, TEXT("Could not preload font asset '%s'."), *File);
			}
		}
	}
	Surface->Impl->FontAssets = MakeShared<const TMap<FString, TArray<uint8>>, ESPMode::ThreadSafe>(
		MoveTemp(PreloadedFonts));
	ActiveSurface = Surface;
	UE_LOG(LogNativeWebAffineUi, Display, TEXT("AffineUI surface created; preloaded %d font asset(s)."),
	       Surface->Impl->FontAssets->Num());
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
	if (Impl->bClosed || ContainsEmbeddedNull(Content.Html) || ContainsEmbeddedNull(Content.Css))
	{
		UE_LOG(LogNativeWebAffineUi, Warning, TEXT("Rejected content update (closed=%s html chars=%d css chars=%d)."),
		       Impl->bClosed ? TEXT("true") : TEXT("false"), Content.Html.Len(), Content.Css.Len());
		return false;
	}
	if (Impl->Desired.Content.Html != Content.Html)
	{
		++Impl->DocumentEpoch;
		Impl->PendingMutations.Reset();
		Impl->LiveTextUpdates.Reset();
	}
	Impl->Desired.Content = Content;
	++Impl->DesiredRevision;
	Impl->bDirty = true;
	UE_LOG(LogNativeWebAffineUi, Verbose,
	       TEXT("Content update queued (revision=%llu epoch=%llu html chars=%d css chars=%d)."),
	       static_cast<unsigned long long>(Impl->DesiredRevision),
	       static_cast<unsigned long long>(Impl->DocumentEpoch), Content.Html.Len(), Content.Css.Len());
	StartPending();
	return true;
}

bool FAffineUiSurface::LoadContentFromFiles(const FString& HtmlPath, const FString& CssPath, FString& OutError)
{
	check(IsInGameThread());
	OutError.Reset();
	if (Impl->bClosed)
	{
		OutError = TEXT("Surface is closed.");
		UE_LOG(LogNativeWebAssets, Warning, TEXT("File load rejected for a closed surface."));
		return false;
	}
	FAffineUiContent NewContent;
	if (!FFileHelper::LoadFileToString(NewContent.Html, *HtmlPath))
	{
		OutError = FString::Printf(TEXT("Could not read HTML file: %s"), *HtmlPath);
		UE_LOG(LogNativeWebAssets, Error, TEXT("%s"), *OutError);
		return false;
	}
	if (!FFileHelper::LoadFileToString(NewContent.Css, *CssPath))
	{
		OutError = FString::Printf(TEXT("Could not read CSS file: %s"), *CssPath);
		UE_LOG(LogNativeWebAssets, Error, TEXT("%s"), *OutError);
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
	if (Impl->bClosed || !IsValidDescriptor(PixelSize, DpiScale))
	{
		UE_LOG(LogNativeWebAffineUi, Warning, TEXT("Rejected surface resize to %dx%d at DPI %.2f (closed=%s)."),
		       PixelSize.X, PixelSize.Y, DpiScale, Impl->bClosed ? TEXT("true") : TEXT("false"));
		return false;
	}
	if (Impl->Desired.PixelSize == PixelSize && Impl->Desired.DpiScale == DpiScale) { return true; }
	Impl->Desired.PixelSize = PixelSize;
	Impl->Desired.DpiScale = DpiScale;
	++Impl->DesiredRevision;
	Impl->bDirty = true;
	UE_LOG(LogNativeWebAffineUi, Verbose, TEXT("Surface resize queued to %dx%d at DPI %.2f (revision=%llu)."),
	       PixelSize.X, PixelSize.Y, DpiScale, static_cast<unsigned long long>(Impl->DesiredRevision));
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

bool FAffineUiSurface::SetTextById(const FString& ElementId, const FString& Text)
{
	check(IsInGameThread());
	if (Impl->bClosed || ElementId.IsEmpty() || ContainsEmbeddedNull(ElementId) || ContainsEmbeddedNull(Text))
	{
		UE_LOG(LogNativeWebAffineUi, VeryVerbose, TEXT("Rejected text update (closed=%s, element id chars=%d)."),
		       Impl->bClosed ? TEXT("true") : TEXT("false"), ElementId.Len());
		return false;
	}
	Impl->LiveTextUpdates.Add(ElementId, Text);
	UE_LOG(LogNativeWebAffineUi, VeryVerbose, TEXT("Queued text update for element '%s' (%d chars)."),
	       *ElementId, Text.Len());
	return RequestRedraw();
}

bool FAffineUiSurface::QueueMutation(const FAffineUiMutation& Mutation)
{
	check(IsInGameThread());
	if (Impl->bClosed || Mutation.DocumentEpoch != Impl->DocumentEpoch ||
		ContainsEmbeddedNull(Mutation.Name) || ContainsEmbeddedNull(Mutation.Value))
	{
		UE_LOG(LogNativeWebAffineUi, VeryVerbose,
		       TEXT("Rejected mutation kind=%d (closed=%s epoch=%llu current=%llu)."),
		       static_cast<int32>(Mutation.Kind), Impl->bClosed ? TEXT("true") : TEXT("false"),
		       static_cast<unsigned long long>(Mutation.DocumentEpoch),
		       static_cast<unsigned long long>(Impl->DocumentEpoch));
		return false;
	}
	Impl->PendingMutations.Add(Mutation);
	UE_LOG(LogNativeWebAffineUi, VeryVerbose, TEXT("Queued DOM mutation kind=%d node=%llu."),
	       static_cast<int32>(Mutation.Kind), static_cast<unsigned long long>(Mutation.NodeId));
	return RequestRedraw();
}

uint64 FAffineUiSurface::GetDocumentEpoch() const
{
	check(IsInGameThread());
	return Impl->DocumentEpoch;
}

bool FAffineUiSurface::QueueInput(const FAffineUiInput& Input)
{
	check(IsInGameThread());
	if (Impl->bClosed || !Impl->Output.IsValid() || Impl->OutputDocumentEpoch != Impl->DocumentEpoch)
	{
		UE_LOG(LogNativeWebInput, VeryVerbose,
		       TEXT("Dropped surface input kind=%d (closed=%s output=%s outputEpoch=%llu currentEpoch=%llu)."),
		       static_cast<int32>(Input.Kind), Impl->bClosed ? TEXT("true") : TEXT("false"),
		       Impl->Output.IsValid() ? TEXT("true") : TEXT("false"),
		       static_cast<unsigned long long>(Impl->OutputDocumentEpoch),
		       static_cast<unsigned long long>(Impl->DocumentEpoch));
		return false;
	}
	UE_LOG(LogNativeWebInput, VeryVerbose, TEXT("Dispatching surface input kind=%d at (%.3f, %.3f)."),
	       static_cast<int32>(Input.Kind), Input.Position.X, Input.Position.Y);


	const FIntPoint Size(Impl->Output->SizeX, Impl->Output->SizeY);
	Impl->Renderer.DispatchInput(Input, Size, Impl->DocumentEpoch, AsShared());
	return true;
}

void FAffineUiSurface::SetOnClick(FAffineUiClick Callback)
{
	check(IsInGameThread());
	if (!Impl->bClosed) { Impl->OnClick = MoveTemp(Callback); }
}

void FAffineUiSurface::OnInputComplete(uint64 DocumentEpoch, TArray<uint64> Clicks, bool bNeedsRedraw)
{
	check(IsInGameThread());
	if (Impl->bClosed || DocumentEpoch != Impl->DocumentEpoch)
	{
		UE_LOG(LogNativeWebInput, VeryVerbose,
		       TEXT("Ignored stale input completion (callback epoch=%llu current=%llu)."),
		       static_cast<unsigned long long>(DocumentEpoch),
		       static_cast<unsigned long long>(Impl->DocumentEpoch));
		return;
	}
	for (uint64 NodeId : Clicks)
	{
		UE_LOG(LogNativeWebInput, VeryVerbose, TEXT("Surface click callback for node %llu."),
		       static_cast<unsigned long long>(NodeId));
		FAffineUiClick Callback = Impl->OnClick;
		Callback.ExecuteIfBound(NodeId);
		if (Impl->bClosed || DocumentEpoch != Impl->DocumentEpoch) { return; }
	}
	if (bNeedsRedraw) { RequestInputRedraw(); }
}

void FAffineUiSurface::RequestInputRedraw()
{
	check(IsInGameThread());
	if (Impl->bClosed) { return; }

	// Input can arrive faster than the renderer can publish frames. Keep the
	// in-flight revision publishable, then schedule one redraw for the latest
	// AffineUI state instead of invalidating it for every pointer move.
	if (!Impl->bDirty)
	{
		Impl->bDirty = true;
		if (!Impl->bInFlight) { ++Impl->DesiredRevision; }
	}
	StartPending();
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
	UE_LOG(LogNativeWebRender, VeryVerbose,
	       TEXT("Submitting surface render revision=%llu epoch=%llu size=%dx%d dpi=%.2f."),
	       static_cast<unsigned long long>(Impl->DesiredRevision),
	       static_cast<unsigned long long>(Impl->DocumentEpoch), Impl->Desired.PixelSize.X,
	       Impl->Desired.PixelSize.Y, Impl->Desired.DpiScale);
	Impl->CollectRetired();
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	if (!Target)
	{
		Impl->LastError = TEXT("Could not allocate the output render target.");
		UE_LOG(LogNativeWebRender, Error, TEXT("%s"), *Impl->LastError);
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
	Impl->PendingDocumentEpoch = Impl->DocumentEpoch;
	Impl->bInFlight = true;
	Impl->bDirty = false;
	TArray<FAffineUiTextMutation> TextMutations;
	TextMutations.Reserve(Impl->LiveTextUpdates.Num());
	for (const TPair<FString, FString>& Pair : Impl->LiveTextUpdates)
	{
		FAffineUiTextMutation& Mutation = TextMutations.AddDefaulted_GetRef();
		Mutation.ElementId = Pair.Key;
		Mutation.Text = Pair.Value;
	}
	TArray<FAffineUiMutation> Mutations = MoveTemp(Impl->PendingMutations);
	Impl->PendingMutations.Reset();
	Impl->Renderer.Draw(Target->GameThread_GetRenderTargetResource(), Impl->Desired.PixelSize,
	                    Impl->Desired.DpiScale, Impl->Desired.ClearColor, Impl->Desired.Content,
	                    Impl->FontAssets.ToSharedRef(), Impl->PendingRevision,
	                    MoveTemp(TextMutations), MoveTemp(Mutations), Impl->DocumentEpoch, AsShared());
}

void FAffineUiSurface::OnRenderComplete(uint64 Revision, EAffineUiRenderResult Result)
{
	check(IsInGameThread());
	if (Result == EAffineUiRenderResult::Deferred)
	{
		if (Impl->bClosed || !Impl->bInFlight || Revision != Impl->PendingRevision)
		{
			return;
		}
		Impl->bInFlight = false;
		Impl->bDirty = true;
		Impl->Pending.Reset();
		if (!Impl->RetryTicker.IsValid())
		{
			TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> WeakSurface = AsShared();
			Impl->RetryTicker = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([WeakSurface](float)
				{
					TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Pinned = WeakSurface.Pin();
					if (!Pinned || Pinned->Impl->bClosed) { return false; }
					Pinned->Impl->RetryTicker.Reset();
					Pinned->StartPending();
					return false;
				}), 0.01f);
		}
		return;
	}
	if (Impl->bClosed || !Impl->bInFlight || Revision != Impl->PendingRevision)
	{
		UE_LOG(LogNativeWebRender, VeryVerbose,
		       TEXT("Ignored stale render completion revision=%llu (closed=%s inFlight=%s pending=%llu)."),
		       static_cast<unsigned long long>(Revision), Impl->bClosed ? TEXT("true") : TEXT("false"),
		       Impl->bInFlight ? TEXT("true") : TEXT("false"),
		       static_cast<unsigned long long>(Impl->PendingRevision));
		return;
	}
	TSharedRef<FAffineUiSurface, ESPMode::ThreadSafe> KeepAlive = AsShared();
	(void)KeepAlive;
	Impl->bInFlight = false;
	const bool bSuccess = Result == EAffineUiRenderResult::Success;
	if (bSuccess && Revision == Impl->DesiredRevision)
	{
		TStrongObjectPtr<UTextureRenderTarget2D> PreviousOutput = MoveTemp(Impl->Output);
		Impl->Output.Reset(Impl->Pending.Get());
		Impl->OutputDocumentEpoch = Impl->PendingDocumentEpoch;
		Impl->Pending.Reset();
		Impl->LastError.Reset();
		UE_LOG(LogNativeWebRender, VeryVerbose, TEXT("Published render revision=%llu (texture=%dx%d)."),
		       static_cast<unsigned long long>(Revision), Impl->Output->SizeX, Impl->Output->SizeY);
		// Hold the surface through callbacks that can close it or drop their last reference.
		FAffineUiTextureReady Callback = Impl->OnTextureReady;
		Callback.ExecuteIfBound(Impl->Output.Get());
		// Queue retirement after presenter callbacks. Their render-thread texture
		// snapshots are therefore ordered before this fence, while UE's RHI refs
		// continue to own any already prepared Slate/RDG work.
		if (!Impl->bClosed && PreviousOutput.IsValid())
		{
			Impl->Retired.Add(MakeUnique<FImpl::FRetiredTexture>(MoveTemp(PreviousOutput)));
			if (!Impl->RetirementTicker.IsValid())
			{
				TWeakPtr<FAffineUiSurface, ESPMode::ThreadSafe> WeakSurface = AsShared();
				Impl->RetirementTicker = FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda([WeakSurface](float)
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
	}
	else
	{
		Impl->Pending.Reset();
		if (!bSuccess && Revision == Impl->DesiredRevision)
		{
			Impl->LastError = TEXT(
				"AffineUI render submission failed; the previous texture was retained. See LogNativeWebRender.");
			UE_LOG(LogNativeWebRender, Error, TEXT("%s"), *Impl->LastError);
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
	if (Impl->bClosed)
	{
		UE_LOG(LogNativeWebAffineUi, VeryVerbose, TEXT("Repeated surface close ignored."));
		return;
	}
	UE_LOG(LogNativeWebAffineUi, Display, TEXT("Closing AffineUI surface (revision=%llu epoch=%llu)."),
	       static_cast<unsigned long long>(Impl->DesiredRevision),
	       static_cast<unsigned long long>(Impl->DocumentEpoch));
	Impl->bClosed = true;
	Impl->OnTextureReady.Unbind();
	Impl->OnError.Unbind();
	Impl->OnClick.Unbind();
	if (Impl->RetirementTicker.IsValid())
	{
		FTSTicker::RemoveTicker(Impl->RetirementTicker);
		Impl->RetirementTicker.Reset();
	}
	if (Impl->RetryTicker.IsValid())
	{
		FTSTicker::RemoveTicker(Impl->RetryTicker);
		Impl->RetryTicker.Reset();
	}
	Impl->Renderer.Shutdown();
	Impl->LiveTextUpdates.Reset();
	Impl->PendingMutations.Reset();
	Impl->Pending.Reset();
	Impl->Output.Reset();
	Impl->Retired.Reset();
	if (ActiveSurface.Pin().Get() == this) { ActiveSurface.Reset(); }
	UE_LOG(LogNativeWebAffineUi, Verbose, TEXT("AffineUI surface closed."));
}
