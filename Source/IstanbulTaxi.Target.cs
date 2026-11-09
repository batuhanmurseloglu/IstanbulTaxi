using UnrealBuildTool;

public class IstanbulTaxiTarget : TargetRules
{
	public IstanbulTaxiTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("IstanbulTaxi");
	}
}