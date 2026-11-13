/**
 * @file Source/NativeWeb/NativeWeb.Build.cs
 * @brief Defines Unreal Build Tool rules for the NativeWeb module.
 * @details The Win64 D3D11 view runtime; QuickJS and view lifecycle are game-thread owned.
 * @ingroup NativeWeb
 */

using UnrealBuildTool;

/**
 * @brief Declares the native web type and its operations.
 * @ingroup NativeWeb
 */
public class NativeWeb : ModuleRules
{
    public NativeWeb(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDependencyModuleNames.Add("QuickJSUnreal");
        }
        PrivateDependencyModuleNames.AddRange(new string[] { "AffineUnreal", "InputCore", "Projects", "RHI", "RenderCore", "Renderer", "Slate", "SlateCore" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D11RHI");
        }
    }
}
