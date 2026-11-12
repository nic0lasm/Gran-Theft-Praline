using UnrealBuildTool;

public class GranTheftPralineEditorTarget : TargetRules
{
	public GranTheftPralineEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GranTheftPraline");
		ExtraModuleNames.Add("GTPMass");
	}
}
