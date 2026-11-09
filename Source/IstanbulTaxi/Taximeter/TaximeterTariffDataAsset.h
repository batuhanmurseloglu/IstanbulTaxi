#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TaximeterTariffDataAsset.generated.h"

/**
 * One taxi segment's UKOME tariff. All money is in KURUŞ (1 TL = 100 kuruş) to avoid float drift.
 * Defaults = İstanbul Sarı Taksi, UKOME tariff effective 20 July 2026. No day/night difference in İstanbul.
 */
UCLASS(BlueprintType)
class ISTANBULTAXI_API UTaximeterTariffDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** e.g. "Sarı Taksi" — use a String Table entry (ST_Taximeter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff")
	FText SegmentName;

	/** Açılış ücreti. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff", meta = (ClampMin = "0"))
	int32 OpeningFareKurus = 7194;

	/** Kilometre başı. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff", meta = (ClampMin = "1"))
	int32 PerKmKurus = 4792;

	/** Zaman tarifesi (saatlik bekleme). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff", meta = (ClampMin = "0"))
	int32 HourlyWaitKurus = 59890;

	/** İndi-bindi: minimum charged for any trip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff", meta = (ClampMin = "0"))
	int32 MinimumFareKurus = 23000;

	/**
	 * Real taximeters switch to time billing when driving slower than the speed at which
	 * distance billing equals time billing (Hourly / PerKm ≈ 12.5 km/h). Uncheck to set manually.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff")
	bool bAutoWaitThreshold = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff", meta = (EditCondition = "!bAutoWaitThreshold", ClampMin = "1"))
	float ManualWaitThresholdKmh = 12.5f;

	/** Display increments in steps like a real meter (0 = continuous). 100 = jumps of 1 TL. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff", meta = (ClampMin = "0"))
	int32 DisplayStepKurus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tariff", meta = (MultiLine = "true"))
	FString SourceNote = TEXT("İBB UKOME — Sarı taksi, 20.07.2026 itibarıyla");

	UFUNCTION(BlueprintPure, Category = "Tariff")
	float GetWaitThresholdKmh() const
	{
		return bAutoWaitThreshold ? (float)HourlyWaitKurus / (float)FMath::Max(PerKmKurus, 1) : ManualWaitThresholdKmh;
	}

	/** What an honest trip "should" cost — used by passengers to judge the meter (M2) and route honesty. */
	UFUNCTION(BlueprintPure, Category = "Tariff")
	int32 EstimateFareKurus(float DistanceKm, float WaitingMinutes = 0.0f) const
	{
		const double Raw = OpeningFareKurus + PerKmKurus * (double)DistanceKm + HourlyWaitKurus * (WaitingMinutes / 60.0);
		return FMath::Max(MinimumFareKurus, FMath::RoundToInt(Raw));
	}

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TaximeterTariff"), GetFName());
	}
};

UENUM(BlueprintType)
enum class ETaxiTollDirection : uint8
{
	Both,
	EuropeToAsia,
	AsiaToEurope
};

/** Row of DT_Tolls. Car (1. sınıf) prices; tolls are added to the fare (passenger pays) and paid by the driver via HGS. */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FTaxiTollRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll")
	FGameplayTag CrossingTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll", meta = (ClampMin = "0"))
	int32 FeeKurus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll")
	bool bHasNightRate = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll", meta = (EditCondition = "bHasNightRate", ClampMin = "0"))
	int32 NightFeeKurus = 0;

	/** Night window [Start, End) in game hours; may wrap past midnight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll", meta = (EditCondition = "bHasNightRate", ClampMin = "0", ClampMax = "24"))
	float NightStartHour = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll", meta = (EditCondition = "bHasNightRate", ClampMin = "0", ClampMax = "24"))
	float NightEndHour = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll")
	ETaxiTollDirection ChargedDirection = ETaxiTollDirection::Both;

	int32 GetFeeAtHour(float Hour) const
	{
		if (!bHasNightRate) return FeeKurus;
		const bool bNight = NightStartHour <= NightEndHour
			? (Hour >= NightStartHour && Hour < NightEndHour)
			: (Hour >= NightStartHour || Hour < NightEndHour);
		return bNight ? NightFeeKurus : FeeKurus;
	}
};
