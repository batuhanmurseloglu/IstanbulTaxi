#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IstanbulTrafficManager.generated.h"

class UStaticMeshComponent;
class USoundBase;

UENUM(BlueprintType)
enum class ETrafficVehicleType : uint8
{
	Sedan        UMETA(DisplayName = "Binek Otomobil"),
	YellowTaxi   UMETA(DisplayName = "Sarı Taksi"),
	Dolmus       UMETA(DisplayName = "Sarı/Mavi Dolmuş / Minibüs"),
	IETTBus      UMETA(DisplayName = "İETT Belediye Otobüsü"),
	DeliveryVan  UMETA(DisplayName = "Kargo / Panelvan")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTrafficHornHonked, AActor*, HonkingVehicle, FVector, WorldLocation, FString, ReasonText);

/**
 * Lightweight C++ Istanbul AI Traffic Vehicle.
 * Moves along spline/waypoint loops across main boulevards and narrow side streets,
 * exhibiting authentic Istanbul behaviors:
 *  - Stop-and-go congestion (dur-kalk) & dynamic headway braking behind player or other AI
 *  - Dolmuş ad-hoc curb stops with blinking hazard lights (dörtlüler)
 *  - Aggressive nose-in lane changing between parallel lanes
 *  - Double-parked hazard mode in narrow side streets (requiring the player to carefully squeeze past)
 *  - Impatient horn honking (audio + visual indicator) when blocked
 */
UCLASS()
class ISTANBULTAXI_API ATrafficVehicle : public AActor
{
	GENERATED_BODY()

public:
	ATrafficVehicle();

	virtual void Tick(float DeltaSeconds) override;

	void InitializeTrafficVehicle(ETrafficVehicleType InType, const TArray<FVector>& InWaypoints,
		float InCruiseSpeedKmh, float InLateralOffsetCm, bool bInStationaryHazard = false, int32 StartWaypointIdx = 0);

	/** Allows switching lanes dynamically by adjusting LateralOffsetCm. */
	void SetTargetLateralOffset(float NewLateralOffsetCm) { TargetLateralOffsetCm = NewLateralOffsetCm; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	ETrafficVehicleType GetVehicleType() const { return VehicleType; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CabinMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RoofStripeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> HazardLampMeshes;

private:
	void BuildVisualsForType(ETrafficVehicleType InType);
	void TintMesh(UStaticMeshComponent* Comp, const FLinearColor& Color);
	void TriggerHorn(const FString& Reason);

	ETrafficVehicleType VehicleType = ETrafficVehicleType::Sedan;
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

	// Lane-change & impatience timers
	float LaneChangeCooldown = 9.0f;
	float BlockedDuration = 0.0f;
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

	/** Spawns the full set of moving & double-parked Istanbul vehicles around Origin. */
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
