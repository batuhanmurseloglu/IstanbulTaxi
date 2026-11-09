#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IstanbulTrafficManager.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class USoundBase;

/**
 * Canonical Istanbul vehicle visual archetypes (Task 2.1).
 */
UENUM(BlueprintType)
enum class EIstanbulVehicleType : uint8
{
	YellowTaxi  UMETA(DisplayName = "Sarı Taksi (#F5C400)"),
	IETTBus     UMETA(DisplayName = "İETT Belediye Otobüsü (#C8102E / Beyaz)"),
	Dolmus      UMETA(DisplayName = "Sarı Minibüs / Dolmuş"),
	DeliveryVan UMETA(DisplayName = "Kargo / Panelvan (Çift Sıra Park)")
};

using ETrafficVehicleType = EIstanbulVehicleType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTrafficHornHonked, AActor*, HonkingVehicle, FVector, WorldLocation, FString, ReasonText);

/**
 * Mid-poly procedural Istanbul AI Traffic Vehicle.
 * Builds its entire body into a single-draw-call UProceduralMeshComponent section via
 * BuildVisualMesh(EIstanbulVehicleType) using FIstanbulMeshUtils::AppendBeveledBox & AppendCylinder.
 */
UCLASS()
class ISTANBULTAXI_API ATrafficVehicle : public AActor
{
	GENERATED_BODY()

public:
	ATrafficVehicle();

	virtual void Tick(float DeltaSeconds) override;

	/** Builds the mid-poly beveled procedural mesh for the specified Istanbul vehicle archetype. */
	UFUNCTION(BlueprintCallable, Category = "Traffic|Visuals")
	void BuildVisualMesh(EIstanbulVehicleType InType);

	void InitializeTrafficVehicle(EIstanbulVehicleType InType, const TArray<FVector>& InWaypoints,
		float InCruiseSpeedKmh, float InLateralOffsetCm, bool bInStationaryHazard = false, int32 StartWaypointIdx = 0);

	void SetTargetLateralOffset(float NewLateralOffsetCm) { TargetLateralOffsetCm = NewLateralOffsetCm; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	EIstanbulVehicleType GetVehicleType() const { return VehicleType; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool IsHonkingNow() const { return HornVisualTimer > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool AreHazardsBlinking() const { return bHazardLightsActive; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	FString GetDisplayLabel() const;

	UPROPERTY(BlueprintAssignable, Category = "Traffic")
	FOnTrafficHornHonked OnHornHonked;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Single-draw-call merged mid-poly beveled body mesh (hood, cabin, trunk, livery bands, lights). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> VehicleProcMesh;

	/** Rotating wheel cylinders. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	/** Blinking hazard lamps toggled by timer for double-parked delivery vans and dolmuş stops. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> HazardLampMeshes;

private:
	void TintMesh(UStaticMeshComponent* Comp, const FLinearColor& Color);
	void TriggerHorn(const FString& Reason);

	EIstanbulVehicleType VehicleType = EIstanbulVehicleType::YellowTaxi;
	TArray<FVector> Waypoints;
	int32 CurrentWaypointIdx = 0;

	float CruiseSpeedCms = 1100.0f;
	float CurrentSpeedCms = 0.0f;
	float CurrentLateralOffsetCm = 0.0f;
	float TargetLateralOffsetCm = 0.0f;

	bool bStationaryDoubleParked = false;
	bool bHazardLightsActive = false;

	// Dolmuş ad-hoc stop state
	float AdHocStopTimer = 0.0f;
	float NextAdHocStopCooldown = 14.0f;

	// Deadlock recovery / lane-change / impatience timers
	float LaneChangeCooldown = 9.0f;
	float BlockedDuration = 0.0f;
	float DeadlockTimer = 0.0f;
	float ReverseEscapeTimer = 0.0f;
	float HornCooldown = 0.0f;
	float HornVisualTimer = 0.0f;
	float HazardBlinkAccum = 0.0f;
	float WheelSpinDeg = 0.0f;
};

/**
 * Spawns and coordinates the city's active traffic loop (Main Boulevard + Side Street + Double-Parked hazards).
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulTrafficManager : public AActor
{
	GENERATED_BODY()

public:
	AIstanbulTrafficManager();

	void PopulateIstanbulTraffic(const FVector& Origin, const TArray<FVector>& BoulevardLoopRight,
		const TArray<FVector>& BoulevardLoopLeft, const TArray<FVector>& SideStreetLoop);

	UFUNCTION(BlueprintPure, Category = "Traffic")
	const TArray<ATrafficVehicle*>& GetSpawnedVehicles() const { return Vehicles; }

	UPROPERTY(BlueprintAssignable, Category = "Traffic")
	FOnTrafficHornHonked OnAnyVehicleHonked;

private:
	UFUNCTION()
	void ForwardHornEvent(AActor* HonkingVehicle, FVector WorldLocation, FString ReasonText);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATrafficVehicle>> Vehicles;
};
