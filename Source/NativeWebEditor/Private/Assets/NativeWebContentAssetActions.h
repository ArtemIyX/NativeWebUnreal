#pragma once

#include "CoreMinimal.h"

class UNativeWebContentAsset;

namespace NativeWebContentAssetActions
{
	bool IsPathAllowed(const UNativeWebContentAsset& Asset, const FString& Path, FString& OutError);
	bool ImportFile(UNativeWebContentAsset& Asset, const FString& Path, FString& OutError);
	bool ExportFile(const UNativeWebContentAsset& Asset, const FString& Path, FString& OutError);
	bool Clear(UNativeWebContentAsset& Asset, FString& OutError);
}
