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

struct FNativeWebD3D12BridgeSlot;
class FNativeWebD3D12BridgeImpl;

class FNativeWebD3D12Bridge final
{
public:
	struct FSlot
	{
		~FSlot();
		TSharedPtr<FNativeWebD3D12BridgeSlot, ESPMode::ThreadSafe> Native;
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
	void Shutdown();
	explicit FNativeWebD3D12Bridge(TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> InImpl);

private:
	TSharedPtr<FNativeWebD3D12BridgeImpl, ESPMode::ThreadSafe> Impl;
};
#endif
