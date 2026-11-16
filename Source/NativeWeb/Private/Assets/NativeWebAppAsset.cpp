#include "Assets/NativeWebAppAsset.h"

const FPrimaryAssetType UNativeWebAppAsset::PrimaryAssetType(TEXT("NativeWebApp"));

FPrimaryAssetId UNativeWebAppAsset::GetPrimaryAssetId() const
{
	return AppId.IsNone() ? FPrimaryAssetId() : FPrimaryAssetId(PrimaryAssetType, AppId);
}
