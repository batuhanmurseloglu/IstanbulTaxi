#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Environment/IstanbulBuildingBlock.h"
#include "Environment/DestructibleStreetProp.h"
#include "TestSceneBuilder.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UPointLightComponent;
class AIstanbulTrafficManager;
class AIstanbulBuildingBlock;
class ADestructibleStreetProp;

/**
 * Data-driven specification for a single Istanbul road or alley segment.
 */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FIstanbulRoadSegmentSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	FName SegmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	FVector End = FVector(1000.0f, 0.0f, 0.0f);

	/** Drivable roadway width in meters (4.5-6.0m for narrow old-town alleys, 18-22m for boulevards). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	float WidthM = 8.0f;

	/** Longitudinal slope grade in degrees (computed automatically or overridden, 12-18 deg for Cihangir/Galata slopes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	float GradeDegrees = 0.0f;

	/** Raised concrete sidewalk curb height in centimeters (15-25cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	float SidewalkHeightCm = 20.0f;

	/** Surface tag used by UTaxiSurfaceComponent (Surface_Asphalt, Surface_Cobblestone, Surface_TramRail). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	FName SurfaceType = TEXT("Surface_Asphalt");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Road")
	bool bOneWay = false;

	FIstanbulRoadSegmentSpec() = default;
	FIstanbulRoadSegmentSpec(FName InId, const FVector& InStart, const FVector& InEnd, float InWidthM,
		float InSidewalkHeightCm, FName InSurfaceType, bool bInOneWay = false)
		: SegmentId(InId), Start(InStart), End(InEnd), WidthM(InWidthM),
		  SidewalkHeightCm(InSidewalkHeightCm), SurfaceType(InSurfaceType), bOneWay(bInOneWay)
	{
		const float Run2D = FVector::Dist2D(InStart, InEnd);
		GradeDegrees = (Run2D > 1.0f) ? FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(InEnd.Z - InStart.Z), Run2D)) : 0.0f;
	}
};

/**
 * Automated Istanbul Urban Environment & Prototype Builder.
 * Builds a massive, interconnected multi-district Istanbul network:
 *   1. Eminönü Waterfront & Double-Deck Galata Bridge -> Karaköy Loop
 *   2. Steep Galata & Cihangir Cobblestone Slopes (12-17.5 deg) & Galata Kulesi Summit
 *   3. Sirkeci & Sultanahmet Historic Boulevard, Meydan Plaza, Dikilitaş & Mosque Complex
 *   4. Beşiktaş Coastal Highway, Ferry Piers (Vapur İskelesi) & Elevated Bosphorus Bridge Macro-Loop
 */
UCLASS()
class ISTANBULTAXI_API ATestSceneBuilder : public AActor
{
	GENERATED_BODY()

public:
	ATestSceneBuilder();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Istanbul|Scene")
	bool bBuildTestEnvironment = true;

	/** Deterministic seed used by RebuildScene(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Istanbul|Scene")
	int32 Seed = 1453;

	/** Data-driven road network specs (automatically populated with default Istanbul corridor if empty). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Istanbul|Geography")
	TArray<FIstanbulRoadSegmentSpec> RoadSegments;

	/** Data-driven district zone specs (Waterfront, WideAvenue, Plaza, NarrowAlley, HillResidential). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Istanbul|Geography")
	TArray<FIstanbulDistrictSpec> Districts;

	/** Data-driven architectural building specs spawned via AIstanbulBuildingBlock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Istanbul|Geography")
	TArray<FIstanbulBuildingSpec> BuildingSpecs;

	/** Curb height default (cm) for generated sidewalks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Istanbul|Geography")
	float DefaultCurbHeightCm = 20.0f;

	/** Distance (cm) within which streetlamp point lights are dynamically enabled around the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Istanbul|Atmosphere")
	float ActiveStreetLightRadiusCm = 7500.0f;

	/** Destroys all previously generated scene actors/sections and regenerates deterministically from Seed. */
	UFUNCTION(BlueprintCallable, Category = "Istanbul|Scene")
	void RebuildScene();

	UFUNCTION(BlueprintCallable, Category = "Istanbul|Scene")
	void BuildPrototypeScene();

	/**
	 * Automated Quality Gate: validates node Z continuity, minimum free roadway width (>= 3.5m),
	 * maximum slope grade (<= 18.0 deg), and building/cumba corridor clearance. Returns true if 0 violations.
	 */
	UFUNCTION(BlueprintCallable, Category = "Istanbul|Validation")
	bool ValidateNetwork();

	int32 GetSpawnedActorCount() const { return SpawnedActorCount; }
	int32 GetSpawnedHISMInstanceCount() const { return SpawnedHISMInstanceCount; }
	int32 GetSpawnedProcMeshSectionCount() const { return SpawnedProcMeshSectionCount; }

	static const FName GeneratedActorTag;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Smooth procedural mesh for asphalt boulevards, Galata Bridge deck, sidewalks, stone walls, cables, and lane markings. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> CorridorProcMesh;

	/** Smooth procedural mesh tagged Surface_Cobblestone for steep 12-18 deg hills and old-town alleys. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> CobblestoneProcMesh;

	/** HISM for yellow-black bollards, streetlamp poles, Galata Bridge fishing rods, and café props. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BollardHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LampPoleHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FishingRodHISM;

	/** Pool of near-player warm streetlamp point lights culled by distance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UPointLightComponent>> NearLampLights;

private:
	void PopulateDefaultRoadSpecs(const FVector& Origin);
	void PopulateDefaultDistrictsAndBuildings(const FVector& Origin);
	int32 SolveAndBuildJunctions(struct FIstanbulMeshSectionData& AsphaltRoadSection,
		struct FIstanbulMeshSectionData& CobbleRoadSection,
		struct FIstanbulMeshSectionData& SidewalkSection,
		const FLinearColor& AsphaltCol, const FLinearColor& CobbleCol, const FLinearColor& SidewalkCol);
	void SpawnWorldBoundaryBlockers(const FVector& Origin);

	AActor* SpawnShape(const TCHAR* ShapePath, const FString& Label, const FVector& Location,
		const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color,
		bool bCollidable = false, FName SurfaceActorTag = NAME_None);

	AActor* SpawnBox(const FString& Label, const FVector& Location, const FVector& Scale,
		const FLinearColor& Color, bool bCollidable = false, const FRotator& Rotation = FRotator::ZeroRotator,
		FName SurfaceActorTag = NAME_None);

	ADestructibleStreetProp* SpawnDestructibleProp(EStreetPropType PropType, const FString& Label,
		const FVector& Location, const FRotator& Rotation, bool bSignIsStop = false);

	void SpawnIstanbulApartmentBlock(const FString& NamePrefix, const FVector& BaseLocation, float YawDeg,
		int32 Floors, float WidthM, float DepthM, const FLinearColor& FacadeColor, bool bHasCumba, bool bHasShopAwning,
		FRandomStream& Rng);

	void SpawnParkedCurbCar(const FString& Label, const FVector& Location, const FRotator& Rotation, const FLinearColor& BodyColor);
	void SpawnIETTBusStop(const FString& Label, const FVector& Location, float YawDeg);
	void SpawnStreetLamp(const FString& Label, const FVector& Location, float YawDeg);
	void SpawnHistoricalStoneWall(const FString& Label, const FVector& CenterLocation, const FRotator& Rotation, float LengthM, float HeightM);
	void SpawnGalataBridgeAndApproach(const FVector& Origin);
	void SpawnOldTownSquareAndFountain(const FVector& SquareCenter);
	void SpawnSlopeStopSignAndSwitchback(const FVector& Origin);
	void SpawnGalataTowerLandmark(const FVector& RidgeBase);
	void SpawnBosphorusScenicOverlook(const FVector& Origin);
	void SpawnFerryPierAndVessel(const FString& PierPrefix, const FVector& PierLocation, float YawDeg);
	void SpawnDetailedMosqueComplex(const FVector& HilltopBase);
	void SpawnHGSBridgeCanopy(const FVector& GateCenter);
	void SpawnAnimatedPedestriansAndHailingPassenger(const FVector& Origin);
	void SpawnBosphorusMovingFerries(const FVector& Origin);

	TArray<FVector> StreetLampLightWorldPositions;
	int32 SpawnedActorCount = 0;
	int32 SpawnedHISMInstanceCount = 0;
	int32 SpawnedProcMeshSectionCount = 0;
	bool bBuilt = false;
};
