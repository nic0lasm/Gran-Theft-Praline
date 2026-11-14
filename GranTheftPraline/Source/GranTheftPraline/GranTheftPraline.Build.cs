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
			"PhysicsCore",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"EngineSettings",
			"ImageWrapper",
			"RHI",
			"RenderCore",
			"AudioExtensions",
			"DeveloperSettings",
			"AssetRegistry",
			"AudioMixer"
		});

		// Editor-only tools called from the editor's Python (GTPAnimGraphTools: the player's upper-body layer).
		// Their code is under WITH_EDITOR, so the packaged game does not link these modules.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"AnimGraph",
				"AnimGraphRuntime",
				"BlueprintGraph",
				"KismetCompiler"
			});
		}
	}
}
