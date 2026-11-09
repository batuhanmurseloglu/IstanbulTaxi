using UnrealBuildTool;

public class IstanbulTaxiEditorTarget : TargetRules
{
	public IstanbulTaxiEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("IstanbulTaxi");
	}
}