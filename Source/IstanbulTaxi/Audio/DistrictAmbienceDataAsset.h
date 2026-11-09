#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DistrictAmbienceDataAsset.generated.h"

class USoundBase;
class USoundSubmix;

/**
 * Acoustic profile for a district or zone (e.g. Sultanahmet, Eminönü Waterfront, Levent Highway).
 * Placed on ADistrictAmbienceVolume or as the city-wide fallback.
 */
UCLASS(BlueprintType)
class ISTANBULTAXI_API UDistrictAmbienceDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	FGameplayTag DistrictTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	FText DisplayName;

	/** Looping MetaSound bed receiving: TrafficMurmur, TeaGardenClatter, CrowdChatter, SeagullIntensity, TimePhaseHours. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> AmbienceMetaSound;

	/** Base distant traffic murmur (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layers", meta = (ClampMin = "0", ClampMax = "1"))
	float BaseTrafficMurmur = 0.5f;

	/** Çay bahçesi / tulip-glass spoon clatter & backgammon dice (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layers", meta = (ClampMin = "0", ClampMax = "1"))
	float TeaGardenClatter = 0.25f;

	/** Street pedestrian / vendor chatter (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layers", meta = (ClampMin = "0", ClampMax = "1"))
	float CrowdChatter = 0.4f;

	/** Minimum seagull intensity even when inland in this district (0..1). Waterfront proximity adds on top. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layers", meta = (ClampMin = "0", ClampMax = "1"))
	float BaseSeagullIntensity = 0.1f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("DistrictAmbience"), GetFName());
	}
};

/**
 * Global audio config for UAudioAmbienceSubsystem (assigned in Project Settings > İstanbul Taksi).
 */
UCLASS(BlueprintType)
class ISTANBULTAXI_API UAudioAmbienceConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Fallback district profile when outside all ADistrictAmbienceVolumes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Districts")
	TObjectPtr<UDistrictAmbienceDataAsset> DefaultDistrictAmbience;

	/** Default ezan sound per prayer tag (Prayer.Sabah -> Saba makam, Prayer.Aksam -> Segah, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ezan")
	TMap<FGameplayTag, TObjectPtr<USoundBase>> DefaultEzanSounds;

	/** Distance (cm) from Bosphorus/Golden Horn/Marmara shoreline where seagulls are at 100% intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seagulls", meta = (ClampMin = "0"))
	float SeagullFullIntensityDistanceCm = 8000.0f; // 80 m

	/** Distance (cm) from shoreline where coastal seagulls fade out to the district's BaseSeagullIntensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seagulls", meta = (ClampMin = "100"))
	float SeagullFadeOutDistanceCm = 65000.0f; // 650 m

	// ---- Interior / Exterior Window Occlusion Filter ----

	/** Low-pass filter cutoff (Hz) when inside the cabin with windows CLOSED (muffled street). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WindowOcclusion", meta = (ClampMin = "100", ClampMax = "20000"))
	float InteriorWindowClosedLPFHz = 950.0f;

	/** Low-pass filter cutoff (Hz) when windows are fully OPEN or in exterior chase camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WindowOcclusion", meta = (ClampMin = "100", ClampMax = "20000"))
	float ExteriorOrWindowOpenLPFHz = 20000.0f;

	/** Exterior bed volume multiplier when inside the cabin with windows CLOSED. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WindowOcclusion", meta = (ClampMin = "0", ClampMax = "1"))
	float InteriorWindowClosedVolumeScale = 0.42f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("AudioAmbienceConfig"), GetFName());
	}
};
