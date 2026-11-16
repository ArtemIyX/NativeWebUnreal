#pragma once

#include "CoreMinimal.h"

class UClass;
class FSlateStyleSet;

class FNativeWebAssetStyle
{
public:
	static void Initialize();
	static void Shutdown();

private:
	static void RegisterAssetClassBrushes(const UClass* AssetClass, const TCHAR* ResourceName);
	static TSharedPtr<class FSlateStyleSet> StyleSet;
};
