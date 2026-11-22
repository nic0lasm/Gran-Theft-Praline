using UnrealBuildTool;

public class GranTheftPralineTarget : TargetRules
{
	public GranTheftPralineTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GranTheftPraline");
		ExtraModuleNames.Add("GTPMass");

		// Trace's control server listens on 0.0.0.0:1985 in every non-Shipping build
		// ("LogTrace: Control listening on port 1985"), so Windows asks for a firewall rule for
		// each new package path. No argument or .ini turns it off; this define does, and needs
		// the game's own build environment. Trace itself (to a file or to Insights) still works.
		BuildEnvironment = TargetBuildEnvironment.Unique;
		GlobalDefinitions.Add("UE_TRACE_ALLOW_TCP_CONTROL=0");
	}
}
