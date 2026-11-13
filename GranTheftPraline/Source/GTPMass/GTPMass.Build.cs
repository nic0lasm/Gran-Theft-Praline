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
			"Json"
		});

		// MassTraffic is Epic code copied from City Sample into Plugins/MassTraffic, out of Git
		// (docs/gameplay/traffic-mass.md). Without it the module builds and
		// -Traffic=mass falls back to the legacy traffic.
		string MassTrafficDir = Path.Combine(ModuleDirectory, "..", "..", "Plugins", "MassTraffic");
		bool bMassTraffic = File.Exists(Path.Combine(MassTrafficDir, "MassTraffic.uplugin"));
		if (bMassTraffic)
		{
			PrivateDependencyModuleNames.Add("MassTraffic");
		}
		PublicDefinitions.Add("WITH_GTP_MASSTRAFFIC=" + (bMassTraffic ? "1" : "0"));
	}
}
