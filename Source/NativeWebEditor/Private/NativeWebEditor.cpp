#include "Modules/ModuleManager.h"
#include "Misc/App.h"

#include "PropertyEditorModule.h"
#include "Assets/NativeWebAssetStyle.h"
#include "Assets/NativeWebContentAssetDetails.h"

class FNativeWebEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningCommandlet()) return;
		FNativeWebAssetStyle::Initialize();
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		for (const FName ClassName : {FName(TEXT("NativeWebHtmlAsset")), FName(TEXT("NativeWebCssAsset")),
		                              FName(TEXT("NativeWebJavaScriptAsset")), FName(TEXT("NativeWebFontAsset"))})
		{
			PropertyEditor.RegisterCustomClassLayout(ClassName,
				FOnGetDetailCustomizationInstance::CreateStatic(&FNativeWebContentAssetDetails::MakeInstance));
		}
		PropertyEditor.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		FNativeWebAssetStyle::Shutdown();
		if (IsRunningCommandlet() || !FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor"))) return;
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		for (const FName ClassName : {FName(TEXT("NativeWebHtmlAsset")), FName(TEXT("NativeWebCssAsset")),
		                              FName(TEXT("NativeWebJavaScriptAsset")), FName(TEXT("NativeWebFontAsset"))})
		{
			PropertyEditor.UnregisterCustomClassLayout(ClassName);
		}
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
};

IMPLEMENT_MODULE(FNativeWebEditorModule, NativeWebEditor)
