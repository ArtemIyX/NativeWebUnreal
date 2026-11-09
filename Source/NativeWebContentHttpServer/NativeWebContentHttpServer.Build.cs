using UnrealBuildTool;

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
