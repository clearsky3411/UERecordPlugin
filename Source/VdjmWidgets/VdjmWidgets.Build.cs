using UnrealBuildTool;

public class VdjmWidgets : ModuleRules
{
    public VdjmWidgets(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UMG",
                "VdjmRecorder"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "MediaAssets",
                "Slate",
                "SlateCore"
            }
        );
    }
}
