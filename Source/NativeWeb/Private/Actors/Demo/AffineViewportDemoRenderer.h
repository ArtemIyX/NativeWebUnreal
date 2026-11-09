#pragma once

class FTextureRenderTargetResource;

// Owned by the actor; the implementation and all native renderer operations run on the RHI lane.
class FAffineViewportDemoRenderer
{
public:
	static FAffineViewportDemoRenderer* Start(FTextureRenderTargetResource* Target);
	static bool DiagnosticsEnabled();
	static void SampleLater(FAffineViewportDemoRenderer* Renderer, FTextureRenderTargetResource* Target);
	static void Stop(FAffineViewportDemoRenderer* Renderer);

private:
	class FState;
	FState* State = nullptr;
};
