using UnrealBuildTool;

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
        PrivateDependencyModuleNames.AddRange(new string[] { "AffineUnreal", "InputCore", "RHI", "RenderCore", "Slate", "SlateCore" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D11RHI");
        }
    }
}
