#pragma once

#include "IDetailCustomization.h"

class FNativeWebContentAssetDetails final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
