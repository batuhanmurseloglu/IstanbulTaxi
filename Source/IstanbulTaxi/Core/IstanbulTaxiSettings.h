#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "IstanbulTaxiSettings.generated.h"

class UTaxiSurfaceResponseDataAsset;
class UTaximeterTariffDataAsset;
class UTaxiRidePolicyDataAsset;
class UTimeOfDayPresetDataAsset;
class UAudioAmbienceConfigDataAsset;
class UDataTable;

/**
 * Project-wide defaults (Project Settings > Game > İstanbul Taksi).
 * Holds soft references to the "default" DataAssets each system falls back to,
 * so nothing is hardcoded and actors stay decoupled.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "İstanbul Taksi"))
class ISTANBULTAXI_API UIstanbulTaxiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UIstanbulTaxiSettings();

	static const UIstanbulTaxiSettings* Get() { return GetDefault<UIstanbulTaxiSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Fallback surface→response table used by any vehicle that doesn't override it. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle")
	TSoftObjectPtr<UTaxiSurfaceResponseDataAsset> DefaultSurfaceResponses;

	/** Default taximeter tariff (DA_Tariff_SariTaksi). */
	UPROPERTY(Config, EditAnywhere, Category = "Taximeter")
	TSoftObjectPtr<UTaximeterTariffDataAsset> DefaultTariff;

	/** Toll table (DT_Tolls, row type TaxiTollRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Taximeter", meta = (RequiredAssetDataTags = "RowStructure=/Script/IstanbulTaxi.TaxiTollRow"))
	TSoftObjectPtr<UDataTable> TollTable;

	/** Passenger boarding / honesty / tipping rules (DA_RidePolicy_Default). */
	UPROPERTY(Config, EditAnywhere, Category = "Missions")
	TSoftObjectPtr<UTaxiRidePolicyDataAsset> DefaultRidePolicy;

	/** Default 24h atmospheric preset & MPC_Environment reference (DA_TimeOfDay_Istanbul). */
	UPROPERTY(Config, EditAnywhere, Category = "Environment")
	TSoftObjectPtr<UTimeOfDayPresetDataAsset> DefaultTimeOfDayPreset;

	/** Default city-wide audio ambience & ezan config (DA_AudioAmbience_Default). */
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<UAudioAmbienceConfigDataAsset> DefaultAudioAmbienceConfig;

	/** Real latitude used by TimeOfDay / prayer times (M3, M5). */
	UPROPERTY(Config, EditAnywhere, Category = "World", meta = (ClampMin = "-90", ClampMax = "90"))
	float Latitude = 41.01f;

	UPROPERTY(Config, EditAnywhere, Category = "World", meta = (ClampMin = "-180", ClampMax = "180"))
	float Longitude = 28.97f;

	/** UTC offset in hours (Türkiye = +3, no DST). */
	UPROPERTY(Config, EditAnywhere, Category = "World")
	float UtcOffsetHours = 3.0f;
};
