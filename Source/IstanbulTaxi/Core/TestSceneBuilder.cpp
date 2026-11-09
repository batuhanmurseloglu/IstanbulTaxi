#include "Core/TestSceneBuilder.h"
#include "Core/IstanbulMeshUtils.h"
#include "Environment/TimeOfDayActor.h"
#include "Environment/TimeOfDaySubsystem.h"
#include "Environment/BosphorusWaterVolume.h"
#include "Taximeter/TollGateVolume.h"
#include "Audio/DistrictAmbienceVolume.h"
#include "Audio/DistrictAmbienceDataAsset.h"
#include "Audio/MosqueActor.h"
#include "Traffic/IstanbulTrafficManager.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "IstanbulTaxi.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Atmosphere/AtmosphericFog.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Vehicle/TaxiPawn.h"

const FName ATestSceneBuilder::GeneratedActorTag(TEXT("IstanbulGenerated"));

// =============================================================================
// Console Commands (Task 3: Istanbul.Rebuild, Istanbul.TeleportToSlope,
//                   Istanbul.TeleportToBosphorus, Istanbul.ToggleDebugSlope)
// =============================================================================
static FAutoConsoleCommandWithWorld GCmdIstanbulRebuild(
	TEXT("Istanbul.Rebuild"),
	TEXT("Destroys and deterministically regenerates the procedural Istanbul scene from ATestSceneBuilder::Seed."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		if (ATestSceneBuilder* Builder = Cast<ATestSceneBuilder>(UGameplayStatics::GetActorOfClass(World, ATestSceneBuilder::StaticClass())))
		{
			Builder->RebuildScene();
		}
	}));

static FAutoConsoleCommandWithWorld GCmdIstanbulTeleportSlope(
	TEXT("Istanbul.TeleportToSlope"),
	TEXT("Teleports the player taxi to the base of the 17.6-deg Cihangir switchback / stop-sign slope."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(Pawn)) Taxi->ResetVehicle();
			Pawn->SetActorLocationAndRotation(FVector(13500.0f, 4150.0f, 120.0f), FRotator(17.5f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogIstanbulTaxi, Log, TEXT("Istanbul.TeleportToSlope -> Teleported player to 17.6-deg Cihangir slope base."));
		}
	}));

static FAutoConsoleCommandWithWorld GCmdIstanbulTeleportBosphorus(
	TEXT("Istanbul.TeleportToBosphorus"),
	TEXT("Teleports the player taxi to the Bosphorus Scenic Overlook / Coastal Quay ledge."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(Pawn)) Taxi->ResetVehicle();
			Pawn->SetActorLocationAndRotation(FVector(17200.0f, -650.0f, 45.0f), FRotator(0.0f, -25.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogIstanbulTaxi, Log, TEXT("Istanbul.TeleportToBosphorus -> Teleported player to Bosphorus Scenic Quay."));
		}
	}));

static FAutoConsoleCommandWithWorld GCmdIstanbulToggleDebugSlope(
	TEXT("Istanbul.ToggleDebugSlope"),
	TEXT("Toggles continuous slope angle & grade % debug logging under the player taxi."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			const bool bNewState = !Taxi->IsDebugSlopeTelemetryEnabled();
			Taxi->SetDebugSlopeTelemetry(bNewState);
			UE_LOG(LogIstanbulTaxi, Log, TEXT("Istanbul.ToggleDebugSlope -> %s (CurrentSlope=%.2f deg)"),
				bNewState ? TEXT("ENABLED") : TEXT("DISABLED"), Taxi->GetCurrentSlopeAngleDegrees());
		}
	}));

ATestSceneBuilder::ATestSceneBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f; // Low-frequency light distance culling

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	CorridorProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CorridorProcMesh"));
	CorridorProcMesh->SetupAttachment(SceneRoot);
	CorridorProcMesh->bUseComplexAsSimpleCollision = true;
	CorridorProcMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CorridorProcMesh->ComponentTags.Add(TEXT("Surface_Asphalt"));

	CobblestoneProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CobblestoneProcMesh"));
	CobblestoneProcMesh->SetupAttachment(SceneRoot);
	CobblestoneProcMesh->bUseComplexAsSimpleCollision = true;
	CobblestoneProcMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CobblestoneProcMesh->ComponentTags.Add(TEXT("Surface_Cobblestone"));

	BollardHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("BollardHISM"));
	BollardHISM->SetupAttachment(SceneRoot);
	BollardHISM->SetCollisionProfileName(TEXT("BlockAll"));

	LampPoleHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LampPoleHISM"));
	LampPoleHISM->SetupAttachment(SceneRoot);
	LampPoleHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FishingRodHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FishingRodHISM"));
	FishingRodHISM->SetupAttachment(SceneRoot);
	FishingRodHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 8 pooled dynamic point lights that move to the nearest streetlamps around the player
	for (int32 i = 0; i < 8; ++i)
	{
		UPointLightComponent* PL = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("NearStreetLight_%d"), i));
		PL->SetupAttachment(SceneRoot);
		PL->SetIntensity(2800.0f);
		PL->SetAttenuationRadius(1800.0f);
		PL->SetLightColor(FLinearColor(1.0f, 0.78f, 0.42f));
		PL->SetCastShadows(false);
		NearLampLights.Add(PL);
	}
}

void ATestSceneBuilder::BeginPlay()
{
	Super::BeginPlay();
	if (bBuildTestEnvironment && !bBuilt)
	{
		RebuildScene();
	}
}

void ATestSceneBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Cheap proximity-based activation for the 8 pooled streetlamp point lights
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player || StreetLampLightWorldPositions.Num() == 0) return;

	const FVector PlayerLoc = Player->GetActorLocation();
	TArray<int32> SortedIndices;
	SortedIndices.Reserve(StreetLampLightWorldPositions.Num());
	for (int32 i = 0; i < StreetLampLightWorldPositions.Num(); ++i)
	{
		SortedIndices.Add(i);
	}
	SortedIndices.Sort([&](int32 A, int32 B)
	{
		return FVector::DistSquared(StreetLampLightWorldPositions[A], PlayerLoc)
			 < FVector::DistSquared(StreetLampLightWorldPositions[B], PlayerLoc);
	});

	const float MaxDistSq = FMath::Square(ActiveStreetLightRadiusCm);
	for (int32 i = 0; i < NearLampLights.Num(); ++i)
	{
		UPointLightComponent* PL = NearLampLights[i];
		if (!PL) continue;
		if (i < SortedIndices.Num())
		{
			const FVector& LampPos = StreetLampLightWorldPositions[SortedIndices[i]];
			const bool bInRange = FVector::DistSquared(LampPos, PlayerLoc) <= MaxDistSq;
			PL->SetVisibility(bInRange);
			if (bInRange)
			{
				PL->SetWorldLocation(LampPos);
			}
		}
		else
		{
			PL->SetVisibility(false);
		}
	}
}

void ATestSceneBuilder::RebuildScene()
{
	UWorld* W = GetWorld();
	if (!W) return;

	// 1. Destroy all previously generated actors with GeneratedActorTag
	TArray<AActor*> ExistingGenerated;
	UGameplayStatics::GetAllActorsWithTag(W, GeneratedActorTag, ExistingGenerated);
	for (AActor* Act : ExistingGenerated)
	{
		if (Act && Act != this)
		{
			Act->Destroy();
		}
	}

	// 2. Clear procedural mesh sections and HISM instances
	if (CorridorProcMesh)    CorridorProcMesh->ClearAllMeshSections();
	if (CobblestoneProcMesh) CobblestoneProcMesh->ClearAllMeshSections();
	if (BollardHISM)         BollardHISM->ClearInstances();
	if (LampPoleHISM)        LampPoleHISM->ClearInstances();
	if (FishingRodHISM)      FishingRodHISM->ClearInstances();
	StreetLampLightWorldPositions.Reset();
	SpawnedActorCount = 0;
	SpawnedHISMInstanceCount = 0;
	SpawnedProcMeshSectionCount = 0;
	bBuilt = false;

	BuildPrototypeScene();
}

void ATestSceneBuilder::PopulateDefaultRoadSpecs(const FVector& Origin)
{
	RoadSegments.Reset();

	// =========================================================================
	// 1. EMINÖNÜ -> SİRKECİ -> SULTANAHMET COASTAL & TRAM AVENUE (Z = 0)
	// Subdivided at exact junction nodes so every intersection shares identical (X,Y,Z).
	// =========================================================================
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Blvd_Eminonu_West"),
		Origin + FVector(-4000.0f, 0.0f, 0.0f),
		Origin + FVector(3500.0f, 0.0f, 0.0f),
		22.0f, DefaultCurbHeightCm, TEXT("Surface_Asphalt"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Blvd_Eminonu_Mid1"),
		Origin + FVector(3500.0f, 0.0f, 0.0f),
		Origin + FVector(7200.0f, 0.0f, 0.0f),
		22.0f, DefaultCurbHeightCm, TEXT("Surface_Asphalt"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Blvd_Eminonu_Mid2"),
		Origin + FVector(7200.0f, 0.0f, 0.0f),
		Origin + FVector(8800.0f, 0.0f, 0.0f),
		22.0f, DefaultCurbHeightCm, TEXT("Surface_Asphalt"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Blvd_Eminonu_Mid3"),
		Origin + FVector(8800.0f, 0.0f, 0.0f),
		Origin + FVector(16800.0f, 0.0f, 0.0f),
		22.0f, DefaultCurbHeightCm, TEXT("Surface_Asphalt"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Blvd_Eminonu_East"),
		Origin + FVector(16800.0f, 0.0f, 0.0f),
		Origin + FVector(20500.0f, 0.0f, 0.0f),
		22.0f, DefaultCurbHeightCm, TEXT("Surface_Asphalt"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Blvd_Sirkeci_Avenue"),
		Origin + FVector(20500.0f, 0.0f, 0.0f),
		Origin + FVector(24800.0f, 0.0f, 0.0f),
		20.0f, DefaultCurbHeightCm, TEXT("Surface_Asphalt"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Blvd_BridgeApproach_East"),
		Origin + FVector(24800.0f, 0.0f, 0.0f),
		Origin + FVector(38000.0f, 0.0f, 0.0f),
		20.0f, DefaultCurbHeightCm, TEXT("Surface_Asphalt"), false));

	// =========================================================================
	// 2. GALATA BRIDGE APPROACH & TWO-LEVEL GOLDEN HORN DECK
	// =========================================================================
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Bridge_GalataApproach"),
		Origin + FVector(8800.0f, 0.0f, 0.0f),
		Origin + FVector(8800.0f, -3600.0f, 260.0f),
		16.0f, 22.0f, TEXT("Surface_Asphalt"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Bridge_GalataMainDeck"),
		Origin + FVector(8800.0f, -3600.0f, 260.0f),
		Origin + FVector(8800.0f, -11600.0f, 260.0f),
		18.0f, 24.0f, TEXT("Surface_Asphalt"), false));

	// =========================================================================
	// 3. LOOP 1 & LOOP 2: TARİHİ YARIMADA (SİRKECİ -> SULTANAHMET PLAZA GRID)
	// =========================================================================
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("OldTown_WestAlley"),
		Origin + FVector(7200.0f, 0.0f, 0.0f),
		Origin + FVector(7200.0f, 3800.0f, 0.0f),
		5.4f, 18.0f, TEXT("Surface_Cobblestone"), true));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("OldTown_WestAlley_North"),
		Origin + FVector(7200.0f, 3800.0f, 0.0f),
		Origin + FVector(7200.0f, 4800.0f, 0.0f),
		5.0f, 18.0f, TEXT("Surface_Cobblestone"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("OldTown_CrossSpine_West"),
		Origin + FVector(7200.0f, 3800.0f, 0.0f),
		Origin + FVector(11600.0f, 3800.0f, 0.0f),
		5.8f, 18.0f, TEXT("Surface_Cobblestone"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("OldTown_CrossSpine_Mid"),
		Origin + FVector(11600.0f, 3800.0f, 0.0f),
		Origin + FVector(13500.0f, 3800.0f, 0.0f),
		5.8f, 18.0f, TEXT("Surface_Cobblestone"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("OldTown_CrossSpine_Plaza"),
		Origin + FVector(13500.0f, 3800.0f, 0.0f),
		Origin + FVector(16800.0f, 3800.0f, 0.0f),
		6.0f, 18.0f, TEXT("Surface_Cobblestone"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("OldTown_EastReturn"),
		Origin + FVector(16800.0f, 3800.0f, 0.0f),
		Origin + FVector(16800.0f, 0.0f, 0.0f),
		5.4f, 18.0f, TEXT("Surface_Cobblestone"), true));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("OldTown_DeadEndAlley"),
		Origin + FVector(11600.0f, 3800.0f, 0.0f),
		Origin + FVector(11600.0f, 5650.0f, 0.0f),
		4.6f, 20.0f, TEXT("Surface_Cobblestone"), false));

	// Sirkeci -> Sultanahmet Wide Avenue Loop Extension (completes Loop 2)
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Sultanahmet_EastAvenue"),
		Origin + FVector(16800.0f, 3800.0f, 0.0f),
		Origin + FVector(24800.0f, 3800.0f, 0.0f),
		14.5f, 20.0f, TEXT("Surface_Cobblestone"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Sultanahmet_SouthConnector"),
		Origin + FVector(24800.0f, 3800.0f, 0.0f),
		Origin + FVector(24800.0f, 0.0f, 0.0f),
		14.5f, 20.0f, TEXT("Surface_Asphalt"), false));

	// =========================================================================
	// 4. LOOP 3: GALATA / CİHANGİR STEEP HILLS (13.2° - 17.5°) & UPPER RIDGE
	// =========================================================================
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Slope1_BaseApron"),
		Origin + FVector(3500.0f, 0.0f, 0.0f),
		Origin + FVector(3500.0f, 2100.0f, 0.0f),
		8.2f, 20.0f, TEXT("Surface_Cobblestone"), false));

	// Slope 1: West Galata Climb (~13.2° grade: 55m horizontal run, +12.9m vertical rise to Z=+1290cm)
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Slope1_GalataClimb_13Deg"),
		Origin + FVector(3500.0f, 2100.0f, 0.0f),
		Origin + FVector(3500.0f, 7600.0f, 1290.0f),
		8.2f, 20.0f, TEXT("Surface_Cobblestone"), false));

	// Upper Galata / Sultanahmet Ridge Connector (Z = +1290cm, split at switchback node X=+118m)
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("UpperRidge_West"),
		Origin + FVector(3500.0f, 7600.0f, 1290.0f),
		Origin + FVector(11800.0f, 7600.0f, 1290.0f),
		8.8f, 22.0f, TEXT("Surface_Cobblestone"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("UpperRidge_East"),
		Origin + FVector(11800.0f, 7600.0f, 1290.0f),
		Origin + FVector(20500.0f, 7600.0f, 1290.0f),
		8.8f, 22.0f, TEXT("Surface_Cobblestone"), false));

	// Slope 2: Steep Cihangir Switchback Lower Leg (~17.5° grade: 28m run, +8.85m rise to midpoint stop-sign landing at Z=+885cm)
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Slope2_CihangirSteep_17Deg_LegA"),
		Origin + FVector(13500.0f, 3800.0f, 0.0f),
		Origin + FVector(13500.0f, 6600.0f, 885.0f),
		6.0f, 20.0f, TEXT("Surface_Cobblestone"), true));

	// Slope 2 Switchback Upper Leg (~11.6° grade from midpoint stop-sign landing Z=+885cm up to Upper Ridge Z=+1290cm)
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Slope2_CihangirSwitchback_15Deg_LegB"),
		Origin + FVector(13500.0f, 6600.0f, 885.0f),
		Origin + FVector(11800.0f, 7600.0f, 1290.0f),
		6.0f, 20.0f, TEXT("Surface_Cobblestone"), true));

	// Slope 3: Tophane / Beşiktaş Descent (~14.0° grade: 52m run from Z=+1290cm back to Z=0)
	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Slope3_TophaneDescent_14Deg"),
		Origin + FVector(20500.0f, 7600.0f, 1290.0f),
		Origin + FVector(20500.0f, 2400.0f, 0.0f),
		8.2f, 20.0f, TEXT("Surface_Cobblestone"), false));

	RoadSegments.Add(FIstanbulRoadSegmentSpec(
		TEXT("Slope3_BaseApron"),
		Origin + FVector(20500.0f, 2400.0f, 0.0f),
		Origin + FVector(20500.0f, 0.0f, 0.0f),
		8.2f, 20.0f, TEXT("Surface_Cobblestone"), false));
}

void ATestSceneBuilder::PopulateDefaultDistrictsAndBuildings(const FVector& Origin)
{
	Districts.Reset();
	BuildingSpecs.Reset();

	// 1. District Specifications (Task 1.1 & 1.2 Zoning Contrast)
	{
		FIstanbulDistrictSpec Waterfront;
		Waterfront.DistrictName = TEXT("Eminönü - Karaköy Sahil Kordonu");
		Waterfront.ZoneType = EIstanbulDistrictZoneType::Waterfront;
		Waterfront.Bounds = FBox(Origin + FVector(-4500.0f, -12000.0f, -500.0f), Origin + FVector(38500.0f, 1500.0f, 1200.0f));
		Waterfront.MinFloors = 4;
		Waterfront.MaxFloors = 6;
		Waterfront.Density = 0.65f;
		Waterfront.FogDensityMultiplier = 1.15f;
		Waterfront.AmbientWarmthBias = 0.12f;
		Districts.Add(Waterfront);

		FIstanbulDistrictSpec WideAve;
		WideAve.DistrictName = TEXT("Sirkeci - Sultanahmet Tramvay Caddesi");
		WideAve.ZoneType = EIstanbulDistrictZoneType::WideAvenue;
		WideAve.Bounds = FBox(Origin + FVector(16500.0f, 1200.0f, -100.0f), Origin + FVector(26500.0f, 5200.0f, 2400.0f));
		WideAve.MinFloors = 5;
		WideAve.MaxFloors = 8;
		WideAve.Density = 0.88f;
		WideAve.FogDensityMultiplier = 0.95f;
		WideAve.AmbientWarmthBias = 0.08f;
		Districts.Add(WideAve);

		FIstanbulDistrictSpec PlazaZone;
		PlazaZone.DistrictName = TEXT("Sultanahmet Dikilitaş Meydanı");
		PlazaZone.ZoneType = EIstanbulDistrictZoneType::Plaza;
		PlazaZone.Bounds = FBox(Origin + FVector(13800.0f, 2400.0f, -100.0f), Origin + FVector(16600.0f, 5200.0f, 1800.0f));
		PlazaZone.MinFloors = 3;
		PlazaZone.MaxFloors = 5;
		PlazaZone.Density = 0.45f;
		PlazaZone.FogDensityMultiplier = 0.90f;
		PlazaZone.AmbientWarmthBias = 0.15f;
		Districts.Add(PlazaZone);

		FIstanbulDistrictSpec NarrowAlleys;
		NarrowAlleys.DistrictName = TEXT("Tarihi Yarımada Dar Ara Sokaklar");
		NarrowAlleys.ZoneType = EIstanbulDistrictZoneType::NarrowAlley;
		NarrowAlleys.Bounds = FBox(Origin + FVector(6500.0f, 1500.0f, -100.0f), Origin + FVector(16800.0f, 6200.0f, 1600.0f));
		NarrowAlleys.MinFloors = 3;
		NarrowAlleys.MaxFloors = 6;
		NarrowAlleys.Density = 0.95f;
		NarrowAlleys.FogDensityMultiplier = 1.28f;
		NarrowAlleys.AmbientWarmthBias = 0.35f;
		Districts.Add(NarrowAlleys);

		FIstanbulDistrictSpec HillZone;
		HillZone.DistrictName = TEXT("Galata & Cihangir Yokuşları");
		HillZone.ZoneType = EIstanbulDistrictZoneType::HillResidential;
		HillZone.Bounds = FBox(Origin + FVector(2000.0f, 2000.0f, 0.0f), Origin + FVector(22000.0f, 11500.0f, 3800.0f));
		HillZone.MinFloors = 4;
		HillZone.MaxFloors = 7;
		HillZone.Density = 0.92f;
		HillZone.FogDensityMultiplier = 1.18f;
		HillZone.AmbientWarmthBias = 0.28f;
		Districts.Add(HillZone);
	}

	// 2. Batched AIstanbulBuildingBlock Specs (placed outside all road corridors + sidewalk clearance >= 220cm)
	// Row A: Sirkeci - Sultanahmet Wide Avenue North Facade (5-7 story buildings, shopfronts, awnings, Turkish shop signs)
	for (int32 i = 0; i < 4; ++i)
	{
		FIstanbulBuildingSpec Spec;
		Spec.BaseLocation = Origin + FVector(18400.0f + i * 1650.0f, 5450.0f, 0.0f);
		Spec.YawDegrees = -90.0f;
		Spec.FrontageWidthM = 15.2f;
		Spec.DepthM = 12.5f;
		Spec.Floors = 5 + (i % 3);
		Spec.bHasCumba = (i % 2 == 1);
		Spec.MaxAllowedProjectionCm = 85.0f;
		Spec.bPitchedRoof = (i % 2 == 0);
		Spec.GroundFloorType = EIstanbulGroundFloorType::ShopfrontAwning;
		Spec.StyleSeed = Seed + 101 + i * 37;
		BuildingSpecs.Add(Spec);
	}

	// Row B: Old-Town Narrow Alley Overhanging Cumba Row + Spanning Arched Gateway (>= 3.2m vertical clearance)
	{
		// Spanning arched gateway across OldTown_WestAlley at X=+72m, Y=+21.5m (road width 5.4m, arch clearance 3.9m >= 3.2m)
		FIstanbulBuildingSpec ArchBuilding;
		ArchBuilding.BaseLocation = Origin + FVector(5650.0f, 2150.0f, 0.0f);
		ArchBuilding.YawDegrees = 0.0f;
		ArchBuilding.FrontageWidthM = 12.0f;
		ArchBuilding.DepthM = 11.0f;
		ArchBuilding.Floors = 4;
		ArchBuilding.bHasCumba = true;
		ArchBuilding.MaxAllowedProjectionCm = 85.0f;
		ArchBuilding.GroundFloorType = EIstanbulGroundFloorType::ShopfrontAwning;
		ArchBuilding.bSpanAlleyGateway = true;
		ArchBuilding.AlleySpanWidthM = 5.4f;
		ArchBuilding.StyleSeed = Seed + 211;
		BuildingSpecs.Add(ArchBuilding);

		// Tightly packed Old-Town bitişik nizam row along OldTown_CrossSpine North side
		for (int32 i = 0; i < 3; ++i)
		{
			FIstanbulBuildingSpec AlleySpec;
			AlleySpec.BaseLocation = Origin + FVector(8350.0f + i * 1350.0f, 4950.0f, 0.0f);
			AlleySpec.YawDegrees = -90.0f;
			AlleySpec.FrontageWidthM = 12.2f;
			AlleySpec.DepthM = 10.5f;
			AlleySpec.Floors = 3 + (i % 3);
			AlleySpec.bHasCumba = true;
			AlleySpec.MaxAllowedProjectionCm = 90.0f;
			AlleySpec.bPitchedRoof = true;
			AlleySpec.GroundFloorType = EIstanbulGroundFloorType::ShopfrontAwning;
			AlleySpec.StyleSeed = Seed + 307 + i * 43;
			BuildingSpecs.Add(AlleySpec);
		}
	}
}

int32 ATestSceneBuilder::SolveAndBuildJunctions(FIstanbulMeshSectionData& AsphaltRoadSection,
	FIstanbulMeshSectionData& CobbleRoadSection, FIstanbulMeshSectionData& SidewalkSection,
	const FLinearColor& AsphaltCol, const FLinearColor& CobbleCol, const FLinearColor& SidewalkCol)
{
	// Group all segment endpoints within 50cm in 2D into shared junction nodes, unify Z elevation,
	// and generate rounded curb corner returns (3-6m radius) + crosswalk zebra strips.
	struct FJunctionNode
	{
		FVector Position = FVector::ZeroVector;
		TArray<int32> SegmentIndices;
		TArray<bool> bIsEndEndpoint;
		float MaxRoadWidthM = 6.0f;
		float MaxCurbHeightCm = 20.0f;
		bool bAnyCobble = false;
	};

	TArray<FJunctionNode> Nodes;
	const float SnapTolerance2DCm = 50.0f;

	auto RegisterEndpoint = [&](int32 SegIdx, bool bEnd)
	{
		const FVector& Pt = bEnd ? RoadSegments[SegIdx].End : RoadSegments[SegIdx].Start;
		for (FJunctionNode& Node : Nodes)
		{
			if (FVector::Dist2D(Node.Position, Pt) <= SnapTolerance2DCm)
			{
				Node.SegmentIndices.Add(SegIdx);
				Node.bIsEndEndpoint.Add(bEnd);
				Node.MaxRoadWidthM = FMath::Max(Node.MaxRoadWidthM, RoadSegments[SegIdx].WidthM);
				Node.MaxCurbHeightCm = FMath::Max(Node.MaxCurbHeightCm, RoadSegments[SegIdx].SidewalkHeightCm);
				if (RoadSegments[SegIdx].SurfaceType == TEXT("Surface_Cobblestone"))
				{
					Node.bAnyCobble = true;
				}
				return;
			}
		}
		FJunctionNode NewNode;
		NewNode.Position = Pt;
		NewNode.SegmentIndices.Add(SegIdx);
		NewNode.bIsEndEndpoint.Add(bEnd);
		NewNode.MaxRoadWidthM = RoadSegments[SegIdx].WidthM;
		NewNode.MaxCurbHeightCm = RoadSegments[SegIdx].SidewalkHeightCm;
		NewNode.bAnyCobble = (RoadSegments[SegIdx].SurfaceType == TEXT("Surface_Cobblestone"));
		Nodes.Add(NewNode);
	};

	for (int32 i = 0; i < RoadSegments.Num(); ++i)
	{
		RegisterEndpoint(i, false);
		RegisterEndpoint(i, true);
	}

	int32 BuiltJunctionCount = 0;
	for (FJunctionNode& Node : Nodes)
	{
		if (Node.SegmentIndices.Num() < 2)
		{
			continue;
		}

		// Enforce identical node elevation across all connected segments
		float SumZ = 0.0f;
		for (int32 k = 0; k < Node.SegmentIndices.Num(); ++k)
		{
			const FIstanbulRoadSegmentSpec& Seg = RoadSegments[Node.SegmentIndices[k]];
			SumZ += Node.bIsEndEndpoint[k] ? Seg.End.Z : Seg.Start.Z;
		}
		const float UnifiedZ = SumZ / (float)Node.SegmentIndices.Num();
		Node.Position.Z = UnifiedZ;

		for (int32 k = 0; k < Node.SegmentIndices.Num(); ++k)
		{
			FIstanbulRoadSegmentSpec& Seg = RoadSegments[Node.SegmentIndices[k]];
			if (Node.bIsEndEndpoint[k])
			{
				Seg.End.X = Node.Position.X;
				Seg.End.Y = Node.Position.Y;
				Seg.End.Z = UnifiedZ;
			}
			else
			{
				Seg.Start.X = Node.Position.X;
				Seg.Start.Y = Node.Position.Y;
				Seg.Start.Z = UnifiedZ;
			}
			const float Run2D = FVector::Dist2D(Seg.Start, Seg.End);
			Seg.GradeDegrees = (Run2D > 1.0f)
				? FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Seg.End.Z - Seg.Start.Z), Run2D))
				: 0.0f;
		}

		// Emit junction patch with rounded curb corner returns & crosswalks where >= 3 segments meet (T/Cross/Y)
		const bool bIsMultiWayIntersection = (Node.SegmentIndices.Num() >= 3);
		const float PatchRadiusCm = FMath::Clamp(Node.MaxRoadWidthM * 48.0f, 300.0f, 600.0f);
		FIstanbulMeshUtils::AppendJunctionPatch(
			Node.bAnyCobble ? CobbleRoadSection : AsphaltRoadSection,
			SidewalkSection,
			Node.Position,
			PatchRadiusCm,
			Node.MaxCurbHeightCm,
			Node.bAnyCobble ? CobbleCol : AsphaltCol,
			SidewalkCol,
			bIsMultiWayIntersection);
		++BuiltJunctionCount;
	}

	return BuiltJunctionCount;
}

void ATestSceneBuilder::SpawnWorldBoundaryBlockers(const FVector& Origin)
{
	// Soft boundary blockers disguised as historic stone walls, municipal roadwork barriers, and gatehouse buildings
	// so the player can never reach the void at the ends of generated geometry.
	SpawnHistoricalStoneWall(TEXT("Boundary_WestBoulevardGate"),
		Origin + FVector(-4150.0f, 0.0f, 180.0f), FRotator(0.0f, 90.0f, 0.0f), 26.0f, 3.6f);
	SpawnHistoricalStoneWall(TEXT("Boundary_EastBoulevardGate"),
		Origin + FVector(38150.0f, 0.0f, 180.0f), FRotator(0.0f, 90.0f, 0.0f), 26.0f, 3.6f);
	SpawnHistoricalStoneWall(TEXT("Boundary_GalataBridgeSouthCap"),
		Origin + FVector(8800.0f, -11750.0f, 440.0f), FRotator::ZeroRotator, 20.0f, 3.6f);
	SpawnHistoricalStoneWall(TEXT("Boundary_DeadEndAlleyWall"),
		Origin + FVector(11600.0f, 5780.0f, 160.0f), FRotator::ZeroRotator, 8.0f, 3.2f);
	SpawnHistoricalStoneWall(TEXT("Boundary_WestAlleyNorthWall"),
		Origin + FVector(7200.0f, 4920.0f, 160.0f), FRotator::ZeroRotator, 8.0f, 3.2f);
}

bool ATestSceneBuilder::ValidateNetwork()
{
	int32 ElevationMismatches = 0;
	int32 WidthViolations = 0;
	int32 GradeViolations = 0;
	int32 BuildingIntersectionViolations = 0;

	// 1. Check segment widths (>= 3.5m) and grades (<= 18.0 deg)
	for (const FIstanbulRoadSegmentSpec& Seg : RoadSegments)
	{
		if (Seg.WidthM < 3.5f)
		{
			++WidthViolations;
			UE_LOG(LogIstanbulTaxi, Warning, TEXT("ValidateNetwork: Segment '%s' free width %.2fm < 3.5m minimum!"),
				*Seg.SegmentId.ToString(), Seg.WidthM);
		}
		if (Seg.GradeDegrees > 18.0f + KINDA_SMALL_NUMBER)
		{
			++GradeViolations;
			UE_LOG(LogIstanbulTaxi, Warning, TEXT("ValidateNetwork: Segment '%s' grade %.2f deg > 18.0 deg maximum!"),
				*Seg.SegmentId.ToString(), Seg.GradeDegrees);
		}
	}

	// 2. Check shared node elevations across all connected segments
	for (int32 i = 0; i < RoadSegments.Num(); ++i)
	{
		for (int32 j = i + 1; j < RoadSegments.Num(); ++j)
		{
			const FVector PtsA[2] = { RoadSegments[i].Start, RoadSegments[i].End };
			const FVector PtsB[2] = { RoadSegments[j].Start, RoadSegments[j].End };
			for (int32 a = 0; a < 2; ++a)
			{
				for (int32 b = 0; b < 2; ++b)
				{
					if (FVector::Dist2D(PtsA[a], PtsB[b]) <= 50.0f &&
						FMath::Abs(PtsA[a].Z - PtsB[b].Z) > 1.0f)
					{
						++ElevationMismatches;
						UE_LOG(LogIstanbulTaxi, Warning,
							TEXT("ValidateNetwork: Node Z mismatch between '%s' (Z=%.1f) and '%s' (Z=%.1f)!"),
							*RoadSegments[i].SegmentId.ToString(), PtsA[a].Z,
							*RoadSegments[j].SegmentId.ToString(), PtsB[b].Z);
					}
				}
			}
		}
	}

	// 3. Check building footprints (including Cumba overhang) against drivable road corridors
	for (int32 bIdx = 0; bIdx < BuildingSpecs.Num(); ++bIdx)
	{
		const FIstanbulBuildingSpec& B = BuildingSpecs[bIdx];
		const float EffectiveRadiusCm = FMath::Min(B.FrontageWidthM, B.DepthM) * 50.0f;
		for (const FIstanbulRoadSegmentSpec& Seg : RoadSegments)
		{
			const FVector ClosestPt = FMath::ClosestPointOnSegment(B.BaseLocation, Seg.Start, Seg.End);
			const float Dist2DCm = FVector::Dist2D(B.BaseLocation, ClosestPt);
			const float MinSafeDistCm = (Seg.WidthM * 50.0f) + EffectiveRadiusCm - 25.0f;
			if (Dist2DCm < MinSafeDistCm)
			{
				++BuildingIntersectionViolations;
				UE_LOG(LogIstanbulTaxi, Warning,
					TEXT("ValidateNetwork: BuildingSpec[%d] at (%.0f, %.0f) intrudes into road segment '%s' (Dist=%.0fcm < %.0fcm)!"),
					bIdx, B.BaseLocation.X, B.BaseLocation.Y, *Seg.SegmentId.ToString(), Dist2DCm, MinSafeDistCm);
			}
		}
	}

	const int32 TotalViolations = ElevationMismatches + WidthViolations + GradeViolations + BuildingIntersectionViolations;
	UE_LOG(LogIstanbulTaxi, Log,
		TEXT("ATestSceneBuilder::ValidateNetwork() -> %s | Segments=%d, Buildings=%d | Violations=%d (ElevationMismatches=%d, WidthViolations=%d, GradeViolations=%d, BuildingIntersections=%d)"),
		(TotalViolations == 0) ? TEXT("PASSED (0 violations)") : TEXT("FAILED"),
		RoadSegments.Num(), BuildingSpecs.Num(), TotalViolations,
		ElevationMismatches, WidthViolations, GradeViolations, BuildingIntersectionViolations);

	return (TotalViolations == 0);
}

AActor* ATestSceneBuilder::SpawnShape(const TCHAR* ShapePath, const FString& Label, const FVector& Location,
	const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color, bool bCollidable, FName SurfaceActorTag)
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* MeshActor = W->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (!MeshActor) return nullptr;

	MeshActor->Tags.Add(GeneratedActorTag);
	if (!SurfaceActorTag.IsNone())
	{
		MeshActor->Tags.Add(SurfaceActorTag);
	}

	UStaticMeshComponent* SMC = MeshActor->GetStaticMeshComponent();
	SMC->SetMobility(EComponentMobility::Movable);
	if (UStaticMesh* Mesh = FIstanbulMeshUtils::LoadBasicShapeSafe(ShapePath))
	{
		SMC->SetStaticMesh(Mesh);
	}
	SMC->SetWorldScale3D(Scale);
	SMC->SetCollisionEnabled(bCollidable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollidable)
	{
		SMC->SetCollisionProfileName(TEXT("BlockAll"));
	}

	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, MeshActor);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		SMC->SetMaterial(0, MID);
	}
#if WITH_EDITOR
	MeshActor->SetActorLabel(Label);
#endif
	++SpawnedActorCount;
	return MeshActor;
}

AActor* ATestSceneBuilder::SpawnBox(const FString& Label, const FVector& Location, const FVector& Scale,
	const FLinearColor& Color, bool bCollidable, const FRotator& Rotation, FName SurfaceActorTag)
{
	return SpawnShape(TEXT("/Engine/BasicShapes/Cube.Cube"), Label, Location, Rotation, Scale, Color, bCollidable, SurfaceActorTag);
}

void ATestSceneBuilder::SpawnIstanbulApartmentBlock(const FString& NamePrefix, const FVector& BaseLocation,
	float YawDeg, int32 Floors, float WidthM, float DepthM, const FLinearColor& FacadeColor, bool bHasCumba, bool bHasShopAwning,
	FRandomStream& Rng)
{
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Fwd = Rot.Vector();
	const FVector Right = FRotationMatrix(Rot).GetScaledAxis(EAxis::Y);

	const float FloorH = 3.2f;
	const float TotalHM = Floors * FloorH;

	// Ground-floor weathered stone rustication plinth + upper plaster facade
	SpawnBox(NamePrefix + TEXT("_StoneGround"), BaseLocation + FVector(0.0f, 0.0f, 160.0f),
		FVector(DepthM + 0.08f, WidthM + 0.06f, 3.2f), FLinearColor(0.46f, 0.43f, 0.39f), true, Rot);
	SpawnBox(NamePrefix + TEXT("_Body"), BaseLocation + FVector(0.0f, 0.0f, TotalHM * 50.0f),
		FVector(DepthM, WidthM, TotalHM), FacadeColor, true, Rot);

	// Terracotta tiled roof cap
	SpawnBox(NamePrefix + TEXT("_Roof"), BaseLocation + FVector(0.0f, 0.0f, TotalHM * 100.0f + 25.0f),
		FVector(DepthM + 0.4f, WidthM + 0.3f, 0.45f), FLinearColor(0.58f, 0.22f, 0.14f), false, Rot);

	// Ground-floor dark shopfront glazing
	SpawnBox(NamePrefix + TEXT("_ShopFront"), BaseLocation + Fwd * (DepthM * 50.0f + 6.0f) + FVector(0.0f, 0.0f, 145.0f),
		FVector(0.12f, WidthM * 0.86f, 2.5f), FLinearColor(0.08f, 0.10f, 0.14f), false, Rot);

	if (bHasShopAwning)
	{
		static const FLinearColor AwningColors[4] = {
			FLinearColor(0.75f, 0.12f, 0.10f), // Dönerci / Büfe Red
			FLinearColor(0.06f, 0.48f, 0.22f), // Eczane / Manav Green
			FLinearColor(0.10f, 0.32f, 0.72f), // Tekel / Market Blue
			FLinearColor(0.88f, 0.58f, 0.06f)  // Fırın / Simit Sarayı Amber
		};
		const FLinearColor AwningCol = AwningColors[Rng.RandRange(0, 3)];
		SpawnBox(NamePrefix + TEXT("_ShopAwning"),
			BaseLocation + Fwd * (DepthM * 50.0f + 95.0f) + FVector(0.0f, 0.0f, 290.0f),
			FVector(1.8f, WidthM * 0.88f, 0.22f), AwningCol, false, FRotator(-14.0f, YawDeg, 0.0f));
	}

	if (bHasCumba && Floors >= 3)
	{
		const float CumbaH = (Floors - 1) * FloorH * 0.78f;
		const float CumbaZ = 340.0f + CumbaH * 50.0f;
		SpawnBox(NamePrefix + TEXT("_Cumba"),
			BaseLocation + Fwd * (DepthM * 50.0f + 68.0f) + FVector(0.0f, 0.0f, CumbaZ),
			FVector(1.35f, WidthM * 0.56f, CumbaH), FacadeColor * 0.92f, false, Rot);
	}

	for (int32 f = 1; f < Floors; ++f)
	{
		const float WinZ = f * FloorH * 100.0f + 160.0f;
		const FLinearColor WinCol = (Rng.FRand() < 0.35f)
			? FLinearColor(0.95f, 0.78f, 0.38f)
			: FLinearColor(0.14f, 0.20f, 0.28f);
		SpawnBox(FString::Printf(TEXT("%s_WinF%d"), *NamePrefix, f),
			BaseLocation + Fwd * (DepthM * 50.0f + (bHasCumba ? 138.0f : 8.0f)) + FVector(0.0f, 0.0f, WinZ),
			FVector(0.10f, WidthM * (bHasCumba ? 0.48f : 0.76f), 1.15f), WinCol, false, Rot);
	}

	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), NamePrefix + TEXT("_WaterTank"),
		BaseLocation + Right * (WidthM * 22.0f) + FVector(0.0f, 0.0f, TotalHM * 100.0f + 90.0f),
		FRotator::ZeroRotator, FVector(1.1f, 1.1f, 1.2f), FLinearColor(0.62f, 0.65f, 0.68f), false);
}

void ATestSceneBuilder::SpawnParkedCurbCar(const FString& Label, const FVector& Location,
	const FRotator& Rotation, const FLinearColor& BodyColor)
{
	SpawnBox(Label + TEXT("_Body"), Location + FVector(0.0f, 0.0f, 55.0f),
		FVector(4.1f, 1.72f, 0.55f), BodyColor, true, Rotation);
	SpawnBox(Label + TEXT("_Cabin"), Location + FVector(0.0f, 0.0f, 102.0f),
		FVector(2.1f, 1.50f, 0.48f), FLinearColor(0.12f, 0.15f, 0.18f), false, Rotation);
}

void ATestSceneBuilder::SpawnIETTBusStop(const FString& Label, const FVector& Location, float YawDeg)
{
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Fwd = Rot.Vector();
	const FVector Right = FRotationMatrix(Rot).GetScaledAxis(EAxis::Y);

	SpawnBox(Label + TEXT("_BackWall"), Location - Fwd * 75.0f + FVector(0.0f, 0.0f, 125.0f),
		FVector(0.12f, 3.8f, 2.4f), FLinearColor(0.16f, 0.22f, 0.28f), true, Rot);
	SpawnBox(Label + TEXT("_Canopy"), Location + FVector(0.0f, 0.0f, 250.0f),
		FVector(1.75f, 4.1f, 0.16f), FLinearColor(0.95f, 0.72f, 0.02f), false, Rot);
	SpawnBox(Label + TEXT("_Bench"), Location - Fwd * 45.0f + FVector(0.0f, 0.0f, 48.0f),
		FVector(0.45f, 2.4f, 0.12f), FLinearColor(0.45f, 0.28f, 0.14f), false, Rot);
	SpawnBox(Label + TEXT("_SignPole"), Location + Right * 235.0f + FVector(0.0f, 0.0f, 145.0f),
		FVector(0.18f, 0.42f, 2.8f), FLinearColor(0.82f, 0.12f, 0.12f), false, Rot);
}

void ATestSceneBuilder::SpawnStreetLamp(const FString& Label, const FVector& Location, float YawDeg)
{
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Fwd = Rot.Vector();

	if (LampPoleHISM && LampPoleHISM->GetStaticMesh())
	{
		LampPoleHISM->AddInstance(FTransform(FRotator::ZeroRotator, Location + FVector(0.0f, 0.0f, 380.0f), FVector(0.18f, 0.18f, 7.6f)), true);
		++SpawnedHISMInstanceCount;
	}
	else
	{
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Label + TEXT("_Pole"),
			Location + FVector(0.0f, 0.0f, 380.0f), FRotator::ZeroRotator,
			FVector(0.18f, 0.18f, 7.6f), FLinearColor(0.20f, 0.22f, 0.25f), false);
	}

	SpawnBox(Label + TEXT("_Arm"), Location + Fwd * 95.0f + FVector(0.0f, 0.0f, 745.0f),
		FVector(1.9f, 0.16f, 0.14f), FLinearColor(0.20f, 0.22f, 0.25f), false, Rot);
	const FVector BulbWorldPos = Location + Fwd * 175.0f + FVector(0.0f, 0.0f, 732.0f);
	SpawnBox(Label + TEXT("_Bulb"), BulbWorldPos,
		FVector(0.48f, 0.34f, 0.14f), FLinearColor(1.0f, 0.85f, 0.45f), false, Rot);

	StreetLampLightWorldPositions.Add(BulbWorldPos - FVector(0.0f, 0.0f, 35.0f));
}

void ATestSceneBuilder::SpawnHistoricalStoneWall(const FString& Label, const FVector& CenterLocation,
	const FRotator& Rotation, float LengthM, float HeightM)
{
	const FLinearColor LimestoneCol(0.52f, 0.49f, 0.44f);
	const FLinearColor KhorasanBrickCol(0.58f, 0.24f, 0.18f);
	const FLinearColor CopingStoneCol(0.64f, 0.61f, 0.56f);

	SpawnBox(Label + TEXT("_StoneCore"), CenterLocation,
		FVector(LengthM, 0.55f, HeightM), LimestoneCol, true, Rotation);

	const FVector UpVec = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Z);
	SpawnBox(Label + TEXT("_BrickBand1"), CenterLocation - UpVec * (HeightM * 18.0f),
		FVector(LengthM * 1.002f, 0.59f, 0.18f), KhorasanBrickCol, false, Rotation);
	SpawnBox(Label + TEXT("_BrickBand2"), CenterLocation + UpVec * (HeightM * 22.0f),
		FVector(LengthM * 1.002f, 0.59f, 0.18f), KhorasanBrickCol, false, Rotation);
	SpawnBox(Label + TEXT("_Coping"), CenterLocation + UpVec * (HeightM * 50.0f + 10.0f),
		FVector(LengthM * 1.01f, 0.68f, 0.20f), CopingStoneCol, false, Rotation);
}

void ATestSceneBuilder::SpawnGalataBridgeAndApproach(const FVector& Origin)
{
	// Two-level Galata Bridge feel at X = +88m extending across the Golden Horn (Y = -36m .. -116m, Z = +260cm):
	// Level 1 (Lower Deck): Golden Horn fish-restaurant promenade & arched steel piers
	// Level 2 (Upper Deck): Roadway + T1 tram tracks + iconic Galata Bridge balustrade & HISM fishing rods!
	const FVector BridgeStart = Origin + FVector(8800.0f, -3600.0f, 260.0f);
	const FLinearColor BridgeSteel(0.24f, 0.28f, 0.32f);
	const FLinearColor LowerArcadeWarm(0.95f, 0.72f, 0.32f);

	// Lower restaurant deck under the roadway
	SpawnBox(TEXT("GalataBridge_LowerDeck"), BridgeStart + FVector(0.0f, -4000.0f, -155.0f),
		FVector(19.5f, 78.0f, 0.35f), BridgeSteel, true);
	SpawnBox(TEXT("GalataBridge_RestaurantGlow"), BridgeStart + FVector(0.0f, -4000.0f, -85.0f),
		FVector(16.0f, 72.0f, 1.1f), LowerArcadeWarm, false);

	// Upper fishing railings on both sides + 28 instanced angler fishing rods leaning out over the Golden Horn!
	SpawnBox(TEXT("GalataBridge_RailWest"), BridgeStart + FVector(-940.0f, -4000.0f, 65.0f),
		FVector(0.18f, 80.0f, 1.05f), BridgeSteel, true);
	SpawnBox(TEXT("GalataBridge_RailEast"), BridgeStart + FVector( 940.0f, -4000.0f, 65.0f),
		FVector(0.18f, 80.0f, 1.05f), BridgeSteel, true);

	if (FishingRodHISM && FishingRodHISM->GetStaticMesh())
	{
		for (int32 r = 0; r < 16; ++r)
		{
			const float RY = -3800.0f - r * 460.0f;
			// West railing rods leaning outward (-X) at 42 deg
			FishingRodHISM->AddInstance(FTransform(
				FRotator(0.0f, 180.0f, -42.0f),
				Origin + FVector(8800.0f - 995.0f, RY, 260.0f + 165.0f),
				FVector(0.035f, 0.035f, 2.8f)), true);
			// East railing rods leaning outward (+X) at 42 deg
			FishingRodHISM->AddInstance(FTransform(
				FRotator(0.0f, 0.0f, -42.0f),
				Origin + FVector(8800.0f + 995.0f, RY, 260.0f + 165.0f),
				FVector(0.035f, 0.035f, 2.8f)), true);
			SpawnedHISMInstanceCount += 2;
		}
	}
}

void ATestSceneBuilder::SpawnOldTownSquareAndFountain(const FVector& SquareCenter)
{
	// Sultanahmet-style Plaza (Meydan) with an Ottoman octagonal marble fountain (Meydan Çeşmesi)
	// AND an iconic Obelisk of Theodosius (Dikilitaş) centerpiece on a cobblestone apron!
	const FLinearColor MarbleCol(0.86f, 0.84f, 0.78f);
	const FLinearColor LeadCapCol(0.32f, 0.36f, 0.42f);
	const FLinearColor AswanGraniteCol(0.76f, 0.52f, 0.46f);
	const FLinearColor BronzeBlockCol(0.28f, 0.24f, 0.16f);

	// Cobblestone plaza apron around the monuments (outside the 6m road lane)
	const FVector NorthPlazaCenter = SquareCenter + FVector(0.0f, 920.0f, 0.0f);
	SpawnBox(TEXT("SultanahmetPlaza_CobbleApron"), NorthPlazaCenter + FVector(0.0f, 0.0f, 6.0f),
		FVector(24.0f, 11.0f, 0.12f), FLinearColor(0.29f, 0.28f, 0.26f), true, FRotator::ZeroRotator, TEXT("Surface_Cobblestone"));

	// 1. Ottoman Octagonal Marble Fountain (Meydan Çeşmesi) on West side of Plaza
	const FVector FountainPos = NorthPlazaCenter + FVector(-560.0f, 80.0f, 0.0f);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Meydan_FountainBasin"),
		FountainPos + FVector(0.0f, 0.0f, 35.0f), FRotator::ZeroRotator,
		FVector(4.6f, 4.6f, 0.7f), MarbleCol, true);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Meydan_FountainKiosk"),
		FountainPos + FVector(0.0f, 0.0f, 195.0f), FRotator::ZeroRotator,
		FVector(2.8f, 2.8f, 2.6f), MarbleCol * 0.95f, true);
	SpawnShape(TEXT("/Engine/BasicShapes/Cone.Cone"), TEXT("Meydan_FountainRoof"),
		FountainPos + FVector(0.0f, 0.0f, 385.0f), FRotator::ZeroRotator,
		FVector(3.8f, 3.8f, 1.5f), LeadCapCol, false);

	// 2. Sultanahmet Dikilitaş (Obelisk of Theodosius) on East side of Plaza
	const FVector ObeliskPos = NorthPlazaCenter + FVector(560.0f, 80.0f, 0.0f);
	SpawnBox(TEXT("Dikilitas_SteppedPlinth"), ObeliskPos + FVector(0.0f, 0.0f, 45.0f),
		FVector(3.4f, 3.4f, 0.9f), MarbleCol, true);
	SpawnBox(TEXT("Dikilitas_ReliefPedestal"), ObeliskPos + FVector(0.0f, 0.0f, 175.0f),
		FVector(2.4f, 2.4f, 1.7f), MarbleCol * 0.94f, true);
	SpawnBox(TEXT("Dikilitas_BronzeSupports"), ObeliskPos + FVector(0.0f, 0.0f, 280.0f),
		FVector(1.9f, 1.9f, 0.45f), BronzeBlockCol, false);
	SpawnBox(TEXT("Dikilitas_LowerGraniteShaft"), ObeliskPos + FVector(0.0f, 0.0f, 680.0f),
		FVector(1.55f, 1.55f, 7.6f), AswanGraniteCol, true);
	SpawnBox(TEXT("Dikilitas_UpperGraniteShaft"), ObeliskPos + FVector(0.0f, 0.0f, 1340.0f),
		FVector(1.18f, 1.18f, 5.8f), AswanGraniteCol * 1.04f, true);
	SpawnShape(TEXT("/Engine/BasicShapes/Cone.Cone"), TEXT("Dikilitas_Pyramidion"),
		ObeliskPos + FVector(0.0f, 0.0f, 1730.0f), FRotator(0.0f, 45.0f, 0.0f),
		FVector(1.55f, 1.55f, 2.2f), AswanGraniteCol * 1.08f, false);
}

void ATestSceneBuilder::SpawnSlopeStopSignAndSwitchback(const FVector& Origin)
{
	// Midpoint landing at Cihangir 17.5° steep hill (X = +135m, Y = +66m, Z = +885cm) with red "DUR" (STOP) octagon
	// forcing the player to perform a hill-start on an ~18° grade!
	const FVector MidLanding = Origin + FVector(13500.0f, 6600.0f, 885.0f);
	SpawnBox(TEXT("Switchback_LandingPad"), MidLanding + FVector(-400.0f, 200.0f, -12.0f),
		FVector(14.0f, 10.0f, 0.35f), FLinearColor(0.25f, 0.24f, 0.23f), true, FRotator::ZeroRotator, TEXT("Surface_Cobblestone"));

	// Red "DUR" (STOP) Sign at the slope's midpoint junction
	const FVector SignBase = MidLanding + FVector(340.0f, -120.0f, 0.0f);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("SlopeStopSign_Pole"),
		SignBase + FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator,
		FVector(0.08f, 0.08f, 2.4f), FLinearColor(0.65f, 0.68f, 0.72f), true);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("SlopeStopSign_Disc"),
		SignBase + FVector(0.0f, 0.0f, 235.0f), FRotator(90.0f, 0.0f, 0.0f),
		FVector(0.75f, 0.75f, 0.06f), FLinearColor(0.88f, 0.08f, 0.08f), false);

	// White stop line painted across the steep cobblestone slope right before the midpoint junction
	SpawnBox(TEXT("SlopeStopLine"), MidLanding + FVector(0.0f, -220.0f, 4.0f),
		FVector(5.6f, 0.45f, 0.04f), FLinearColor(0.95f, 0.95f, 0.95f), false);
}

void ATestSceneBuilder::SpawnGalataTowerLandmark(const FVector& RidgeBase)
{
	const FLinearColor GenoeseStone(0.62f, 0.57f, 0.49f);
	const FLinearColor LeadConeCol(0.28f, 0.32f, 0.38f);

	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_MainShaft"),
		RidgeBase + FVector(0.0f, 0.0f, 1850.0f), FRotator::ZeroRotator,
		FVector(14.5f, 14.5f, 37.0f), GenoeseStone, true);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_Belt1"),
		RidgeBase + FVector(0.0f, 0.0f, 2400.0f), FRotator::ZeroRotator,
		FVector(15.2f, 15.2f, 0.6f), GenoeseStone * 1.1f, false);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_Balcony"),
		RidgeBase + FVector(0.0f, 0.0f, 3650.0f), FRotator::ZeroRotator,
		FVector(16.8f, 16.8f, 1.1f), FLinearColor(0.76f, 0.72f, 0.64f), false);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_Arcade"),
		RidgeBase + FVector(0.0f, 0.0f, 3920.0f), FRotator::ZeroRotator,
		FVector(13.8f, 13.8f, 4.5f), FLinearColor(0.95f, 0.78f, 0.38f), false);
	SpawnShape(TEXT("/Engine/BasicShapes/Cone.Cone"), TEXT("Galata_ConicalRoof"),
		RidgeBase + FVector(0.0f, 0.0f, 4850.0f), FRotator::ZeroRotator,
		FVector(15.0f, 15.0f, 15.0f), LeadConeCol, false);
}

void ATestSceneBuilder::SpawnBosphorusScenicOverlook(const FVector& Origin)
{
	const FLinearColor GraniteCol(0.46f, 0.47f, 0.49f);
	const FLinearColor IronRailCol(0.12f, 0.14f, 0.16f);

	SpawnBox(TEXT("Overlook_Seawall"), Origin + FVector(15000.0f, -1620.0f, -15.0f),
		FVector(420.0f, 0.8f, 0.85f), FLinearColor(0.34f, 0.35f, 0.37f), true);
	SpawnBox(TEXT("Overlook_BelvedereDeck"), Origin + FVector(18500.0f, -1820.0f, 14.0f),
		FVector(48.0f, 6.5f, 0.28f), GraniteCol, true);
	SpawnBox(TEXT("Overlook_TopRail"), Origin + FVector(15000.0f, -1590.0f, 82.0f),
		FVector(360.0f, 0.08f, 0.06f), IronRailCol, false);

	for (int32 i = 0; i < 8; ++i)
	{
		const float BX = 6500.0f + i * 2400.0f;
		SpawnBox(FString::Printf(TEXT("Overlook_Bench_%d"), i), Origin + FVector(BX, -1480.0f, 45.0f),
			FVector(2.2f, 0.55f, 0.45f), FLinearColor(0.45f, 0.26f, 0.12f), false);
	}
}

void ATestSceneBuilder::SpawnDetailedMosqueComplex(const FVector& HilltopBase)
{
	const FLinearColor StoneCol(0.84f, 0.81f, 0.75f);
	const FLinearColor LeadDomeCol(0.36f, 0.40f, 0.45f);

	SpawnBox(TEXT("Mosque_HarimHall"), HilltopBase + FVector(0.0f, 0.0f, 700.0f),
		FVector(22.0f, 22.0f, 14.0f), StoneCol, true);
	SpawnShape(TEXT("/Engine/BasicShapes/Sphere.Sphere"), TEXT("Mosque_MainDome"),
		HilltopBase + FVector(0.0f, 0.0f, 1420.0f), FRotator::ZeroRotator,
		FVector(16.5f, 16.5f, 12.5f), LeadDomeCol, false);

	const FVector SemiOffsets[4] = {
		FVector(850.0f, 0.0f, 1180.0f), FVector(-850.0f, 0.0f, 1180.0f),
		FVector(0.0f, 850.0f, 1180.0f), FVector(0.0f, -850.0f, 1180.0f)
	};
	for (int32 i = 0; i < 4; ++i)
	{
		SpawnShape(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FString::Printf(TEXT("Mosque_SemiDome_%d"), i),
			HilltopBase + SemiOffsets[i], FRotator::ZeroRotator,
			FVector(9.5f, 9.5f, 7.5f), LeadDomeCol, false);
	}

	const FVector MinaretBases[2] = {
		HilltopBase + FVector(1350.0f, -1350.0f, 0.0f),
		HilltopBase + FVector(1350.0f,  1350.0f, 0.0f)
	};
	for (int32 m = 0; m < 2; ++m)
	{
		const FString MName = FString::Printf(TEXT("Mosque_Minaret_%d"), m);
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), MName + TEXT("_Shaft"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 2100.0f), FRotator::ZeroRotator,
			FVector(2.2f, 2.2f, 42.0f), StoneCol, true);
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), MName + TEXT("_Serefe1"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 2650.0f), FRotator::ZeroRotator,
			FVector(3.4f, 3.4f, 0.7f), FLinearColor(0.92f, 0.88f, 0.78f), false);
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), MName + TEXT("_Serefe2"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 3550.0f), FRotator::ZeroRotator,
			FVector(3.2f, 3.2f, 0.7f), FLinearColor(0.92f, 0.88f, 0.78f), false);
		SpawnShape(TEXT("/Engine/BasicShapes/Cone.Cone"), MName + TEXT("_Spire"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 4550.0f), FRotator::ZeroRotator,
			FVector(2.1f, 2.1f, 7.5f), LeadDomeCol, false);
	}
}

void ATestSceneBuilder::SpawnHGSBridgeCanopy(const FVector& GateCenter)
{
	for (int32 Lane = -2; Lane <= 2; ++Lane)
	{
		const float Y = Lane * 580.0f;
		SpawnBox(FString::Printf(TEXT("HGS_Island_%d"), Lane), GateCenter + FVector(0.0f, Y, 45.0f),
			FVector(9.5f, 0.65f, 0.9f), FLinearColor(0.95f, 0.72f, 0.02f), true);
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FString::Printf(TEXT("HGS_Pillar_%d"), Lane),
			GateCenter + FVector(0.0f, Y, 340.0f), FRotator::ZeroRotator,
			FVector(0.65f, 0.65f, 6.8f), FLinearColor(0.18f, 0.22f, 0.28f), false);
	}

	SpawnBox(TEXT("HGS_MainCanopy"), GateCenter + FVector(0.0f, 0.0f, 680.0f),
		FVector(3.2f, 26.0f, 1.15f), FLinearColor(0.06f, 0.35f, 0.78f), false);
	SpawnBox(TEXT("HGS_SignStrip"), GateCenter + FVector(-165.0f, 0.0f, 680.0f),
		FVector(0.15f, 24.5f, 0.75f), FLinearColor(0.98f, 0.52f, 0.04f), false);

	const FVector BridgeTowerPos = GateCenter + FVector(8500.0f, 0.0f, 0.0f);
	const FLinearColor BridgeRed(0.76f, 0.14f, 0.12f);
	const FLinearColor CableSteel(0.72f, 0.75f, 0.80f);
	SpawnBox(TEXT("BosphorusBridge_TowerL"), BridgeTowerPos + FVector(0.0f, -1350.0f, 3200.0f),
		FVector(3.8f, 3.8f, 64.0f), BridgeRed, true);
	SpawnBox(TEXT("BosphorusBridge_TowerR"), BridgeTowerPos + FVector(0.0f,  1350.0f, 3200.0f),
		FVector(3.8f, 3.8f, 64.0f), BridgeRed, true);
	SpawnBox(TEXT("BosphorusBridge_CrossBeam1"), BridgeTowerPos + FVector(0.0f, 0.0f, 2800.0f),
		FVector(2.6f, 27.0f, 2.4f), BridgeRed, false);
	SpawnBox(TEXT("BosphorusBridge_CrossBeam2"), BridgeTowerPos + FVector(0.0f, 0.0f, 5400.0f),
		FVector(2.6f, 27.0f, 2.4f), BridgeRed, false);

	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float CableY = Side * 1350.0f;
		SpawnBox(FString::Printf(TEXT("Bridge_MainCableBack_%d"), Side),
			BridgeTowerPos + FVector(-3600.0f, CableY, 3100.0f),
			FVector(82.0f, 0.45f, 0.45f), CableSteel, false, FRotator(36.0f, 0.0f, 0.0f));
		SpawnBox(FString::Printf(TEXT("Bridge_MainCableSpan_%d"), Side),
			BridgeTowerPos + FVector(3800.0f, CableY, 3400.0f),
			FVector(86.0f, 0.45f, 0.45f), CableSteel, false, FRotator(-32.0f, 0.0f, 0.0f));

		for (int32 h = 1; h <= 5; ++h)
		{
			const float HX = -h * 1100.0f;
			const float HHeightM = FMath::Max(8.0f, 52.0f - h * 8.5f);
			SpawnBox(FString::Printf(TEXT("Bridge_Hanger_%d_%d"), Side, h),
				BridgeTowerPos + FVector(HX, CableY, HHeightM * 50.0f),
				FVector(0.12f, 0.12f, HHeightM), CableSteel, false);
		}
	}
}

void ATestSceneBuilder::BuildPrototypeScene()
{
	UWorld* W = GetWorld();
	if (!W || bBuilt) return;
	bBuilt = true;

	FRandomStream Rng(Seed);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Configure HISM static meshes & materials safely
	if (UStaticMesh* CylMesh = FIstanbulMeshUtils::LoadBasicShapeSafe(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		BollardHISM->SetStaticMesh(CylMesh);
		LampPoleHISM->SetStaticMesh(CylMesh);
		FishingRodHISM->SetStaticMesh(CylMesh);
		if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* BollardMID = UMaterialInstanceDynamic::Create(BaseMat, this);
			BollardMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.96f, 0.72f, 0.02f));
			BollardHISM->SetMaterial(0, BollardMID);

			UMaterialInstanceDynamic* PoleMID = UMaterialInstanceDynamic::Create(BaseMat, this);
			PoleMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.18f, 0.20f, 0.24f));
			LampPoleHISM->SetMaterial(0, PoleMID);

			UMaterialInstanceDynamic* RodMID = UMaterialInstanceDynamic::Create(BaseMat, this);
			RodMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.12f, 0.14f));
			FishingRodHISM->SetMaterial(0, RodMID);
		}
	}

	// =========================================================================
	// 1. PERMANENT AUTO-EXPOSURE LOCK & BALANCED LIGHTING
	// =========================================================================
	ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(W, ADirectionalLight::StaticClass()));
	if (!Sun)
	{
		Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(-28.0f, 165.0f, 0.0f), SpawnParams);
	}
	if (Sun && Sun->GetLightComponent())
	{
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* DLC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			DLC->SetAtmosphereSunLight(true);
			DLC->SetIntensity(36.0f);
			DLC->SetLightColor(FLinearColor(1.0f, 0.88f, 0.72f)); // Warm late-afternoon Istanbul sun
		}
	}

	ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(W, ASkyLight::StaticClass()));
	if (!Sky)
	{
		Sky = W->SpawnActor<ASkyLight>(FVector(0, 0, 1000), FRotator::ZeroRotator, SpawnParams);
	}
	if (Sky && Sky->GetLightComponent())
	{
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->bRealTimeCapture = true;
		Sky->GetLightComponent()->SetIntensity(4.5f);
		Sky->GetLightComponent()->RecaptureSky();
	}

	if (!UGameplayStatics::GetActorOfClass(W, ASkyAtmosphere::StaticClass()))
	{
		W->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

	AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(W, AExponentialHeightFog::StaticClass()));
	if (!Fog)
	{
		Fog = W->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 200), FRotator::ZeroRotator, SpawnParams);
	}

	APostProcessVolume* PP = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(W, APostProcessVolume::StaticClass()));
	if (!PP)
	{
		PP = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}
	if (PP)
	{
		PP->bUnbound = true;
		PP->Priority = 100.0f;
		PP->Settings.bOverride_AutoExposureMethod = true;
		PP->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		PP->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		PP->Settings.AutoExposureApplyPhysicalCameraExposure = false;
		PP->Settings.bOverride_AutoExposureMinBrightness = true;
		PP->Settings.AutoExposureMinBrightness = 1.0f;
		PP->Settings.bOverride_AutoExposureMaxBrightness = true;
		PP->Settings.AutoExposureMaxBrightness = 1.0f;
		PP->Settings.bOverride_AutoExposureBias = true;
		PP->Settings.AutoExposureBias = -4.0f;
	}

	ATimeOfDayActor* TODActor = Cast<ATimeOfDayActor>(UGameplayStatics::GetActorOfClass(W, ATimeOfDayActor::StaticClass()));
	if (!TODActor)
	{
		TODActor = W->SpawnActor<ATimeOfDayActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}
	if (TODActor)
	{
		TODActor->SunLight = Sun;
		TODActor->SkyLight = Sky;
		TODActor->HeightFog = Fog;
		TODActor->PostProcessVolume = PP;
		if (UTimeOfDaySubsystem* TimeSub = W->GetSubsystem<UTimeOfDaySubsystem>())
		{
			TimeSub->RegisterSceneActors(Sun, Sky, Fog, PP, TODActor->PresetOverride);
		}
	}

	const FVector Origin = GetActorLocation();
	if (RoadSegments.Num() == 0)
	{
		PopulateDefaultRoadSpecs(Origin);
	}
	if (Districts.Num() == 0 || BuildingSpecs.Num() == 0)
	{
		PopulateDefaultDistrictsAndBuildings(Origin);
	}

	// =========================================================================
	// 2. DATA-DRIVEN CORRIDOR & JUNCTION SOLVER (Smooth Procedural Mesh)
	// =========================================================================
	FIstanbulMeshSectionData AsphaltRoadSection;
	FIstanbulMeshSectionData SidewalkSection;
	FIstanbulMeshSectionData CobbleRoadSection;
	FIstanbulMeshSectionData CatenaryCableSection;

	const FLinearColor AsphaltCol(0.08f, 0.085f, 0.095f);
	const FLinearColor CobbleCol(0.26f, 0.25f, 0.24f);
	const FLinearColor SidewalkCol(0.42f, 0.43f, 0.45f);

	const int32 SolvedJunctionCount = SolveAndBuildJunctions(
		AsphaltRoadSection, CobbleRoadSection, SidewalkSection,
		AsphaltCol, CobbleCol, SidewalkCol);

	float MinSlopeDeg = 90.0f;
	float MaxSlopeDeg = 0.0f;
	int32 InclinedSegmentCount = 0;

	for (const FIstanbulRoadSegmentSpec& Seg : RoadSegments)
	{
		const bool bIsCobble = (Seg.SurfaceType == TEXT("Surface_Cobblestone"));
		const int32 Subdivs = (Seg.GradeDegrees > 1.0f) ? 20 : 6;
		if (Seg.GradeDegrees > 1.0f)
		{
			MinSlopeDeg = FMath::Min(MinSlopeDeg, Seg.GradeDegrees);
			MaxSlopeDeg = FMath::Max(MaxSlopeDeg, Seg.GradeDegrees);
			++InclinedSegmentCount;
		}

		FIstanbulMeshUtils::AppendRoadRibbonWithSidewalks(
			bIsCobble ? CobbleRoadSection : AsphaltRoadSection,
			SidewalkSection,
			Seg.Start, Seg.End,
			Seg.WidthM * 100.0f,
			220.0f,
			Seg.SidewalkHeightCm,
			bIsCobble ? CobbleCol : AsphaltCol,
			SidewalkCol,
			Subdivs);
	}

	// Base coastal ground slab under the boulevard so intersections are seamless
	SpawnBox(TEXT("Road_CoastalBoulevard"), Origin + FVector(17000.0f, 0.0f, -26.0f),
		FVector(420.0f, 22.0f, 0.5f), AsphaltCol, true, FRotator::ZeroRotator, TEXT("Surface_Asphalt"));

	// T1 Tramway Raised Concrete Bed & Embedded Steel Rails along the median strip
	SpawnBox(TEXT("Tram_RailBed"), Origin + FVector(8000.0f, -20.0f, 1.5f),
		FVector(180.0f, 2.8f, 0.05f), FLinearColor(0.28f, 0.29f, 0.31f), true, FRotator::ZeroRotator, TEXT("Surface_TramRail"));
	SpawnBox(TEXT("Tram_SteelRailL"), Origin + FVector(8000.0f, -92.0f, 4.5f),
		FVector(180.0f, 0.08f, 0.03f), FLinearColor(0.65f, 0.68f, 0.72f), false);
	SpawnBox(TEXT("Tram_SteelRailR"), Origin + FVector(8000.0f,  52.0f, 4.5f),
		FVector(180.0f, 0.08f, 0.03f), FLinearColor(0.65f, 0.68f, 0.72f), false);

	// Patched Asphalt & Pothole clusters along the boulevard
	SpawnBox(TEXT("Surface_PatchedStrip1"), Origin + FVector(5200.0f, 450.0f, 1.0f),
		FVector(14.0f, 3.6f, 0.03f), FLinearColor(0.05f, 0.05f, 0.06f), true, FRotator::ZeroRotator, TEXT("Surface_PatchedAsphalt"));
	SpawnBox(TEXT("Surface_Pothole1"), Origin + FVector(6800.0f, 380.0f, 2.0f),
		FVector(1.8f, 1.6f, 0.06f), FLinearColor(0.03f, 0.03f, 0.035f), true, FRotator::ZeroRotator, TEXT("Surface_Pothole"));
	SpawnBox(TEXT("Surface_Pothole2"), Origin + FVector(14200.0f, -420.0f, 2.0f),
		FVector(2.1f, 1.8f, 0.06f), FLinearColor(0.03f, 0.03f, 0.035f), true, FRotator::ZeroRotator, TEXT("Surface_Pothole"));

	// Boulevard Bollards (via HISM), Streetlamps & Catenary Tram Wires
	for (int32 i = 0; i < 20; ++i)
	{
		const float X = i * 2000.0f - 2000.0f;
		SpawnBox(FString::Printf(TEXT("Blvd_DashR_%d"), i), Origin + FVector(X,  520.0f, 1.5f),
			FVector(6.0f, 0.18f, 0.02f), FLinearColor(0.92f, 0.92f, 0.92f), false);
		SpawnBox(FString::Printf(TEXT("Blvd_DashL_%d"), i), Origin + FVector(X, -520.0f, 1.5f),
			FVector(6.0f, 0.18f, 0.02f), FLinearColor(0.92f, 0.92f, 0.92f), false);

		if (BollardHISM && BollardHISM->GetStaticMesh())
		{
			BollardHISM->AddInstance(FTransform(FRotator::ZeroRotator, Origin + FVector(X, -1120.0f, 48.0f), FVector(0.24f, 0.24f, 0.95f)), true);
			++SpawnedHISMInstanceCount;
		}

		if (i % 2 == 0)
		{
			SpawnStreetLamp(FString::Printf(TEXT("Lamp_Sea_%d"), i),  Origin + FVector(X, -1180.0f, 20.0f),  90.0f);
			SpawnStreetLamp(FString::Printf(TEXT("Lamp_City_%d"), i), Origin + FVector(X,  1180.0f, 20.0f), -90.0f);

			if (X >= 0.0f && X <= 18000.0f)
			{
				FIstanbulMeshUtils::AppendCatenaryCable(CatenaryCableSection,
					Origin + FVector(X, -1160.0f, 690.0f),
					Origin + FVector(X,  1160.0f, 690.0f),
					38.0f, 3.5f, 8, FLinearColor(0.05f, 0.05f, 0.06f));
			}
		}
	}

	// Galata Bridge Two-Level Deck & Fishing Rods, Old-Town Square Fountain, and Midpoint Slope Stop-Sign
	SpawnGalataBridgeAndApproach(Origin);
	SpawnOldTownSquareAndFountain(Origin + FVector(15200.0f, 3800.0f, 0.0f));
	SpawnSlopeStopSignAndSwitchback(Origin);
	SpawnBosphorusScenicOverlook(Origin);

	// İETT Bus Stops (Eminönü İskele Durağı at +25m & Kabataş Durağı at +155m)
	SpawnIETTBusStop(TEXT("IETT_Stop_Eminonu"), Origin + FVector(2500.0f, 1320.0f, 22.0f), -90.0f);
	SpawnIETTBusStop(TEXT("IETT_Stop_Kabatas"), Origin + FVector(15500.0f, 1320.0f, 22.0f), -90.0f);

	// Pickup Marker (+30m at Eminönü İETT Stop) & Drop-off Marker (+180m before HGS Bridge)
	SpawnBox(TEXT("Proto_PickupMarker_30m"), Origin + FVector(3000.0f, 1020.0f, 180.0f),
		FVector(0.75f, 0.75f, 3.6f), FLinearColor(0.15f, 0.95f, 0.35f), false);
	SpawnBox(TEXT("Proto_DropoffMarker_180m"), Origin + FVector(18000.0f, 1020.0f, 180.0f),
		FVector(0.75f, 0.75f, 3.6f), FLinearColor(1.0f, 0.68f, 0.1f), false);

	// HGS / OGS Toll Plaza at X = +235m + Bosphorus Bridge Towers at X = +320m
	const FVector TollPos = Origin + FVector(23500.0f, 0.0f, 180.0f);
	if (!UGameplayStatics::GetActorOfClass(W, ATollGateVolume::StaticClass()))
	{
		AActor* TollActor = W->SpawnActor<ATollGateVolume>(TollPos, FRotator::ZeroRotator, SpawnParams);
		if (TollActor) TollActor->Tags.Add(GeneratedActorTag);
	}
	SpawnHGSBridgeCanopy(Origin + FVector(23500.0f, 0.0f, 0.0f));

	// =========================================================================
	// 3. OLD-TOWN & STEEP BACKSTREET ARCHITECTURE, STONE WALLS & CATENARY CABLES
	// =========================================================================
	SpawnHistoricalStoneWall(TEXT("HistWall_Uphill_Left"),    Origin + FVector(3010.0f,  4350.0f, 710.0f), FRotator(13.2f,  90.0f, 0.0f), 58.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_Uphill_Right"),   Origin + FVector(3990.0f,  4350.0f, 710.0f), FRotator(13.2f,  90.0f, 0.0f), 58.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_Downhill_Left"),  Origin + FVector(20010.0f, 4350.0f, 710.0f), FRotator(14.0f, -90.0f, 0.0f), 58.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_Downhill_Right"), Origin + FVector(20990.0f, 4350.0f, 710.0f), FRotator(14.0f, -90.0f, 0.0f), 58.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_RidgeTerrace"),   Origin + FVector(12000.0f, 5950.0f, 960.0f), FRotator::ZeroRotator,         150.0f, 5.4f);

	static const FLinearColor IstanbulPalette[7] = {
		FLinearColor(0.76f, 0.56f, 0.36f),
		FLinearColor(0.68f, 0.34f, 0.28f),
		FLinearColor(0.74f, 0.72f, 0.66f),
		FLinearColor(0.45f, 0.58f, 0.68f),
		FLinearColor(0.55f, 0.64f, 0.48f),
		FLinearColor(0.72f, 0.62f, 0.52f),
		FLinearColor(0.62f, 0.46f, 0.42f)
	};

	// Row 1: Boulevard city-side apartment blocks
	for (int32 b = 0; b < 9; ++b)
	{
		if (b == 1 || b == 6) continue; // Leave openings for Old-Town alleys at X=+72m and X=+168m
		const float BX = 5400.0f + b * 1600.0f;
		const int32 Floors = 4 + (b % 3);
		SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_Blvd_%d"), b),
			Origin + FVector(BX, 2350.0f, 0.0f), -90.0f, Floors, 14.8f, 13.0f,
			IstanbulPalette[b % 7], (b % 2 == 0), true, Rng);
	}

	// Row 2: Steep Galata Climb flanking apartments
	for (int32 b = 0; b < 3; ++b)
	{
		const float BY = 2600.0f + b * 1700.0f;
		const float BZ = 280.0f + b * 340.0f;
		SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UphillL_%d"), b),
			Origin + FVector(2150.0f, BY, BZ), 0.0f, 5, 16.5f, 13.0f,
			IstanbulPalette[(b + 2) % 7], true, true, Rng);
		SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UphillR_%d"), b),
			Origin + FVector(4850.0f, BY, BZ), 180.0f, 4 + (b % 2), 16.5f, 13.0f,
			IstanbulPalette[(b + 4) % 7], true, (b == 0), Rng);
	}

	// Row 3: Upper Historic Ridge tightly-packed bitişik nizam row houses (Z = +1290cm)
	for (int32 b = 0; b < 9; ++b)
	{
		const float BX = 5400.0f + b * 1600.0f;
		if (b != 4 && b != 5) // Leave switchback junction opening at X=+118m..135m
		{
			SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UpperSouth_%d"), b),
				Origin + FVector(BX, 6150.0f, 1290.0f), 90.0f, 4 + (b % 3), 15.4f, 12.0f,
				IstanbulPalette[(b + 1) % 7], true, true, Rng);
		}

		if (b < 3 || b > 5)
		{
			SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UpperNorth_%d"), b),
				Origin + FVector(BX, 9050.0f, 1290.0f), -90.0f, 5 + (b % 2), 15.4f, 12.0f,
				IstanbulPalette[(b + 3) % 7], true, true, Rng);
		}

		if (BollardHISM && BollardHISM->GetStaticMesh())
		{
			BollardHISM->AddInstance(FTransform(FRotator::ZeroRotator, Origin + FVector(BX, 7110.0f, 1335.0f), FVector(0.22f, 0.22f, 0.85f)), true);
			++SpawnedHISMInstanceCount;
		}

		// Yellow-black alternating-band vertex-color bollard + sagging laundry lines / hanging street lanterns
		FIstanbulMeshUtils::AppendStripedBollard(SidewalkSection, Origin + FVector(BX + 350.0f, 7090.0f, 1295.0f), 11.0f, 86.0f);
		FIstanbulMeshUtils::AppendBackstreetLaundryOrLantern(CatenaryCableSection,
			Origin + FVector(BX, 6800.0f, 2120.0f),
			Origin + FVector(BX + 450.0f, 8400.0f, 2120.0f),
			65.0f, (b % 2 == 0), Rng);

		// Sidewalk tea-garden café table & Istanbul street cat silhouette clusters
		if (b % 2 == 1)
		{
			FIstanbulMeshUtils::AppendCafeTableAndCatCluster(SidewalkSection,
				Origin + FVector(BX + 180.0f, 6920.0f, 1312.0f), 90.0f, Rng);
		}

		SpawnStreetLamp(FString::Printf(TEXT("UpperAlleyLamp_%d"), b),
			Origin + FVector(BX - 520.0f, 7090.0f, 1305.0f), 90.0f);
	}

	// Coursed historic stone retaining wall with per-block vertex-color noise along the Old-Town Square & Galata climb
	FIstanbulMeshUtils::AppendStoneWallWithVariation(SidewalkSection,
		FTransform(FRotator(0.0f, 0.0f, 0.0f), Origin + FVector(12000.0f, 5880.0f, 320.0f)),
		6400.0f, 480.0f, 65.0f, Rng);
	FIstanbulMeshUtils::AppendCafeTableAndCatCluster(SidewalkSection,
		Origin + FVector(14450.0f, 3450.0f, 18.0f), 0.0f, Rng);
	FIstanbulMeshUtils::AppendCafeTableAndCatCluster(SidewalkSection,
		Origin + FVector(15950.0f, 3450.0f, 18.0f), 180.0f, Rng);

	// Commit procedural road, sidewalk, cobblestone, and catenary cable sections!
	UMaterialInterface* ProcMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	AsphaltRoadSection.CommitToComponent(CorridorProcMesh, 0, true, ProcMat);
	SidewalkSection.CommitToComponent(CorridorProcMesh, 1, true, ProcMat);
	CatenaryCableSection.CommitToComponent(CorridorProcMesh, 2, false, ProcMat);
	CobbleRoadSection.CommitToComponent(CobblestoneProcMesh, 0, true, ProcMat);
	SpawnedProcMeshSectionCount += 4;

	// Spawn batched AIstanbulBuildingBlock for data-driven BuildingSpecs (with Cumbas, spanning alley arch, and shop signs)
	if (BuildingSpecs.Num() > 0)
	{
		if (AIstanbulBuildingBlock* BBlock = W->SpawnActor<AIstanbulBuildingBlock>(Origin, FRotator::ZeroRotator, SpawnParams))
		{
			BBlock->Tags.Add(GeneratedActorTag);
			BBlock->BuildBlockFromSpecs(BuildingSpecs);
			++SpawnedActorCount;
			SpawnedHISMInstanceCount += BBlock->GetHISMPropCount();
			SpawnedProcMeshSectionCount += 1;
		}
	}

	// Soft world-edge boundary blockers (disguised as historic stone walls/gatehouses)
	SpawnWorldBoundaryBlockers(Origin);

	// Cars parked half-on-the-sidewalk + Eminönü Quay Taxi Rank
	SpawnParkedCurbCar(TEXT("TaxiRank_Quay1"),    Origin + FVector( 800.0f, -920.0f,    5.0f), FRotator(0.0f,   0.0f,  0.0f), FLinearColor(0.95f, 0.76f, 0.0f));
	SpawnParkedCurbCar(TEXT("TaxiRank_Quay2"),    Origin + FVector( 150.0f, -920.0f,    5.0f), FRotator(0.0f,   0.0f,  0.0f), FLinearColor(0.95f, 0.76f, 0.0f));
	SpawnParkedCurbCar(TEXT("ParkedCar_Uphill1"), Origin + FVector(3120.0f, 3100.0f,  365.0f), FRotator(13.2f, 90.0f,  9.0f), FLinearColor(0.75f, 0.78f, 0.82f));
	SpawnParkedCurbCar(TEXT("ParkedCar_Upper1"),  Origin + FVector(6800.0f, 7140.0f, 1305.0f), FRotator(0.0f,   2.0f, -9.0f), FLinearColor(0.16f, 0.22f, 0.35f));
	SpawnParkedCurbCar(TEXT("ParkedCar_Upper2"),  Origin + FVector(9800.0f, 8060.0f, 1305.0f), FRotator(0.0f, 178.0f,  9.0f), FLinearColor(0.58f, 0.12f, 0.12f));
	SpawnParkedCurbCar(TEXT("ParkedCar_Upper3"),  Origin + FVector(15600.0f, 7140.0f, 1305.0f), FRotator(0.0f,  0.0f, -9.0f), FLinearColor(0.82f, 0.82f, 0.80f));

	// =========================================================================
	// 4. HILLTOP MOSQUE SILHOUETTE, GALATA TOWER, BOSPHORUS WATER & AUDIO ZONES
	// =========================================================================
	const FVector MosqueHillPos = Origin + FVector(11800.0f, 10600.0f, 1290.0f);
	SpawnDetailedMosqueComplex(MosqueHillPos);
	SpawnGalataTowerLandmark(Origin + FVector(3500.0f, 9600.0f, 1290.0f));

	if (!UGameplayStatics::GetActorOfClass(W, AMosqueActor::StaticClass()))
	{
		USoundBase* PlaceholderChime = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));

		AMosqueActor* MosqueHill = W->SpawnActor<AMosqueActor>(MosqueHillPos, FRotator::ZeroRotator, SpawnParams);
		if (MosqueHill)
		{
			MosqueHill->Tags.Add(GeneratedActorTag);
			MosqueHill->MosqueName = FText::FromString(TEXT("Sultanahmet Tepesi Camii"));
			MosqueHill->MinStaggerDelaySeconds = 0.4f;
			MosqueHill->MaxStaggerDelaySeconds = 2.0f;
			MosqueHill->FallbackEzanSound = PlaceholderChime;
		}

		AMosqueActor* MosqueCoast = W->SpawnActor<AMosqueActor>(Origin + FVector(19500.0f, -1800.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
		if (MosqueCoast)
		{
			MosqueCoast->Tags.Add(GeneratedActorTag);
			MosqueCoast->MosqueName = FText::FromString(TEXT("Sahil Yeni Cami"));
			MosqueCoast->MinStaggerDelaySeconds = 3.5f;
			MosqueCoast->MaxStaggerDelaySeconds = 8.5f;
			MosqueCoast->FallbackEzanSound = PlaceholderChime;
		}
	}

	if (!UGameplayStatics::GetActorOfClass(W, ADistrictAmbienceVolume::StaticClass()))
	{
		ADistrictAmbienceVolume* CoastZone = W->SpawnActor<ADistrictAmbienceVolume>(Origin + FVector(11000.0f, 0.0f, 200.0f), FRotator::ZeroRotator, SpawnParams);
		if (CoastZone)
		{
			CoastZone->Tags.Add(GeneratedActorTag);
			UDistrictAmbienceDataAsset* EminonuProf = NewObject<UDistrictAmbienceDataAsset>(CoastZone);
			EminonuProf->DistrictTag = IstanbulTaxiTags::District_HistoricPeninsula;
			EminonuProf->DisplayName = FText::FromString(TEXT("Eminönü / Karaköy Sahil Bulvarı"));
			EminonuProf->BaseTrafficMurmur = 0.72f;
			EminonuProf->TeaGardenClatter = 0.35f;
			EminonuProf->CrowdChatter = 0.65f;
			EminonuProf->BaseSeagullIntensity = 0.45f;
			CoastZone->AmbienceProfile = EminonuProf;
			CoastZone->Priority = 5;
		}

		ADistrictAmbienceVolume* HillZone = W->SpawnActor<ADistrictAmbienceVolume>(Origin + FVector(12000.0f, 7600.0f, 1380.0f), FRotator::ZeroRotator, SpawnParams);
		if (HillZone)
		{
			HillZone->Tags.Add(GeneratedActorTag);
			UDistrictAmbienceDataAsset* SultanProf = NewObject<UDistrictAmbienceDataAsset>(HillZone);
			SultanProf->DistrictTag = IstanbulTaxiTags::District_HistoricPeninsula;
			SultanProf->DisplayName = FText::FromString(TEXT("Cihangir / Galata / Tarihi Yarımada Yokuşu"));
			SultanProf->BaseTrafficMurmur = 0.35f;
			SultanProf->TeaGardenClatter = 0.80f;
			SultanProf->CrowdChatter = 0.55f;
			SultanProf->BaseSeagullIntensity = 0.20f;
			HillZone->AmbienceProfile = SultanProf;
			HillZone->Priority = 15;
		}
	}

	if (!UGameplayStatics::GetActorOfClass(W, AWaterfrontSplineActor::StaticClass()))
	{
		AWaterfrontSplineActor* Shore = W->SpawnActor<AWaterfrontSplineActor>(Origin + FVector(0.0f, -1650.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
		if (Shore && Shore->ShorelineSpline)
		{
			Shore->Tags.Add(GeneratedActorTag);
			Shore->ShorelineSpline->ClearSplinePoints();
			Shore->ShorelineSpline->AddSplinePoint(Origin + FVector(-4000.0f, -1650.0f, 0.0f), ESplineCoordinateSpace::World);
			Shore->ShorelineSpline->AddSplinePoint(Origin + FVector(38000.0f, -1650.0f, 0.0f), ESplineCoordinateSpace::World);
			Shore->ShorelineSpline->UpdateSpline();
		}
	}

	if (!UGameplayStatics::GetActorOfClass(W, ABosphorusWaterVolume::StaticClass()))
	{
		if (ABosphorusWaterVolume* Water = W->SpawnActor<ABosphorusWaterVolume>(Origin, FRotator::ZeroRotator, SpawnParams))
		{
			Water->Tags.Add(GeneratedActorTag);
		}
	}

	// =========================================================================
	// 5. ISTANBUL AI TRAFFIC NETWORK (Boulevard + Cobblestone Hill + Double-Parked)
	// =========================================================================
	int32 TrafficCount = 0;
	if (!UGameplayStatics::GetActorOfClass(W, AIstanbulTrafficManager::StaticClass()))
	{
		AIstanbulTrafficManager* TrafficMgr = W->SpawnActor<AIstanbulTrafficManager>(Origin, FRotator::ZeroRotator, SpawnParams);
		if (TrafficMgr)
		{
			TrafficMgr->Tags.Add(GeneratedActorTag);
			TArray<FVector> BlvdRight = {
				Origin + FVector(-2000.0f,  480.0f, 5.0f),
				Origin + FVector( 6000.0f,  480.0f, 5.0f),
				Origin + FVector(14000.0f,  480.0f, 5.0f),
				Origin + FVector(22000.0f,  480.0f, 5.0f),
				Origin + FVector(32000.0f,  480.0f, 5.0f),
				Origin + FVector(32000.0f, -480.0f, 5.0f),
				Origin + FVector(-2000.0f, -480.0f, 5.0f)
			};

			TArray<FVector> BlvdLeft = {
				Origin + FVector(31000.0f, -480.0f, 5.0f),
				Origin + FVector(22000.0f, -480.0f, 5.0f),
				Origin + FVector(13000.0f, -480.0f, 5.0f),
				Origin + FVector( 4000.0f, -480.0f, 5.0f),
				Origin + FVector(-2000.0f, -480.0f, 5.0f),
				Origin + FVector(-2000.0f,  480.0f, 5.0f),
				Origin + FVector(31000.0f,  480.0f, 5.0f)
			};

			TArray<FVector> SideStreetLoop = {
				Origin + FVector( 3500.0f,  800.0f,    5.0f),
				Origin + FVector( 3500.0f, 7600.0f, 1295.0f),
				Origin + FVector(11500.0f, 7600.0f, 1295.0f),
				Origin + FVector(20500.0f, 7600.0f, 1295.0f),
				Origin + FVector(20500.0f,  800.0f,    5.0f),
				Origin + FVector(12000.0f,  480.0f,    5.0f)
			};

			TrafficMgr->PopulateIstanbulTraffic(Origin, BlvdRight, BlvdLeft, SideStreetLoop);
			TrafficCount = TrafficMgr->GetSpawnedVehicles().Num();
		}
	}

	int32 CountTaxi = 0, CountBus = 0, CountDolmus = 0, CountVan = 0, CountScooter = 0, ChokepointCount = 0;
	if (AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(W, AIstanbulTrafficManager::StaticClass())))
	{
		ChokepointCount = TM->GetChokepoints().Num();
		for (const ATrafficVehicle* V : TM->GetSpawnedVehicles())
		{
			if (!V) continue;
			switch (V->GetVehicleType())
			{
			case EIstanbulVehicleType::YellowTaxi:     ++CountTaxi;    break;
			case EIstanbulVehicleType::IETTBus:        ++CountBus;     break;
			case EIstanbulVehicleType::Dolmus:         ++CountDolmus;  break;
			case EIstanbulVehicleType::DeliveryVan:    ++CountVan;     break;
			case EIstanbulVehicleType::ScooterCourier: ++CountScooter; break;
			}
		}
	}

	const bool bNetworkValid = ValidateNetwork();

	UE_LOG(LogIstanbulTaxi, Log,
		TEXT("ATestSceneBuilder::RebuildScene(Seed=%d) Summary: Roads=%d | Junctions=%d | Districts=%d | Buildings=%d | Slopes=%d (MinGrade=%.1f deg, MaxGrade=%.1f deg) | Chokepoints=%d | Vehicles=%d (YellowTaxi=%d, IETTBus=%d, Dolmus=%d, DeliveryVan=%d, Scooter=%d) | Props(Actors=%d, HISMInstances=%d, ProcMeshSections=%d, StreetLampPoints=%d) | ValidateNetwork=%s."),
		Seed, RoadSegments.Num(), SolvedJunctionCount, Districts.Num(), BuildingSpecs.Num(), InclinedSegmentCount,
		(InclinedSegmentCount > 0 ? MinSlopeDeg : 0.0f), MaxSlopeDeg, ChokepointCount,
		TrafficCount, CountTaxi, CountBus, CountDolmus, CountVan, CountScooter,
		SpawnedActorCount, SpawnedHISMInstanceCount, SpawnedProcMeshSectionCount, StreetLampLightWorldPositions.Num(),
		bNetworkValid ? TEXT("PASS") : TEXT("FAIL"));
}
