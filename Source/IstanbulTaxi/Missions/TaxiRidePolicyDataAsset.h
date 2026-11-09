#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TaxiRidePolicyDataAsset.generated.h"

/** Tunable rules for how passengers board, judge the meter/route and pay. M6 archetypes will scale these. */
UCLASS(BlueprintType)
class ISTANBULTAXI_API UTaxiRidePolicyDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Radius (cm) around pickup/drop-off where the taxi counts as "there". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stops", meta = (ClampMin = "100"))
	float StopRadius = 1200.0f;

	/** Taxi must be slower than this (km/h)… */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stops", meta = (ClampMin = "0"))
	float StopSpeedKmh = 3.0f;

	/** …for this long (s) for the passenger to get in / out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stops", meta = (ClampMin = "0"))
	float BoardingSeconds = 2.0f;

	/** Seconds after boarding to start the meter before the passenger notices ("Abi taksimetreyi açmadın!"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Honesty", meta = (ClampMin = "0"))
	float MeterGraceSeconds = 20.0f;

	/** Straight-line → road distance factor when a ride has no precomputed optimal distance (until M10 lane graph). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Honesty", meta = (ClampMin = "1"))
	float StraightLineToRoadFactor = 1.35f;

	/** Driven/optimal distance above this costs 1 star ("Neden bu yoldan gittik?"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Honesty", meta = (ClampMin = "1"))
	float SuspiciousRouteRatio = 1.25f;

	/** Above this the passenger disputes the fare (−2 stars, pays capped amount). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Honesty", meta = (ClampMin = "1"))
	float DisputeRouteRatio = 1.6f;

	/** On dispute, passenger pays at most expected honest fare × this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Honesty", meta = (ClampMin = "0.5"))
	float DisputePayCap = 1.1f;

	/** "Üstü kalsın": happy passengers round up to the next multiple of this (kuruş). 1000 = 10 TL. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tips", meta = (ClampMin = "0"))
	int32 TipRoundUpKurus = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tips", meta = (ClampMin = "0", ClampMax = "1"))
	float TipChanceAtFiveStars = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tips", meta = (ClampMin = "0", ClampMax = "1"))
	float TipChanceAtFourStars = 0.2f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TaxiRidePolicy"), GetFName());
	}
};
