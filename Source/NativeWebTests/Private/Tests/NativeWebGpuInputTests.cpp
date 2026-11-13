#if WITH_DEV_AUTOMATION_TESTS

#include "Presentation/NativeWebGpuPresenter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebGpuTargetContractTest,
                                 "NativeWeb.Presentation.GpuTargetContract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebGpuTargetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(TEXT("10-bit sRGB SDR scene targets are supported"),
	         FNativeWebGpuPresenter::IsSupportedSceneTarget(PF_A2B10G10R10,
	                                                        EDisplayOutputFormat::SDR_sRGB,
	                                                        EDisplayColorGamut::sRGB_D65));
	TestTrue(TEXT("8-bit BGRA sRGB SDR scene targets are supported"),
	         FNativeWebGpuPresenter::IsSupportedSceneTarget(PF_B8G8R8A8,
	                                                        EDisplayOutputFormat::SDR_sRGB,
	                                                        EDisplayColorGamut::sRGB_D65));
	TestFalse(TEXT("HDR output is rejected"),
	          FNativeWebGpuPresenter::IsSupportedSceneTarget(PF_A2B10G10R10,
	                                                         EDisplayOutputFormat::HDR_ACES_1000nit_ST2084,
	                                                         EDisplayColorGamut::Rec2020_D65));
	TestFalse(TEXT("Non-sRGB SDR output is rejected"),
	          FNativeWebGpuPresenter::IsSupportedSceneTarget(PF_A2B10G10R10,
	                                                         EDisplayOutputFormat::SDR_ExplicitGammaMapping,
	                                                         EDisplayColorGamut::sRGB_D65));
	return true;
}

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeWebGpuInputCoordinateTest,
                                 "NativeWeb.Presentation.GpuInputCoordinateConversion",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeWebGpuInputCoordinateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FIntRect Rect(FIntPoint(100, 50), FIntPoint(900, 650));
	FVector2D Normalized;
	TestTrue(TEXT("Top left of the view rectangle converts"),
	         FNativeWebGpuPresenter::ConvertViewportPoint(FVector2D(100, 50), Rect, Normalized));
	TestTrue(TEXT("Top left maps to normalized origin"), Normalized.Equals(FVector2D::ZeroVector));
	TestTrue(TEXT("Bottom right interior converts"),
	         FNativeWebGpuPresenter::ConvertViewportPoint(FVector2D(899, 649), Rect, Normalized));
	TestTrue(TEXT("Bottom right interior is less than one"), Normalized.X < 1.0 && Normalized.Y < 1.0);
	TestFalse(TEXT("Points left of the view are rejected"),
	          FNativeWebGpuPresenter::ConvertViewportPoint(FVector2D(99, 50), Rect, Normalized));
	TestTrue(TEXT("Captured drag coordinates may continue outside the view"),
	         FNativeWebGpuPresenter::ConvertViewportPoint(FVector2D(99, 50), Rect, Normalized, true));
	TestTrue(TEXT("Outside drag coordinates preserve negative normalized values"), Normalized.X < 0.0);
	TestFalse(TEXT("Empty view rectangles are rejected"),
	          FNativeWebGpuPresenter::ConvertViewportPoint(FVector2D::ZeroVector, FIntRect(), Normalized));
	return true;
}

#endif
