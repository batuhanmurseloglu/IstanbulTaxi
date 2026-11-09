#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TaximeterComponent.generated.h"

class UTaximeterTariffDataAsset;

UENUM(BlueprintType)
enum class ETaximeterState : uint8
{
	Free   UMETA(DisplayName = "Boş"),
	Hired  UMETA(DisplayName = "Dolu"),
	Paused UMETA(DisplayName = "Beklemede")
};

/** Itemised fare at the end of a hire. Money in kuruş. */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FTaxiFareReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Fare") int32 OpeningKurus = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fare") int32 DistanceKurus = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fare") int32 WaitingKurus = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fare") int32 TollsKurus = 0;
	/** Top-up applied so the meter part reaches the indi-bindi minimum. */
	UPROPERTY(BlueprintReadOnly, Category = "Fare") int32 MinimumTopUpKurus = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Fare") int32 TotalKurus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Fare") float DistanceKm = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Fare") float WaitingSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Fare") float DurationSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Fare") TArray<FGameplayTag> TollCrossings;
	/** False if the meter was never started for this ride. */
	UPROPERTY(BlueprintReadOnly, Category = "Fare") bool bValid = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaximeterStateChanged, ETaximeterState, NewState, FGameplayTag, StateTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaximeterFareUpdated, int32, DisplayedFareKurus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaximeterHireEnded, const FTaxiFareReceipt&, Receipt);
/** bAddedToFare = passenger reimburses; driver always pays HGS (Economy M7 deducts FeeKurus from wallet). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTaximeterTollCharged, FGameplayTag, Crossing, int32, FeeKurus, bool, bAddedToFare);

/**
 * İstanbul taximeter: Free → Hired ⇄ Paused → Free.
 * While Hired it bills distance, OR waiting time when slower than the tariff's threshold (≈12.5 km/h),
 * exactly like a real Turkish taksimetre. Tolls are added on top; indi-bindi minimum applied at the end.
 */
UCLASS(ClassGroup = (IstanbulTaxi), meta = (BlueprintSpawnableComponent))
class ISTANBULTAXI_API UTaximeterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTaximeterComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Overrides Project Settings default tariff (e.g. turquoise/black taxi upgrade). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taximeter")
	TObjectPtr<UTaximeterTariffDataAsset> Tariff;

	/** Ignore GPS-like jitter: speeds below this (km/h) count as fully stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taximeter", meta = (ClampMin = "0"))
	float StandstillSpeedKmh = 1.0f;

	// ---- Control ----
	UFUNCTION(BlueprintCallable, Category = "Taximeter") bool StartHire();
	UFUNCTION(BlueprintCallable, Category = "Taximeter") bool TogglePause();
	UFUNCTION(BlueprintCallable, Category = "Taximeter") FTaxiFareReceipt EndHire();

	/** Single-button behaviour for the driver: Free → Hired, Hired/Paused → End. */
	UFUNCTION(BlueprintCallable, Category = "Taximeter") void ToggleMeter();

	/** Cycles through the 4 real UKOME segments (Sarı -> Turkuaz -> 8+1 -> Siyah) for runtime testing. */
	UFUNCTION(BlueprintCallable, Category = "Taximeter") void CycleTariffPreset();

	/** Called by ATollGateVolume. */
	UFUNCTION(BlueprintCallable, Category = "Taximeter")
	void ChargeToll(FGameplayTag Crossing, int32 FeeKurus);

	// ---- Queries ----
	UFUNCTION(BlueprintPure, Category = "Taximeter") ETaximeterState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "Taximeter") FGameplayTag GetStateTag() const;
	UFUNCTION(BlueprintPure, Category = "Taximeter") bool IsRunning() const { return State != ETaximeterState::Free; }
	/** Raw running total incl. tolls, before the indi-bindi minimum. */
	UFUNCTION(BlueprintPure, Category = "Taximeter") int32 GetCurrentFareKurus() const;
	/** What the physical display shows (stepped). */
	UFUNCTION(BlueprintPure, Category = "Taximeter") int32 GetDisplayedFareKurus() const;
	UFUNCTION(BlueprintPure, Category = "Taximeter") float GetHireDistanceKm() const { return DistanceKm; }
	UFUNCTION(BlueprintPure, Category = "Taximeter") bool IsBillingWaitTime() const { return bBillingWait; }
	UFUNCTION(BlueprintPure, Category = "Taximeter") const UTaximeterTariffDataAsset* GetActiveTariff() const { return ActiveTariff; }

	/** "1.234,56" using Turkish number formatting. */
	UFUNCTION(BlueprintPure, Category = "Taximeter")
	static FText FormatKurus(int32 Kurus);

	UPROPERTY(BlueprintAssignable, Category = "Taximeter") FOnTaximeterStateChanged OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Taximeter") FOnTaximeterFareUpdated OnFareUpdated;
	UPROPERTY(BlueprintAssignable, Category = "Taximeter") FOnTaximeterHireEnded OnHireEnded;
	UPROPERTY(BlueprintAssignable, Category = "Taximeter") FOnTaximeterTollCharged OnTollCharged;

private:
	void SetState(ETaximeterState NewState);
	void ResetAccumulators();
	void BroadcastFareIfChanged();

	UPROPERTY(Transient)
	TObjectPtr<const UTaximeterTariffDataAsset> ActiveTariff;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTaximeterTariffDataAsset>> BuiltInTariffs;

	int32 BuiltInTariffIndex = 0;

	ETaximeterState State = ETaximeterState::Free;
	double DistanceChargeKurus = 0.0;
	double WaitingChargeKurus = 0.0;
	int32 TollsKurus = 0;
	float DistanceKm = 0.0f;
	float WaitingSeconds = 0.0f;
	float DurationSeconds = 0.0f;
	bool bBillingWait = false;
	int32 LastBroadcastFare = -1;
	TArray<FGameplayTag> TollCrossings;
};
