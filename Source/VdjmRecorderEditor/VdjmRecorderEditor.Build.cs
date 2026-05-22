using UnrealBuildTool;

public class VdjmRecorderEditor : ModuleRules
{
    public VdjmRecorderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "VdjmAssetRegistry",
                "VdjmRecorder"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "ApplicationCore",
                "AssetRegistry",
                "AssetTools",
                "ContentBrowser",
                "CoreUObject",
                "EditorFramework",
                "Engine",
                "InputCore",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd",
                "VdjmVcard"
            }
        );
    }
}
