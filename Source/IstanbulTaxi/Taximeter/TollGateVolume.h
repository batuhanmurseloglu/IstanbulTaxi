#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "TollGateVolume.generated.h"

class UBoxComponent;
class UArrowComponent;

/**
 * HGS toll gate. Place across a bridge/tunnel carriageway, pick the DT_Tolls row (or use FallbackToll).
 * For one-way-charged crossings, rotate the actor so the ARROW points toward ASIA.
 */
UCLASS()
class ISTANBULTAXI_API ATollGateVolume : public AActor
{
	GENERATED_BODY()

public:
	ATollGateVolume();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll", meta = (RowType = "/Script/IstanbulTaxi.TaxiTollRow"))
	FDataTableRowHandle Toll;

	/** Used automatically when Toll DataTable is not assigned yet (defaults to 15 Temmuz Şehitler Köprüsü = 59,00 TL). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toll")
	FTaxiTollRow FallbackToll;

	/** Same vehicle can't be charged twice by this gate within this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toll", meta = (ClampMin = "0"))
	float RechargeCooldown = 10.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Toll") TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Toll") TObjectPtr<UArrowComponent> TowardAsiaArrow;

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	TMap<TWeakObjectPtr<AActor>, float> LastChargeTime;
};
