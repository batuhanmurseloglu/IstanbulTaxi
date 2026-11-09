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

/**
 * Procedural Bosphorus Strait Water Volume (Task 1.3):
 *  - 64x64 subdivided UProceduralMeshComponent water surface with layered sine-wave vertex displacement,
 *    shore-to-deep vertex color gradient (turquoise -> deep Bosphorus navy), and UV scrolling at a capped rate.
 *  - Coastal quay wall with stone coping edge, HISM bollards, and guard rail along the Eminönü-Karaköy shore.
 *  - Distant extruded silhouette layer at 2.2-3.2 km (Süleymaniye Mosque with 4 minarets, Galata Tower,
 *    and 15 Temmuz Bosphorus Suspension Bridge towers + parabolic cable curve) built from point arrays.
 *  - Physics-aware UBoxComponent kill/respawn trigger that teleports the player taxi back to its last dry checkpoint.
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
	FVector2D WaterExtentCm = FVector2D(46000.0f, 24000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Water")
	float WaveAmplitudeCm = 18.0f;

	/** Cap vertex update frequency (seconds between mesh section updates) for frame-time sanity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Performance")
	float WaveUpdateInterval = 0.066f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Color")
	FLinearColor ShallowTurquoiseColor = FLinearColor(0.04f, 0.34f, 0.42f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bosphorus|Color")
	FLinearColor DeepBosphorusNavyColor = FLinearColor(0.01f, 0.09f, 0.20f, 1.0f);

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
