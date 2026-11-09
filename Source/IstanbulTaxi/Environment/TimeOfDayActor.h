#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeOfDayActor.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;
class UTimeOfDayPresetDataAsset;

/**
 * Drop one in the persistent level and pick your Sun, SkyLight, HeightFog, and PostProcessVolume.
 * Registers them with UTimeOfDaySubsystem on BeginPlay.
 */
UCLASS()
class ISTANBULTAXI_API ATimeOfDayActor : public AActor
{
	GENERATED_BODY()

public:
	ATimeOfDayActor();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "TimeOfDay")
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "TimeOfDay")
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "TimeOfDay")
	TObjectPtr<AExponentialHeightFog> HeightFog;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "TimeOfDay")
	TObjectPtr<APostProcessVolume> PostProcessVolume;

	/** Optional level-specific preset override. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeOfDay")
	TObjectPtr<UTimeOfDayPresetDataAsset> PresetOverride;

protected:
	virtual void BeginPlay() override;
};
