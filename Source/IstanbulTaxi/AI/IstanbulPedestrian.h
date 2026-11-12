// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IstanbulPedestrian.generated.h"

class UProceduralMeshComponent;
class UCapsuleComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EPedestrianRole : uint8
{
	SidewalkWalker     UMETA(DisplayName = "Kaldırım Yayası"),
	CrosswalkPedestrian UMETA(DisplayName = "Yaya Geçidi Yayası"),
	HailingPassenger   UMETA(DisplayName = "El Sallayan Müşteri")
};

/**
 * Lightweight procedural 3D articulated Istanbul pedestrian actor.
 * Articulated head, torso, swinging arms and legs, varied Turkish clothing palettes,
 * crosswalk street crossing with vehicle dodging & Turkish shouting, and visual passenger hailing.
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulPedestrian : public AActor
{
	GENERATED_BODY()

public:
	AIstanbulPedestrian();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void InitializePedestrian(EPedestrianRole InRole, const TArray<FVector>& InWaypoints, int32 InSeed = 0);
	void SetAsHailingPassenger(const FVector& InPickupPoint, const FGuid& InAssociatedRideId);

	EPedestrianRole GetRole() const { return Role; }
	bool IsBoarded() const { return bHasBoardedTaxi; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> CapsuleComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> LeftArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> RightArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> LeftLegMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> RightLegMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> HailingMarkerMesh;

private:
	EPedestrianRole Role = EPedestrianRole::SidewalkWalker;
	TArray<FVector> PatrolWaypoints;
	int32 CurrentWaypointIdx = 0;
	float WalkSpeedCms = 135.0f; // ~4.8 km/h human walking pace
	float WalkCycleTime = 0.0f;
	float DodgeCooldown = 0.0f;
	bool bIsDodging = false;
	FVector DodgeDirection = FVector::ZeroVector;

	// Hailing passenger state
	FGuid AssociatedRideId;
	bool bIsWalkingToTaxi = false;
	bool bHasBoardedTaxi = false;
	float HailingArmWaveTime = 0.0f;
	TWeakObjectPtr<AActor> TargetTaxi;

	FLinearColor ShirtColor;
	FLinearColor PantsColor;
	FLinearColor SkinColor;

	void BuildArticulatedBody(int32 Seed);
	void UpdateWalkAnimation(float DeltaTime, float SpeedRatio);
	void CheckVehicleDodge(float DeltaTime);
};
