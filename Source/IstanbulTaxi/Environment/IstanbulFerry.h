// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "IstanbulFerry.generated.h"

class UProceduralMeshComponent;
class UAudioComponent;

/**
 * Iconic procedural Istanbul Passenger Ferry (Şehir Hatları Vapuru).
 * Multi-deck passenger cabins, warm interior window illumination, black-and-yellow smokestack funnel,
 * moving along Bosphorus sea routes and periodically sounding deep resonant fog horn (Vapur Düdüğü).
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulFerry : public AActor
{
	GENERATED_BODY()

public:
	AIstanbulFerry();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void InitializeRoute(const FString& InFerryName, const TArray<FVector>& InRouteWaypoints, float InCruiseSpeedKnots = 12.0f);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> HullProcMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> SuperstructureProcMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> FunnelAndMastsProcMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> WakeFoamProcMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> HornAudio;

private:
	FString FerryName = TEXT("ŞH-BARIŞ MANÇO");
	TArray<FVector> RouteWaypoints;
	int32 CurrentTargetWaypoint = 0;
	float CruiseSpeedCms = 580.0f; // ~11.3 knots
	float HornTimer = 25.0f;
	float WakeTime = 0.0f;

	void BuildProceduralFerryMesh();
	void SoundFerryHorn();
};
