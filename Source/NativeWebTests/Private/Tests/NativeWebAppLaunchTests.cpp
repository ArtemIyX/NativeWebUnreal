#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Assets/NativeWebAppAsset.h"
#include "Assets/NativeWebContentAssets.h"
#include "Async/NativeWebCreateAppViewAsyncAction.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "DynamicRHI.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "NativeWebView.h"
#include "Tests/NativeWebAppLaunchTestTypes.h"
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FNativeWebAppLaunchFixture
	{
		TStrongObjectPtr<ULocalPlayer> LocalPlayer;
		TStrongObjectPtr<UNativeWebLocalPlayerSubsystem> Subsystem;
		TStrongObjectPtr<UNativeWebAppAsset> AppAsset;
		TStrongObjectPtr<UNativeWebHtmlAsset> HtmlAsset;
		TStrongObjectPtr<UNativeWebCssAsset> CssAsset;
		bool bSubsystemDeinitialized = false;

		FNativeWebAppLaunchFixture()
		{
			if (!IsValid(GEngine)) return;
			LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
			if (LocalPlayer.IsValid())
			{
				Subsystem.Reset(NewObject<UNativeWebLocalPlayerSubsystem>(LocalPlayer.Get()));
			}
		}

		~FNativeWebAppLaunchFixture()
		{
			DeinitializeSubsystem();
		}

		bool IsReady() const
		{
			return IsValid(GEngine) && LocalPlayer.IsValid() && Subsystem.IsValid();
		}

		void DeinitializeSubsystem()
		{
			if (!bSubsystemDeinitialized && Subsystem.IsValid())
			{
				Subsystem->Deinitialize();
				bSubsystemDeinitialized = true;
			}
		}

		void ExpectThrowingScriptLogs(FAutomationTestBase& Test, const UNativeWebAppAsset& App, const TCHAR* ViewName) const
		{
			const FString SourceName = FString::Printf(TEXT("<app-id:%s:JavaScript>"), *App.AppId.ToString());
			Test.AddExpectedError(
				FString::Printf(TEXT("JavaScript evaluation failed in '%s': Error: app script failure."), *SourceName),
				EAutomationExpectedErrorFlags::Contains, 1, false);
			Test.AddExpectedError(
				FString::Printf(TEXT("Script '%s' failed in view '%s': Error: app script failure"), *SourceName, ViewName),
				EAutomationExpectedErrorFlags::Contains, 1, false);
		}

		UNativeWebAppAsset* MakeThrowingApp(bool bEvaluateAtStart)
		{
			AppAsset.Reset(NewObject<UNativeWebAppAsset>(GetTransientPackage()));
			UNativeWebAppAsset* App = AppAsset.Get();
			App->AppId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			App->HtmlMode = ENativeWebHtmlSourceMode::String;
			App->HtmlString = TEXT("<html><body><div id=\"status\"></div></body></html>");
			App->JavaScriptMode = ENativeWebOptionalSourceMode::String;
			App->JavaScriptString = TEXT("throw new Error('app script failure');");
			App->bEvaluateJavaScriptOnStart = bEvaluateAtStart;
			return App;
		}
	};

	struct FNativeWebAppAsyncCancelState
	{
		FNativeWebAppLaunchFixture Fixture;
		FAutomationTestBase* Test = nullptr;
		FName ViewName = TEXT("AsyncCancel");
		int32 CompletionCount = 0;
		FNativeWebError CompletionError;
	};

	struct FNativeWebAppPartialDependencyState
	{
		FNativeWebAppLaunchFixture Fixture;
		FAutomationTestBase* Test = nullptr;
		FName ViewName = NAME_None;
		FString MissingCssPath;
		FNativeWebSurfaceConfig Surface;
		FNativeWebPresentationConfig Presentation;
		TWeakObjectPtr<UNativeWebView> CompletedView;
		FNativeWebError CompletionError;
		TStrongObjectPtr<UNativeWebCssAsset> ValidCssAsset;
		int32 CompletionCount = 0;
		int32 Phase = 0;
		int32 SettleFrames = 0;
		double DeadlineSeconds = 0.0;

		void Launch(TSharedRef<FNativeWebAppPartialDependencyState, ESPMode::ThreadSafe> Self)
		{
			CompletionCount = 0;
			CompletedView.Reset();
			CompletionError = {};
			SettleFrames = 0;
			DeadlineSeconds = FPlatformTime::Seconds() + 10.0;
			const TWeakPtr<FNativeWebAppPartialDependencyState, ESPMode::ThreadSafe> WeakState(Self);
			Fixture.Subsystem->CreateAppViewAsync(TSoftObjectPtr<UNativeWebAppAsset>(Fixture.AppAsset.Get()), ViewName,
				Surface, Presentation, FNativeWebCreateAppViewComplete::CreateLambda(
					[WeakState](UNativeWebView* View, const FNativeWebError& Error)
					{
						if (const TSharedPtr<FNativeWebAppPartialDependencyState, ESPMode::ThreadSafe> State = WeakState.Pin())
						{
							++State->CompletionCount;
							State->CompletedView = View;
							State->CompletionError = Error;
						}
					}));
		}
	};

	class FNativeWebAppPartialDependencyLatentCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FNativeWebAppPartialDependencyLatentCommand(
			TSharedRef<FNativeWebAppPartialDependencyState, ESPMode::ThreadSafe> InState)
			: State(MoveTemp(InState))
		{
		}

		virtual bool Update() override
		{
			if (State->CompletionCount == 0)
			{
				if (FPlatformTime::Seconds() < State->DeadlineSeconds) return false;
				State->Test->TestTrue(TEXT("async app request completes before its 10-second deadline"), false);
				State->Fixture.DeinitializeSubsystem();
				return true;
			}
			if (State->SettleFrames++ == 0) return false;

			if (State->Phase == 0)
			{
				State->Test->TestEqual(TEXT("partial dependency failure completes exactly once"), State->CompletionCount, 1);
				State->Test->TestNull(TEXT("partial dependency failure publishes no view"), State->CompletedView.Get());
				State->Test->TestEqual(TEXT("partial CSS failure has source-specific code"),
					State->CompletionError.Code, FName(TEXT("CssAssetLoadFailed")));
				State->Test->TestEqual(TEXT("partial CSS failure reports the exact missing path"),
					State->CompletionError.SourceName, State->MissingCssPath);
				State->Test->TestEqual(TEXT("partial CSS failure retains the reserved view name"),
					State->CompletionError.ViewName, State->ViewName);
				State->Test->TestTrue(TEXT("partial CSS failure explains that the selected asset did not load"),
					State->CompletionError.Message.Contains(TEXT("did not load")));
				State->Test->TestNull(TEXT("partial failure leaves no registered view"),
					State->Fixture.Subsystem->FindView(State->ViewName));

				State->ValidCssAsset.Reset(NewObject<UNativeWebCssAsset>(GetTransientPackage()));
				const ANSICHAR CssBytes[] = "#status { color: #fff; }";
				TArray<uint8> CssPayload;
				CssPayload.Append(reinterpret_cast<const uint8*>(CssBytes), UE_ARRAY_COUNT(CssBytes) - 1);
				State->ValidCssAsset->SetImportedContent(MoveTemp(CssPayload), TEXT("retry.css"));
				UNativeWebAppAsset* App = State->Fixture.AppAsset.Get();
				App->CssAsset = TSoftObjectPtr<UNativeWebCssAsset>(State->ValidCssAsset.Get());
				State->Phase = 1;
				State->Launch(State);
				return false;
			}

			State->Test->TestEqual(TEXT("successful retry completes exactly once"), State->CompletionCount, 1);
			UNativeWebView* View = State->CompletedView.Get();
			State->Test->TestNotNull(TEXT("successful retry publishes a view"), View);
			State->Test->TestTrue(TEXT("successful retry returns no error code"), State->CompletionError.Code.IsNone());
			State->Test->TestTrue(TEXT("successful retry returns no error message"), State->CompletionError.Message.IsEmpty());
			State->Test->TestTrue(TEXT("successful retry registers the returned view under the same name"),
				View && State->Fixture.Subsystem->FindView(State->ViewName) == View);
			if (View)
			{
				FNativeWebError StartupCheckError;
				const bool bStartupScriptRan = View->EvaluateScript(
					TEXT("if (globalThis.__NativeWebCoverageStarted !== true) throw new Error('startup script did not run');"),
					TEXT("NativeWebCoverageStartupCheck"), StartupCheckError);
				State->Test->TestTrue(TEXT("successful retry evaluates the configured startup JavaScript"), bStartupScriptRan);
				State->Test->TestTrue(TEXT("startup JavaScript verification returns no error"), StartupCheckError.Code.IsNone());
				State->Test->TestTrue(TEXT("successful retry view is destroyed"), State->Fixture.Subsystem->DestroyView(State->ViewName));
				State->Test->TestNull(TEXT("destroyed retry view is absent from the map"),
					State->Fixture.Subsystem->FindView(State->ViewName));
			}
			return true;
		}

	private:
		TSharedRef<FNativeWebAppPartialDependencyState, ESPMode::ThreadSafe> State;
	};

	struct FNativeWebAppBlueprintProxyState
	{
		FNativeWebAppLaunchFixture Fixture;
		FAutomationTestBase* Test = nullptr;
		TStrongObjectPtr<UNativeWebAppLaunchTestReceiver> Receiver;
		TStrongObjectPtr<UNativeWebCreateAppViewAsyncAction> Action;
		FName ViewName = TEXT("BlueprintProxySuccess");
		double DeadlineSeconds = 0.0;
		int32 SettleFrames = 0;
	};

	class FNativeWebAppBlueprintProxyLatentCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FNativeWebAppBlueprintProxyLatentCommand(
			TSharedRef<FNativeWebAppBlueprintProxyState, ESPMode::ThreadSafe> InState)
			: State(MoveTemp(InState))
		{
		}

		virtual bool Update() override
		{
			if (State->Receiver->SuccessCount == 0)
			{
				if (State->Receiver->FailureCount > 0)
				{
					State->Test->TestEqual(TEXT("valid app proxy does not report Failure"), State->Receiver->FailureCount, 0);
					State->Fixture.DeinitializeSubsystem();
					return true;
				}
				if (FPlatformTime::Seconds() < State->DeadlineSeconds) return false;
				State->Test->TestTrue(TEXT("Blueprint async proxy completes before its 10-second deadline"), false);
				State->Fixture.DeinitializeSubsystem();
				return true;
			}
			if (State->SettleFrames++ == 0) return false;

			State->Test->TestEqual(TEXT("successful proxy broadcasts Success exactly once"), State->Receiver->SuccessCount, 1);
			State->Test->TestEqual(TEXT("successful proxy does not broadcast Failure"), State->Receiver->FailureCount, 0);
			UNativeWebView* View = State->Receiver->LastView.Get();
			State->Test->TestNotNull(TEXT("successful proxy supplies a view"), View);
			State->Test->TestTrue(TEXT("successful proxy supplies an empty error"), State->Receiver->LastError.Code.IsNone());
			State->Test->TestTrue(TEXT("successful proxy supplies no error message"), State->Receiver->LastError.Message.IsEmpty());
			State->Test->TestTrue(TEXT("successful proxy registers its view"),
				View && State->Fixture.Subsystem->FindView(State->ViewName) == View);
			State->Action->Activate();
			State->Test->TestEqual(TEXT("activating a completed proxy again does not rebroadcast"), State->Receiver->SuccessCount, 1);
			if (View)
			{
				State->Test->TestTrue(TEXT("successful proxy view is destroyed"), State->Fixture.Subsystem->DestroyView(State->ViewName));
				State->Test->TestNull(TEXT("destroyed proxy view is absent from the map"),
					State->Fixture.Subsystem->FindView(State->ViewName));
			}

			TStrongObjectPtr<UNativeWebAppLaunchTestReceiver> FailureReceiver(
				NewObject<UNativeWebAppLaunchTestReceiver>(GetTransientPackage()));
			TStrongObjectPtr<UNativeWebCreateAppViewAsyncAction> FailureAction(
				UNativeWebCreateAppViewAsyncAction::CreateAppViewAsync(nullptr, TSoftObjectPtr<UNativeWebAppAsset>(),
					TEXT("BlueprintProxyFailure"), FNativeWebSurfaceConfig(), FNativeWebPresentationConfig()));
			FailureAction->Success.AddDynamic(FailureReceiver.Get(), &UNativeWebAppLaunchTestReceiver::ReceiveSuccess);
			FailureAction->Failure.AddDynamic(FailureReceiver.Get(), &UNativeWebAppLaunchTestReceiver::ReceiveFailure);
			FailureAction->Activate();
			FailureAction->Activate();
			State->Test->TestEqual(TEXT("missing subsystem proxy broadcasts Failure exactly once"), FailureReceiver->FailureCount, 1);
			State->Test->TestEqual(TEXT("missing subsystem proxy does not broadcast Success"), FailureReceiver->SuccessCount, 0);
			State->Test->TestNull(TEXT("failed proxy supplies no view"), FailureReceiver->LastView.Get());
			State->Test->TestEqual(TEXT("failed proxy supplies MissingSubsystem"),
				FailureReceiver->LastError.Code, FName(TEXT("MissingSubsystem")));
			State->Test->TestEqual(TEXT("failed proxy retains the requested view name"),
				FailureReceiver->LastError.ViewName, FName(TEXT("BlueprintProxyFailure")));
			FailureAction->Activate();
			State->Test->TestEqual(TEXT("activating a failed proxy again does not rebroadcast"), FailureReceiver->FailureCount, 1);
			return true;
		}

	private:
		TSharedRef<FNativeWebAppBlueprintProxyState, ESPMode::ThreadSafe> State;
	};

	class FNativeWebAppAsyncCancelLatentCommand final : public IAutomationLatentCommand
	{
	public:
		FNativeWebAppAsyncCancelLatentCommand(TSharedRef<FNativeWebAppAsyncCancelState, ESPMode::ThreadSafe> InState,
		                                    bool bInWaitForDependencies)
			: State(MoveTemp(InState)), bWaitForDependencies(bInWaitForDependencies)
		{
		}

		virtual bool Update() override
		{
			if (!State->Fixture.Subsystem->IsAutomationLoadCompletionHeld(State->ViewName, bWaitForDependencies) &&
				++FramesWaited < 120) return false;
			State->Test->TestTrue(TEXT("test hold is reached before completion"),
				State->Fixture.Subsystem->IsAutomationLoadCompletionHeld(State->ViewName, bWaitForDependencies));
			State->Test->TestEqual(TEXT("held launch has not completed"), State->CompletionCount, 0);
			FNativeWebError DuplicateError;
			int32 DuplicateCompletionCount = 0;
			State->Fixture.Subsystem->CreateAppViewAsync(TSoftObjectPtr<UNativeWebAppAsset>(), State->ViewName, FNativeWebSurfaceConfig(),
				FNativeWebPresentationConfig(), FNativeWebCreateAppViewComplete::CreateLambda(
				[&](UNativeWebView*, const FNativeWebError& Error)
				{
					++DuplicateCompletionCount;
					DuplicateError = Error;
				}));
			State->Test->TestEqual(TEXT("duplicate in-flight name rejects synchronously exactly once"), DuplicateCompletionCount, 1);
			State->Test->TestEqual(TEXT("duplicate in-flight name reports NameInUse"), DuplicateError.Code, FName(TEXT("NameInUse")));
			State->Fixture.DeinitializeSubsystem();
			State->Test->TestEqual(TEXT("teardown dispatches the original completion once"), State->CompletionCount, 1);
			State->Test->TestEqual(TEXT("teardown reports structured cancellation"), State->CompletionError.Code, FName(TEXT("LaunchCancelled")));
			return true;
		}

	private:
		TSharedRef<FNativeWebAppAsyncCancelState, ESPMode::ThreadSafe> State;
		bool bWaitForDependencies = false;
		int32 FramesWaited = 0;
	};

	bool HasSupportedRhi()
	{
		return GDynamicRHI && (GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D11 ||
		                       GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppDelayedScriptAndFailureSourceTest,
	"NativeWeb.AppAsset.DelayedEvaluationAndScriptDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppDelayedScriptAndFailureSourceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!HasSupportedRhi())
	{
		AddWarning(TEXT("Requires a D3D11 or D3D12 RHI for the real app view fixture."));
		return true;
	}
	FNativeWebAppLaunchFixture Fixture;
	if (!Fixture.IsReady()) return TestTrue(TEXT("Engine-owned local player fixture is ready"), false);
	UNativeWebAppAsset* App = Fixture.MakeThrowingApp(false);
	FNativeWebSurfaceConfig Surface;
	Surface.PixelSize = FIntPoint(64, 64);
	FNativeWebPresentationConfig Presentation;
	Presentation.PresentationMode = ENativeWebPresentationMode::None;
	FNativeWebError Error;
	UNativeWebView* View = Fixture.Subsystem->CreateAppView(TEXT("Delayed"), App, Surface, Presentation, Error);
	TestNotNull(TEXT("App view publishes when startup evaluation is disabled"), View);
	if (!View) return false;
	Fixture.ExpectThrowingScriptLogs(*this, *App, TEXT("Delayed"));
	TestTrue(TEXT("A throwing script is retained without running during creation"), View->EvaluateAppScript(Error) == false);
	TestEqual(TEXT("Delayed evaluation preserves the structured script error"), Error.Code, FName(TEXT("ScriptError")));
	TestTrue(TEXT("Delayed error names the app identifier and JavaScript slot"),
	         Error.SourceName == FString::Printf(TEXT("<app-id:%s:JavaScript>"), *App->AppId.ToString()));
	TestTrue(TEXT("Delayed JavaScript error retains a script stack"), !Error.ScriptStack.IsEmpty());
	TestTrue(TEXT("Test closes the launched view"), Fixture.Subsystem->DestroyView(TEXT("Delayed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppStartupRollbackAndNameReuseTest,
	"NativeWeb.AppAsset.StartupFailureRollsBackAndReusesName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppStartupRollbackAndNameReuseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!HasSupportedRhi())
	{
		AddWarning(TEXT("Requires a D3D11 or D3D12 RHI for the real app view fixture."));
		return true;
	}
	FNativeWebAppLaunchFixture Fixture;
	if (!Fixture.IsReady()) return TestTrue(TEXT("Engine-owned local player fixture is ready"), false);
	UNativeWebAppAsset* App = Fixture.MakeThrowingApp(true);
	FNativeWebSurfaceConfig Surface;
	Surface.PixelSize = FIntPoint(64, 64);
	FNativeWebPresentationConfig Presentation;
	Presentation.PresentationMode = ENativeWebPresentationMode::None;
	FNativeWebError Error;
	Fixture.ExpectThrowingScriptLogs(*this, *App, TEXT("Reusable"));
	TestNull(TEXT("Startup JavaScript failure returns no published view"),
	         Fixture.Subsystem->CreateAppView(TEXT("Reusable"), App, Surface, Presentation, Error));
	TestEqual(TEXT("Startup failure propagates ScriptError"), Error.Code, FName(TEXT("ScriptError")));
	TestTrue(TEXT("Startup failure keeps source details"), !Error.SourceName.IsEmpty() && !Error.ScriptStack.IsEmpty());
	TestNull(TEXT("Failed startup leaves no live map entry"), Fixture.Subsystem->FindView(TEXT("Reusable")));
	App->JavaScriptMode = ENativeWebOptionalSourceMode::None;
	App->bEvaluateJavaScriptOnStart = false;
	UNativeWebView* Reused = Fixture.Subsystem->CreateAppView(TEXT("Reusable"), App, Surface, Presentation, Error);
	TestNotNull(TEXT("Failed startup releases its local name for reuse"), Reused);
	if (Reused)
	{
		TestFalse(TEXT("Open app view without a selected script returns an explicit error"), Reused->EvaluateAppScript(Error));
		TestEqual(TEXT("Absent app script reports NoAppScript"), Error.Code, FName(TEXT("NoAppScript")));
		Fixture.Subsystem->DestroyView(TEXT("Reusable"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppAsyncAppStageCancellationTest,
	"NativeWeb.AppAsset.AsyncAppStageCancellationAndDuplicateName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppAsyncAppStageCancellationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TSharedRef<FNativeWebAppAsyncCancelState, ESPMode::ThreadSafe> State = MakeShared<FNativeWebAppAsyncCancelState, ESPMode::ThreadSafe>();
	State->Test = this;
	if (!State->Fixture.IsReady()) return TestTrue(TEXT("Engine-owned local player fixture is ready"), false);
	State->Fixture.Subsystem->SetAutomationLoadCompletionHolds(true, false);
	UNativeWebAppAsset* App = State->Fixture.MakeThrowingApp(false);
	State->Fixture.Subsystem->CreateAppViewAsync(TSoftObjectPtr<UNativeWebAppAsset>(App), State->ViewName,
		FNativeWebSurfaceConfig(), FNativeWebPresentationConfig(), FNativeWebCreateAppViewComplete::CreateLambda(
		[State](UNativeWebView*, const FNativeWebError& Error)
		{
			++State->CompletionCount;
			State->CompletionError = Error;
		}));
	ADD_LATENT_AUTOMATION_COMMAND(FNativeWebAppAsyncCancelLatentCommand(State, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppAsyncDependencyStageCancellationTest,
	"NativeWeb.AppAsset.AsyncDependencyStageCancellationAndDuplicateName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppAsyncDependencyStageCancellationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TSharedRef<FNativeWebAppAsyncCancelState, ESPMode::ThreadSafe> State = MakeShared<FNativeWebAppAsyncCancelState, ESPMode::ThreadSafe>();
	State->Test = this;
	if (!State->Fixture.IsReady()) return TestTrue(TEXT("Engine-owned local player fixture is ready"), false);
	State->Fixture.Subsystem->SetAutomationLoadCompletionHolds(false, true);
	UNativeWebAppAsset* App = State->Fixture.MakeThrowingApp(false);
	State->Fixture.HtmlAsset.Reset(NewObject<UNativeWebHtmlAsset>(GetTransientPackage()));
	UNativeWebHtmlAsset* Html = State->Fixture.HtmlAsset.Get();
	const ANSICHAR HtmlBytes[] = "<html><body></body></html>";
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(HtmlBytes), UE_ARRAY_COUNT(HtmlBytes) - 1);
	Html->SetImportedContent(MoveTemp(Bytes), TEXT("async.html"));
	App->HtmlMode = ENativeWebHtmlSourceMode::Asset;
	App->HtmlAsset = Html;
	State->Fixture.Subsystem->CreateAppViewAsync(TSoftObjectPtr<UNativeWebAppAsset>(App), State->ViewName,
		FNativeWebSurfaceConfig(), FNativeWebPresentationConfig(), FNativeWebCreateAppViewComplete::CreateLambda(
		[State](UNativeWebView*, const FNativeWebError& Error)
		{
			++State->CompletionCount;
			State->CompletionError = Error;
		}));
	ADD_LATENT_AUTOMATION_COMMAND(FNativeWebAppAsyncCancelLatentCommand(State, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppAsyncPartialDependencyThenRetryTest,
	"NativeWeb.AppAsset.AsyncPartialDependencyThenRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppAsyncPartialDependencyThenRetryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!HasSupportedRhi())
	{
		AddWarning(TEXT("Requires a D3D11 or D3D12 RHI for the real app view fixture."));
		return true;
	}
	TSharedRef<FNativeWebAppPartialDependencyState, ESPMode::ThreadSafe> State =
		MakeShared<FNativeWebAppPartialDependencyState, ESPMode::ThreadSafe>();
	State->Test = this;
	if (!State->Fixture.IsReady()) return TestTrue(TEXT("Engine-owned local player fixture is ready"), false);

	State->Fixture.HtmlAsset.Reset(NewObject<UNativeWebHtmlAsset>(GetTransientPackage()));
	const ANSICHAR HtmlBytes[] = "<html><body><div id=\"status\"></div></body></html>";
	TArray<uint8> HtmlPayload;
	HtmlPayload.Append(reinterpret_cast<const uint8*>(HtmlBytes), UE_ARRAY_COUNT(HtmlBytes) - 1);
	State->Fixture.HtmlAsset->SetImportedContent(MoveTemp(HtmlPayload), TEXT("partial-retry.html"));
	UNativeWebAppAsset* App = State->Fixture.MakeThrowingApp(true);
	App->HtmlMode = ENativeWebHtmlSourceMode::Asset;
	App->HtmlAsset = TSoftObjectPtr<UNativeWebHtmlAsset>(State->Fixture.HtmlAsset.Get());
	App->CssMode = ENativeWebOptionalSourceMode::Asset;
	App->JavaScriptMode = ENativeWebOptionalSourceMode::String;
	App->JavaScriptString = TEXT("globalThis.__NativeWebCoverageStarted = true;");
	App->bEvaluateJavaScriptOnStart = true;
	const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	State->MissingCssPath = FString::Printf(TEXT("/Game/__NativeWebCoverage_%s.MissingCss"), *Unique);
	App->CssAsset = TSoftObjectPtr<UNativeWebCssAsset>(FSoftObjectPath(State->MissingCssPath));
	State->ViewName = FName(*FString::Printf(TEXT("PartialRetry_%s"), *Unique));
	State->Surface.PixelSize = FIntPoint(64, 64);
	State->Presentation.PresentationMode = ENativeWebPresentationMode::None;
	State->Phase = 0;
	State->Launch(State);
	ADD_LATENT_AUTOMATION_COMMAND(FNativeWebAppPartialDependencyLatentCommand(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebAppBlueprintAsyncActionCompletionTest,
	"NativeWeb.AppAsset.BlueprintAsyncActionCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebAppBlueprintAsyncActionCompletionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!HasSupportedRhi())
	{
		AddWarning(TEXT("Requires a D3D11 or D3D12 RHI for the real app view fixture."));
		return true;
	}
	TSharedRef<FNativeWebAppBlueprintProxyState, ESPMode::ThreadSafe> State =
		MakeShared<FNativeWebAppBlueprintProxyState, ESPMode::ThreadSafe>();
	State->Test = this;
	if (!State->Fixture.IsReady()) return TestTrue(TEXT("Engine-owned local player fixture is ready"), false);
	State->Receiver.Reset(NewObject<UNativeWebAppLaunchTestReceiver>(GetTransientPackage()));
	UNativeWebAppAsset* App = State->Fixture.MakeThrowingApp(false);
	State->Fixture.HtmlAsset.Reset(NewObject<UNativeWebHtmlAsset>(GetTransientPackage()));
	const ANSICHAR HtmlBytes[] = "<html><body><div id=\"status\"></div></body></html>";
	TArray<uint8> HtmlPayload;
	HtmlPayload.Append(reinterpret_cast<const uint8*>(HtmlBytes), UE_ARRAY_COUNT(HtmlBytes) - 1);
	State->Fixture.HtmlAsset->SetImportedContent(MoveTemp(HtmlPayload), TEXT("blueprint-proxy.html"));
	App->HtmlMode = ENativeWebHtmlSourceMode::Asset;
	App->HtmlAsset = TSoftObjectPtr<UNativeWebHtmlAsset>(State->Fixture.HtmlAsset.Get());
	App->JavaScriptMode = ENativeWebOptionalSourceMode::None;
	App->bEvaluateJavaScriptOnStart = false;
	FNativeWebSurfaceConfig Surface;
	Surface.PixelSize = FIntPoint(64, 64);
	FNativeWebPresentationConfig Presentation;
	Presentation.PresentationMode = ENativeWebPresentationMode::None;
	State->Action.Reset(UNativeWebCreateAppViewAsyncAction::CreateAppViewAsync(
		State->Fixture.Subsystem.Get(), TSoftObjectPtr<UNativeWebAppAsset>(App), State->ViewName, Surface, Presentation));
	State->Action->Success.AddDynamic(State->Receiver.Get(), &UNativeWebAppLaunchTestReceiver::ReceiveSuccess);
	State->Action->Failure.AddDynamic(State->Receiver.Get(), &UNativeWebAppLaunchTestReceiver::ReceiveFailure);
	TestEqual(TEXT("proxy does not broadcast before activation"), State->Receiver->SuccessCount + State->Receiver->FailureCount, 0);
	State->Action->Activate();
	State->Action->Activate();
	State->DeadlineSeconds = FPlatformTime::Seconds() + 10.0;
	ADD_LATENT_AUTOMATION_COMMAND(FNativeWebAppBlueprintProxyLatentCommand(State));
	return true;
}

#endif
