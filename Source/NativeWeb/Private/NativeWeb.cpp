

#include "NativeWeb.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogNativeWeb);
DEFINE_LOG_CATEGORY(LogNativeWebSubsystem);
DEFINE_LOG_CATEGORY(LogNativeWebView);
DEFINE_LOG_CATEGORY(LogNativeWebAssets);
DEFINE_LOG_CATEGORY(LogNativeWebScript);
DEFINE_LOG_CATEGORY(LogNativeWebMessaging);
DEFINE_LOG_CATEGORY(LogNativeWebInput);
DEFINE_LOG_CATEGORY(LogNativeWebViewport);
DEFINE_LOG_CATEGORY(LogNativeWebAffineUi);
DEFINE_LOG_CATEGORY(LogNativeWebRender);
DEFINE_LOG_CATEGORY(LogNativeWebTick);


class FNativeWebModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NativeWeb"));
		if (Plugin.IsValid())
		{
			AddShaderSourceDirectoryMapping(TEXT("/Plugin/NativeWeb"), FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
			UE_LOG(LogNativeWebRender, Display, TEXT("Mapped NativeWeb shaders from '%s'."),
			       *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
		}
		else
		{
			UE_LOG(LogNativeWebRender, Error, TEXT("Could not locate the NativeWeb plugin for shader source mapping."));
		}
		UE_LOG(LogNativeWeb, Display, TEXT("NativeWeb module starting."));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogNativeWeb, Display, TEXT("NativeWeb module shutting down."));
	}
};

IMPLEMENT_MODULE(FNativeWebModule, NativeWeb)
