#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "NativeWebAppAssetValidator.generated.h"

UCLASS()
class UNativeWebAppAssetValidator final : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(const FAssetData& AssetData, UObject* Asset,
	                                             FDataValidationContext& Context) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& AssetData, UObject* Asset,
	                                                                  FDataValidationContext& Context) override;
};
