/**
 * @file Source/NativeWebContentHttpServer/NativeWebContentHttpServer.Build.cs
 * @brief Defines Unreal Build Tool rules for the NativeWebContentHttpServer module.
 * @details Game-instance content HTTP and image request services.
 * @ingroup NativeWebContentHttpServer
 */

using UnrealBuildTool;

/**
 * @brief Declares the native web content http server type and its operations.
 * @ingroup NativeWebContentHttpServer
 */
public class NativeWebContentHttpServer : ModuleRules
{
    public NativeWebContentHttpServer(ReadOnlyTargetRules InTarget) : base(InTarget)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "HTTPServer",
                "ImageWrapper",
                "Json",
                "Slate",
                "SlateCore"
            }
        );
    }
}
