#if WITH_DEV_AUTOMATION_TESTS

#include "NativeWebGpuPresenter.h"
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
