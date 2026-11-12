using UnrealBuildTool;

// Our code on top of Mass and ZoneGraph (ADR-0015). Kept apart from the game module so the
// game still builds when the Mass plugins (and MassTraffic, installed out of Git) are absent.
public class GTPMass : ModuleRules
{
	public GTPMass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ZoneGraph"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});
	}
}
