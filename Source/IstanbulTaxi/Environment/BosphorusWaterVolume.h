#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "BosphorusWaterVolume.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/**
 * Procedural Bosphorus Strait Water Volume (Task 1.3):
 *  - Massive 5000m x 5000m sea coverage at Z = -120cm (64x64 animated nearshore grid + 5000m horizon ocean plane),
 *    vivid deep-turquoise/navy PBR water material (Roughness=0.08, Specular=0.92), and white shoreline foam strip.
 *  - Low-profile coastal quay curb and bollard promenade with clear unobstructed water views and Galata Bridge opening.
 *  - Iconic Maiden's Tower (Kız Kulesi) islet out in the Bosphorus with a glowing lighthouse beacon.
 *  - Distant extruded silhouette layer at 2.2-3.2 km + physics-aware water kill/respawn trigger.
 */
UCLASS()
class ISTANBULTAXI_API ABosphorusWaterVolume : public AActor
{
	GENERATED_BODY()

public:
	ABosphorusWaterVolume();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Grid resolution along X and Y (default 64x64 = 4,096 quads). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Water", meta = (ClampMin = "16", ClampMax = "128"))
	int32 GridResolution = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Water")
	FVector2D WaterExtentCm = FVector2D(500000.0f, 500000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Water")
	float WaveAmplitudeCm = 22.0f;

	/** Cap vertex update frequency (seconds between mesh section updates) for frame-time sanity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Performance")
	float WaveUpdateInterval = 0.066f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Color")
	FLinearColor ShallowTurquoiseColor = FLinearColor(0.02f, 0.48f, 0.58f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Color")
	FLinearColor DeepBosphorusNavyColor = FLinearColor(0.01f, 0.14f, 0.32f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Color")
	FLinearColor DistantSilhouetteColor = FLinearColor(0.11f, 0.16f, 0.24f, 1.0f);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<USceneComponent> Root;

	/** 64x64 subdivided animated water surface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<UProceduralMeshComponent> WaterProcMesh;

	/** Coastal stone quay ledge, guard rail, and distant 2-4 km extruded silhouettes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<UProceduralMeshComponent> QuayAndSilhouetteProcMesh;

	/** HISM bollards along the coastal quay ledge. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> QuayBollardHISM;

	/** Overlap trigger volume that respawns the player vehicle at the last dry checkpoint if it falls into the strait. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<UBoxComponent> WaterKillTrigger;

	/** Steaming Şehir Hatları Ferry (Vapur) cruising along the strait. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<USceneComponent> FerryRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TArray<TObjectPtr<UStaticMeshComponent>> FerryParts;

	/** Glowing lighthouse lantern beacon atop Maiden's Tower (Kız Kulesi). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<UPointLightComponent> KizKulesiBeaconLight;

private:
	void BuildWaterGridMesh();
	void UpdateWaterWaveVertices(float TimeSec);
	void BuildQuayWallAndDistantSilhouettes();

	UFUNCTION()
	void OnWaterKillBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UStaticMeshComponent* AddPart(const FName& Name, USceneComponent* Parent, const UStaticMesh* Mesh,
		const FVector& RelLoc, const FVector& RelScale, const FRotator& RelRot = FRotator::ZeroRotator);

	UMaterialInstanceDynamic* TintPart(UStaticMeshComponent* Comp, const FLinearColor& Color);

	// Cached 64x64 water buffers for fast in-place UpdateMeshSection_LinearColor
	TArray<FVector> BaseWaterVertices;
	TArray<FVector> AnimatedWaterVertices;
	TArray<int32> WaterTriangles;
	TArray<FVector> WaterNormals;
	TArray<FVector2D> BaseWaterUVs;
	TArray<FVector2D> AnimatedWaterUVs;
	TArray<FLinearColor> WaterVertexColors;
	TArray<FProcMeshTangent> WaterTangents;

	FTransform LastSafeDryTransform;
	float WaveTime = 0.0f;
	float WaveUpdateAccum = 0.0f;
	float CheckpointSampleAccum = 0.0f;
	float FerryProgress = 0.0f;
};
