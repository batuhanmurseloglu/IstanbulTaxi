using UnrealBuildTool;

public class IstanbulTaxi : ModuleRules
{
	public IstanbulTaxi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput",
			"ChaosVehicles", "ChaosVehiclesCore", "PhysicsCore",
			"GameplayTags",
			"DeveloperSettings",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore",
			"MetasoundEngine", "AudioMixer"
		});

		// Added in later milestones (plugins are already enabled in the .uproject):
		// M5: "MetasoundEngine", "AudioMixer"
		// M8/M9: "MassEntity", "MassCommon", "MassSpawner", "MassMovement", "MassActors",
		//        "MassLOD", "MassRepresentation", "MassSimulation", "ZoneGraph"
	}
}
