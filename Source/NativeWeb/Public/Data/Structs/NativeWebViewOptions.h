#pragma once

#include "CoreMinimal.h"
#include "NativeWebViewOptions.generated.h"

USTRUCT(BlueprintType)
struct NATIVEWEB_API FNativeWebViewOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FIntPoint PixelSize = FIntPoint(480, 240);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	float DpiScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FColor ClearColor = FColor(0, 0, 0, 255);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	bool bCreateViewportOverlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	int32 ViewportZOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString Html;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString Css;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NativeWeb")
	FString PackagedAssetRoot;
};
