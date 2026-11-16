using UnrealBuildTool;

public class NativeWebEditor : ModuleRules
{
	public NativeWebEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "NativeWeb", "AssetRegistry", "AssetDefinition",
			"DataValidation", "DesktopPlatform", "EngineAssetDefinitions", "Projects", "PropertyEditor",
			"Slate", "SlateCore", "UnrealEd"
		});
	}
}
