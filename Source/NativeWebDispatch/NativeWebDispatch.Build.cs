/**
 * @file Source/NativeWebDispatch/NativeWebDispatch.Build.cs
 * @brief Defines Unreal Build Tool rules for the NativeWebDispatch module.
 * @details Exact-key factory decoding and typed handler routing.
 * @ingroup NativeWebDispatch
 */

using UnrealBuildTool;

/**
 * @brief Declares the native web dispatch type and its operations.
 * @ingroup NativeWebDispatch
 */
public class NativeWebDispatch : ModuleRules
{
    public NativeWebDispatch(ReadOnlyTargetRules Target) : base(Target)
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
                "Slate",
                "SlateCore"
            }
        );
    }
}