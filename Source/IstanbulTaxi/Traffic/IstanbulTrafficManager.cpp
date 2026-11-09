#include "Traffic/IstanbulTrafficManager.h"
#include "Core/IstanbulMeshUtils.h"
#include "Core/TestSceneBuilder.h"
#include "Vehicle/TaxiPawn.h"
#include "Missions/PassengerInteractionComponent.h"
#include "IstanbulTaxi.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

// =============================================================================
// Task 2.4 Console Commands:
//   - Istanbul.TeleportToChokepoint <index>
//   - Istanbul.ToggleChokepointDebug
//   - Istanbul.SpawnTraffic <type> <count>
//   - Istanbul.Stats
// =============================================================================

static FAutoConsoleCommandWithWorldAndArgs GCmdTeleportToChokepoint(
	TEXT("Istanbul.TeleportToChokepoint"),
	TEXT("Teleports the player taxi to chokepoint <index> (0..3). Usage: Istanbul.TeleportToChokepoint 0"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 Idx = (Args.Num() > 0) ? FCString::Atoi(*Args[0]) : 0;
		AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(World, AIstanbulTrafficManager::StaticClass()));
		APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		if (!TM || !Pawn || TM->GetChokepoints().Num() == 0)
		{
			UE_LOG(LogIstanbulTaxi, Warning, TEXT("Istanbul.TeleportToChokepoint: No AIstanbulTrafficManager or chokepoints found."));
			return;
		}
		const int32 ClampedIdx = FMath::Clamp(Idx, 0, TM->GetChokepoints().Num() - 1);
		const FIstanbulChokepointSpec& CP = TM->GetChokepoints()[ClampedIdx];
		if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(Pawn)) Taxi->ResetVehicle();

		// Place player 12m before the chokepoint facing it
		const FVector ApproachOffset(-950.0f, -650.0f, 80.0f);
		const FVector SpawnLoc = CP.Location + ApproachOffset;
		const FRotator SpawnRot = (CP.Location - SpawnLoc).Rotation();
		Pawn->SetActorLocationAndRotation(SpawnLoc, FRotator(0.0f, SpawnRot.Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

		UE_LOG(LogIstanbulTaxi, Log, TEXT("Istanbul.TeleportToChokepoint %d -> '%s' (FreeGap=%.2fm, Desc='%s') at %s"),
			ClampedIdx, *CP.ChokepointId.ToString(), CP.FreeGapWidthM, *CP.Description, *CP.Location.ToCompactString());
	}));

static FAutoConsoleCommandWithWorld GCmdToggleChokepointDebug(
	TEXT("Istanbul.ToggleChokepointDebug"),
	TEXT("Toggles 3D debug visualization of active chokepoints, free gap widths, and blocking vehicles."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		if (AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(World, AIstanbulTrafficManager::StaticClass())))
		{
			const bool bNewState = !TM->IsChokepointDebugDrawEnabled();
			TM->SetChokepointDebugDraw(bNewState);
			UE_LOG(LogIstanbulTaxi, Log, TEXT("Istanbul.ToggleChokepointDebug -> %s (%d chokepoints)"),
				bNewState ? TEXT("ENABLED") : TEXT("DISABLED"), TM->GetChokepoints().Num());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdSpawnTraffic(
	TEXT("Istanbul.SpawnTraffic"),
	TEXT("Spawns <count> traffic vehicles of <type> (taxi|bus|dolmus|van|scooter). Usage: Istanbul.SpawnTraffic dolmus 2"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(World, AIstanbulTrafficManager::StaticClass()));
		if (!TM) return;

		const FString TypeStr = (Args.Num() > 0) ? Args[0].ToLower() : TEXT("taxi");
		const int32 Count = (Args.Num() > 1) ? FMath::Clamp(FCString::Atoi(*Args[1]), 1, 16) : 1;

		EIstanbulVehicleType Type = EIstanbulVehicleType::YellowTaxi;
		if (TypeStr.Contains(TEXT("bus")) || TypeStr.Contains(TEXT("iett")) || TypeStr == TEXT("1"))
		{
			Type = EIstanbulVehicleType::IETTBus;
		}
		else if (TypeStr.Contains(TEXT("dolmus")) || TypeStr.Contains(TEXT("minibus")) || TypeStr == TEXT("2"))
		{
			Type = EIstanbulVehicleType::Dolmus;
		}
		else if (TypeStr.Contains(TEXT("van")) || TypeStr.Contains(TEXT("kargo")) || TypeStr == TEXT("3"))
		{
			Type = EIstanbulVehicleType::DeliveryVan;
		}
		else if (TypeStr.Contains(TEXT("scooter")) || TypeStr.Contains(TEXT("kurye")) || TypeStr.Contains(TEXT("motor")) || TypeStr == TEXT("4"))
		{
			Type = EIstanbulVehicleType::ScooterCourier;
		}

		const int32 Spawned = TM->SpawnExtraTrafficByType(Type, Count);
		UE_LOG(LogIstanbulTaxi, Log, TEXT("Istanbul.SpawnTraffic -> Spawned %d vehicle(s) of type '%s' (Total active=%d)."),
			Spawned, *TypeStr, TM->GetSpawnedVehicles().Num());
	}));

static FAutoConsoleCommandWithWorld GCmdIstanbulStats(
	TEXT("Istanbul.Stats"),
	TEXT("Prints live Istanbul scene statistics: roads, junctions, buildings, HISM instances, proc-mesh sections, traffic & LOD counts."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) return;
		ATestSceneBuilder* Builder = Cast<ATestSceneBuilder>(UGameplayStatics::GetActorOfClass(World, ATestSceneBuilder::StaticClass()));
		AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(World, AIstanbulTrafficManager::StaticClass()));

		int32 TotalVehicles = 0, LowLodCount = 0;
		int32 ByType[5] = { 0, 0, 0, 0, 0 };
		if (TM)
		{
			for (const ATrafficVehicle* V : TM->GetSpawnedVehicles())
			{
				if (!V) continue;
				++TotalVehicles;
				if (V->IsUsingLowLOD()) ++LowLodCount;
				const int32 TIdx = FMath::Clamp(static_cast<int32>(V->GetVehicleType()), 0, 4);
				++ByType[TIdx];
			}
		}

		UE_LOG(LogIstanbulTaxi, Log,
			TEXT("Istanbul.Stats -> Roads=%d | Districts=%d | Buildings=%d | SpawnedActors=%d | HISMInstances=%d | ProcMeshSections=%d | Vehicles=%d (HighDetail=%d, LowLOD80m=%d | Taxi=%d, Bus=%d, Dolmus=%d, Van=%d, Scooter=%d) | Chokepoints=%d"),
			Builder ? Builder->RoadSegments.Num() : 0,
			Builder ? Builder->Districts.Num() : 0,
			Builder ? Builder->BuildingSpecs.Num() : 0,
			Builder ? Builder->GetSpawnedActorCount() : 0,
			Builder ? Builder->GetSpawnedHISMInstanceCount() : 0,
			Builder ? Builder->GetSpawnedProcMeshSectionCount() : 0,
			TotalVehicles, TotalVehicles - LowLodCount, LowLodCount,
			ByType[0], ByType[1], ByType[2], ByType[3], ByType[4],
			TM ? TM->GetChokepoints().Num() : 0);
	}));

// =============================================================================
// UHornResponseComponent (Task 2.3)
// =============================================================================

UHornResponseComponent::UHornResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHornResponseComponent::OnPlayerHornHeard(float HoldDurationSec, const FVector& HornWorldLocation, FString& OutReactionSummary)
{
	ATrafficVehicle* OwnerVehicle = Cast<ATrafficVehicle>(GetOwner());
	if (!OwnerVehicle) return false;

	const float DistCm = FVector::Dist(OwnerVehicle->GetActorLocation(), HornWorldLocation);
	if (DistCm > 2500.0f) // 25m hearing radius
	{
		return false;
	}

	const bool bAggressiveLongHorn = (HoldDurationSec >= 0.35f);

	// 1. Polite/yielding driver or movable double-parked van/dolmus: pull aside to widen the gap!
	if (DriverMood == EIstanbulDriverMood::PoliteYielding ||
		(DriverMood == EIstanbulDriverMood::StoicNormal && bAggressiveLongHorn) ||
		Patience >= 0.55f)
	{
		OwnerVehicle->NudgeAsideForPlayer(bAggressiveLongHorn ? 78.0f : 55.0f, 95.0f);
		OutReactionSummary = FString::Printf(TEXT("%s sağa yanaşıp yol verdi"), *OwnerVehicle->GetDisplayLabel());
		return true;
	}

	// 2. Impatient or low-patience (< 0.35) driver: honks back!
	if (DriverMood == EIstanbulDriverMood::ImpatientHonker ||
		DriverMood == EIstanbulDriverMood::AggressiveTailgater ||
		Patience < 0.35f)
	{
		OwnerVehicle->TriggerHorn(bAggressiveLongHorn
			? TEXT("Karşı korna: Ne basıyorsun birader, uçacak halimiz yok!")
			: TEXT("Kısa korna karşılığı"));
		// Still nudges slightly on a long aggressive honk so the road never permanently blocks the player
		if (bAggressiveLongHorn)
		{
			OwnerVehicle->NudgeAsideForPlayer(48.0f, 60.0f);
		}
		OutReactionSummary = FString::Printf(TEXT("%s kornayla karşılık verdi"), *OwnerVehicle->GetDisplayLabel());
		return true;
	}

	OutReactionSummary = FString::Printf(TEXT("%s istifini bozmadı"), *OwnerVehicle->GetDisplayLabel());
	return false;
}

// =============================================================================
// ATrafficVehicle (Task 2.1, 2.2, 2.3)
// =============================================================================

ATrafficVehicle::ATrafficVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	VehicleProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VehicleProcMesh"));
	VehicleProcMesh->SetupAttachment(Root);
	VehicleProcMesh->bUseComplexAsSimpleCollision = true;
	VehicleProcMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	VehicleProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	VehicleLodMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VehicleLodMesh"));
	VehicleLodMesh->SetupAttachment(Root);
	VehicleLodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VehicleLodMesh->SetVisibility(false);

	HornResponse = CreateDefaultSubobject<UHornResponseComponent>(TEXT("HornResponse"));

	FrontPlateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FrontPlateText"));
	FrontPlateText->SetupAttachment(Root);
	FrontPlateText->SetHorizontalAlignment(EHTA_Center);
	FrontPlateText->SetVerticalAlignment(EVRTA_TextCenter);
	FrontPlateText->SetWorldSize(11.0f);
	FrontPlateText->SetTextRenderColor(FColor(15, 15, 18));
	FrontPlateText->LDMaxDrawDistance = 3200.0f; // Culled beyond 32m

	RearPlateText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RearPlateText"));
	RearPlateText->SetupAttachment(Root);
	RearPlateText->SetHorizontalAlignment(EHTA_Center);
	RearPlateText->SetVerticalAlignment(EVRTA_TextCenter);
	RearPlateText->SetWorldSize(11.0f);
	RearPlateText->SetTextRenderColor(FColor(15, 15, 18));
	RearPlateText->LDMaxDrawDistance = 3200.0f; // Culled beyond 32m

	RouteSignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RouteSignText"));
	RouteSignText->SetupAttachment(Root);
	RouteSignText->SetHorizontalAlignment(EHTA_Center);
	RouteSignText->SetVerticalAlignment(EVRTA_TextCenter);
	RouteSignText->SetWorldSize(16.0f);
	RouteSignText->SetTextRenderColor(FColor(255, 195, 20));
	RouteSignText->LDMaxDrawDistance = 4500.0f; // Near-player only (45m)
	RouteSignText->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!CubeFinder.Succeeded())
	{
		UE_LOG(LogIstanbulTaxi, Warning, TEXT("ATrafficVehicle: Cube BasicShape not found in constructor."));
	}
	if (!CylFinder.Succeeded())
	{
		UE_LOG(LogIstanbulTaxi, Warning, TEXT("ATrafficVehicle: Cylinder BasicShape not found in constructor."));
	}

	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel_%d"), i));
		W->SetupAttachment(Root);
		W->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
		W->SetRelativeScale3D(FVector(0.64f, 0.64f, 0.24f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CylFinder.Succeeded()) W->SetStaticMesh(CylFinder.Object);
		WheelMeshes.Add(W);
	}

	for (int32 i = 0; i < 2; ++i)
	{
		UStaticMeshComponent* H = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HazardLamp_%d"), i));
		H->SetupAttachment(Root);
		H->SetRelativeScale3D(FVector(0.16f, 0.28f, 0.14f));
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CubeFinder.Succeeded()) H->SetStaticMesh(CubeFinder.Object);
		HazardLampMeshes.Add(H);
	}
}

void ATrafficVehicle::TintMesh(UStaticMeshComponent* Comp, const FLinearColor& Color)
{
	if (!Comp) return;
	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Comp->SetMaterial(0, MID);
	}
}

void ATrafficVehicle::BuildIstanbulVehicleMeshSections(EIstanbulVehicleType InType, int32 StyleSeed,
	FIstanbulMeshSectionData& OutHighDetail, FIstanbulMeshSectionData& OutLowLod,
	float& OutLengthCm, float& OutWidthCm)
{
	OutHighDetail.Reset();
	OutLowLod.Reset();

	FRandomStream Rng(StyleSeed);
	// Task 2.2: Per-instance slight color/dirt variation (±4% brightness/saturation jitter)
	const float DirtJitter = Rng.FRandRange(0.96f, 1.04f);

	const FLinearColor DarkGlass(0.06f, 0.09f, 0.13f, 1.0f);
	const FLinearColor WheelArchCol(0.04f, 0.04f, 0.05f, 1.0f);
	const FLinearColor BumperDark(0.10f, 0.11f, 0.13f, 1.0f);
	const FLinearColor HeadlightCol(1.00f, 0.96f, 0.78f, 1.0f);
	const FLinearColor TaillightCol(0.92f, 0.06f, 0.05f, 1.0f);
	const FLinearColor PlateWhite(0.95f, 0.95f, 0.96f, 1.0f);
	const FLinearColor PlateBlueTR(0.04f, 0.24f, 0.78f, 1.0f);

	OutLengthCm = 445.0f;
	OutWidthCm  = 180.0f;

	switch (InType)
	{
	case EIstanbulVehicleType::YellowTaxi:
	{
		// 1. Yellow Taxi (~4.45m x 1.80m x 1.48m): Beveled hood, raked windshield, cabin, trunk,
		//    front & rear bumpers, wheel arches, side mirrors, roof "TAKSİ" sign.
		OutLengthCm = 445.0f;
		OutWidthCm  = 180.0f;
		const FLinearColor TaxiYellow = FLinearColor::FromSRGBColor(FColor(0xF5, 0xC4, 0x00)) * DirtJitter;
		const FLinearColor CheckerBand(0.05f, 0.05f, 0.06f, 1.0f);
		const FLinearColor RoofSignYellow(1.0f, 0.92f, 0.18f, 1.0f);

		// Lower sill chassis
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 44.0f)),
			FVector(216.0f, 88.0f, 18.0f), 5.0f, TaxiYellow * 0.94f);
		// Front & Rear Bumpers
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(218.0f, 0.0f, 38.0f)),
			FVector(10.0f, 89.0f, 12.0f), 4.0f, BumperDark);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-218.0f, 0.0f, 40.0f)),
			FVector(10.0f, 89.0f, 13.0f), 4.0f, BumperDark);
		// Front Hood (slightly sloped)
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FRotator(-4.0f, 0.0f, 0.0f), FVector(138.0f, 0.0f, 65.0f)),
			FVector(78.0f, 86.0f, 18.0f), 6.0f, TaxiYellow);
		// Raked Front Windshield & Rear Backlight
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FRotator(-28.0f, 0.0f, 0.0f), FVector(74.0f, 0.0f, 96.0f)),
			FVector(32.0f, 76.0f, 4.0f), 1.5f, DarkGlass);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FRotator(26.0f, 0.0f, 0.0f), FVector(-96.0f, 0.0f, 96.0f)),
			FVector(30.0f, 75.0f, 4.0f), 1.5f, DarkGlass);
		// Center Passenger Cabin & Dark Glass Window Band
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-12.0f, 0.0f, 96.0f)),
			FVector(104.0f, 78.0f, 34.0f), 8.0f, TaxiYellow);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-12.0f, 0.0f, 98.0f)),
			FVector(92.0f, 80.5f, 19.0f), 3.5f, DarkGlass);
		// Rear Trunk Deck
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-156.0f, 0.0f, 68.0f)),
			FVector(62.0f, 85.0f, 20.0f), 6.0f, TaxiYellow);
		// Black Checkered Waist Band
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 62.0f)),
			FVector(219.0f, 90.0f, 4.5f), 1.0f, CheckerBand);
		// Left & Right Side Mirrors
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(58.0f, Side * 95.0f, 88.0f)),
				FVector(7.0f, 10.0f, 5.5f), 1.5f, TaxiYellow);
		}
		// Roof-mounted "TAKSİ" light dome
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-5.0f, 0.0f, 136.0f)),
			FVector(18.0f, 42.0f, 8.5f), 2.5f, RoofSignYellow);

		// 80m Low-Poly LOD Mesh (2 boxes: body + cabin glass)
		FIstanbulMeshUtils::AppendBeveledBox(OutLowLod, FTransform(FVector(0.0f, 0.0f, 58.0f)),
			FVector(220.0f, 88.0f, 28.0f), 2.0f, TaxiYellow);
		FIstanbulMeshUtils::AppendBeveledBox(OutLowLod, FTransform(FVector(-12.0f, 0.0f, 102.0f)),
			FVector(102.0f, 78.0f, 22.0f), 2.0f, DarkGlass);
		break;
	}
	case EIstanbulVehicleType::IETTBus:
	{
		// 2. İETT Bus (11.8m long, 2.5m wide, 3.1m tall): Red/white livery, dark window band,
		//    front route sign board, 3 doors on right side, recessed wheel wells, rooftop AC pod.
		OutLengthCm = 1180.0f;
		OutWidthCm  = 250.0f;
		const FLinearColor IETTRed   = FLinearColor::FromSRGBColor(FColor(0xC8, 0x10, 0x2E)) * DirtJitter;
		const FLinearColor IETTWhite = FLinearColor(0.94f, 0.94f, 0.96f, 1.0f) * DirtJitter;
		const FLinearColor DoorDark(0.08f, 0.09f, 0.11f, 1.0f);
		const FLinearColor RouteAmber(1.00f, 0.58f, 0.04f, 1.0f);

		// Lower Red Hull (#C8102E) + Front/Rear Bumpers
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 78.0f)),
			FVector(585.0f, 124.0f, 46.0f), 10.0f, IETTRed);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(588.0f, 0.0f, 46.0f)),
			FVector(8.0f, 125.0f, 16.0f), 3.0f, BumperDark);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-588.0f, 0.0f, 46.0f)),
			FVector(8.0f, 125.0f, 16.0f), 3.0f, BumperDark);
		// Crisp White Upper Livery & Coach Crown
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 134.0f)),
			FVector(586.0f, 125.0f, 16.0f), 4.0f, IETTWhite);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 208.0f)),
			FVector(582.0f, 122.0f, 62.0f), 12.0f, IETTWhite);
		// Dark Panoramic Window Band
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(4.0f, 0.0f, 198.0f)),
			FVector(585.0f, 124.5f, 36.0f), 6.0f, DarkGlass);
		// 3 Passenger Folding Doors on the Right Side (+Y = +125.5cm)
		const float DoorOffsetsX[3] = { 440.0f, 45.0f, -410.0f };
		for (int32 d = 0; d < 3; ++d)
		{
			FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(DoorOffsetsX[d], 125.5f, 132.0f)),
				FVector(58.0f, 2.0f, 94.0f), 1.0f, DoorDark);
		}
		// Rooftop AC Pods (bringing total height to ~3.1m)
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(210.0f, 0.0f, 286.0f)),
			FVector(135.0f, 82.0f, 18.0f), 5.0f, IETTWhite * 0.92f);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-220.0f, 0.0f, 282.0f)),
			FVector(95.0f, 78.0f, 15.0f), 4.0f, IETTWhite * 0.92f);
		// Front LED Destination Sign Board
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(586.0f, 0.0f, 248.0f)),
			FVector(3.0f, 82.0f, 14.0f), 1.0f, RouteAmber);

		// 80m Low-Poly LOD Mesh
		FIstanbulMeshUtils::AppendBeveledBox(OutLowLod, FTransform(FVector(0.0f, 0.0f, 95.0f)),
			FVector(585.0f, 124.0f, 62.0f), 2.0f, IETTRed);
		FIstanbulMeshUtils::AppendBeveledBox(OutLowLod, FTransform(FVector(0.0f, 0.0f, 210.0f)),
			FVector(582.0f, 122.0f, 55.0f), 2.0f, IETTWhite);
		break;
	}
	case EIstanbulVehicleType::Dolmus:
	{
		// 3. Dolmuş (5.6m long, 2.05m wide, 2.35m tall): Yellow/cream body, black waist stripe,
		//    boxy Transit-style nose, sliding right-side door seam, roof route sign, rear bumper step.
		OutLengthCm = 560.0f;
		OutWidthCm  = 205.0f;
		const FLinearColor DolmusYellow = FLinearColor::FromSRGBColor(FColor(0xF2, 0xC6, 0x18)) * DirtJitter;
		const FLinearColor BlackStripe(0.05f, 0.05f, 0.06f, 1.0f);

		// Main Transit Lower Van Box + Rear Bumper Step
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 74.0f)),
			FVector(276.0f, 101.0f, 42.0f), 8.0f, DolmusYellow);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-284.0f, 0.0f, 38.0f)),
			FVector(10.0f, 98.0f, 8.0f), 2.0f, BumperDark);
		// Boxy Transit Sloped Nose / Hood
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FRotator(-8.0f, 0.0f, 0.0f), FVector(232.0f, 0.0f, 88.0f)),
			FVector(48.0f, 96.0f, 26.0f), 8.0f, DolmusYellow);
		// High-Roof Minibus Passenger Greenhouse (~2.35m tall)
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-24.0f, 0.0f, 152.0f)),
			FVector(230.0f, 96.0f, 46.0f), 10.0f, DolmusYellow * 0.98f);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-18.0f, 0.0f, 148.0f)),
			FVector(234.0f, 98.5f, 25.0f), 4.0f, DarkGlass);
		// Black Beltline Stripe + Right-Side Sliding-Door Track Seam
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 98.0f)),
			FVector(278.0f, 103.0f, 6.0f), 1.5f, BlackStripe);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(35.0f, 103.5f, 114.0f)),
			FVector(58.0f, 1.8f, 62.0f), 0.5f, BlackStripe);
		// Roof Route Sign ("BEŞİKTAŞ - TAKSİM")
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(185.0f, 0.0f, 206.0f)),
			FVector(14.0f, 56.0f, 9.0f), 2.0f, FLinearColor(1.0f, 0.78f, 0.12f, 1.0f));

		// 80m Low-Poly LOD Mesh
		FIstanbulMeshUtils::AppendBeveledBox(OutLowLod, FTransform(FVector(0.0f, 0.0f, 112.0f)),
			FVector(276.0f, 100.0f, 78.0f), 2.0f, DolmusYellow);
		break;
	}
	case EIstanbulVehicleType::DeliveryVan:
	{
		// 4. Delivery Van (5.2m long, 2.0m wide, 2.45m tall): Cab + taller cargo box,
		//    white/blue/grey color variants, rear double-door seam + ladder rack or open door detail.
		OutLengthCm = 520.0f;
		OutWidthCm  = 200.0f;
		static const FLinearColor VanBodyPalette[3] = {
			FLinearColor(0.92f, 0.94f, 0.96f, 1.0f), // Classic Logistics White
			FLinearColor(0.18f, 0.36f, 0.64f, 1.0f), // Fleet Blue
			FLinearColor(0.68f, 0.71f, 0.75f, 1.0f)  // Industrial Silver-Grey
		};
		const FLinearColor VanBody = VanBodyPalette[FMath::Abs(StyleSeed) % 3] * DirtJitter;
		const FLinearColor CargoStripe(0.08f, 0.38f, 0.78f, 1.0f);
		const FLinearColor SeamDark(0.12f, 0.14f, 0.16f, 1.0f);

		// Forward Driver Cab + Hood
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(165.0f, 0.0f, 86.0f)),
			FVector(86.0f, 94.0f, 52.0f), 8.0f, VanBody);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(176.0f, 0.0f, 108.0f)),
			FVector(76.0f, 96.0f, 22.0f), 4.0f, DarkGlass);
		// Taller Cargo Box (~2.45m top height)
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-58.0f, 0.0f, 134.0f)),
			FVector(182.0f, 100.0f, 98.0f), 6.0f, VanBody);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-58.0f, 0.0f, 108.0f)),
			FVector(183.5f, 101.5f, 12.0f), 2.0f, CargoStripe);
		// Rear Double-Door Vertical Center Seam + Rear Step Bumper + Right-Side Ladder Rack
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-241.0f, 0.0f, 134.0f)),
			FVector(2.0f, 3.0f, 92.0f), 0.5f, SeamDark);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-244.0f, -62.0f, 134.0f)),
			FVector(2.5f, 12.0f, 78.0f), 1.0f, SeamDark);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-246.0f, 0.0f, 40.0f)),
			FVector(9.0f, 98.0f, 8.0f), 2.0f, BumperDark);

		// 80m Low-Poly LOD Mesh
		FIstanbulMeshUtils::AppendBeveledBox(OutLowLod, FTransform(FVector(-20.0f, 0.0f, 122.0f)),
			FVector(250.0f, 98.0f, 88.0f), 2.0f, VanBody);
		break;
	}
	case EIstanbulVehicleType::ScooterCourier:
	{
		// 5. Optional 5th Archetype: Motorbike / Scooter Courier (Kurye) weaving along road edges
		OutLengthCm = 210.0f;
		OutWidthCm  = 82.0f;
		const FLinearColor ScooterBody(0.88f, 0.18f, 0.14f, 1.0f);
		const FLinearColor CourierBoxCol(0.98f, 0.62f, 0.04f, 1.0f);
		const FLinearColor RiderJacket(0.14f, 0.18f, 0.26f, 1.0f);
		const FLinearColor HelmetWhite(0.94f, 0.94f, 0.96f, 1.0f);

		// Scooter Lower Deck & Front Leg Shield Fairing
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(0.0f, 0.0f, 38.0f)),
			FVector(78.0f, 24.0f, 16.0f), 4.0f, ScooterBody);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FRotator(-18.0f, 0.0f, 0.0f), FVector(58.0f, 0.0f, 68.0f)),
			FVector(16.0f, 26.0f, 32.0f), 4.0f, ScooterBody);
		// Rider Torso + White Helmet
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FRotator(10.0f, 0.0f, 0.0f), FVector(-4.0f, 0.0f, 96.0f)),
			FVector(18.0f, 22.0f, 28.0f), 5.0f, RiderJacket);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(2.0f, 0.0f, 134.0f)),
			FVector(14.0f, 14.0f, 14.0f), 5.0f, HelmetWhite);
		// Rear Thermal Delivery Top-Box ("KURYE")
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-58.0f, 0.0f, 88.0f)),
			FVector(24.0f, 26.0f, 24.0f), 4.0f, CourierBoxCol);

		// 80m Low-Poly LOD Mesh
		FIstanbulMeshUtils::AppendBeveledBox(OutLowLod, FTransform(FVector(0.0f, 0.0f, 75.0f)),
			FVector(85.0f, 26.0f, 52.0f), 2.0f, CourierBoxCol);
		break;
	}
	}

	const float AxleX = OutLengthCm * 0.32f;
	const float AxleY = OutWidthCm * 0.48f;

	// Recessed dark wheel arches + emissive headlights/taillights + front/rear Turkish license plate quads
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector( AxleX, Side * (AxleY - 4.0f), 36.0f)),
			FVector(34.0f, 9.0f, 22.0f), 3.0f, WheelArchCol);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-AxleX, Side * (AxleY - 4.0f), 36.0f)),
			FVector(34.0f, 9.0f, 22.0f), 3.0f, WheelArchCol);

		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(OutLengthCm * 0.5f + 1.0f, Side * OutWidthCm * 0.32f, 56.0f)),
			FVector(3.0f, 12.0f, 7.5f), 1.0f, HeadlightCol);
		FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(-OutLengthCm * 0.5f - 1.0f, Side * OutWidthCm * 0.32f, 58.0f)),
			FVector(3.0f, 13.0f, 7.5f), 1.0f, TaillightCol);
	}

	// Front & Rear Turkish License Plate Quads (White plate + Blue "TR" strip on left)
	const float FrontPlateX = OutLengthCm * 0.5f + 2.5f;
	const float RearPlateX  = -OutLengthCm * 0.5f - 2.5f;
	FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(FrontPlateX, 0.0f, 38.0f)),
		FVector(1.2f, 26.0f, 5.5f), 0.3f, PlateWhite);
	FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(FrontPlateX + 0.4f, -22.0f, 38.0f)),
		FVector(1.0f, 4.0f, 5.5f), 0.2f, PlateBlueTR);

	FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(RearPlateX, 0.0f, 42.0f)),
		FVector(1.2f, 26.0f, 5.5f), 0.3f, PlateWhite);
	FIstanbulMeshUtils::AppendBeveledBox(OutHighDetail, FTransform(FVector(RearPlateX - 0.4f, 22.0f, 42.0f)),
		FVector(1.0f, 4.0f, 5.5f), 0.2f, PlateBlueTR);
}

void ATrafficVehicle::BuildVisualMesh(EIstanbulVehicleType InType)
{
	VehicleType = InType;
	InstanceStyleSeed = GetUniqueID() * 31 + static_cast<int32>(InType) * 97;

	FIstanbulMeshSectionData BodySection;
	FIstanbulMeshSectionData LodSection;
	float LengthCm = 445.0f;
	float WidthCm  = 180.0f;

	BuildIstanbulVehicleMeshSections(InType, InstanceStyleSeed, BodySection, LodSection, LengthCm, WidthCm);

	UMaterialInterface* VtxMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	BodySection.CommitToComponent(VehicleProcMesh, 0, true, VtxMat);
	LodSection.CommitToComponent(VehicleLodMesh, 0, false, VtxMat);

	// Configure near-player Turkish license plate text ("34 T 1453" / "34 M 4120" / "34 KY 892")
	const int32 PlateNum = 1000 + (FMath::Abs(InstanceStyleSeed) % 8999);
	const TCHAR* PlateMid = (InType == EIstanbulVehicleType::YellowTaxi) ? TEXT("T")
		: (InType == EIstanbulVehicleType::Dolmus) ? TEXT("M")
		: (InType == EIstanbulVehicleType::IETTBus) ? TEXT("TN")
		: TEXT("KY");
	const FText PlateLabel = FText::FromString(FString::Printf(TEXT("34 %s %d"), PlateMid, PlateNum));

	if (FrontPlateText)
	{
		FrontPlateText->SetText(PlateLabel);
		FrontPlateText->SetRelativeLocationAndRotation(FVector(LengthCm * 0.5f + 4.2f, 0.0f, 38.0f), FRotator(0.0f, 0.0f, 0.0f));
	}
	if (RearPlateText)
	{
		RearPlateText->SetText(PlateLabel);
		RearPlateText->SetRelativeLocationAndRotation(FVector(-LengthCm * 0.5f - 4.2f, 0.0f, 42.0f), FRotator(0.0f, 180.0f, 0.0f));
	}
	if (RouteSignText)
	{
		if (InType == EIstanbulVehicleType::IETTBus)
		{
			RouteSignText->SetText(FText::FromString(TEXT("28T EMİNÖNÜ - BEŞİKTAŞ")));
			RouteSignText->SetRelativeLocationAndRotation(FVector(LengthCm * 0.5f + 4.0f, 0.0f, 248.0f), FRotator::ZeroRotator);
			RouteSignText->SetVisibility(true);
		}
		else if (InType == EIstanbulVehicleType::Dolmus)
		{
			RouteSignText->SetText(FText::FromString(TEXT("TAKSİM - BEŞİKTAŞ")));
			RouteSignText->SetRelativeLocationAndRotation(FVector(198.0f, 0.0f, 206.0f), FRotator::ZeroRotator);
			RouteSignText->SetVisibility(true);
		}
		else
		{
			RouteSignText->SetVisibility(false);
		}
	}

	const float AxleX = LengthCm * 0.32f;
	const float AxleY = (InType == EIstanbulVehicleType::ScooterCourier) ? 0.0f : (WidthCm * 0.48f);

	const FVector WPos[4] = {
		FVector( AxleX, -AxleY, 32.0f),
		FVector( AxleX,  AxleY, 32.0f),
		FVector(-AxleX, -AxleY, 32.0f),
		FVector(-AxleX,  AxleY, 32.0f)
	};
	for (int32 i = 0; i < 4; ++i)
	{
		if (WheelMeshes.IsValidIndex(i) && WheelMeshes[i])
		{
			WheelMeshes[i]->SetRelativeLocation(WPos[i]);
			TintMesh(WheelMeshes[i], FLinearColor(0.04f, 0.04f, 0.05f));
			if (InType == EIstanbulVehicleType::ScooterCourier && (i == 1 || i == 3))
			{
				WheelMeshes[i]->SetVisibility(false);
			}
		}
	}

	for (int32 i = 0; i < 2; ++i)
	{
		const float SideSign = (i == 0) ? -1.0f : 1.0f;
		if (HazardLampMeshes.IsValidIndex(i) && HazardLampMeshes[i])
		{
			HazardLampMeshes[i]->SetRelativeLocation(FVector(-LengthCm * 0.5f - 3.0f, SideSign * WidthCm * 0.35f, 72.0f));
			TintMesh(HazardLampMeshes[i], FLinearColor(1.0f, 0.48f, 0.0f));
		}
	}
}

void ATrafficVehicle::InitializeTrafficVehicle(EIstanbulVehicleType InType, const TArray<FVector>& InWaypoints,
	float InCruiseSpeedKmh, float InLateralOffsetCm, bool bInStationaryHazard, int32 StartWaypointIdx)
{
	BuildVisualMesh(InType);
	Waypoints = InWaypoints;
	CruiseSpeedCms = InCruiseSpeedKmh / 0.036f;
	CurrentSpeedCms = bInStationaryHazard ? 0.0f : CruiseSpeedCms * 0.8f;
	CurrentLateralOffsetCm = InLateralOffsetCm;
	TargetLateralOffsetCm = InLateralOffsetCm;
	bStationaryDoubleParked = bInStationaryHazard;
	bHazardLightsActive = bInStationaryHazard;
	StationaryBaseLocation = GetActorLocation();
	StationaryTargetOffset = FVector::ZeroVector;
	CurrentWaypointIdx = (Waypoints.Num() > 0) ? (StartWaypointIdx % Waypoints.Num()) : 0;
	NextAdHocStopCooldown = FMath::FRandRange(10.0f, 22.0f);
	LaneChangeCooldown = FMath::FRandRange(7.0f, 16.0f);
	ChokepointWaitThresholdSec = FMath::FRandRange(8.0f, 15.0f);
	LastProgressCheckLoc = GetActorLocation();

	if (HornResponse)
	{
		const int32 MoodRoll = FMath::Abs(InstanceStyleSeed) % 4;
		HornResponse->DriverMood = static_cast<EIstanbulDriverMood>(MoodRoll);
		HornResponse->Patience = (MoodRoll == 0) ? 0.85f : (MoodRoll == 1) ? 0.60f : (MoodRoll == 2) ? 0.32f : 0.22f;
	}
}

void ATrafficVehicle::NudgeAsideForPlayer(float PullAsideCm, float ForwardNudgeCm)
{
	if (bStationaryDoubleParked)
	{
		if (!bHasNudgedForPlayer)
		{
			bHasNudgedForPlayer = true;
			const FVector RightVec = GetActorRightVector();
			const FVector FwdVec = GetActorForwardVector();
			StationaryTargetOffset = RightVec * PullAsideCm + FwdVec * ForwardNudgeCm;
			StationaryResetTimer = 22.0f;
		}
	}
	else
	{
		TargetLateralOffsetCm += (TargetLateralOffsetCm >= 0.0f) ? PullAsideCm : -PullAsideCm;
		AdHocStopTimer = 0.0f;
	}
}

FString ATrafficVehicle::GetDisplayLabel() const
{
	switch (VehicleType)
	{
	case EIstanbulVehicleType::YellowTaxi:     return TEXT("Sarı Taksi");
	case EIstanbulVehicleType::Dolmus:         return TEXT("Minibüs / Dolmuş");
	case EIstanbulVehicleType::IETTBus:        return TEXT("İETT Otobüsü");
	case EIstanbulVehicleType::DeliveryVan:    return TEXT("Kargo Panelvan");
	case EIstanbulVehicleType::ScooterCourier: return TEXT("Motorlu Kurye");
	default:                                   return TEXT("Araç");
	}
}

void ATrafficVehicle::TriggerHorn(const FString& Reason)
{
	if (HornCooldown > 0.0f) return;
	HornCooldown = FMath::FRandRange(3.2f, 6.0f);
	HornVisualTimer = 1.6f;

	// Vehicle-specific horn pitch with safe null-checked sound playback
	if (USoundBase* HornCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1")))
	{
		const float Pitch = (VehicleType == EIstanbulVehicleType::IETTBus) ? 0.45f
			: (VehicleType == EIstanbulVehicleType::Dolmus) ? 0.62f
			: (VehicleType == EIstanbulVehicleType::ScooterCourier) ? 1.18f
			: 0.82f;
		UGameplayStatics::PlaySoundAtLocation(this, HornCue, GetActorLocation(), 0.85f, Pitch);
	}
	OnHornHonked.Broadcast(this, GetActorLocation(), Reason);
}

void ATrafficVehicle::UpdateDistanceLOD(float DistToPlayerCm)
{
	// Task 2.1: Beyond 80m (8000cm), swap to simplified box+glass mesh and disable wheel spin animation
	const bool bShouldUseLowLOD = (DistToPlayerCm > 8000.0f);
	if (bShouldUseLowLOD != bUsingLowLOD)
	{
		bUsingLowLOD = bShouldUseLowLOD;
		if (VehicleProcMesh) VehicleProcMesh->SetVisibility(!bUsingLowLOD);
		if (VehicleLodMesh)  VehicleLodMesh->SetVisibility(bUsingLowLOD);
		for (int32 i = 0; i < WheelMeshes.Num(); ++i)
		{
			if (WheelMeshes[i])
			{
				const bool bHideScooterPair = (VehicleType == EIstanbulVehicleType::ScooterCourier && (i == 1 || i == 3));
				WheelMeshes[i]->SetVisibility(!bUsingLowLOD && !bHideScooterPair);
			}
		}
	}
}

void ATrafficVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HornCooldown > 0.0f)    HornCooldown -= DeltaSeconds;
	if (HornVisualTimer > 0.0f) HornVisualTimer -= DeltaSeconds;

	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const float DistToPlayerCm = Player ? FVector::Dist(GetActorLocation(), Player->GetActorLocation()) : 0.0f;
	UpdateDistanceLOD(DistToPlayerCm);

	// Blink orange hazard lights (dörtlüler) via timer when double-parked or making a dolmuş curb stop
	if (bHazardLightsActive && !bUsingLowLOD)
	{
		HazardBlinkAccum += DeltaSeconds;
		const bool bLampOn = FMath::Fmod(HazardBlinkAccum, 0.75f) < 0.38f;
		for (UStaticMeshComponent* H : HazardLampMeshes)
		{
			if (H) H->SetVisibility(bLampOn);
		}
	}
	else
	{
		for (UStaticMeshComponent* H : HazardLampMeshes)
		{
			if (H) H->SetVisibility(false);
		}
	}

	if (bStationaryDoubleParked || Waypoints.Num() < 2)
	{
		// Smoothly interpolate stationary pull-aside offset when nudged by player waiting or honking
		if (!StationaryTargetOffset.IsNearlyZero() || bHasNudgedForPlayer)
		{
			const FVector DesiredPos = StationaryBaseLocation + StationaryTargetOffset;
			SetActorLocation(FMath::VInterpTo(GetActorLocation(), DesiredPos, DeltaSeconds, 2.8f));
			if (StationaryResetTimer > 0.0f)
			{
				StationaryResetTimer -= DeltaSeconds;
				if (StationaryResetTimer <= 0.0f && DistToPlayerCm > 1800.0f)
				{
					StationaryTargetOffset = FVector::ZeroVector;
					bHasNudgedForPlayer = false;
				}
			}
		}

		if (Player && DistToPlayerCm < 750.0f && Player->GetVelocity().Size() < 120.0f)
		{
			BlockedDuration += DeltaSeconds;
			if (BlockedDuration > 2.2f)
			{
				// After player waits ~2.2s behind a double-parked blocker, it nudges aside so the player can pass!
				NudgeAsideForPlayer(62.0f, 85.0f);
				TriggerHorn(TEXT("Çift sıra park: Şoför hafif sağa kırıp yol açtı!"));
				BlockedDuration = 0.0f;
			}
		}
		else
		{
			BlockedDuration = 0.0f;
		}
		return;
	}

	// Dolmuş ad-hoc curb stop behavior
	if (VehicleType == EIstanbulVehicleType::Dolmus)
	{
		if (AdHocStopTimer > 0.0f)
		{
			AdHocStopTimer -= DeltaSeconds;
			bHazardLightsActive = true;
			CurrentSpeedCms = FMath::FInterpTo(CurrentSpeedCms, 0.0f, DeltaSeconds, 4.5f);
			if (AdHocStopTimer <= 0.0f)
			{
				bHazardLightsActive = false;
				NextAdHocStopCooldown = FMath::FRandRange(14.0f, 26.0f);
			}
		}
		else
		{
			NextAdHocStopCooldown -= DeltaSeconds;
			if (NextAdHocStopCooldown <= 0.0f)
			{
				AdHocStopTimer = FMath::FRandRange(3.2f, 5.5f);
			}
		}
	}

	// Forward obstacle / chokepoint yield (8-15s wait -> honk -> reverse/re-route)
	float DesiredSpeedCms = (AdHocStopTimer > 0.0f) ? 0.0f : CruiseSpeedCms;
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TrafficHeadway), false, this);
		const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 75.0f) + GetActorForwardVector() * 280.0f;
		const FVector End   = Start + GetActorForwardVector() * 1450.0f;
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(75.0f), Params))
		{
			const AActor* Blocker = Hit.GetActor();
			if (Blocker && (Blocker->IsA<APawn>() || Blocker->IsA<ATrafficVehicle>()))
			{
				const float GapCm = Hit.Distance;
				// Low-patience tailgater follows closer behind the player
				const float StopGapCm = (HornResponse && HornResponse->Patience < 0.35f) ? 340.0f : 480.0f;
				if (GapCm < StopGapCm)
				{
					DesiredSpeedCms = 0.0f;
					BlockedDuration += DeltaSeconds;
					DeadlockTimer += DeltaSeconds;
					if (BlockedDuration > 1.8f)
					{
						TriggerHorn(FString::Printf(TEXT("%s kornaya bastı (Dar geçiş / Dur-kalk)"), *GetDisplayLabel()));
						BlockedDuration = 0.0f;
					}
					// Task 2.3: Wait 8-15 seconds at chokepoint, honk, then reverse/shift lateral offset to clear deadlock
					if (DeadlockTimer >= ChokepointWaitThresholdSec)
					{
						TriggerHorn(FString::Printf(TEXT("%s dar sokakta %.0fs bekledi, geri manevra yapıyor!"), *GetDisplayLabel(), ChokepointWaitThresholdSec));
						DeadlockTimer = 0.0f;
						ReverseEscapeTimer = 1.8f;
						TargetLateralOffsetCm = (TargetLateralOffsetCm <= 0.0f) ? 145.0f : -145.0f;
					}
				}
				else
				{
					DesiredSpeedCms *= FMath::Clamp((GapCm - StopGapCm) / 950.0f, 0.15f, 1.0f);
					BlockedDuration = FMath::Max(0.0f, BlockedDuration - DeltaSeconds);
					DeadlockTimer = FMath::Max(0.0f, DeadlockTimer - DeltaSeconds * 0.5f);
				}
			}
		}
		else
		{
			BlockedDuration = 0.0f;
			DeadlockTimer = 0.0f;
		}
	}

	// Global Stuck Detector: guarantees no AI vehicle ever deadlocks permanently anywhere on the map
	GlobalStuckTimer += DeltaSeconds;
	if (GlobalStuckTimer >= 11.0f)
	{
		GlobalStuckTimer = 0.0f;
		if (AdHocStopTimer <= 0.0f && FVector::DistSquared2D(GetActorLocation(), LastProgressCheckLoc) < FMath::Square(120.0f))
		{
			ReverseEscapeTimer = 1.6f;
			TargetLateralOffsetCm = (TargetLateralOffsetCm <= 0.0f) ? 150.0f : -150.0f;
			CurrentWaypointIdx = (CurrentWaypointIdx + 1) % Waypoints.Num();
		}
		LastProgressCheckLoc = GetActorLocation();
	}

	if (ReverseEscapeTimer > 0.0f)
	{
		ReverseEscapeTimer -= DeltaSeconds;
		DesiredSpeedCms = -280.0f;
	}

	LaneChangeCooldown -= DeltaSeconds;
	if (LaneChangeCooldown <= 0.0f && FMath::Abs(TargetLateralOffsetCm) > 150.0f && AdHocStopTimer <= 0.0f)
	{
		if (FMath::FRand() < 0.35f)
		{
			TargetLateralOffsetCm = -TargetLateralOffsetCm;
		}
		LaneChangeCooldown = FMath::FRandRange(9.0f, 18.0f);
	}

	CurrentSpeedCms = FMath::FInterpTo(CurrentSpeedCms, DesiredSpeedCms, DeltaSeconds, (DesiredSpeedCms < CurrentSpeedCms) ? 5.5f : 2.2f);
	const float PrevLat = CurrentLateralOffsetCm;
	CurrentLateralOffsetCm = FMath::FInterpTo(CurrentLateralOffsetCm, TargetLateralOffsetCm, DeltaSeconds, 2.4f);

	const FVector PrevWP = Waypoints[(CurrentWaypointIdx - 1 + Waypoints.Num()) % Waypoints.Num()];
	const FVector TargetWP = Waypoints[CurrentWaypointIdx];
	const FVector SegDir = (TargetWP - PrevWP).GetSafeNormal2D();
	const FVector RightVec(-SegDir.Y, SegDir.X, 0.0f);
	const FVector OffsetTarget = TargetWP + RightVec * CurrentLateralOffsetCm;

	const FVector ToTarget = OffsetTarget - GetActorLocation();
	const float Dist2D = ToTarget.Size2D();
	if (Dist2D < 320.0f)
	{
		CurrentWaypointIdx = (CurrentWaypointIdx + 1) % Waypoints.Num();
	}

	const FVector MoveDir = ToTarget.GetSafeNormal();
	FVector NewLoc = GetActorLocation() + MoveDir * (CurrentSpeedCms * DeltaSeconds);

	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(TrafficGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, NewLoc + FVector(0, 0, 260.0f), NewLoc - FVector(0, 0, 360.0f), ECC_Visibility, GroundParams))
	{
		NewLoc.Z = FMath::FInterpTo(GetActorLocation().Z, GroundHit.ImpactPoint.Z + 2.0f, DeltaSeconds, 14.0f);
	}

	const FRotator DesiredRot = MoveDir.IsNearlyZero() ? GetActorRotation() : MoveDir.Rotation();
	const float YawDelta = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredRot.Yaw);
	CurrentSteerYawDeg = FMath::Clamp(YawDelta * 1.4f + (CurrentLateralOffsetCm - PrevLat) * 0.4f, -32.0f, 32.0f);

	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaSeconds, 6.0f);
	SetActorLocationAndRotation(NewLoc, NewRot, false);

	// Wheel rotation & front wheel steering (disabled beyond 80m LOD distance)
	if (!bUsingLowLOD)
	{
		WheelSpinDeg = FMath::Fmod(WheelSpinDeg + (CurrentSpeedCms * DeltaSeconds / 31.0f) * (180.0f / PI), 360.0f);
		for (int32 i = 0; i < WheelMeshes.Num(); ++i)
		{
			if (UStaticMeshComponent* W = WheelMeshes[i])
			{
				const float FrontSteer = (i < 2) ? CurrentSteerYawDeg : 0.0f;
				W->SetRelativeRotation(FRotator(WheelSpinDeg, FrontSteer, 90.0f));
			}
		}
	}
}

// =============================================================================
// AIstanbulTrafficManager
// =============================================================================

AIstanbulTrafficManager::AIstanbulTrafficManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // Bounded 10 Hz manager check for chokepoint entry & debug draw
}

void AIstanbulTrafficManager::ForwardHornEvent(AActor* HonkingVehicle, FVector WorldLocation, FString ReasonText)
{
	OnAnyVehicleHonked.Broadcast(HonkingVehicle, WorldLocation, ReasonText);
}

int32 AIstanbulTrafficManager::BroadcastPlayerHorn(const FVector& PlayerLocation, float HoldDurationSec)
{
	int32 Responders = 0;
	const bool bLongAggressive = (HoldDurationSec >= 0.35f);
	for (ATrafficVehicle* V : Vehicles)
	{
		if (!V || !V->GetHornResponse()) continue;
		FString ReactionSummary;
		if (V->GetHornResponse()->OnPlayerHornHeard(HoldDurationSec, PlayerLocation, ReactionSummary))
		{
			++Responders;
			OnHornResponse.Broadcast(V, bLongAggressive, ReactionSummary);
		}
	}
	return Responders;
}

int32 AIstanbulTrafficManager::SpawnExtraTrafficByType(EIstanbulVehicleType Type, int32 Count)
{
	UWorld* W = GetWorld();
	if (!W || CachedBoulevardRight.Num() < 2) return 0;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const TArray<FVector>& Path = (i % 2 == 0) ? CachedBoulevardRight : CachedSideStreetLoop;
		if (Path.Num() < 2) continue;
		const int32 Idx = (Vehicles.Num() + i) % Path.Num();
		const int32 NextIdx = (Idx + 1) % Path.Num();
		const FVector Dir = (Path[NextIdx] - Path[Idx]).GetSafeNormal2D();
		const FVector Pos = Path[Idx] + FVector(i * 320.0f, 0.0f, 12.0f);
		if (ATrafficVehicle* V = W->SpawnActor<ATrafficVehicle>(Pos, Dir.Rotation(), Params))
		{
			V->Tags.Add(ATestSceneBuilder::GeneratedActorTag);
			V->InitializeTrafficVehicle(Type, Path, 40.0f, (i % 2 == 0) ? 220.0f : 0.0f, false, NextIdx);
			V->OnHornHonked.AddDynamic(this, &AIstanbulTrafficManager::ForwardHornEvent);
			Vehicles.Add(V);
			++Spawned;
		}
	}
	return Spawned;
}

void AIstanbulTrafficManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* W = GetWorld();
	if (!W) return;

	// Detect player entering any of the 4 active chokepoints and fire OnChokepointEntered
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(W, 0))
	{
		const FVector PlayerLoc = Player->GetActorLocation();
		int32 ActiveIdx = INDEX_NONE;
		for (int32 i = 0; i < Chokepoints.Num(); ++i)
		{
			if (FVector::DistSquared(PlayerLoc, Chokepoints[i].Location) <= FMath::Square(1100.0f))
			{
				ActiveIdx = i;
				break;
			}
		}
		if (ActiveIdx != INDEX_NONE && ActiveIdx != LastEnteredChokepointIdx)
		{
			LastEnteredChokepointIdx = ActiveIdx;
			OnChokepointEntered.Broadcast(Chokepoints[ActiveIdx].ChokepointId, Chokepoints[ActiveIdx].FreeGapWidthM);
		}
		else if (ActiveIdx == INDEX_NONE)
		{
			LastEnteredChokepointIdx = INDEX_NONE;
		}
	}

	if (bDebugDrawChokepoints)
	{
		for (int32 i = 0; i < Chokepoints.Num(); ++i)
		{
			const FIstanbulChokepointSpec& CP = Chokepoints[i];
			DrawDebugSphere(W, CP.Location + FVector(0, 0, 120.0f), CP.FreeGapWidthM * 50.0f, 16, FColor::Orange, false, 0.15f, 0, 3.0f);
			DrawDebugString(W, CP.Location + FVector(0, 0, 280.0f),
				FString::Printf(TEXT("[CP %d] %s (Gap: %.2fm)"), i, *CP.ChokepointId.ToString(), CP.FreeGapWidthM),
				nullptr, FColor::Yellow, 0.15f, true);
		}
	}
}

void AIstanbulTrafficManager::PopulateIstanbulTraffic(const FVector& Origin,
	const TArray<FVector>& BoulevardLoopRight, const TArray<FVector>& BoulevardLoopLeft,
	const TArray<FVector>& SideStreetLoop)
{
	UWorld* W = GetWorld();
	if (!W) return;

	CachedOrigin = Origin;
	CachedBoulevardRight = BoulevardLoopRight;
	CachedBoulevardLeft = BoulevardLoopLeft;
	CachedSideStreetLoop = SideStreetLoop;
	Chokepoints.Reset();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto SpawnFlowVehicle = [&](EIstanbulVehicleType Type, const TArray<FVector>& Path, int32 StartWP, float SpeedKmh, float LatOffset) -> ATrafficVehicle*
	{
		if (Path.Num() < 2) return nullptr;
		const int32 Idx = StartWP % Path.Num();
		const int32 NextIdx = (Idx + 1) % Path.Num();
		const FVector Dir = (Path[NextIdx] - Path[Idx]).GetSafeNormal2D();
		const FVector Right(-Dir.Y, Dir.X, 0.0f);
		const FVector Pos = Path[Idx] + Right * LatOffset + FVector(0, 0, 10.0f);
		ATrafficVehicle* V = W->SpawnActor<ATrafficVehicle>(Pos, Dir.Rotation(), Params);
		if (V)
		{
			V->Tags.Add(ATestSceneBuilder::GeneratedActorTag);
			V->InitializeTrafficVehicle(Type, Path, SpeedKmh, LatOffset, false, NextIdx);
			V->OnHornHonked.AddDynamic(this, &AIstanbulTrafficManager::ForwardHornEvent);
			Vehicles.Add(V);
		}
		return V;
	};

	// 1. Main Coastal Boulevard (Eminönü -> Galata Bridge / Beşiktaş Approach, including 5th ScooterCourier archetype!)
	SpawnFlowVehicle(EIstanbulVehicleType::Dolmus,         BoulevardLoopRight, 0, 38.0f,  260.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,     BoulevardLoopRight, 1, 52.0f, -240.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::IETTBus,        BoulevardLoopRight, 2, 34.0f,  280.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::ScooterCourier, BoulevardLoopRight, 3, 58.0f,  390.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::DeliveryVan,    BoulevardLoopRight, 4, 44.0f,  260.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,     BoulevardLoopRight, 5, 50.0f, -240.0f);

	// Opposing boulevard flow
	SpawnFlowVehicle(EIstanbulVehicleType::IETTBus,        BoulevardLoopLeft,  0, 36.0f,  260.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::Dolmus,         BoulevardLoopLeft,  2, 42.0f, -230.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::ScooterCourier, BoulevardLoopLeft,  3, 56.0f,  380.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::DeliveryVan,    BoulevardLoopLeft,  4, 48.0f,  250.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,     BoulevardLoopLeft,  5, 50.0f, -230.0f);

	// 2. Narrow Cobblestone Side Street Loop (Galata / Cihangir Yokuşu)
	SpawnFlowVehicle(EIstanbulVehicleType::Dolmus,         SideStreetLoop, 0, 24.0f, 0.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,     SideStreetLoop, 2, 28.0f, 0.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::ScooterCourier, SideStreetLoop, 4, 32.0f, 110.0f);

	// 3. Stationary / Movable Chokepoint Blockers & Parked Eminönü Taxi Rank
	auto SpawnStationary = [&](EIstanbulVehicleType Type, const FVector& RelPos, float YawDeg, bool bHazardBlink) -> ATrafficVehicle*
	{
		TArray<FVector> Dummy;
		Dummy.Add(Origin + RelPos);
		ATrafficVehicle* V = W->SpawnActor<ATrafficVehicle>(Origin + RelPos, FRotator(0.0f, YawDeg, 0.0f), Params);
		if (V)
		{
			V->Tags.Add(ATestSceneBuilder::GeneratedActorTag);
			V->InitializeTrafficVehicle(Type, Dummy, 0.0f, 0.0f, true, 0);
			V->OnHornHonked.AddDynamic(this, &AIstanbulTrafficManager::ForwardHornEvent);
			Vehicles.Add(V);
		}
		return V;
	};

	// Populate the 4 Data-Driven Istanbul Chokepoints (FreeGapWidthM = 2.45m .. 2.80m)
	{
		// Chokepoint 0: Double-parked Delivery Van on the 13.2-deg Galata Climb
		ATrafficVehicle* VanSlope = SpawnStationary(EIstanbulVehicleType::DeliveryVan, FVector(3730.0f, 4850.0f, 720.0f), 90.0f, true);
		FIstanbulChokepointSpec CP0;
		CP0.ChokepointId = TEXT("Chokepoint_0_GalataSlopeVan");
		CP0.Location = Origin + FVector(3500.0f, 4850.0f, 720.0f);
		CP0.FreeGapWidthM = 2.65f;
		CP0.Description = TEXT("Galata yokuşunda dörtlüleri açık çift sıra kargo panelvan (2.65m geçiş)");
		if (VanSlope) CP0.BlockingVehicles.Add(VanSlope);
		Chokepoints.Add(CP0);

		// Chokepoint 1: İETT Bus angled at the Sirkeci / Old-Town East corner
		ATrafficVehicle* CornerBus = SpawnStationary(EIstanbulVehicleType::IETTBus, FVector(16580.0f, 920.0f, 5.0f), 55.0f, true);
		FIstanbulChokepointSpec CP1;
		CP1.ChokepointId = TEXT("Chokepoint_1_SirkeciCornerBus");
		CP1.Location = Origin + FVector(16800.0f, 920.0f, 5.0f);
		CP1.FreeGapWidthM = 2.80f;
		CP1.Description = TEXT("Sirkeci köşe dönüşünde yarım şerit kapatan İETT otobüsü (2.80m geçiş)");
		if (CornerBus) CP1.BlockingVehicles.Add(CornerBus);
		Chokepoints.Add(CP1);

		// Chokepoint 2: Dolmuş stopped mid-road on Upper Cihangir Ridge loading passengers
		ATrafficVehicle* RidgeDolmus = SpawnStationary(EIstanbulVehicleType::Dolmus, FVector(12500.0f, 7740.0f, 1295.0f), 2.0f, true);
		FIstanbulChokepointSpec CP2;
		CP2.ChokepointId = TEXT("Chokepoint_2_CihangirDolmusStop");
		CP2.Location = Origin + FVector(12500.0f, 7600.0f, 1295.0f);
		CP2.FreeGapWidthM = 2.55f;
		CP2.Description = TEXT("Cihangir sırtında yol ortasında yolcu indiren sarı dolmuş (2.55m geçiş)");
		if (RidgeDolmus) CP2.BlockingVehicles.Add(RidgeDolmus);
		Chokepoints.Add(CP2);

		// Chokepoint 3: Narrow Old-Town Alley under the Spanning Arched Gateway + Delivery Van
		ATrafficVehicle* AlleyVan = SpawnStationary(EIstanbulVehicleType::DeliveryVan, FVector(7345.0f, 2600.0f, 5.0f), 90.0f, true);
		FIstanbulChokepointSpec CP3;
		CP3.ChokepointId = TEXT("Chokepoint_3_OldTownArchGateway");
		CP3.Location = Origin + FVector(7200.0f, 2350.0f, 5.0f);
		CP3.FreeGapWidthM = 2.45f;
		CP3.bIsGatewayChokepoint = true;
		CP3.Description = TEXT("Tarihi Yarımada kemerli geçit altı + kargo aracı dar boğazı (2.45m geçiş)");
		if (AlleyVan) CP3.BlockingVehicles.Add(AlleyVan);
		Chokepoints.Add(CP3);
	}

	// Parked Yellow Taxis in a Taxi Rank (Taksi Durağı) at the Eminönü quay
	SpawnStationary(EIstanbulVehicleType::YellowTaxi, FVector(1800.0f, -920.0f, 5.0f), 0.0f, false);
	SpawnStationary(EIstanbulVehicleType::YellowTaxi, FVector(1150.0f, -920.0f, 5.0f), 0.0f, false);
	SpawnStationary(EIstanbulVehicleType::YellowTaxi, FVector( 500.0f, -920.0f, 5.0f), 0.0f, false);

	// Automatically bind Chokepoint & HornResponse delegates to the player's UPassengerInteractionComponent
	if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(UGameplayStatics::GetPlayerPawn(W, 0)))
	{
		if (UPassengerInteractionComponent* PassComp = Taxi->GetPassengerInteraction())
		{
			OnChokepointEntered.AddUniqueDynamic(PassComp, &UPassengerInteractionComponent::NotifyChokepointEntered);
			OnHornResponse.AddUniqueDynamic(PassComp, &UPassengerInteractionComponent::NotifyHornExchange);
		}
	}
}
