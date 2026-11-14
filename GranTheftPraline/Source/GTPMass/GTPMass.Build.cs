using System.IO;
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
			"ZoneGraph",
			"MassCore",
			"MassEntity",
			"MassCommon",
			"MassSimulation",
			"MassSpawner",
			"GranTheftPraline"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"AssetRegistry"
		});
		if (Target.bBuildEditor)
		{
			// UGTPAssetReflection::ReplacePackageReferences compiles the Blueprints it changes.
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// MassTraffic is Epic code: City Sample's plugin "Traffic" (module MassTraffic) and the
		// RuleProcessor plugin it needs, copied from City Sample into Plugins/Traffic and
		// Plugins/RuleProcessor, out of Git
		// (docs/gameplay/traffic-mass.md). Without it the module builds and
		// -Traffic=mass falls back to the legacy traffic.
		string PluginsDir = Path.Combine(ModuleDirectory, "..", "..", "Plugins");
		bool bMassTraffic = File.Exists(Path.Combine(PluginsDir, "Traffic", "Traffic.uplugin"))
			|| File.Exists(Path.Combine(PluginsDir, "MassTraffic", "MassTraffic.uplugin"));
		if (bMassTraffic)
		{
			// MassMovement: FMassVelocityFragment, which MassTraffic reads on its obstacles (GTPCrowd);
			// MassZoneGraphNavigation: the lane a car is on (GTPMassTraffic's log);
			// MassRepresentation and MassLOD: the cars' visualization trait (ShowNearCarsAsInstances).
			PrivateDependencyModuleNames.AddRange(new string[] { "MassTraffic", "MassMovement", "MassZoneGraphNavigation", "MassRepresentation", "MassLOD" });
		}
		PublicDefinitions.Add("WITH_GTP_MASSTRAFFIC=" + (bMassTraffic ? "1" : "0"));
	}
}
