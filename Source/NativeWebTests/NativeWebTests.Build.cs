/**
 * @file Source/NativeWebTests/NativeWebTests.Build.cs
 * @brief Defines Unreal Build Tool rules for the NativeWebTests module.
 * @details Uncooked automation tests for the NativeWeb plugin.
 * @ingroup NativeWebTests
 */

using UnrealBuildTool;

/**
 * @brief Declares the native web tests type and its operations.
 * @ingroup NativeWebTests
 */
public class NativeWebTests : ModuleRules
{
    public NativeWebTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "NativeWeb",
                "NativeWebDispatch",
                "NativeWebWebSocketServer",
                "RHI",
                "Slate",
                "SlateCore"
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("QuickJSUnreal");
        }

        PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "..", "NativeWeb", "Private"));
    }
}
