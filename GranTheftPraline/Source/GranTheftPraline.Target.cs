using UnrealBuildTool;

public class GranTheftPralineTarget : TargetRules
{
	public GranTheftPralineTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GranTheftPraline");
	}
}
