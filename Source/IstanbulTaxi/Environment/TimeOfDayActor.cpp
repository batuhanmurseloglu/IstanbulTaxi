#include "Environment/TimeOfDayActor.h"
#include "Environment/TimeOfDaySubsystem.h"

ATimeOfDayActor::ATimeOfDayActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATimeOfDayActor::BeginPlay()
{
	Super::BeginPlay();

	if (UTimeOfDaySubsystem* Sub = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		Sub->RegisterSceneActors(SunLight, SkyLight, HeightFog, PostProcessVolume, PresetOverride);
	}
}
