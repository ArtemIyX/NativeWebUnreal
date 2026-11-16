#pragma once

#include "AssetDefinition_DataAsset.h"
#include "Assets/NativeWebAppAsset.h"
#include "Assets/NativeWebContentAssets.h"
#include "NativeWebAssetDefinitions.generated.h"

UCLASS(Abstract)
class NATIVEWEBEDITOR_API UNativeWebAssetDefinitionBase : public UAssetDefinition_DataAsset
{
	GENERATED_BODY()

protected:
	virtual void BuildFilters(TArray<FAssetFilterData>& OutFilters) const override;
};

UCLASS()
class NATIVEWEBEDITOR_API UAssetDefinition_NativeWebAppAsset final : public UNativeWebAssetDefinitionBase
{
	GENERATED_BODY()

public:
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UNativeWebAppAsset::StaticClass(); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor::FromSRGBColor(FColor(52, 120, 212)); }
};

UCLASS()
class NATIVEWEBEDITOR_API UAssetDefinition_NativeWebHtmlAsset final : public UNativeWebAssetDefinitionBase
{
	GENERATED_BODY()

public:
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UNativeWebHtmlAsset::StaticClass(); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor::FromSRGBColor(FColor(168, 173, 181)); }
};

UCLASS()
class NATIVEWEBEDITOR_API UAssetDefinition_NativeWebCssAsset final : public UNativeWebAssetDefinitionBase
{
	GENERATED_BODY()

public:
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UNativeWebCssAsset::StaticClass(); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor::FromSRGBColor(FColor(97, 188, 232)); }
};

UCLASS()
class NATIVEWEBEDITOR_API UAssetDefinition_NativeWebJavaScriptAsset final : public UNativeWebAssetDefinitionBase
{
	GENERATED_BODY()

public:
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UNativeWebJavaScriptAsset::StaticClass(); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor::FromSRGBColor(FColor(232, 148, 54)); }
};

UCLASS()
class NATIVEWEBEDITOR_API UAssetDefinition_NativeWebFontAsset final : public UNativeWebAssetDefinitionBase
{
	GENERATED_BODY()

public:
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UNativeWebFontAsset::StaticClass(); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor::FromSRGBColor(FColor(49, 81, 138)); }
};
