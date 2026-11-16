#if WITH_DEV_AUTOMATION_TESTS

#include "AffineUI/AffineUiSurface.h"
#include "DynamicRHI.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffineUiSurfaceLifecycleTest,
                                 "NativeWeb.AffineUi.Surface.Lifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffineUiSurfaceLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!GDynamicRHI || GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11)
	{
		AddWarning(TEXT("This test requires an isolated D3D11 editor session (-dx11)."));
		return true;
	}
	FAffineUiSurfaceDesc Desc;
	Desc.Content.Html = TEXT("<html><body><p>Initial content</p></body></html>");
	FString Error;
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Surface = FAffineUiSurface::Create(Desc, Error);
	if (!TestTrue(TEXT("A surface can be created"), Surface.IsValid())) { return false; }

	FString OtherError;
	FAffineUiSurfaceDesc OtherDesc = Desc;
	OtherDesc.Content.Html = TEXT("<html><body><p>Independent surface</p></body></html>");
	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Other = FAffineUiSurface::Create(OtherDesc, OtherError);
	if (!TestTrue(TEXT("A second surface can be created concurrently"), Other.IsValid()))
	{
		AddError(OtherError);
		Surface->Close();
		return false;
	}
	TestNotEqual(TEXT("Concurrent surfaces have independent owners"), Surface.Get(), Other.Get());
	TestTrue(TEXT("The first surface accepts its own content update"), Surface->SetContent(Desc.Content));
	TestTrue(TEXT("The second surface accepts its independent content update"), Other->SetContent(OtherDesc.Content));
	TestFalse(TEXT("Zero width is rejected"), Surface->Resize(FIntPoint(0, 240)));
	TestFalse(TEXT("Oversized targets are rejected"), Surface->Resize(FIntPoint(4097, 240)));
	TestFalse(TEXT("Nonpositive DPI is rejected"), Surface->Resize(FIntPoint(480, 240), 0.0f));
	TestTrue(TEXT("Content replacement is accepted"), Surface->SetContent(Desc.Content));
	TestTrue(TEXT("A valid resize is accepted"), Surface->Resize(FIntPoint(320, 160), 1.0f));
	TestTrue(TEXT("A redraw is accepted"), Surface->RequestRedraw());
	UTextureRenderTarget2D* PreviousOutput = Surface->GetOutputTexture();
	const FString Missing = FPaths::Combine(FPaths::ProjectSavedDir(), FGuid::NewGuid().ToString(),
	                                        TEXT("missing.html"));
	AddExpectedError(TEXT("Could not read HTML file:"), EAutomationExpectedErrorFlags::Contains, 1, false);
	TestFalse(TEXT("A missing HTML file is reported"), Surface->LoadContentFromFiles(Missing, Missing, Error));
	TestFalse(TEXT("A missing file produces an error"), Error.IsEmpty());
	TestTrue(TEXT("Failed loading keeps the published output"), Surface->GetOutputTexture() == PreviousOutput);
	const FString ExampleHtml = FPaths::Combine(FPaths::ProjectPluginsDir(),
	                                            TEXT("NativeWeb/Resources/Demo/viewport.html"));
	if (FPaths::FileExists(ExampleHtml))
	{
		AddExpectedError(TEXT("Could not read CSS file:"), EAutomationExpectedErrorFlags::Contains, 1, false);
		TestFalse(TEXT("A missing CSS file is reported"), Surface->LoadContentFromFiles(ExampleHtml, Missing, Error));
		TestTrue(TEXT("The CSS error identifies the failed file"), Error.Contains(TEXT("CSS")));
		TestTrue(TEXT("A missing CSS file keeps the published output"), Surface->GetOutputTexture() == PreviousOutput);
	}
	Surface->Close();
	Surface->Close();
	TestFalse(TEXT("Closed surfaces reject new content"), Surface->SetContent(Desc.Content));
	TestFalse(TEXT("Closed surfaces reject resize"), Surface->Resize(FIntPoint(320, 200)));
	Surface.Reset();
	TestTrue(TEXT("The concurrent surface remains usable after its peer closes"),
		Other->Resize(FIntPoint(240, 120)) && Other->RequestRedraw());
	Other->Close();
	TestFalse(TEXT("Each closed surface rejects further resize"), Other->Resize(FIntPoint(320, 200)));
	Other.Reset();

	TSharedPtr<FAffineUiSurface, ESPMode::ThreadSafe> Recreated = FAffineUiSurface::Create(Desc, OtherError);
	TestTrue(TEXT("A surface can be created again after close"), Recreated.IsValid());
	if (Recreated) { Recreated->Close(); }
	return true;
}

#endif
