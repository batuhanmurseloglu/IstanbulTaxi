#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TimeOfDayPresetDataAsset.generated.h"

class UMaterialParameterCollection;

/** Visual + atmospheric target for one of the 8 Istanbul time phases. */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FTimeOfDayPhaseSample
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FGameplayTag PhaseTag;

	/** Center hour of this phase in [0, 24) used for smooth interpolation between phases. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0", ClampMax = "24"))
	float KeyHour = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	float SunIntensityLux = 80000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	FLinearColor SunColor = FLinearColor(1.0f, 0.96f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	float SkyLightIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	float FogDensity = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog")
	FLinearColor FogInscatteringColor = FLinearColor(0.55f, 0.65f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exposure")
	float ExposureCompensation = 0.0f;

	/** 0..1 drives streetlamps and shop signage in MPC_Environment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive", meta = (ClampMin = "0", ClampMax = "1"))
	float StreetLightEmissive = 0.0f;

	/** 0..1 drives apartment/office window emissive masks in MPC_Environment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive", meta = (ClampMin = "0", ClampMax = "1"))
	float WindowEmissive = 0.0f;

	/** 0..1 drives Bosphorus bridge LED shows (turns on at Akşam/Dusk, dims LateNight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Emissive", meta = (ClampMin = "0", ClampMax = "1"))
	float BridgeLedIntensity = 0.0f;
};

/**
 * Designer-tunable 24h atmospheric curve across the 8 Istanbul time phases,
 * plus the MaterialParameterCollection updated every frame.
 */
UCLASS(BlueprintType)
class ISTANBULTAXI_API UTimeOfDayPresetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTimeOfDayPresetDataAsset();

	/** MaterialParameterCollection (MPC_Environment) receiving TimeOfDay, SunAltitude, StreetLightEmissive, WindowEmissive, BridgeLedIntensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MPC")
	TObjectPtr<UMaterialParameterCollection> EnvironmentMPC;

	/** Game minutes that pass per 1 real-time second (default 1.0 -> 24-min full day/night cycle). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock", meta = (ClampMin = "0"))
	float GameMinutesPerRealSecond = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock", meta = (ClampMin = "0", ClampMax = "24"))
	float StartHour = 8.5f;

	/** Default calendar date (used for seasonal sun declination & Diyanet prayer times). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock", meta = (ClampMin = "1", ClampMax = "12"))
	int32 StartMonth = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock", meta = (ClampMin = "1", ClampMax = "31"))
	int32 StartDay = 15;

	/** Ordered by KeyHour ascending. Constructor populates sensible Istanbul defaults for all 8 phases. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phases")
	TArray<FTimeOfDayPhaseSample> Phases;

	/** Evaluates smoothly interpolated sample at Hour [0, 24) and returns the active phase tag. */
	FTimeOfDayPhaseSample EvaluateAtHour(float Hour, FGameplayTag& OutActivePhaseTag) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TimeOfDayPreset"), GetFName());
	}
};
