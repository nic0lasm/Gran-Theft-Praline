using UnrealBuildTool;

public class GranTheftPraline : ModuleRules
{
	public GranTheftPraline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"ChaosVehicles",
			"AIModule",
			"PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"EngineSettings",
			"ImageWrapper",
			"RHI",
			"RenderCore",
			"AudioExtensions"
		});
	}
}
