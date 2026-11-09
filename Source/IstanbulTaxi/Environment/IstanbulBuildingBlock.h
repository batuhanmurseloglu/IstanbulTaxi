#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "IstanbulBuildingBlock.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UBoxComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EIstanbulDistrictZoneType : uint8
{
	WideAvenue      UMETA(DisplayName = "Geniş Bulvar / Cadde (14-20m)"),
	NarrowAlley     UMETA(DisplayName = "Dar Ara Sokak (4.5-6m)"),
	Plaza           UMETA(DisplayName = "Meydan (Sultanahmet / Eminönü)"),
	Waterfront      UMETA(DisplayName = "Sahil / Kordon"),
	HillResidential UMETA(DisplayName = "Yokuş Konut / Cumbalı Sokak")
};

UENUM(BlueprintType)
enum class EIstanbulGroundFloorType : uint8
{
	ShopfrontAwning UMETA(DisplayName = "Esnaf Dükkan & Tente"),
	ArchedStoneDoor UMETA(DisplayName = "Kemerli Taş Kapı"),
	RusticatedStone UMETA(DisplayName = "Kesme Taş Zemin")
};

/**
 * Data-driven district zoning specification (Task 1.1 & 1.2).
 */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FIstanbulDistrictSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	FName DistrictName = TEXT("Eminonu");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	FBox Bounds = FBox(FVector(-5000, -5000, -500), FVector(5000, 5000, 5000));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	EIstanbulDistrictZoneType ZoneType = EIstanbulDistrictZoneType::WideAvenue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	int32 MinFloors = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	int32 MaxFloors = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	float Density = 1.0f;

	/** Atmosphere mood hint: >1.0 makes narrow alleys warmer and hazier; <1.0 makes waterfronts crisper. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	float FogDensityMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "District")
	float AmbientWarmthBias = 0.0f;
};

/**
 * Specification for a single attached Istanbul building facade within an AIstanbulBuildingBlock (Task 1.3).
 */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FIstanbulBuildingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FVector BaseLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	float YawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	float FrontageWidthM = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	float DepthM = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building", meta = (ClampMin = "2", ClampMax = "8"))
	int32 Floors = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	EIstanbulGroundFloorType GroundFloorType = EIstanbulGroundFloorType::ShopfrontAwning;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	bool bPitchedRoof = true;

	/** Projects 0.6-1.0m outward on slanted corbels (clamped to sidewalk clearance so it never intrudes into the road). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	bool bHasCumba = true;

	/** Maximum safe projection (cm) allowed before reaching the curb edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	float MaxAllowedProjectionCm = 95.0f;

	/** Optional spanning archway/passage over a narrow alley with >= 3.6m vertical drivable clearance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	bool bSpanAlleyGateway = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	float AlleySpanWidthM = 5.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	int32 StyleSeed = 101;
};

/**
 * Merged procedural city block generator (Task 1.3).
 * Batches an entire row/block of attached multi-story Istanbul buildings (stone ground floors, string courses,
 * pilasters, corbelled Cumbas, roof cornices, chimneys, and Turkish shop signs) into a single UProceduralMeshComponent
 * with cheap per-building UBoxComponent collision and distance-culled HISM detail props.
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulBuildingBlock : public AActor
{
	GENERATED_BODY()

public:
	AIstanbulBuildingBlock();

	/** Generates all buildings in BuildingSpecs into a single batched procedural mesh + HISM props. */
	UFUNCTION(BlueprintCallable, Category = "Istanbul|Architecture")
	void BuildBlockFromSpecs(const TArray<FIstanbulBuildingSpec>& InSpecs);

	UFUNCTION(BlueprintPure, Category = "Istanbul|Architecture")
	int32 GetGeneratedBuildingCount() const { return GeneratedBuildingCount; }

	UFUNCTION(BlueprintPure, Category = "Istanbul|Architecture")
	int32 GetGeneratedCumbaCount() const { return GeneratedCumbaCount; }

	UFUNCTION(BlueprintPure, Category = "Istanbul|Architecture")
	int32 GetHISMPropCount() const { return GeneratedHISMPropCount; }

	const TArray<FBox>& GetBuildingWorldCollisionBoxes() const { return BuildingCollisionBoxes; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Single batched procedural mesh for all facades, cumbas, cornices, roofs, and shop awnings in this block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> BatchedFacadeMesh;

	/** HISM for repeated AC outdoor units, window shutters, rooftop water tanks, and flower pots (distance-culled). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DetailCubeHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RooftopCylinderHISM;

	/** Simple box collision components (1 per building) so physics never collides against complex facade triangles. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UBoxComponent>> SimpleCollisionBoxes;

	/** Near-player distance-culled Turkish shop signs ("ECZANE", "FIRIN", "BAKKAL", "ÇAY OCAĞI"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UTextRenderComponent>> ShopSignTexts;

private:
	TArray<FBox> BuildingCollisionBoxes;
	int32 GeneratedBuildingCount = 0;
	int32 GeneratedCumbaCount = 0;
	int32 GeneratedHISMPropCount = 0;
};
