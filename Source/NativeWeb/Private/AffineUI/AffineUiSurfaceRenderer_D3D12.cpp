#include "AffineUiSurfaceRenderer_D3D12.h"

#include "Async/Async.h"
#include "DynamicRHI.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "NativeWeb.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Templates/Function.h"

#if PLATFORM_WINDOWS
#include "ID3D11DynamicRHI.h"
#include "ID3D12DynamicRHI.h"
#include "d3d11_4.h"
#include "d3d12.h"
#include "dxgi1_6.h"
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
#pragma pop_macro("FW_NORMAL")
#pragma pop_macro("FW_LIGHT")
#pragma pop_macro("FW_THIN")
#pragma pop_macro("small")
#pragma pop_macro("check")

struct FNativeWebD3D12BridgeSlot
{
	TRefCountPtr<ID3D12Resource> Transfer12;
	TRefCountPtr<ID3D11Texture2D> Transfer11;
	TRefCountPtr<ID3D11Texture2D> Color;
	TRefCountPtr<ID3D11RenderTargetView> ColorView;
	TRefCountPtr<ID3D11Texture2D> Depth;
	TRefCountPtr<ID3D11DepthStencilView> DepthView;
	FGPUFenceRHIRef UnrealCopyFence;
	uint64 ProducerValue = 0;
	bool bBusy = false;
};

class FNativeWebD3D12BridgeImpl final : public TSharedFromThis<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe>
{
public:
	enum class EAcquireResult : uint8
	{
		Ready,
		Busy,
		Failure
	};

	static TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> Create(FIntPoint Size, FString& OutError)
	{
		TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> Result = MakeShared<
			FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe>();
		if (!Result->Initialize(Size, OutError)) return nullptr;
		return Result;
	}

	~FNativeWebD3D12BridgeImpl()
	{
		bShuttingDown.Store(true);
		Slots.Reset();
		RetiredSlots.Reset();
	}

	EAcquireResult AcquireSlot(FIntPoint Size, TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& OutSlot,
	                           FString& OutError)
	{
		OutSlot.Reset();
		if (bShuttingDown.Load())
		{
			OutError = TEXT("The NativeWeb D3D12 bridge is shutting down.");
			return EAcquireResult::Failure;
		}
		PollCompletedSlots(Slots);
		CollectRetiredSlots();
		if (Size != CurrentSize)
		{
			UE_LOG(LogNativeWebRender, Display,
			       TEXT("D3D12 transfer size changed from %dx%d to %dx%d; retiring the old slot generation."),
			       CurrentSize.X, CurrentSize.Y, Size.X, Size.Y);
			for (TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& Slot : Slots)
			{
				RetiredSlots.Add(MoveTemp(Slot));
			}
			Slots.Reset();
			if (!CreateSlots(Size, OutError)) return EAcquireResult::Failure;
		}
		for (const TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& Slot : Slots)
		{
			if (!Slot->bBusy)
			{
				Slot->bBusy = true;
				OutSlot = Slot;
				return EAcquireResult::Ready;
			}
		}
		return EAcquireResult::Busy;
	}

	ID3D11Device* GetDevice() const { return Device11.GetReference(); }
	ID3D11DeviceContext* GetImmediateContext() const { return Immediate11.GetReference(); }
	ID3D11DeviceContext* GetDeferredContext() const { return Deferred11.GetReference(); }

	bool SubmitProducer(FNativeWebD3D12BridgeSlot& Slot, ID3D11CommandList* Commands, FString& OutError)
	{
		if (!Commands || bShuttingDown.Load())
		{
			Slot.bBusy = false;
			OutError = TEXT("The D3D11 producer command list is unavailable.");
			return false;
		}
		Immediate11->ExecuteCommandList(Commands, true);
		const uint64 Value = ++NextProducerValue;
		const HRESULT SignalResult = ImmediateContext4->Signal(ReadyFence11.GetReference(), Value);
		if (FAILED(SignalResult))
		{
			OutError = FString::Printf(TEXT("D3D11 producer fence signal failed (HRESULT 0x%08x)."),
			                           static_cast<uint32>(SignalResult));
			Slot.bBusy = false;
			return false;
		}
		Slot.ProducerValue = Value;
		Immediate11->Flush();
		const HRESULT DeviceRemovedReason = Device11->GetDeviceRemovedReason();
		if (FAILED(DeviceRemovedReason))
		{
			OutError = FString::Printf(TEXT("The private D3D11 producer device was removed (HRESULT 0x%08x)."),
			                           static_cast<uint32>(DeviceRemovedReason));
			Slot.bBusy = false;
			return false;
		}
		return true;
	}

	bool QueueCopy(const TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& Slot, FTextureRHIRef Output,
	               TFunction<void(EAffineUiRenderResult)>&& Completion)
	{
		if (!Slot.IsValid() || !Output.IsValid() || bShuttingDown.Load())
		{
			if (Slot.IsValid()) Slot->bBusy = false;
			return false;
		}

		const TSharedRef<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> Self = AsShared();
		const FTextureRHIRef TransitionTexture = Output;
		Async(EAsyncExecution::ThreadPool,
		      [Self, Slot, Output = MoveTemp(Output), TransitionTexture,
			      Completion = MoveTemp(Completion)]() mutable
		      {
			      const double StartTime = FPlatformTime::Seconds();
			      for (;;)
			      {
				      if (Self->bShuttingDown.Load()) return;
				      const HRESULT Device11Reason = Self->Device11->GetDeviceRemovedReason();
				      const HRESULT Device12Reason = Self->Device12 ? Self->Device12->GetDeviceRemovedReason() : S_OK;
				      if (FAILED(Device11Reason) || FAILED(Device12Reason) ||
					      (FPlatformTime::Seconds() - StartTime) > 5.0)
				      {
					      ENQUEUE_RENDER_COMMAND(NativeWebD3D12ProducerFailure)(
						      [Self, Slot, Completion = MoveTemp(Completion)](FRHICommandListImmediate&) mutable
						      {
							      Slot->bBusy = false;
							      Completion(EAffineUiRenderResult::Failure);
						      });
					      return;
				      }
				      if (Self->ReadyFence12->GetCompletedValue() >= Slot->ProducerValue) break;
				      FPlatformProcess::SleepNoStats(0.001f);
			      }

			      ENQUEUE_RENDER_COMMAND(NativeWebD3D12Copy)(
				      [Self, Slot, Output = MoveTemp(Output), TransitionTexture,
					      Completion = MoveTemp(Completion)](
				      FRHICommandListImmediate& RHICmdList) mutable
				      {
					      RHICmdList.Transition(FRHITransitionInfo(TransitionTexture.GetReference(),
					                                               ERHIAccess::Unknown,
					                                               ERHIAccess::CopyDest));
					      RHICmdList.EnqueueLambda(
						      [Self, Slot, Output = MoveTemp(Output), Completion = MoveTemp(Completion)](
						      FRHICommandListBase& ExecutingCmdList) mutable
						      {
							      Self->CopyOnExecutingCommandList(ExecutingCmdList, Slot, Output,
							                                       MoveTemp(Completion));
						      });
					      RHICmdList.Transition(FRHITransitionInfo(TransitionTexture.GetReference(),
					                                               ERHIAccess::CopyDest,
					                                               ERHIAccess::SRVMask));
				      });
		      });
		return true;
	}

	void Shutdown()
	{
		if (bShuttingDown.Load()) return;
		bShuttingDown.Store(true);
		const TSharedRef<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> Self = AsShared();
		Async(EAsyncExecution::ThreadPool, [Self]()
		{
			const double StartTime = FPlatformTime::Seconds();
			for (;;)
			{
				FNativeWebD3D12BridgeImpl::PollCompletedSlots(Self->Slots);
				Self->CollectRetiredSlots();
				bool bAllRetired = true;
				for (const TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& Slot : Self->Slots)
				{
					if (Slot->bBusy)
					{
						bAllRetired = false;
						break;
					}
				}
				bAllRetired = bAllRetired && Self->RetiredSlots.IsEmpty();
				if (bAllRetired || (FPlatformTime::Seconds() - StartTime) > 5.0) break;
				FPlatformProcess::SleepNoStats(0.001f);
			}
			Self->Slots.Reset();
			Self->RetiredSlots.Reset();
		});
	}

private:
	static void PollCompletedSlots(TArray<TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>>& InSlots)
	{
		for (const TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& Slot : InSlots)
		{
			if (Slot->bBusy && Slot->UnrealCopyFence && Slot->UnrealCopyFence->Poll())
			{
				Slot->UnrealCopyFence.SafeRelease();
				Slot->bBusy = false;
			}
		}
	}

	void CollectRetiredSlots()
	{
		RetiredSlots.RemoveAll([](const TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& Slot)
		{
			if (!Slot->bBusy) return true;
			if (Slot->UnrealCopyFence && Slot->UnrealCopyFence->Poll())
			{
				Slot->UnrealCopyFence.SafeRelease();
				Slot->bBusy = false;
				return true;
			}
			return false;
		});
	}

	bool Initialize(FIntPoint Size, FString& OutError)
	{
		if (!IsRHID3D12())
		{
			OutError = TEXT("NativeWeb D3D12 bridge initialization requires the D3D12 RHI.");
			return false;
		}
		ID3D12DynamicRHI* D3D12Rhi = GetID3D12DynamicRHI();
		Device12 = D3D12Rhi->RHIGetDevice(0);
		NodeMask = D3D12Rhi->RHIGetDeviceNodeMask(0);
		const TArray<FD3D12MinimalAdapterDesc> AdapterDescs = D3D12Rhi->RHIGetAdapterDescs();
		if (!Device12 || AdapterDescs.Num() <= 0 || NodeMask == 0 || (NodeMask & (NodeMask - 1)) != 0 ||
			AdapterDescs[0].NumDeviceNodes == 0 || (NodeMask >> AdapterDescs[0].NumDeviceNodes) != 0 ||
			!SameLuid(Device12->GetAdapterLuid(), AdapterDescs[0].Desc.AdapterLuid))
		{
			OutError = TEXT("The active UE D3D12 device has no valid single-node adapter identity.");
			return false;
		}

		TRefCountPtr<IDXGIFactory1> Factory;
		HRESULT Result = CreateDXGIFactory1(IID_PPV_ARGS(Factory.GetInitReference()));
		if (FAILED(Result)) return Fail(OutError, TEXT("CreateDXGIFactory1"), Result);
		for (UINT Index = 0;; ++Index)
		{
			TRefCountPtr<IDXGIAdapter1> Candidate;
			Result = Factory->EnumAdapters1(Index, Candidate.GetInitReference());
			if (Result == DXGI_ERROR_NOT_FOUND) break;
			if (FAILED(Result)) return Fail(OutError, TEXT("EnumAdapters1"), Result);
			DXGI_ADAPTER_DESC1 Desc{};
			Result = Candidate->GetDesc1(&Desc);
			if (FAILED(Result)) return Fail(OutError, TEXT("GetDesc1"), Result);
			if (!(Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && SameLuid(Desc.AdapterLuid, Device12->GetAdapterLuid()))
			{
				Adapter = Candidate;
				break;
			}
		}
		if (!Adapter)
		{
			OutError = TEXT("Could not match the UE D3D12 device LUID to a DXGI adapter.");
			return false;
		}

		D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
		Result = D3D11CreateDevice(Adapter.GetReference(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
		                           D3D11_CREATE_DEVICE_BGRA_SUPPORT, FeatureLevels,
		                           UE_ARRAY_COUNT(FeatureLevels), D3D11_SDK_VERSION,
		                           Device11.GetInitReference(), nullptr, Immediate11.GetInitReference());
		if (FAILED(Result)) return Fail(OutError, TEXT("D3D11CreateDevice"), Result);
		Result = Device11->QueryInterface(__uuidof(ID3D11Device5),
		                                  reinterpret_cast<void**>(Device11f.GetInitReference()));
		if (FAILED(Result)) return Fail(OutError, TEXT("QueryInterface ID3D11Device5"), Result);
		Result = Immediate11->QueryInterface(__uuidof(ID3D11DeviceContext4),
		                                     reinterpret_cast<void**>(ImmediateContext4.GetInitReference()));
		if (FAILED(Result)) return Fail(OutError, TEXT("QueryInterface ID3D11DeviceContext4"), Result);
		Result = Device11->CreateDeferredContext(0, Deferred11.GetInitReference());
		if (FAILED(Result)) return Fail(OutError, TEXT("CreateDeferredContext"), Result);

		Result = Device12->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(ReadyFence12.GetInitReference()));
		if (FAILED(Result)) return Fail(OutError, TEXT("Create shared producer fence"), Result);
		FScopedHandle FenceHandle;
		Result = Device12->CreateSharedHandle(ReadyFence12.GetReference(), nullptr, GENERIC_ALL, nullptr,
		                                      &FenceHandle.Value);
		if (FAILED(Result)) return Fail(OutError, TEXT("Create shared producer fence handle"), Result);
		Result = Device11f->OpenSharedFence(FenceHandle.Value, IID_PPV_ARGS(ReadyFence11.GetInitReference()));
		if (FAILED(Result)) return Fail(OutError, TEXT("Open shared producer fence"), Result);
		return CreateSlots(Size, OutError);
	}

	bool CreateSlots(FIntPoint Size, FString& OutError)
	{
		CurrentSize = Size;
		D3D12_RESOURCE_DESC Desc{};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Width = Size.X;
		Desc.Height = Size.Y;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		Desc.SampleDesc.Count = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
		D3D12_HEAP_PROPERTIES Heap{};
		Heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		Heap.CreationNodeMask = NodeMask;
		Heap.VisibleNodeMask = NodeMask;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe> Slot = MakeShared<
				FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>();
			HRESULT Result = Device12->CreateCommittedResource(&Heap, D3D12_HEAP_FLAG_SHARED, &Desc,
			                                                   D3D12_RESOURCE_STATE_COMMON, nullptr,
			                                                   IID_PPV_ARGS(Slot->Transfer12.GetInitReference()));
			if (FAILED(Result)) return Fail(OutError, TEXT("Create shared transfer texture"), Result);
			FScopedHandle TextureHandle;
			Result = Device12->CreateSharedHandle(Slot->Transfer12.GetReference(), nullptr, GENERIC_ALL, nullptr,
			                                      &TextureHandle.Value);
			if (FAILED(Result)) return Fail(OutError, TEXT("Create shared transfer texture handle"), Result);
			Result = Device11f->OpenSharedResource1(TextureHandle.Value,
			                                        IID_PPV_ARGS(Slot->Transfer11.GetInitReference()));
			if (FAILED(Result)) return Fail(OutError, TEXT("Open shared transfer texture"), Result);

			D3D11_TEXTURE2D_DESC ColorDesc{};
			ColorDesc.Width = Size.X;
			ColorDesc.Height = Size.Y;
			ColorDesc.MipLevels = 1;
			ColorDesc.ArraySize = 1;
			ColorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			ColorDesc.SampleDesc.Count = 1;
			ColorDesc.Usage = D3D11_USAGE_DEFAULT;
			ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			Result = Device11->CreateTexture2D(&ColorDesc, nullptr, Slot->Color.GetInitReference());
			if (FAILED(Result)) return Fail(OutError, TEXT("Create private D3D11 color target"), Result);
			Result = Device11->CreateRenderTargetView(Slot->Color.GetReference(), nullptr,
			                                          Slot->ColorView.GetInitReference());
			if (FAILED(Result)) return Fail(OutError, TEXT("Create private D3D11 color view"), Result);
			D3D11_TEXTURE2D_DESC DepthDesc = ColorDesc;
			DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			Result = Device11->CreateTexture2D(&DepthDesc, nullptr, Slot->Depth.GetInitReference());
			if (FAILED(Result)) return Fail(OutError, TEXT("Create private D3D11 depth target"), Result);
			Result = Device11->CreateDepthStencilView(Slot->Depth.GetReference(), nullptr,
			                                          Slot->DepthView.GetInitReference());
			if (FAILED(Result)) return Fail(OutError, TEXT("Create private D3D11 depth view"), Result);
			Slots.Add(MoveTemp(Slot));
		}
		return true;
	}

	static bool SameLuid(const LUID& A, const LUID& B)
	{
		return A.LowPart == B.LowPart && A.HighPart == B.HighPart;
	}

	static bool Fail(FString& OutError, const TCHAR* Operation, HRESULT Result)
	{
		OutError = FString::Printf(TEXT("%s failed (HRESULT 0x%08x)."), Operation, static_cast<uint32>(Result));
		return false;
	}

	void CopyOnExecutingCommandList(FRHICommandListBase& ExecutingCmdList,
	                                const TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>& Slot,
	                                const FTextureRHIRef& Output,
	                                TFunction<void(EAffineUiRenderResult)>&& Completion)
	{
		ID3D12DynamicRHI* D3D12Rhi = GetID3D12DynamicRHI();
		if (!D3D12Rhi || !Output.IsValid() || D3D12Rhi->RHIGetResourceDeviceIndex(Output.GetReference()) != 0)
		{
			Slot->bBusy = false;
			Completion(EAffineUiRenderResult::Failure);
			return;
		}
		const HRESULT DeviceRemovedBeforeCopy = Device12->GetDeviceRemovedReason();
		if (FAILED(DeviceRemovedBeforeCopy))
		{
			Slot->bBusy = false;
			Completion(EAffineUiRenderResult::Failure);
			return;
		}
		D3D12Rhi->RHIFlushResourceBarriers(ExecutingCmdList, 0);
		ID3D12GraphicsCommandList* CommandList = D3D12Rhi->RHIGetGraphicsCommandList(ExecutingCmdList, 0);
		ID3D12Resource* Destination = D3D12Rhi->RHIGetResource(Output.GetReference());
		if (!CommandList || !Destination || !Slot->Transfer12)
		{
			Slot->bBusy = false;
			Completion(EAffineUiRenderResult::Failure);
			return;
		}
		D3D12Rhi->RHIUpdateResourceResidency(ExecutingCmdList, 0, Output.GetReference());
		CommandList->CopyResource(Destination, Slot->Transfer12.GetReference());
		const HRESULT DeviceRemovedAfterCopy = Device12->GetDeviceRemovedReason();
		if (FAILED(DeviceRemovedAfterCopy))
		{
			Slot->bBusy = false;
			Completion(EAffineUiRenderResult::Failure);
			return;
		}
		Slot->UnrealCopyFence = RHICreateGPUFence(TEXT("NativeWebD3D12Copy"));
		if (!Slot->UnrealCopyFence)
		{
			UE_LOG(LogNativeWebRender, Error, TEXT("Could not allocate the UE GPU fence for a NativeWeb D3D12 copy."));
			Completion(EAffineUiRenderResult::Failure);
			return;
		}
		GDynamicRHI->RHIWriteGPUFence_TopOfPipe(ExecutingCmdList, Slot->UnrealCopyFence.GetReference());
		Completion(EAffineUiRenderResult::Success);
	}

	struct FScopedHandle
	{
		~FScopedHandle() { if (Value) CloseHandle(Value); }
		HANDLE Value = nullptr;
	};

	TRefCountPtr<IDXGIAdapter1> Adapter;
	TRefCountPtr<ID3D11Device> Device11;
	TRefCountPtr<ID3D11Device5> Device11f;
	TRefCountPtr<ID3D11DeviceContext> Immediate11;
	TRefCountPtr<ID3D11DeviceContext4> ImmediateContext4;
	TRefCountPtr<ID3D11DeviceContext> Deferred11;
	TRefCountPtr<ID3D11Fence> ReadyFence11;
	TRefCountPtr<ID3D12Fence> ReadyFence12;
	ID3D12Device* Device12 = nullptr;
	TArray<TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>> Slots;
	TArray<TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe>> RetiredSlots;
	FIntPoint CurrentSize = FIntPoint::ZeroValue;
	uint32 NodeMask = 0;
	uint64 NextProducerValue = 0;
	TAtomic<bool> bShuttingDown{false};
};


FNativeWebD3D12Bridge::FSlot::~FSlot() = default;

TSharedPtr<FNativeWebD3D12Bridge, ESPMode::ThreadSafe> FNativeWebD3D12Bridge::Create(
	FIntPoint Size, FString& OutError)
{
	TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> Impl =
		FNativeWebD3D12BridgeImpl::Create(Size, OutError);
	if (!Impl) return nullptr;
	return MakeShared<FNativeWebD3D12Bridge, ESPMode::ThreadSafe>(MoveTemp(Impl));
}

FNativeWebD3D12Bridge::FNativeWebD3D12Bridge(
	TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> InImpl)
	: Impl(MoveTemp(InImpl))
{
}

FNativeWebD3D12Bridge::~FNativeWebD3D12Bridge() = default;

FNativeWebD3D12Bridge::EAcquireResult FNativeWebD3D12Bridge::AcquireSlot(
	FIntPoint Size, TSharedPtr<FSlot, ESPMode::ThreadSafe>& OutSlot, FString& OutError)
{
	OutSlot.Reset();
	TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe> NativeSlot;
	const FNativeWebD3D12BridgeImpl::EAcquireResult NativeResult =
		Impl->AcquireSlot(Size, NativeSlot, OutError);
	if (NativeResult == FNativeWebD3D12BridgeImpl::EAcquireResult::Busy)
	{
		return EAcquireResult::Busy;
	}
	if (NativeResult == FNativeWebD3D12BridgeImpl::EAcquireResult::Failure)
	{
		return EAcquireResult::Failure;
	}
	OutSlot = MakeShared<FSlot, ESPMode::ThreadSafe>();
	OutSlot->Native = MoveTemp(NativeSlot);
	return EAcquireResult::Ready;
}

ID3D11Device* FNativeWebD3D12Bridge::GetDevice() const { return Impl->GetDevice(); }
ID3D11DeviceContext* FNativeWebD3D12Bridge::GetImmediateContext() const { return Impl->GetImmediateContext(); }
ID3D11DeviceContext* FNativeWebD3D12Bridge::GetDeferredContext() const { return Impl->GetDeferredContext(); }

ID3D11Resource* FNativeWebD3D12Bridge::GetDestination(const FSlot& Slot) const
{
	return Slot.Native.IsValid() ? Slot.Native->Transfer11.GetReference() : nullptr;
}

ID3D11Texture2D* FNativeWebD3D12Bridge::GetColor(const FSlot& Slot) const
{
	return Slot.Native.IsValid() ? Slot.Native->Color.GetReference() : nullptr;
}

ID3D11RenderTargetView* FNativeWebD3D12Bridge::GetColorView(const FSlot& Slot) const
{
	return Slot.Native.IsValid() ? Slot.Native->ColorView.GetReference() : nullptr;
}

ID3D11DepthStencilView* FNativeWebD3D12Bridge::GetDepthView(const FSlot& Slot) const
{
	return Slot.Native.IsValid() ? Slot.Native->DepthView.GetReference() : nullptr;
}

bool FNativeWebD3D12Bridge::SubmitProducer(FSlot& Slot, ID3D11CommandList* Commands, FString& OutError)
{
	return Slot.Native.IsValid() && Impl->SubmitProducer(*Slot.Native, Commands, OutError);
}

void FNativeWebD3D12Bridge::ReleaseSlot(FSlot& Slot)
{
	if (Slot.Native.IsValid()) Slot.Native->bBusy = false;
}

bool FNativeWebD3D12Bridge::QueueCopy(
	const TSharedPtr<FSlot, ESPMode::ThreadSafe>& Slot, FTextureRHIRef Output,
	TFunction<void(EAffineUiRenderResult)>&& Completion)
{
	return Slot.IsValid() && Slot->Native.IsValid() &&
		Impl->QueueCopy(Slot->Native, MoveTemp(Output), MoveTemp(Completion));
}

void FNativeWebD3D12Bridge::Shutdown()
{
	if (Impl) Impl->Shutdown();
}
#endif
