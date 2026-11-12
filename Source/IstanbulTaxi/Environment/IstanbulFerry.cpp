// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#include "Environment/IstanbulFerry.h"
#include "Core/IstanbulMeshUtils.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Core/IstanbulTaxiHUD.h"
#include "IstanbulTaxi.h"

AIstanbulFerry::AIstanbulFerry()
{
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	HullProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HullProcMesh"));
	HullProcMesh->SetupAttachment(RootScene);
	HullProcMesh->SetCollisionProfileName(TEXT("BlockAll"));

	SuperstructureProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SuperstructureProcMesh"));
	SuperstructureProcMesh->SetupAttachment(RootScene);
	SuperstructureProcMesh->SetCollisionProfileName(TEXT("BlockAll"));

	FunnelAndMastsProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FunnelAndMastsProcMesh"));
	FunnelAndMastsProcMesh->SetupAttachment(RootScene);
	FunnelAndMastsProcMesh->SetCollisionProfileName(TEXT("NoCollision"));

	WakeFoamProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WakeFoamProcMesh"));
	WakeFoamProcMesh->SetupAttachment(RootScene);
	WakeFoamProcMesh->SetCollisionProfileName(TEXT("NoCollision"));

	HornAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HornAudio"));
	HornAudio->SetupAttachment(RootScene);
	HornAudio->bAutoActivate = false;
}

void AIstanbulFerry::BeginPlay()
{
	Super::BeginPlay();
	if (HullProcMesh && HullProcMesh->GetNumSections() == 0)
	{
		BuildProceduralFerryMesh();
	}
}

void AIstanbulFerry::InitializeRoute(const FString& InFerryName, const TArray<FVector>& InRouteWaypoints, float InCruiseSpeedKnots)
{
	FerryName = InFerryName;
	RouteWaypoints = InRouteWaypoints;
	CurrentTargetWaypoint = 0;
	CruiseSpeedCms = InCruiseSpeedKnots * 51.444f;
	BuildProceduralFerryMesh();
}

void AIstanbulFerry::BuildProceduralFerryMesh()
{
	// Classic Istanbul Şehir Hatları Vapuru proportions:
	// Length ~56m (5600cm), Beam ~11m (1100cm), Height ~14m
	const FLinearColor HullBlack(0.10f, 0.11f, 0.13f);
	const FLinearColor HullWhite(0.94f, 0.95f, 0.96f);
	const FLinearColor TeakDeck(0.68f, 0.52f, 0.36f);
	const FLinearColor CabinGlass(0.12f, 0.18f, 0.25f);
	const FLinearColor FunnelYellow(0.95f, 0.76f, 0.08f);
	const FLinearColor FunnelBlack(0.12f, 0.12f, 0.14f);
	const FLinearColor WakeWhite(0.88f, 0.94f, 0.98f, 0.65f);

	UMaterialInterface* CommonMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();

	// 1. Lower Hull (Waterline Black + White Freeboard Flare)
	FIstanbulMeshSectionData HullSec;
	// Lower Black Keel
	FIstanbulMeshUtils::AppendTaperedPrism(HullSec, FTransform(FVector(0.0f, 0.0f, 80.0f)),
		FVector2D(2500.0f, 480.0f), FVector2D(2700.0f, 540.0f), FVector(50.0f, 0.0f, 0.0f), 80.0f, HullBlack);
	// Upper White Hull (Sheerline to Main Deck)
	FIstanbulMeshUtils::AppendTaperedPrism(HullSec, FTransform(FVector(0.0f, 0.0f, 220.0f)),
		FVector2D(2700.0f, 540.0f), FVector2D(2800.0f, 560.0f), FVector(60.0f, 0.0f, 0.0f), 60.0f, HullWhite);
	HullSec.CommitToComponent(HullProcMesh, 0, true, CommonMat);

	// 2. Multi-Deck Superstructure (Main Deck Saloon + Upper Open Promenade + Wheelhouse)
	FIstanbulMeshSectionData SuperSec;
	// Main Deck Saloon
	FIstanbulMeshUtils::AppendBeveledBox(SuperSec, FTransform(FVector(-80.0f, 0.0f, 350.0f)),
		FVector(2100.0f, 460.0f, 70.0f), 15.0f, HullWhite);
	// Saloon Windows Row (Dark glass band)
	FIstanbulMeshUtils::AppendBeveledBox(SuperSec, FTransform(FVector(-80.0f, 0.0f, 360.0f)),
		FVector(2050.0f, 468.0f, 38.0f), 5.0f, CabinGlass);
	// Upper Promenade Deck
	FIstanbulMeshUtils::AppendBeveledBox(SuperSec, FTransform(FVector(-120.0f, 0.0f, 490.0f)),
		FVector(1750.0f, 410.0f, 65.0f), 12.0f, HullWhite);
	// Upper Windows
	FIstanbulMeshUtils::AppendBeveledBox(SuperSec, FTransform(FVector(-120.0f, 0.0f, 500.0f)),
		FVector(1700.0f, 418.0f, 35.0f), 4.0f, CabinGlass);
	// Bridge / Wheelhouse (Kaptan Köşkü)
	FIstanbulMeshUtils::AppendBeveledBox(SuperSec, FTransform(FVector(650.0f, 0.0f, 620.0f)),
		FVector(320.0f, 320.0f, 55.0f), 10.0f, HullWhite);
	FIstanbulMeshUtils::AppendBeveledBox(SuperSec, FTransform(FVector(720.0f, 0.0f, 625.0f)),
		FVector(250.0f, 324.0f, 32.0f), 4.0f, CabinGlass);
	SuperSec.CommitToComponent(SuperstructureProcMesh, 0, true, CommonMat);

	// 3. Iconic Yellow/Black Funnel (Baca) + Fore/Aft Masts + Turkish Ensign Flagstaff
	FIstanbulMeshSectionData FunnelSec;
	// Tilted Smokestack Funnel (raked back at 12 deg)
	FIstanbulMeshUtils::AppendCone(FunnelSec,
		FTransform(FRotator(12.0f, 0.0f, 0.0f), FVector(-120.0f, 0.0f, 650.0f)),
		140.0f, 115.0f, 260.0f, 16, FunnelYellow);
	// Black Top Funnel Band
	FIstanbulMeshUtils::AppendCylinder(FunnelSec,
		FTransform(FRotator(12.0f, 0.0f, 0.0f), FVector(-145.0f, 0.0f, 860.0f)),
		114.0f, 55.0f, 16, FunnelBlack);
	// Fore Mast (Direk)
	FIstanbulMeshUtils::AppendCylinder(FunnelSec,
		FTransform(FVector(1150.0f, 0.0f, 750.0f)),
		14.0f, 380.0f, 8, HullWhite);
	FunnelSec.CommitToComponent(FunnelAndMastsProcMesh, 0, false, CommonMat);

	// 4. Trailing Wake Foam Quads
	FIstanbulMeshSectionData WakeSec;
	FIstanbulMeshUtils::AppendTaperedPrism(WakeSec, FTransform(FVector(-3400.0f, 0.0f, 12.0f)),
		FVector2D(1200.0f, 480.0f), FVector2D(2400.0f, 1400.0f), FVector(-600.0f, 0.0f, 0.0f), 6.0f, WakeWhite);
	WakeSec.CommitToComponent(WakeFoamProcMesh, 0, false, CommonMat);
}

void AIstanbulFerry::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Gentle maritime rolling and pitch on Bosphorus waters
	WakeTime += DeltaSeconds;
	const float RollDeg = FMath::Sin(WakeTime * 1.2f) * 1.6f;
	const float PitchDeg = FMath::Cos(WakeTime * 0.8f) * 0.8f;

	// Waypoint navigation along the Bosphorus Strait
	if (RouteWaypoints.Num() > 0)
	{
		const FVector TargetPos = RouteWaypoints[CurrentTargetWaypoint];
		const FVector ToTarget = TargetPos - GetActorLocation();
		const float Dist2D = ToTarget.Size2D();

		if (Dist2D <= 400.0f)
		{
			CurrentTargetWaypoint = (CurrentTargetWaypoint + 1) % RouteWaypoints.Num();
			SoundFerryHorn();
		}
		else
		{
			const FVector MoveDir = ToTarget.GetSafeNormal2D();
			FRotator NewRot = MoveDir.Rotation();
			NewRot.Roll = RollDeg;
			NewRot.Pitch = PitchDeg;

			SetActorRotation(FMath::RInterpTo(GetActorRotation(), NewRot, DeltaSeconds, 1.2f));
			SetActorLocation(GetActorLocation() + MoveDir * (CruiseSpeedCms * DeltaSeconds));
		}
	}

	// Periodic ferry horn whistle
	HornTimer -= DeltaSeconds;
	if (HornTimer <= 0.0f)
	{
		HornTimer = FMath::RandRange(45.0f, 75.0f);
		SoundFerryHorn();
	}
}

void AIstanbulFerry::SoundFerryHorn()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AIstanbulTaxiHUD* HUD = Cast<AIstanbulTaxiHUD>(PC->GetHUD()))
			{
				FString HornMsg = FString::Printf(TEXT("[VAPUR DÜDÜĞÜ] %s Boğaz'da seyrederken düdük çaldı."), *FerryName);
				HUD->PushToast(HornMsg, FLinearColor(0.85f, 0.88f, 0.95f), 5.0f);
			}
		}
	}
}
