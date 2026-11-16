#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Assets/NativeWebContentAssets.h"
#include "NativeWebAppAsset.generated.h"

UENUM(BlueprintType)
enum class ENativeWebHtmlSourceMode : uint8
{
	Default,
	Asset,
	String
};

UENUM(BlueprintType)
enum class ENativeWebOptionalSourceMode : uint8
{
	None,
	Asset,
	String
};

UCLASS(BlueprintType)
class NATIVEWEB_API UNativeWebAppAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category="NativeWeb|App", meta=(ToolTip="Returns this app's stable registry ID based on App ID, independent of its object name."))
	FPrimaryAssetId GetAppPrimaryAssetId() const { return GetPrimaryAssetId(); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category="App", meta=(ToolTip="Stable app identifier used for Asset Manager registration. Must be unique and non-empty."))
	FName AppId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HTML", meta=(ToolTip="Select the HTML source."))
	ENativeWebHtmlSourceMode HtmlMode = ENativeWebHtmlSourceMode::Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HTML", meta=(EditCondition="HtmlMode == ENativeWebHtmlSourceMode::Asset", EditConditionHides, ToolTip="Imported HTML content asset."))
	TSoftObjectPtr<UNativeWebHtmlAsset> HtmlAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HTML", meta=(EditCondition="HtmlMode == ENativeWebHtmlSourceMode::String", EditConditionHides, MultiLine=true, ToolTip="HTML source text."))
	FString HtmlString;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CSS", meta=(ToolTip="Select an optional stylesheet source."))
	ENativeWebOptionalSourceMode CssMode = ENativeWebOptionalSourceMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CSS", meta=(EditCondition="CssMode == ENativeWebOptionalSourceMode::Asset", EditConditionHides, ToolTip="Imported CSS content asset."))
	TSoftObjectPtr<UNativeWebCssAsset> CssAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CSS", meta=(EditCondition="CssMode == ENativeWebOptionalSourceMode::String", EditConditionHides, MultiLine=true, ToolTip="CSS source text."))
	FString CssString;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JavaScript", meta=(ToolTip="Select an optional startup JavaScript source."))
	ENativeWebOptionalSourceMode JavaScriptMode = ENativeWebOptionalSourceMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JavaScript", meta=(EditCondition="JavaScriptMode == ENativeWebOptionalSourceMode::Asset", EditConditionHides, ToolTip="Imported JavaScript content asset."))
	TSoftObjectPtr<UNativeWebJavaScriptAsset> JavaScriptAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JavaScript", meta=(EditCondition="JavaScriptMode == ENativeWebOptionalSourceMode::String", EditConditionHides, MultiLine=true, ToolTip="JavaScript source text."))
	FString JavaScriptString;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JavaScript", meta=(EditCondition="JavaScriptMode != ENativeWebOptionalSourceMode::None", EditConditionHides, ToolTip="Evaluate the selected script before the view is published."))
	bool bEvaluateJavaScriptOnStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fonts", meta=(ToolTip="Imported font assets available to this app using fonts/<filename> URLs."))
	TArray<TSoftObjectPtr<UNativeWebFontAsset>> Fonts;
};
