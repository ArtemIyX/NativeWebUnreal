#include "NativeWebGpuPresenter.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "DynamicRHI.h"
#include "GlobalShader.h"
#include "CommonRenderResources.h"
#include "InputCoreTypes.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "NativeWeb.h"
#include "NativeWebInputRouting.h"
#include "NativeWebView.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"
#include "Widgets/SViewport.h"

namespace
{
	TAutoConsoleVariable<int32> CVarForceGpuInitFailure(
		TEXT("NativeWeb.Presentation.ForceGpuInitFailure"), 0,
		TEXT("When nonzero, NativeWeb refuses GPU presenter initialization to exercise Slate fallback."), ECVF_Cheat);

	struct FNativeWebGpuRenderState
	{
		mutable FCriticalSection Mutex;
		FTextureRHIRef Texture;
		FString Failure;
		bool bEnabled = false;
		bool bReady = false;
	};

	void QueueOutputTextureSnapshot(
		const TSharedRef<FNativeWebGpuRenderState, ESPMode::ThreadSafe>& State,
		FTextureRenderTargetResource* Resource)
	{
		ENQUEUE_RENDER_COMMAND(NativeWebSnapshotOutputTexture)(
			[State, Resource](FRHICommandListImmediate&)
			{
				FTextureRHIRef Texture = Resource ? Resource->GetRenderTargetTexture() : FTextureRHIRef();
				FScopeLock Lock(&State->Mutex);
				State->Texture = MoveTemp(Texture);
			});
	}

	class FNativeWebViewportVS final : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNativeWebViewportVS);
		SHADER_USE_PARAMETER_STRUCT(FNativeWebViewportVS, FGlobalShader);

	public:
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	class FNativeWebViewportPS final : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNativeWebViewportPS);
		SHADER_USE_PARAMETER_STRUCT(FNativeWebViewportPS, FGlobalShader);

	public:
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, WebTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, WebSampler)
			SHADER_PARAMETER(FVector4f, ViewRect)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FNativeWebViewportVS, "/Plugin/NativeWeb/Private/NativeWebViewport.usf", "MainVS",
	                        SF_Vertex);
	IMPLEMENT_GLOBAL_SHADER(FNativeWebViewportPS, "/Plugin/NativeWeb/Private/NativeWebViewport.usf", "MainPS",
	                        SF_Pixel);

	void AddNativeWebPass(FRDGBuilder& GraphBuilder, const FSceneView& View,
	                      const FTextureRHIRef& SceneRHI, const FTextureRHIRef& WebRHI)
	{
		const FIntRect FinalViewRect = View.UnscaledViewRect;
		if (!SceneRHI.IsValid() || !WebRHI.IsValid() || FinalViewRect.IsEmpty()) return;

		FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(SceneRHI, TEXT("NativeWeb.SceneOutput")));
		FRDGTextureRef WebTexture = GraphBuilder.RegisterExternalTexture(
			CreateRenderTarget(WebRHI, TEXT("NativeWeb.Web")));
		FNativeWebViewportPS::FParameters* Parameters = GraphBuilder.AllocParameters<
			FNativeWebViewportPS::FParameters>();
		Parameters->WebTexture = WebTexture;
		Parameters->WebSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->ViewRect = FVector4f(static_cast<float>(FinalViewRect.Min.X),
		                                 static_cast<float>(FinalViewRect.Min.Y),
		                                 static_cast<float>(FinalViewRect.Width()),
		                                 static_cast<float>(FinalViewRect.Height()));
		Parameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);

		const FIntRect DrawRect = FinalViewRect;
		GraphBuilder.AddPass(RDG_EVENT_NAME("NativeWeb GPU Viewport"), Parameters, ERDGPassFlags::Raster,
		                     [Parameters, DrawRect](FRHICommandList& RHICmdList)
		                     {
			                     TShaderMapRef<FNativeWebViewportVS> VertexShader(
				                     GetGlobalShaderMap(GMaxRHIFeatureLevel));
			                     TShaderMapRef<FNativeWebViewportPS> PixelShader(
				                     GetGlobalShaderMap(GMaxRHIFeatureLevel));
			                     FGraphicsPipelineStateInitializer Pipeline;
			                     RHICmdList.ApplyCachedRenderTargets(Pipeline);
			                     Pipeline.BlendState = TStaticBlendState<>::GetRHI();
			                     Pipeline.RasterizerState = TStaticRasterizerState<>::GetRHI();
			                     Pipeline.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			                     Pipeline.PrimitiveType = PT_TriangleList;
			                     Pipeline.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.
				                     VertexDeclarationRHI;
			                     Pipeline.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			                     Pipeline.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			                     SetGraphicsPipelineState(RHICmdList, Pipeline, 0);
			                     SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(),
			                                         *Parameters);
			                     RHICmdList.SetViewport(static_cast<float>(DrawRect.Min.X),
			                                            static_cast<float>(DrawRect.Min.Y), 0.0f,
			                                            static_cast<float>(DrawRect.Max.X),
			                                            static_cast<float>(DrawRect.Max.Y), 1.0f);
			                     RHICmdList.SetScissorRect(true, DrawRect.Min.X, DrawRect.Min.Y, DrawRect.Max.X,
			                                               DrawRect.Max.Y);
			                     RHICmdList.DrawPrimitive(0, 1, 3);
			                     RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		                     });
	}
}

class FNativeWebGpuPresenter::FRenderExtension final : public FSceneViewExtensionBase
{
public:
	FRenderExtension(const FAutoRegister& AutoRegister, FViewport* InViewport, int32 InPlayerIndex,
	                 TSharedRef<FNativeWebGpuRenderState, ESPMode::ThreadSafe> InState)
		: FSceneViewExtensionBase(AutoRegister), TargetViewport(InViewport), PlayerIndex(InPlayerIndex),
		  State(MoveTemp(InState))
	{
	}

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		return Context.Viewport == TargetViewport && !Context.IsStereoSupported();
	}

	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View) override
	{
		if (View.PlayerIndex != PlayerIndex || !View.Family || !View.Family->RenderTarget) return;
		if (View.Family->RenderTarget->GetSceneHDREnabled())
		{
			FScopeLock Lock(&State->Mutex);
			if (State->Failure.IsEmpty())
				State->Failure =
					TEXT("The GPU presenter currently supports SDR output only.");
			return;
		}
		if (View.Family->RenderTarget->GetDisplayOutputFormat() != EDisplayOutputFormat::SDR_sRGB ||
			View.Family->RenderTarget->GetDisplayColorGamut() != EDisplayColorGamut::sRGB_D65)
		{
			FScopeLock Lock(&State->Mutex);
			if (State->Failure.IsEmpty())
				State->Failure = TEXT("The GPU presenter supports only sRGB-D65 SDR output; other SDR output devices use Slate fallback.");
			return;
		}
		const FTextureRHIRef SceneTexture = View.Family->RenderTarget->GetRenderTargetTexture();
		if (!SceneTexture.IsValid())
		{
			FScopeLock Lock(&State->Mutex);
			State->Failure = TEXT("The game scene view has no render target texture.");
			return;
		}
		const EPixelFormat SceneFormat = SceneTexture->GetFormat();
		if (!FNativeWebGpuPresenter::IsSupportedSceneTarget(SceneFormat,
			View.Family->RenderTarget->GetDisplayOutputFormat(),
			View.Family->RenderTarget->GetDisplayColorGamut()))
		{
			FScopeLock Lock(&State->Mutex);
			if (State->Failure.IsEmpty())
				State->Failure = FString::Printf(
					TEXT("The scene target format %s or SDR output encoding is unsupported; GPU presentation requires opaque sRGB SDR."),
					GPixelFormats[SceneFormat].Name);
			return;
		}
		const FRHITextureDesc& SceneDesc = SceneTexture->GetDesc();
		const FIntPoint SceneExtent(SceneDesc.Extent.X, SceneDesc.Extent.Y);
		const FIntRect ViewRect = View.UnscaledViewRect;
		if (SceneDesc.Dimension != ETextureDimension::Texture2D || SceneDesc.ArraySize != 1 ||
			SceneDesc.NumMips != 1 || SceneDesc.NumSamples != 1 ||
			!EnumHasAllFlags(SceneDesc.Flags, ETextureCreateFlags::RenderTargetable) ||
			!EnumHasAllFlags(SceneDesc.Flags, ETextureCreateFlags::ShaderResource) ||
			ViewRect.Min.X < 0 || ViewRect.Min.Y < 0 || ViewRect.Max.X > SceneExtent.X ||
			ViewRect.Max.Y > SceneExtent.Y)
		{
			FScopeLock Lock(&State->Mutex);
			if (State->Failure.IsEmpty())
				State->Failure = TEXT("The scene target is not a compatible single-sample 2D renderable attachment for GPU presentation.");
			return;
		}
		FTextureRHIRef WebTexture;
		{
			FScopeLock Lock(&State->Mutex);
			if (!State->Texture.IsValid()) return;
			WebTexture = State->Texture;
			const FRHITextureDesc& WebDesc = WebTexture->GetDesc();
			if (WebDesc.Format != PF_B8G8R8A8 || WebDesc.Dimension != ETextureDimension::Texture2D ||
				WebDesc.ArraySize != 1 || WebDesc.NumMips != 1 || WebDesc.NumSamples != 1 ||
				!EnumHasAllFlags(WebDesc.Flags, ETextureCreateFlags::ShaderResource) ||
				!EnumHasAnyFlags(WebDesc.Flags, ETextureCreateFlags::SRGB))
			{
				if (State->Failure.IsEmpty())
					State->Failure = TEXT("The AffineUI output is not a single-sample sRGB BGRA8 shader resource.");
				return;
			}
			if (!State->bEnabled)
			{
				State->bReady = true;
				return;
			}
		}
		AddNativeWebPass(GraphBuilder, View, SceneTexture, WebTexture);
	}

	void SetEnabled(bool bEnabled)
	{
		FScopeLock Lock(&State->Mutex);
		State->bEnabled = bEnabled;
	}

public:
	FViewport* TargetViewport = nullptr;
	int32 PlayerIndex = INDEX_NONE;
	TSharedRef<FNativeWebGpuRenderState, ESPMode::ThreadSafe> State;
};

class FNativeWebGpuPresenter::FInputAdapter final : public IInputProcessor
{
public:
	FInputAdapter(UNativeWebView* InView, ULocalPlayer* InPlayer, FViewport* InTargetViewport,
	              const TSharedPtr<SViewport>& InViewport,
	              TSharedRef<FNativeWebGpuRenderState, ESPMode::ThreadSafe> InState)
		: View(InView), Player(InPlayer), TargetViewport(InTargetViewport), ViewportWidget(InViewport),
		  State(MoveTemp(InState))
	{
	}

	virtual void Tick(const float, FSlateApplication& SlateApp, TSharedRef<ICursor>) override
	{
		if (!bEnabled) return;
		ULocalPlayer* PinnedPlayer = Player.Get();
		UGameViewportClient* ViewportClient = PinnedPlayer ? PinnedPlayer->ViewportClient : nullptr;
		const TSharedPtr<SViewport> CurrentWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
		if (!ViewportClient || ViewportClient->Viewport != TargetViewport || CurrentWidget != ViewportWidget.Pin())
		{
			FScopeLock Lock(&State->Mutex);
			if (State->Failure.IsEmpty()) State->Failure = TEXT("The owning game viewport was removed or replaced.");
			bEnabled = false;
			Lock.Unlock();
			ReleaseInput(SlateApp);
			return;
		}
		if (!SlateApp.IsActive())
		{
			ReleaseInput(SlateApp);
			return;
		}
		if (NativeWebInputRouting::IsConsoleActive(PinnedPlayer))
		{
			ReleaseInput(SlateApp);
			return;
		}
		UpdateSurfaceSize();
		if (!HasViewportFocus(SlateApp))
		{
			ReleaseInput(SlateApp);
		}
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		if (!bEnabled || NativeWebInputRouting::IsConsoleActive(Player.Get()) ||
			!IsTargetWindow(SlateApp, Event)) return false;
		if (bPointerPressed)
		{
			MakeInput(Event.GetScreenSpacePosition(), ENativeWebInputType::INPUT_POINTER_MOVE,
			          0.0f, false, SlateApp, true);
			return true;
		}
		const bool bInside = IsViewportHit(SlateApp, Event.GetScreenSpacePosition()) &&
			IsInsideView(Event.GetScreenSpacePosition(), SlateApp);
		if (bInside)
		{
			bPointerInside = MakeInput(Event.GetScreenSpacePosition(), ENativeWebInputType::INPUT_POINTER_MOVE,
			                           0.0f, false, SlateApp);
			return bPointerInside;
		}
		if (bPointerInside)
		{
			FNativeWebInputEvent Leave;
			Leave.Type = ENativeWebInputType::INPUT_POINTER_MOVE;
			Leave.NormalizedPosition = FVector2D(-1.0, -1.0);
			if (UNativeWebView* Pinned = View.Get()) Pinned->ForwardInput(Leave);
			bPointerInside = false;
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		if (!bEnabled || NativeWebInputRouting::IsConsoleActive(Player.Get()) ||
			Event.GetEffectingButton() != EKeys::LeftMouseButton ||
			!IsTargetWindow(SlateApp, Event) || !IsViewportHit(SlateApp, Event.GetScreenSpacePosition()))
			return false;
		if (!MakeInput(Event.GetScreenSpacePosition(), ENativeWebInputType::INPUT_POINTER_DOWN,
		               0.0f, Event.IsShiftDown(), SlateApp))
			return false;
		bPointerPressed = true;
		bPointerInside = true;
		if (TSharedPtr<SViewport> Widget = ViewportWidget.Pin())
		{
			SlateApp.SetKeyboardFocus(Widget, EFocusCause::Mouse);
			if (const TSharedPtr<SWindow> GameWindow = GetViewportWindow(SlateApp))
				SlateApp.GetPlatformApplication()->SetCapture(GameWindow->GetNativeWindow());
		}
		SetFocus(true);
		return true;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		return HandleMouseButtonDownEvent(SlateApp, Event);
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		if (!bEnabled || NativeWebInputRouting::IsConsoleActive(Player.Get()) || !bPointerPressed ||
			Event.GetEffectingButton() != EKeys::LeftMouseButton ||
			!IsTargetWindow(SlateApp, Event))
			return false;
		MakeInput(Event.GetScreenSpacePosition(), ENativeWebInputType::INPUT_POINTER_UP,
		          0.0f, Event.IsShiftDown(), SlateApp, true);
		bPointerPressed = false;
		bPointerInside = false;
		SlateApp.GetPlatformApplication()->SetCapture(nullptr);
		return true;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event,
	                                            const FPointerEvent*) override
	{
		if (!bEnabled || NativeWebInputRouting::IsConsoleActive(Player.Get()) ||
			!IsTargetWindow(SlateApp, Event) ||
			!IsViewportHit(SlateApp, Event.GetScreenSpacePosition()))
			return false;
		return MakeInput(Event.GetScreenSpacePosition(), ENativeWebInputType::INPUT_WHEEL,
		                 Event.GetWheelDelta(), Event.IsShiftDown(), SlateApp);
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		return HandleKey(SlateApp, Event, ENativeWebInputType::INPUT_KEY_DOWN);
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		return HandleKey(SlateApp, Event, ENativeWebInputType::INPUT_KEY_UP);
	}

	void SetVisible(bool bInVisible)
	{
		if (!bInVisible && FSlateApplication::IsInitialized()) ReleaseInput(FSlateApplication::Get());
		bEnabled = bInVisible;
	}

	void Shutdown(const TSharedPtr<FInputAdapter>& Self)
	{
		if (FSlateApplication::IsInitialized()) ReleaseInput(FSlateApplication::Get());
		bEnabled = false;
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Self);
	}

private:
	bool HandleKey(FSlateApplication& SlateApp, const FKeyEvent& Event, ENativeWebInputType Type)
	{
		if (!bEnabled || NativeWebInputRouting::IsConsoleActive(Player.Get()) || !bFocused ||
			!HasViewportFocus(SlateApp)) return false;
		FNativeWebInputEvent Input;
		Input.Type = Type;
		Input.bShift = Event.IsShiftDown();
		if (Event.GetKey() == EKeys::Tab) Input.Key = ENativeWebInputKey::INPUT_KEY_TAB;
		else if (Event.GetKey() == EKeys::Enter) Input.Key = ENativeWebInputKey::INPUT_KEY_ENTER;
		else if (Event.GetKey() == EKeys::SpaceBar) Input.Key = ENativeWebInputKey::INPUT_KEY_SPACE;
		else return false;
		if (UNativeWebView* Pinned = View.Get()) Pinned->ForwardInput(Input);
		return true;
	}

	TSharedPtr<SWindow> GetViewportWindow(FSlateApplication& SlateApp) const
	{
		const TSharedPtr<SViewport> Widget = ViewportWidget.Pin();
		return Widget.IsValid()
			                                       ? SlateApp.FindWidgetWindow(Widget.ToSharedRef())
			                                       : nullptr;
	}

	bool IsTargetWindow(FSlateApplication& SlateApp, const FInputEvent& Event) const
	{
		const TSharedPtr<SWindow> GameWindow = GetViewportWindow(SlateApp);
		if (!GameWindow.IsValid()) return false;

		// FInputEvent::GetWindow() unconditionally dereferences EventPath. Raw
		// platform mouse moves may not carry a path, so never call it directly.
		if (const FWidgetPath* EventPath = Event.GetEventPath())
		{
			if (!EventPath->IsValid()) return false;
			const TSharedRef<SWindow> EventWindow = EventPath->GetWindow();
			return &EventWindow.Get() == GameWindow.Get();
		}

		return SlateApp.GetActiveTopLevelWindow() == GameWindow;
	}

	bool IsViewportHit(FSlateApplication& SlateApp, const FVector2D& ScreenPosition) const
	{
		const TSharedPtr<SViewport> Widget = ViewportWidget.Pin();
		if (!Widget.IsValid()) return false;
		const TSharedPtr<SWindow> Window = SlateApp.FindWidgetWindow(Widget.ToSharedRef());
		if (!Window.IsValid()) return false;
		TArray<TSharedRef<SWindow>> Windows;
		Windows.Add(Window.ToSharedRef());
		const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(ScreenPosition, Windows, false, INDEX_NONE);
		return Path.IsValid() && Path.Widgets.Num() > 0 &&
			&Path.Widgets.Last().Widget.Get() == static_cast<SWidget*>(Widget.Get());
	}

	bool HasViewportFocus(FSlateApplication& SlateApp) const
	{
		const TSharedPtr<SWidget> Focused = SlateApp.GetKeyboardFocusedWidget();
		const TSharedPtr<SViewport> Widget = ViewportWidget.Pin();
		return bFocused && Focused.IsValid() && Widget.IsValid() && Focused == Widget;
	}

	bool GetViewRect(FIntRect& OutRect, FVector2D& OutViewportPixels, FGeometry* OutGeometry = nullptr) const
	{
		ULocalPlayer* PinnedPlayer = Player.Get();
		UGameViewportClient* ViewportClient = PinnedPlayer ? PinnedPlayer->ViewportClient : nullptr;
		if (!ViewportClient || !ViewportClient->Viewport || !ViewportWidget.IsValid()) return false;
		FSceneViewProjectionData ProjectionData;
		if (!PinnedPlayer->GetProjectionData(ViewportClient->Viewport, ProjectionData)) return false;
		OutRect = ProjectionData.GetConstrainedViewRect();
		const FIntPoint PixelSize = ViewportClient->Viewport->GetSizeXY();
		OutViewportPixels = FVector2D(PixelSize.X, PixelSize.Y);
		if (OutGeometry) *OutGeometry = ViewportWidget.Pin()->GetCachedGeometry();
		return !OutRect.IsEmpty() && PixelSize.X > 0 && PixelSize.Y > 0;
	}

	FVector2D ToViewportPixels(const FVector2D& ScreenPosition, FSlateApplication& SlateApp) const
	{
		FIntRect Rect;
		FVector2D PixelSize;
		FGeometry Geometry;
		if (!GetViewRect(Rect, PixelSize, &Geometry)) return FVector2D(-1.0, -1.0);
		const FVector2D Local = Geometry.AbsoluteToLocal(ScreenPosition);
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (LocalSize.X <= 0.0 || LocalSize.Y <= 0.0) return FVector2D(-1.0, -1.0);
		return FVector2D(Local.X * PixelSize.X / LocalSize.X, Local.Y * PixelSize.Y / LocalSize.Y);
	}

	bool IsInsideView(const FVector2D& ScreenPosition, FSlateApplication& SlateApp) const
	{
		FIntRect Rect;
		FVector2D PixelSize;
		if (!GetViewRect(Rect, PixelSize)) return false;
		const FVector2D Pixel = ToViewportPixels(ScreenPosition, SlateApp);
		return Pixel.X >= Rect.Min.X && Pixel.Y >= Rect.Min.Y && Pixel.X < Rect.Max.X && Pixel.Y < Rect.Max.Y;
	}

	bool MakeInput(const FVector2D& ScreenPosition, ENativeWebInputType Type, float Wheel, bool bShift,
	               FSlateApplication& SlateApp, bool bAllowOutside = false)
	{
		FIntRect Rect;
		FVector2D PixelSize;
		if (!GetViewRect(Rect, PixelSize)) return false;
		const FVector2D Pixel = ToViewportPixels(ScreenPosition, SlateApp);
		FVector2D Normalized;
		if (!FNativeWebGpuPresenter::ConvertViewportPoint(Pixel, Rect, Normalized, bAllowOutside)) return false;
		FNativeWebInputEvent Input;
		Input.Type = Type;
		Input.NormalizedPosition = Normalized;
		Input.WheelDelta = Wheel;
		Input.bShift = bShift;
		if (UNativeWebView* Pinned = View.Get()) return Pinned->ForwardInput(Input);
		return false;
	}

	void UpdateSurfaceSize()
	{
		FIntRect Rect;
		FVector2D PixelSize;
		FGeometry Geometry;
		if (!GetViewRect(Rect, PixelSize, &Geometry)) return;
		const float Fit = FMath::Min(1.0f, 4096.0f / static_cast<float>(FMath::Max(Rect.Width(), Rect.Height())));
		const FIntPoint Desired(FMath::Max(1, FMath::RoundToInt(Rect.Width() * Fit)),
		                        FMath::Max(1, FMath::RoundToInt(Rect.Height() * Fit)));
		const float LayoutScale = Geometry.GetAccumulatedLayoutTransform().GetScale();
		const float Dpi = FMath::Clamp(LayoutScale * Fit, 0.01f, 8.0f);
		if (Desired != LastSize || !FMath::IsNearlyEqual(Dpi, LastDpi))
		{
			if (UNativeWebView* Pinned = View.Get()) Pinned->Resize(Desired, Dpi);
			LastSize = Desired;
			LastDpi = Dpi;
		}
	}

	void SetFocus(bool bInFocused)
	{
		if (bFocused == bInFocused) return;
		bFocused = bInFocused;
		FNativeWebInputEvent Event;
		Event.Type = bFocused ? ENativeWebInputType::INPUT_FOCUS_GAINED : ENativeWebInputType::INPUT_FOCUS_LOST;
		if (UNativeWebView* Pinned = View.Get()) Pinned->ForwardInput(Event);
	}

	void ReleaseInput(FSlateApplication& SlateApp)
	{
		if (bPointerPressed)
		{
			FNativeWebInputEvent Event;
			Event.Type = ENativeWebInputType::INPUT_POINTER_UP;
			Event.NormalizedPosition = FVector2D(-1.0, -1.0);
			if (UNativeWebView* Pinned = View.Get()) Pinned->ForwardInput(Event);
			bPointerPressed = false;
			bPointerInside = false;
			SlateApp.GetPlatformApplication()->SetCapture(nullptr);
		}
		SetFocus(false);
		if (const TSharedPtr<SViewport> Widget = ViewportWidget.Pin();
			Widget.IsValid() && SlateApp.GetKeyboardFocusedWidget() == Widget)
		{
			SlateApp.ClearKeyboardFocus(EFocusCause::SetDirectly);
		}
	}

	TWeakObjectPtr<UNativeWebView> View;
	TWeakObjectPtr<ULocalPlayer> Player;
	FViewport* TargetViewport = nullptr;
	TWeakPtr<SViewport> ViewportWidget;
	TSharedRef<FNativeWebGpuRenderState, ESPMode::ThreadSafe> State;
	FIntPoint LastSize = FIntPoint::ZeroValue;
	float LastDpi = 0.0f;
	bool bEnabled = false;
	bool bFocused = false;
	bool bPointerPressed = false;
	bool bPointerInside = false;
};

FNativeWebGpuPresenter::FNativeWebGpuPresenter() = default;
FNativeWebGpuPresenter::~FNativeWebGpuPresenter() { Shutdown(); }

bool FNativeWebGpuPresenter::Initialize(UNativeWebView* View, ULocalPlayer* Player,
                                        UTextureRenderTarget2D* Texture, FString& OutReason)
{
	check(IsInGameThread());
	if (CVarForceGpuInitFailure.GetValueOnGameThread() != 0)
	{
		OutReason = TEXT("GPU initialization failure was requested by NativeWeb.Presentation.ForceGpuInitFailure.");
		return false;
	}
	if (!View || !Player)
	{
		OutReason = TEXT("The view, local player, or output texture is unavailable.");
		return false;
	}
	if (!GDynamicRHI || FCString::Stricmp(GDynamicRHI->GetName(), TEXT("D3D11")) != 0)
	{
		OutReason = TEXT("The GPU viewport presenter currently supports only D3D11.");
		return false;
	}
	UGameViewportClient* ViewportClient = Player->ViewportClient;
	TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
	if (!ViewportClient || !ViewportClient->Viewport || !ViewportWidget.IsValid())
	{
		OutReason = TEXT("The owning game viewport or Slate viewport widget is unavailable.");
		return false;
	}
	if (GEngine && GEngine->IsStereoscopic3D(ViewportClient->Viewport))
	{
		OutReason = TEXT("The GPU viewport presenter does not support stereo rendering.");
		return false;
	}
	if (!FSlateApplication::IsInitialized())
	{
		OutReason = TEXT("Slate input infrastructure is unavailable.");
		return false;
	}
	TSharedRef<FNativeWebGpuRenderState, ESPMode::ThreadSafe> State = MakeShared<
		FNativeWebGpuRenderState, ESPMode::ThreadSafe>();
	RenderExtension = FSceneViewExtensions::NewExtension<FRenderExtension>(ViewportClient->Viewport,
	                                                                       Player->GetLocalPlayerIndex(), State);
	FTextureRenderTargetResource* TextureResource = Texture ? Texture->GameThread_GetRenderTargetResource() : nullptr;
	QueueOutputTextureSnapshot(State, TextureResource);
	InputAdapter = MakeShared<FInputAdapter>(View, Player, ViewportClient->Viewport, ViewportWidget, State);
	if (!FSlateApplication::Get().RegisterInputPreProcessor(InputAdapter, EInputPreProcessorType::Game))
	{
		RenderExtension.Reset();
		InputAdapter.Reset();
		OutReason = TEXT("The game-priority Slate input preprocessor could not be registered.");
		return false;
	}
	UE_LOG(LogNativeWebRender, Display,
	       TEXT("GPU viewport presenter initialized for view '%s'; awaiting first render callback."), *View->GetName());
	return true;
}

void FNativeWebGpuPresenter::SetTexture(UTextureRenderTarget2D* Texture)
{
	check(IsInGameThread());
	if (!RenderExtension) return;
	FTextureRenderTargetResource* TextureResource = Texture ? Texture->GameThread_GetRenderTargetResource() : nullptr;
	QueueOutputTextureSnapshot(RenderExtension->State, TextureResource);
}

void FNativeWebGpuPresenter::Activate(bool bVisible)
{
	check(IsInGameThread());
	if (RenderExtension) RenderExtension->SetEnabled(bVisible);
	if (InputAdapter) InputAdapter->SetVisible(bVisible);
}

void FNativeWebGpuPresenter::SetVisible(bool bVisible)
{
	check(IsInGameThread());
	if (RenderExtension) RenderExtension->SetEnabled(bVisible);
	if (InputAdapter) InputAdapter->SetVisible(bVisible);
}

void FNativeWebGpuPresenter::Shutdown()
{
	const bool bHadPresenter = InputAdapter.IsValid() || RenderExtension.IsValid();
	if (InputAdapter)
	{
		InputAdapter->Shutdown(InputAdapter);
		InputAdapter.Reset();
	}
	if (RenderExtension)
	{
		RenderExtension->SetEnabled(false);
		{
			FScopeLock Lock(&RenderExtension->State->Mutex);
			RenderExtension->State->Texture.SafeRelease();
		}
		RenderExtension.Reset();
	}
	if (bHadPresenter) UE_LOG(LogNativeWebRender, Display, TEXT("GPU viewport render and input hooks detached."));
}

bool FNativeWebGpuPresenter::IsReady() const
{
	if (!RenderExtension) return false;
	FScopeLock Lock(&RenderExtension->State->Mutex);
	return RenderExtension->State->bReady;
}

bool FNativeWebGpuPresenter::GetFailure(FString& OutReason) const
{
	if (!RenderExtension) return false;
	FScopeLock Lock(&RenderExtension->State->Mutex);
	if (RenderExtension->State->Failure.IsEmpty()) return false;
	OutReason = RenderExtension->State->Failure;
	return true;
}
