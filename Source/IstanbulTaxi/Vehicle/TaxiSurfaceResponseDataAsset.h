#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Chaos/ChaosEngineInterface.h"
#include "TaxiSurfaceResponseDataAsset.generated.h"

/** How a single road surface feels, sounds and wears the car. */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FTaxiSurfaceResponse
{
	GENERATED_BODY()

	/** Tag broadcast to other systems (comfort score, audio, damage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	FGameplayTag SurfaceTag;

	/** Multiplier on the wheel's base friction when dry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float GripMultiplier = 1.0f;

	/** Multiplier used at full global wetness (blended by Wetness 0..1 from WeatherSubsystem). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grip", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float WetGripMultiplier = 0.75f;

	/** 0 = glass smooth, 1 = heavy cobbles. Scales camera/force-feedback rumble. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VibrationIntensity = 0.0f;

	/** Rumble frequency in Hz at reference speed (cobbles ~18, patched ~6). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0.5", ClampMax = "60.0"))
	float VibrationFrequency = 10.0f;

	/** Integer passed to the road-noise MetaSound ("SurfaceIndex") to select a layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	int32 AudioSurfaceIndex = 0;

	/** Severity (0..1) of a one-shot jolt when a wheel ENTERS this surface at speed (potholes, rail edges). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntryImpactSeverity = 0.0f;

	/** Suspension wear added per km driven on this surface (0..1 scale of a component's life). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float SuspensionWearPerKm = 0.0005f;
};

/**
 * Maps physical surface types (Project Settings > Physics > Physical Surface) to responses.
 * One asset can be shared by every vehicle; trucks/upgrades can use another.
 */
UCLASS(BlueprintType)
class ISTANBULTAXI_API UTaxiSurfaceResponseDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfaces")
	TMap<TEnumAsByte<EPhysicalSurface>, FTaxiSurfaceResponse> Responses;

	/** Used for SurfaceType_Default, unmapped surfaces, and wheels in the air. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfaces")
	FTaxiSurfaceResponse Fallback;

	UFUNCTION(BlueprintPure, Category = "Surfaces")
	const FTaxiSurfaceResponse& FindResponse(TEnumAsByte<EPhysicalSurface> Surface) const
	{
		const FTaxiSurfaceResponse* Found = Responses.Find(Surface);
		return Found ? *Found : Fallback;
	}

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TaxiSurfaceResponses"), GetFName());
	}
};
