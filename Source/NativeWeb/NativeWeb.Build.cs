using UnrealBuildTool;

public class NativeWeb : ModuleRules
{
    public NativeWeb(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.Add("Core");
    }
}
