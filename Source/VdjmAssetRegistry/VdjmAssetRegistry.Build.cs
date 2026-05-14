using UnrealBuildTool;

public class VdjmAssetRegistry : ModuleRules
{
    public VdjmAssetRegistry(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetRegistry",
                "Json",
                "JsonUtilities",
                "Projects"
            }
        );
    }
}
