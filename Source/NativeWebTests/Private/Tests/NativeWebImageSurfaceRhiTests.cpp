#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "AffineUI/AffineUiSurface.h"
#include "DynamicRHI.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "Images/NativeWebImageGpu.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "Subsystems/NativeWebImageSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FNativeWebImageSurfaceRhiTestState
	{
		FAutomationTestBase* Test = nullptr;
		TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface;
		TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> OtherSurface;
		TStrongObjectPtr<UGameInstance> GameInstances[2];
		TStrongObjectPtr<UNativeWebImageSubsystem> ImageSubsystems[2];
		TStrongObjectPtr<UTexture2D> SourceTexture;
		TStrongObjectPtr<UTexture2D> OtherSourceTexture;
		TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback;
		FString Url;
		FString InstallError;
		uint64 Epochs[2] = {};
		FString CacheKeys[2];
		int32 RenderSerial[2] = {};
		int32 TargetRenderSerial[2] = {};
		bool bInstalled[2] = {};
		bool bClosed[2] = {};
		bool bSawReleasedGpuCharge[2] = {};
		bool bSawRetiringGpuCharge[2] = {};
		bool bRetirementHoldEnabled[2] = {};
		bool bRetirementHoldReleased[2] = {};
		int32 RetirementHoldSamples[2] = {};
		bool bSubsystemDeinitialized[2] = {};
		bool bBudgetAsserted = false;
		bool bSurfacesClosed = false;
		bool bRenderedAfterInstall[2] = {};
		bool bFailed = false;
		int32 SurfaceIndex = 0;
		bool bSecondPixelVerified = false;
		int32 LastObservedRenderSerial = 0;
		double LastAnimationFrameSeconds = 0.0;
		FCriticalSection ReadbackMutex;
		bool bReadbackCopyRecorded = false;
		bool bReadbackComplete = false;
		bool bReadbackPollQueued = false;
		bool bReadbackQueued = false;
		FColor ReadbackSamples[6] = {};
		FString ReadbackError;
		double StartSeconds = 0.0;
	};

	TArray<TSharedPtr<FNativeWebImageSurfaceRhiTestState, ESPMode::ThreadSafe>> GQuarantinedReadbacks;

	class FNativeWebImageSurfaceRhiLatentCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FNativeWebImageSurfaceRhiLatentCommand(TSharedRef<FNativeWebImageSurfaceRhiTestState, ESPMode::ThreadSafe> InState)
			: State(MoveTemp(InState))
		{
		}

		virtual bool Update() override
		{
			ObserveGpuUsage();
			if (State->bFailed)
			{
				if (!State->InstallError.IsEmpty()) State->Test->AddError(State->InstallError);
				return Finish();
			}
			if (FPlatformTime::Seconds() - State->StartSeconds > 20.0)
			{
				State->Test->AddError(TEXT("Timed out waiting for both NativeWeb image installs, redraws, or GPU readbacks."));
				for (int32 Index = 0; Index < 2; ++Index)
					if (State->bClosed[Index] && State->RetirementHoldSamples[Index] < 2)
						State->Test->AddError(TEXT("The real GPU marker was not held across two retirement-accounting samples."));
				bool bReadbackPending = false;
				{
					FScopeLock Lock(&State->ReadbackMutex);
					bReadbackPending = State->bReadbackQueued && !State->bReadbackComplete;
				}
				if (bReadbackPending)
				{
					// Keep the resources captured by queued render work alive if a device
					// never completes the copy. Render callbacks never use Test.
					ReleaseRetirementHolds();
					for (int32 Index = 0; Index < 2; ++Index)
						if (State->bClosed[Index] && State->RetirementHoldSamples[Index] < 2)
							State->Test->AddError(TEXT("Retirement test hold did not remain charged across two game-thread samples."));
					State->Test = nullptr;
					CloseSurface(0);
					CloseSurface(1);
					DeinitializeServices();
					State->SourceTexture.Reset();
					State->OtherSourceTexture.Reset();
					GQuarantinedReadbacks.Add(State);
					return true;
				}
				return Finish();
			}
			if (State->bSecondPixelVerified)
			{
				if (State->bSurfacesClosed)
				{
					uint64 Active[2] = {}, Retiring[2] = {}, Cached[2] = {};
					for (int32 Index = 0; Index < 2; ++Index)
						State->ImageSubsystems[Index]->GetGpuCacheUsage(Active[Index], Retiring[Index], Cached[Index]);
					if (Active[0] == 0 && Retiring[0] == 0 && Cached[0] == 0 &&
						Active[1] == 0 && Retiring[1] == 0 && Cached[1] == 0)
					{
						for (int32 Index = 0; Index < 2; ++Index)
						{
							State->Test->TestTrue(TEXT("closed surface transitions its GPU charge out of active use"),
								State->bSawReleasedGpuCharge[Index]);
							State->Test->TestTrue(TEXT("closed surface eventually releases all GPU cache accounting"),
								Active[Index] + Retiring[Index] + Cached[Index] == 0);
							State->Test->TestTrue(TEXT("retiring charge was held across two game-thread samples"),
								State->bSawRetiringGpuCharge[Index] && State->RetirementHoldSamples[Index] >= 2);
							State->Test->TestTrue(TEXT("test gate was released before normal marker-gated cleanup"),
								State->bRetirementHoldReleased[Index]);
						}
						DeinitializeServices();
						return Finish();
					}
					return false;
				}
				const int32 CurrentRenderSerial = State->RenderSerial[1];
				if (CurrentRenderSerial != State->LastObservedRenderSerial)
				{
					State->LastObservedRenderSerial = CurrentRenderSerial;
					State->LastAnimationFrameSeconds = FPlatformTime::Seconds();
				}
				if (FPlatformTime::Seconds() - State->LastAnimationFrameSeconds < 0.6) return false;
				State->Test->TestTrue(TEXT("finite transform animation advances the survivor surface"),
					CurrentRenderSerial >= State->TargetRenderSerial[1] + 2);
				State->Test->TestEqual(TEXT("finite transform animation stops scheduling idle frames"),
					CurrentRenderSerial, State->LastObservedRenderSerial);
				CloseSurface(1);
				State->bSurfacesClosed = true;
				State->LastAnimationFrameSeconds = FPlatformTime::Seconds();
				return false;
			}
			if (!State->bInstalled[0] || !State->bInstalled[1] || !State->bRenderedAfterInstall[State->SurfaceIndex])
				return false;
			if (!State->bBudgetAsserted)
			{
				for (int32 Index = 0; Index < 2; ++Index)
				{
					uint64 Active = 0, Retiring = 0, Cached = 0;
					State->ImageSubsystems[Index]->GetGpuCacheUsage(Active, Retiring, Cached);
					State->Test->TestTrue(TEXT("normalized image allocation is charged to its game-instance budget"), Active > 0);
					State->Test->TestEqual(TEXT("newly installed image has no retiring charge"), Retiring, static_cast<uint64>(0));
					State->bRetirementHoldEnabled[Index] =
						SetImageGpuRetirementTestHold(State->CacheKeys[Index], true);
					State->Test->TestTrue(TEXT("test retirement hold is installed for the image cache entry"),
						State->bRetirementHoldEnabled[Index]);
				}
				State->bBudgetAsserted = true;
			}
			bool bReadbackQueued = false;
			bool bCopyRecorded = false;
			bool bReadbackComplete = false;
			bool bPollQueued = false;
			{
				FScopeLock Lock(&State->ReadbackMutex);
				bReadbackQueued = State->bReadbackQueued;
				bCopyRecorded = State->bReadbackCopyRecorded;
				bReadbackComplete = State->bReadbackComplete;
				bPollQueued = State->bReadbackPollQueued;
			}
			if (!bReadbackQueued)
			{
				TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> CurrentSurface =
					State->SurfaceIndex == 0 ? State->Surface : State->OtherSurface;
				UTextureRenderTarget2D* Output = CurrentSurface.IsValid() ? CurrentSurface->GetOutputTexture() : nullptr;
				FTextureRenderTargetResource* Resource = Output ? Output->GameThread_GetRenderTargetResource() : nullptr;
				State->Readback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("NativeWebImagePixelTest"));
				const TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = State->Readback;
				{
					FScopeLock Lock(&State->ReadbackMutex);
					State->bReadbackQueued = true;
				}
				ENQUEUE_RENDER_COMMAND(NativeWebImagePixelTestReadback)(
					[State = this->State, Readback, Resource](FRHICommandListImmediate& RHICmdList)
					{
						const FTextureRHIRef OutputRhi = Resource ? Resource->GetRenderTargetTexture() : FTextureRHIRef();
						if (!OutputRhi.IsValid())
						{
							FScopeLock Lock(&State->ReadbackMutex);
							State->ReadbackError = TEXT("The surface did not publish an RHI output texture after redraw.");
							State->bReadbackCopyRecorded = true;
							State->bReadbackComplete = true;
							State->bReadbackPollQueued = false;
							return;
						}
						RHICmdList.Transition(FRHITransitionInfo(OutputRhi.GetReference(), ERHIAccess::SRVMask,
							ERHIAccess::CopySrc));
						Readback->EnqueueCopy(RHICmdList, OutputRhi.GetReference(), FIntVector::ZeroValue, 0,
							FIntVector(16, 16, 1));
						RHICmdList.Transition(FRHITransitionInfo(OutputRhi.GetReference(), ERHIAccess::CopySrc,
							ERHIAccess::SRVMask));
						FScopeLock Lock(&State->ReadbackMutex);
						State->bReadbackCopyRecorded = true;
					});
				return false;
			}
			if (!bCopyRecorded) return false;
			if (!bReadbackComplete && !bPollQueued)
			{
				{
					FScopeLock Lock(&State->ReadbackMutex);
					State->bReadbackPollQueued = true;
				}
				const TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = State->Readback;
				ENQUEUE_RENDER_COMMAND(NativeWebImagePixelTestPollReadback)(
					[State = this->State, Readback](FRHICommandListImmediate&)
					{
						if (!Readback->IsReady())
						{
							FScopeLock Lock(&State->ReadbackMutex);
							State->bReadbackPollQueued = false;
							return;
						}
						int32 RowPitchPixels = 0;
						const FColor* Pixels = static_cast<const FColor*>(Readback->Lock(RowPitchPixels));
						FString ReadbackError;
						FColor Pixel = FColor::Transparent;
						if (!Pixels || RowPitchPixels < 16)
						{
							ReadbackError = TEXT("GPU readback returned no pixels or an invalid row pitch.");
						}
						else
						{
							State->ReadbackSamples[0] = Pixels[1 * RowPitchPixels + 1];
							State->ReadbackSamples[1] = Pixels[1 * RowPitchPixels + 3];
							State->ReadbackSamples[2] = Pixels[2 * RowPitchPixels + 2];
							State->ReadbackSamples[3] = Pixels[0 * RowPitchPixels + 8];
							State->ReadbackSamples[4] = Pixels[0 * RowPitchPixels + 10];
							State->ReadbackSamples[5] = Pixels[6 * RowPitchPixels + 6];
						}
						if (Pixels) Readback->Unlock();
						FScopeLock Lock(&State->ReadbackMutex);
						State->ReadbackError = MoveTemp(ReadbackError);
						State->bReadbackComplete = true;
						State->bReadbackPollQueued = false;
					});
				return false;
			}
			if (bReadbackComplete)
			{
				FColor Samples[6] = {};
				FString ReadbackError;
				{
					FScopeLock Lock(&State->ReadbackMutex);
					FMemory::Memcpy(Samples, State->ReadbackSamples, sizeof(Samples));
					ReadbackError = State->ReadbackError;
				}
				if (!ReadbackError.IsEmpty())
					State->Test->AddError(ReadbackError);
				else
				{
					const bool bRedSurface = State->SurfaceIndex == 0;
					const FString SurfaceLabel = bRedSurface ? TEXT("first red surface") : TEXT("surviving blue surface");
					const bool bCorrectImageChannels = bRedSurface
						? Samples[0].R > 200 && Samples[0].G < 40 && Samples[0].B < 40
						: Samples[0].B > 200 && Samples[0].R < 40 && Samples[0].G < 40;
					const bool bCorrectIntrinsicChannels = bRedSurface
						? Samples[3].R > 200 && Samples[3].G < 40 && Samples[3].B < 40
						: Samples[3].B > 200 && Samples[3].R < 40 && Samples[3].G < 40;
					State->Test->TestTrue(*FString::Printf(TEXT("%s preserves image color at the clipped layer"), *SurfaceLabel),
						bCorrectImageChannels);
					State->Test->TestTrue(*FString::Printf(TEXT("%s clips the oversized image at its viewport"), *SurfaceLabel),
						Samples[1].R < 20 && Samples[1].G < 20 && Samples[1].B < 20);
					State->Test->TestTrue(*FString::Printf(TEXT("%s preserves later overlay stacking"), *SurfaceLabel),
						Samples[2].G > 200 && Samples[2].R < 40 && Samples[2].B < 40);
					State->Test->TestTrue(*FString::Printf(TEXT("%s uses the image intrinsic size"), *SurfaceLabel),
						bCorrectIntrinsicChannels && Samples[4].R < 20 && Samples[4].G < 20 && Samples[4].B < 20);
					State->Test->TestEqual(*FString::Printf(TEXT("%s preserves opaque alpha"), *SurfaceLabel),
						Samples[0].A, static_cast<uint8>(255));
					State->Test->TestEqual(*FString::Printf(TEXT("%s reads the configured clear alpha"), *SurfaceLabel),
						Samples[1].A, static_cast<uint8>(bRedSurface ? 0 : 255));
					const int32 ExpectedHalfAlpha = bRedSurface ? 128 : 255;
					State->Test->TestTrue(*FString::Printf(TEXT("%s stores half-white source-over RGB near 128"), *SurfaceLabel),
						FMath::Abs(static_cast<int32>(Samples[5].R) - 128) <= 12 &&
						FMath::Abs(static_cast<int32>(Samples[5].G) - 128) <= 12 &&
						FMath::Abs(static_cast<int32>(Samples[5].B) - 128) <= 12);
					State->Test->TestTrue(*FString::Printf(TEXT("%s stores expected half-white source-over alpha"), *SurfaceLabel),
						FMath::Abs(static_cast<int32>(Samples[5].A) - ExpectedHalfAlpha) <= 12);
				}
				if (State->SurfaceIndex == 0)
				{
					CloseSurface(0);
					State->SurfaceIndex = 1;
					State->Readback.Reset();
					FScopeLock Lock(&State->ReadbackMutex);
					State->bReadbackCopyRecorded = false;
					State->bReadbackComplete = false;
					State->bReadbackPollQueued = false;
					State->bReadbackQueued = false;
					State->ReadbackError.Reset();
					FMemory::Memzero(State->ReadbackSamples, sizeof(State->ReadbackSamples));
					return false;
				}
				State->bSecondPixelVerified = true;
				State->LastObservedRenderSerial = State->RenderSerial[1];
				State->LastAnimationFrameSeconds = FPlatformTime::Seconds();
				return false;
			}
			return false;
		}

	private:
		void CloseSurface(int32 Index)
		{
			TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe>& Surface = Index == 0 ? State->Surface : State->OtherSurface;
			if (Surface.IsValid())
			{
				Surface->RemoveImage(State->Epochs[Index], 1, State->Url);
				Surface->Close();
				Surface.Reset();
				State->bClosed[Index] = true;
			}
		}

		void ObserveGpuUsage()
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (!State->ImageSubsystems[Index].IsValid()) continue;
				uint64 Active = 0, Retiring = 0, Cached = 0;
				State->ImageSubsystems[Index]->GetGpuCacheUsage(Active, Retiring, Cached);
				if (State->bClosed[Index] && Active == 0 && (Retiring > 0 || Cached > 0))
					State->bSawReleasedGpuCharge[Index] = true;
				if (State->bClosed[Index] && State->bRetirementHoldEnabled[Index] &&
					!State->bRetirementHoldReleased[Index] && Retiring > 0 &&
					DidImageGpuRetirementTestObserveReadyMarker(State->CacheKeys[Index]))
				{
					State->bSawRetiringGpuCharge[Index] = true;
					State->Test->TestEqual(TEXT("retiring allocation has moved entirely out of active use"),
						Active, static_cast<uint64>(0));
					State->Test->TestEqual(TEXT("retiring and idle cached categories do not overlap"),
						Cached, static_cast<uint64>(0));
					State->Test->TestTrue(TEXT("retiring bytes remain positive while the test hold is active"),
						Retiring > 0 && State->bRetirementHoldEnabled[Index]);
					State->Test->TestTrue(TEXT("production GPU marker reached retirement polling while the test hold was active"),
						DidImageGpuRetirementTestObserveReadyMarker(State->CacheKeys[Index]));
					State->Test->TestTrue(TEXT("retiring allocation stays within its game-instance GPU budget"),
						Retiring <= State->ImageSubsystems[Index]->GetMaxGpuBytes());
					++State->RetirementHoldSamples[Index];
					if (State->RetirementHoldSamples[Index] >= 2)
					{
						State->bRetirementHoldReleased[Index] =
							SetImageGpuRetirementTestHold(State->CacheKeys[Index], false);
						State->bRetirementHoldEnabled[Index] = false;
						State->Test->TestTrue(TEXT("test retirement hold releases after two observed samples"),
							State->bRetirementHoldReleased[Index]);
					}
				}
			}
		}

		bool Finish()
		{
			ReleaseRetirementHolds();
			CloseSurface(0);
			CloseSurface(1);
			State->Readback.Reset();
			State->SourceTexture.Reset();
			State->OtherSourceTexture.Reset();
			DeinitializeServices();
			return true;
		}

		void ReleaseRetirementHolds()
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (State->bRetirementHoldEnabled[Index])
				{
					(void)SetImageGpuRetirementTestHold(State->CacheKeys[Index], false);
					State->bRetirementHoldEnabled[Index] = false;
				}
			}
		}

		void DeinitializeServices()
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (State->ImageSubsystems[Index].IsValid() && !State->bSubsystemDeinitialized[Index])
				{
					State->ImageSubsystems[Index]->Deinitialize();
					State->bSubsystemDeinitialized[Index] = true;
				}
			}
		}

		TSharedRef<FNativeWebImageSurfaceRhiTestState, ESPMode::ThreadSafe> State;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebImageSurfaceRhiTest, "NativeWeb.Images.SurfaceRHI.InstallReadbackReleaseClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebImageSurfaceRhiTest::RunTest(const FString&)
{
	if (!GDynamicRHI || (GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11 &&
		GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D12))
	{
		AddWarning(TEXT("This image readback test requires an isolated D3D11 or D3D12 editor session."));
		return true;
	}
	auto CreateSolidTexture = [](FColor Color) -> UTexture2D*
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(2, 2, PF_B8G8R8A8);
		if (!Texture) return nullptr;
		Texture->SRGB = true;
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		FColor* SourcePixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		if (!SourcePixels)
		{
			Mip.BulkData.Unlock();
			return nullptr;
		}
		for (int32 Index = 0; Index < 4; ++Index) SourcePixels[Index] = Color;
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	};
	UTexture2D* RedTexture = CreateSolidTexture(FColor(255, 0, 0, 255));
	UTexture2D* BlueTexture = CreateSolidTexture(FColor(0, 0, 255, 255));
	if (!TestNotNull(TEXT("red transient deterministic image is created"), RedTexture) ||
		!TestNotNull(TEXT("blue transient deterministic image is created"), BlueTexture)) return false;
	FlushRenderingCommands();

	const FString SharedUrl = TEXT("nativeweb-image://1/1");
	const FString LayeredHtml = FString::Printf(
		TEXT("<html><body style='margin:0;background:transparent'><div style='position:absolute;left:0;top:0;width:3px;height:3px;overflow:hidden'>"
		     "<img src='%s' style='display:block;width:5px;height:5px'><div style='position:absolute;left:2px;top:2px;width:1px;height:1px;background:#00ff00'></div></div>"
		     "<img src='%s' style='position:absolute;left:8px;top:0px'><div id='motion' style='position:absolute;left:12px;top:12px;width:2px;height:2px;background:#fff'></div>"
		     "<div style='position:absolute;left:6px;top:6px;width:2px;height:2px;background:rgba(255,255,255,0.5)'></div></body></html>"),
		*SharedUrl, *SharedUrl);
	FAffineUiSurfaceDesc Desc;
	Desc.PixelSize = FIntPoint(16, 16);
	Desc.Content.Html = LayeredHtml;
	FAffineUiSurfaceDesc OtherDesc = Desc;
	OtherDesc.ClearColor = FColor::Black;
	OtherDesc.Content.Css = TEXT(
		"#motion{animation:nativeweb-finite-move 0.4s linear 1 forwards}"
		"@keyframes nativeweb-finite-move{from{transform:translateX(0px)}to{transform:translateX(2px)}}");
	FString Error;
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface = FAffineUiSurface::Create(Desc, Error);
	if (!TestTrue(TEXT("first Affine surface is created on the active RHI"), Surface.IsValid()))
	{
		AddError(Error);
		return false;
	}
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> OtherSurface = FAffineUiSurface::Create(OtherDesc, Error);
	if (!TestTrue(TEXT("second independent Affine surface is created concurrently"), OtherSurface.IsValid()))
	{
		AddError(Error);
		Surface->Close();
		return false;
	}

	TSharedRef<FNativeWebImageSurfaceRhiTestState, ESPMode::ThreadSafe> State =
		MakeShared<FNativeWebImageSurfaceRhiTestState, ESPMode::ThreadSafe>();
	State->Test = this;
	State->Surface = Surface;
	State->OtherSurface = OtherSurface;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		State->GameInstances[Index] = TStrongObjectPtr<UGameInstance>(NewObject<UGameInstance>(GetTransientPackage()));
		State->ImageSubsystems[Index] = TStrongObjectPtr<UNativeWebImageSubsystem>(
			NewObject<UNativeWebImageSubsystem>(State->GameInstances[Index].Get()));
		FObjectSubsystemCollection<UGameInstanceSubsystem> Collection;
		State->ImageSubsystems[Index]->Initialize(Collection);
		State->ImageSubsystems[Index]->SetCacheLimits(128, 32, 0.25f);
		State->ImageSubsystems[Index]->SetGpuCacheBudgetBytes(8ull * 1024 * 1024);
	}
	State->SourceTexture = TStrongObjectPtr<UTexture2D>(RedTexture);
	State->OtherSourceTexture = TStrongObjectPtr<UTexture2D>(BlueTexture);
	State->Epochs[0] = Surface->GetDocumentEpoch();
	State->Epochs[1] = OtherSurface->GetDocumentEpoch();
	State->Url = SharedUrl;
	State->StartSeconds = FPlatformTime::Seconds();
	auto BindReady = [State](const TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe>& InSurface, int32 Index)
	{
		InSurface->SetOnTextureReady(FAffineUiTextureReady::CreateLambda([State, Index](UTextureRenderTarget2D*)
		{
			++State->RenderSerial[Index];
			if (State->bInstalled[Index] && State->RenderSerial[Index] >= State->TargetRenderSerial[Index])
				State->bRenderedAfterInstall[Index] = true;
		}));
	};
	BindReady(Surface, 0);
	BindReady(OtherSurface, 1);
	auto Install = [State](const TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe>& TargetSurface,
		UTexture2D* Texture, int32 Index) -> bool
	{
		const FString BudgetNamespace = State->ImageSubsystems[Index]->GetGpuBudgetNamespace();
		const FString CacheKey = State->ImageSubsystems[Index]->MakeCacheKey(TEXT("test"), TEXT("same-image"));
		State->CacheKeys[Index] = CacheKey;
		return TargetSurface->InstallImage(State->Epochs[Index], 1, State->Url, CacheKey, BudgetNamespace,
			State->ImageSubsystems[Index]->GetMaxGpuBytes(), Texture,
			[State, Index](bool bSuccess, FIntPoint, const FString& InstallError)
			{
				if (!bSuccess)
				{
					State->InstallError = FString::Printf(TEXT("Surface %d image install failed: %s"), Index, *InstallError);
					State->bFailed = true;
					return;
				}
				State->bInstalled[Index] = true;
				State->TargetRenderSerial[Index] = State->RenderSerial[Index] + 1;
			});
	};
	if (!Install(Surface, RedTexture, 0) || !Install(OtherSurface, BlueTexture, 1))
	{
		Surface->Close();
		OtherSurface->Close();
		for (int32 Index = 0; Index < 2; ++Index) State->ImageSubsystems[Index]->Deinitialize();
		AddError(TEXT("A surface rejected a native image install request."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FNativeWebImageSurfaceRhiLatentCommand(State));
	return true;
}

#endif
