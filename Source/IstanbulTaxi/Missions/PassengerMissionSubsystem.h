#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Taximeter/TaximeterComponent.h"
#include "PassengerMissionSubsystem.generated.h"

class UTaxiRidePolicyDataAsset;

UENUM(BlueprintType)
enum class ETaxiRidePhase : uint8
{
	None,
	ToPickup,
	Onboard,
	Completed,
	Cancelled
};

/** A single ride. M6 fills Archetype/Deadline; M2 uses locations + optional optimal distance. */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FTaxiRideRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ride") FGuid RideId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ride") FText PickupName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ride") FText DropoffName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ride") FVector PickupLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ride") FVector DropoffLocation = FVector::ZeroVector;
	/** Shortest road distance (km). 0 = estimate from straight line × policy factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ride") float OptimalDistanceKm = 0.0f;
	/** Passenger archetype DataAsset (M6). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ride") TSoftObjectPtr<UPrimaryDataAsset> Archetype;
};

USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FTaxiRideResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ride") FGuid RideId;
	UPROPERTY(BlueprintReadOnly, Category = "Ride") FTaxiFareReceipt Receipt;
	/** Honest expected fare for the optimal route (+ tolls actually crossed). */
	UPROPERTY(BlueprintReadOnly, Category = "Ride") int32 ExpectedFareKurus = 0;
	/** What the passenger actually hands over for the fare. */
	UPROPERTY(BlueprintReadOnly, Category = "Ride") int32 PaidFareKurus = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ride") int32 TipKurus = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Ride") int32 Rating = 5;
	UPROPERTY(BlueprintReadOnly, Category = "Ride") float OptimalDistanceKm = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Ride") float RouteRatio = 1.0f;
	/** Reasons, e.g. Ride.Note.MeterNotStarted, Ride.Note.LongRoute, Ride.Note.Disputed. */
	UPROPERTY(BlueprintReadOnly, Category = "Ride") FGameplayTagContainer Notes;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRidePhaseChanged, const FTaxiRideRequest&, Ride, ETaxiRidePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRidePassengerRemark, const FTaxiRideRequest&, Ride, FGameplayTag, RemarkTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRideCompleted, const FTaxiRideResult&, Result);

/**
 * Ride lifecycle + fare settlement (M2 slice). Taximeters register themselves; this subsystem
 * listens to their delegates — the meter never knows about missions beyond registration.
 * Economy (M7) listens to OnRideCompleted to credit the wallet. M6 adds hailing, app requests,
 * archetypes, comfort and dialogue on top of this same lifecycle.
 */
UCLASS()
class ISTANBULTAXI_API UPassengerMissionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPassengerMissionSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	void RegisterTaximeter(UTaximeterComponent* Meter);
	void UnregisterTaximeter(UTaximeterComponent* Meter);

	/** Offer/accept a ride. Returns false if one is already active. */
	UFUNCTION(BlueprintCallable, Category = "Missions")
	bool StartRide(FTaxiRideRequest Request);

	/** Debug helper: ride from the taxi's position + offsets (cm). */
	UFUNCTION(BlueprintCallable, Category = "Missions|Debug")
	bool StartDebugRide(FVector PickupOffset = FVector(3000, 0, 0), FVector DropoffOffset = FVector(60000, 20000, 0));

	UFUNCTION(BlueprintCallable, Category = "Missions")
	void CancelRide();

	UFUNCTION(BlueprintPure, Category = "Missions") ETaxiRidePhase GetPhase() const { return Phase; }
	UFUNCTION(BlueprintPure, Category = "Missions") const FTaxiRideRequest& GetActiveRide() const { return ActiveRide; }
	UFUNCTION(BlueprintPure, Category = "Missions") FVector GetCurrentObjectiveLocation() const;

	UPROPERTY(BlueprintAssignable, Category = "Missions") FOnRidePhaseChanged OnRidePhaseChanged;
	UPROPERTY(BlueprintAssignable, Category = "Missions") FOnRidePassengerRemark OnPassengerRemark;
	UPROPERTY(BlueprintAssignable, Category = "Missions") FOnRideCompleted OnRideCompleted;

private:
	void SetPhase(ETaxiRidePhase NewPhase);
	bool IsTaxiStoppedNear(const FVector& Location) const;
	void FinishRide();
	FTaxiRideResult SettleFare(const FTaxiFareReceipt& Receipt) const;

	UFUNCTION()
	void HandleMeterStateChanged(ETaximeterState NewState, FGameplayTag StateTag);

	UPROPERTY(Transient) TObjectPtr<UTaximeterComponent> Taximeter;
	UPROPERTY(Transient) TObjectPtr<const UTaxiRidePolicyDataAsset> Policy;

	FTaxiRideRequest ActiveRide;
	ETaxiRidePhase Phase = ETaxiRidePhase::None;
	float StopTimer = 0.0f;
	float OnboardTime = 0.0f;
	bool bMeterStartedThisRide = false;
	bool bMeterComplaintRaised = false;
};
