#pragma once

#include "CoreMinimal.h"
#include "AffineUiSurface.h"
#include "RHIResources.h"
#include "Templates/Function.h"

#if PLATFORM_WINDOWS
struct ID3D11CommandList;
struct ID3D11DepthStencilView;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11Resource;
struct ID3D11Texture2D;
struct FNativeWebD3D12ImageInput;

struct FNativeWebD3D12BridgeSlot;
class FNativeWebD3D12BridgeImpl;
class FRHICommandList;

class FNativeWebD3D12Bridge final
{
public:
	struct FSlot
	{
		~FSlot();
		TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe> Native;
	};
	struct FInput
	{
		TSharedPtr<FNativeWebD3D12ImageInput, ESPMode::ThreadSafe> Native;
	};

	enum class EAcquireResult : uint8
	{
		Ready,
		Busy,
		Failure
	};

	static TSharedPtr<FNativeWebD3D12Bridge, ESPMode::ThreadSafe> Create(FIntPoint Size, FString& OutError);
	~FNativeWebD3D12Bridge();

	EAcquireResult AcquireSlot(FIntPoint Size, TSharedPtr<FSlot, ESPMode::ThreadSafe>& OutSlot,
	                           FString& OutError);

	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetImmediateContext() const;
	ID3D11DeviceContext* GetDeferredContext() const;
	ID3D11Resource* GetDestination(const FSlot& Slot) const;
	ID3D11Texture2D* GetColor(const FSlot& Slot) const;
	ID3D11RenderTargetView* GetColorView(const FSlot& Slot) const;
	ID3D11DepthStencilView* GetDepthView(const FSlot& Slot) const;

	bool SubmitProducer(FSlot& Slot, ID3D11CommandList* Commands, FString& OutError);
	void ReleaseSlot(FSlot& Slot);
	bool QueueCopy(const TSharedPtr<FSlot, ESPMode::ThreadSafe>& Slot, FTextureRHIRef Output,
	               TFunction<void(EAffineUiRenderResult)>&& Completion);
	bool CreateInput_RHI(FIntPoint Size, TSharedPtr<FInput, ESPMode::ThreadSafe>& OutInput, FString& OutError);
	bool CopyInput_RHI(FRHICommandList& Executing, const FTextureRHIRef& Normalized,
	                   const TSharedRef<FInput, ESPMode::ThreadSafe>& Input, FString& OutError);
	ID3D11Texture2D* GetInputTexture(const FInput& Input) const;
	uint64 GetInputAllocationBytes_RHI(FIntPoint Size) const;
	bool WaitInputsBeforeExecute_RHI(TConstArrayView<TSharedPtr<FInput, ESPMode::ThreadSafe>> Inputs,
	                                 FString& OutError);
	bool MarkInputsConsumed_RHI(TConstArrayView<TSharedPtr<FInput, ESPMode::ThreadSafe>> Inputs,
	                            FString& OutError);
	bool IsInputRetired_RHI(const FInput& Input) const;
	void PollRetirements_RHI();
	bool IsShutdownComplete_RHI() const;
	void Shutdown();
	explicit FNativeWebD3D12Bridge(TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> InImpl);

private:
	TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> Impl;
};
#endif
