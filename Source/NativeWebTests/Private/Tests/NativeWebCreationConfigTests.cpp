#if WITH_DEV_AUTOMATION_TESTS

#include "Data/Structs/NativeWebPresentationConfig.h"
#include "Data/Structs/NativeWebSurfaceConfig.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebCreationConfigDefaultsTest,
	"NativeWeb.Subsystem.CreationConfigDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebCreationConfigDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FNativeWebSurfaceConfig Surface;
	const FNativeWebPresentationConfig Presentation;

	TestEqual(TEXT("Surface defaults to the documented pixel size"), Surface.PixelSize, FIntPoint(480, 240));
	TestEqual(TEXT("Surface defaults to unit DPI"), Surface.DpiScale, 1.0f);
	TestEqual(TEXT("Surface defaults to opaque black"), Surface.ClearColor, FColor(0, 0, 0, 255));
	TestEqual(TEXT("Presentation defaults to GPU viewport"), Presentation.PresentationMode,
	          ENativeWebPresentationMode::GpuViewport);
	TestTrue(TEXT("Presentation defaults to Slate fallback"), Presentation.bFallbackToSlate);
	TestEqual(TEXT("Presentation defaults to zero Slate z-order"), Presentation.ViewportZOrder, 0);
	return true;
}

#endif
