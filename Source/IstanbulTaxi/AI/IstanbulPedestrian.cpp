// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#include "AI/IstanbulPedestrian.h"
#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "Core/IstanbulMeshUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Vehicle/TaxiPawn.h"
#include "Missions/PassengerInteractionComponent.h"
#include "Core/IstanbulTaxiHUD.h"
#include "IstanbulTaxi.h"

AIstanbulPedestrian::AIstanbulPedestrian()
{
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
	CapsuleComp->InitCapsuleSize(35.0f, 90.0f);
	CapsuleComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CapsuleComp->SetGenerateOverlapEvents(true);
	RootComponent = CapsuleComp;

	BodyMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CapsuleComp);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LeftArmMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LeftArmMesh"));
	LeftArmMesh->SetupAttachment(CapsuleComp);
	LeftArmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightArmMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RightArmMesh"));
	RightArmMesh->SetupAttachment(CapsuleComp);
	RightArmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LeftLegMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LeftLegMesh"));
	LeftLegMesh->SetupAttachment(CapsuleComp);
	LeftLegMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightLegMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RightLegMesh"));
	RightLegMesh->SetupAttachment(CapsuleComp);
	RightLegMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HailingMarkerMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HailingMarkerMesh"));
	HailingMarkerMesh->SetupAttachment(CapsuleComp);
	HailingMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AIstanbulPedestrian::BeginPlay()
{
	Super::BeginPlay();
	if (PatrolWaypoints.Num() == 0)
	{
		PatrolWaypoints.Add(GetActorLocation());
		PatrolWaypoints.Add(GetActorLocation() + GetActorForwardVector() * 1200.0f);
	}
	if (BodyMesh && BodyMesh->GetNumSections() == 0)
	{
		BuildArticulatedBody(GetTypeHash(GetActorLocation()));
	}
}

void AIstanbulPedestrian::InitializePedestrian(EPedestrianRole InRole, const TArray<FVector>& InWaypoints, int32 InSeed)
{
	Role = InRole;
	PatrolWaypoints = InWaypoints;
	CurrentWaypointIdx = 0;
	BuildArticulatedBody(InSeed);
}

void AIstanbulPedestrian::SetAsHailingPassenger(const FVector& InPickupPoint, const FGuid& InAssociatedRideId)
{
	Role = EPedestrianRole::HailingPassenger;
	AssociatedRideId = InAssociatedRideId;
	SetActorLocation(InPickupPoint + FVector(0.0f, 0.0f, 92.0f));
	PatrolWaypoints.Reset();
	PatrolWaypoints.Add(InPickupPoint);

	// Create floating yellow diamond marker above head
	FIstanbulMeshSectionData MarkerSec;
	const FLinearColor MarkerYellow(1.0f, 0.85f, 0.08f, 1.0f);
	FIstanbulMeshUtils::AppendBeveledBox(MarkerSec, FTransform(FRotator(45.0f, 45.0f, 0.0f), FVector(0.0f, 0.0f, 135.0f)),
		FVector(14.0f, 14.0f, 14.0f), 2.5f, MarkerYellow);

	UMaterialInterface* PBRMat = FIstanbulMeshUtils::CreatePBRMaterial(this, MarkerYellow, 0.0f, 0.2f, 0.9f);
	MarkerSec.CommitToComponent(HailingMarkerMesh, 0, false, PBRMat);
}

void AIstanbulPedestrian::BuildArticulatedBody(int32 Seed)
{
	FRandomStream Rng(Seed);

	// Turkish street clothing colors (jacket/shirt, jeans/slacks, skin tones)
	static const FLinearColor Shirts[6] = {
		FLinearColor(0.12f, 0.22f, 0.38f), // Navy jacket
		FLinearColor(0.72f, 0.15f, 0.12f), // Red sweater
		FLinearColor(0.85f, 0.85f, 0.88f), // White shirt
		FLinearColor(0.18f, 0.20f, 0.22f), // Charcoal coat
		FLinearColor(0.42f, 0.32f, 0.22f), // Brown leather
		FLinearColor(0.14f, 0.45f, 0.28f)  // Dark green jacket
	};
	static const FLinearColor Pants[4] = {
		FLinearColor(0.10f, 0.14f, 0.22f), // Denim blue
		FLinearColor(0.08f, 0.08f, 0.09f), // Black slacks
		FLinearColor(0.32f, 0.30f, 0.28f), // Grey
		FLinearColor(0.48f, 0.42f, 0.32f)  // Khaki
	};
	static const FLinearColor Skins[3] = {
		FLinearColor(0.92f, 0.78f, 0.68f),
		FLinearColor(0.86f, 0.70f, 0.58f),
		FLinearColor(0.78f, 0.60f, 0.48f)
	};

	ShirtColor = Shirts[Rng.RandRange(0, 5)];
	PantsColor = Pants[Rng.RandRange(0, 3)];
	SkinColor  = Skins[Rng.RandRange(0, 2)];
	const FLinearColor HairColor(0.08f, 0.06f, 0.05f);
	const FLinearColor ShoeColor(0.06f, 0.06f, 0.07f);

	UMaterialInterface* CommonMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();

	// 1. Torso + Head + Neck + Hair on BodyMesh
	FIstanbulMeshSectionData BodySec;
	// Torso (Z = -5 to +35)
	FIstanbulMeshUtils::AppendBeveledBox(BodySec, FTransform(FVector(0.0f, 0.0f, 15.0f)), FVector(14.0f, 20.0f, 25.0f), 3.0f, ShirtColor);
	// Neck
	FIstanbulMeshUtils::AppendCylinder(BodySec, FTransform(FVector(0.0f, 0.0f, 43.0f)), 5.5f, 4.0f, 8, SkinColor);
	// Head
	FIstanbulMeshUtils::AppendBeveledBox(BodySec, FTransform(FVector(0.0f, 0.0f, 58.0f)), FVector(10.0f, 9.0f, 11.0f), 3.0f, SkinColor);
	// Hair
	FIstanbulMeshUtils::AppendBeveledBox(BodySec, FTransform(FVector(-1.0f, 0.0f, 66.0f)), FVector(11.0f, 9.8f, 4.0f), 2.0f, HairColor);
	BodySec.CommitToComponent(BodyMesh, 0, false, CommonMat);

	// 2. Left Arm (shoulder at Y = -23.0f, Z = +34.0f)
	FIstanbulMeshSectionData LeftArmSec;
	FIstanbulMeshUtils::AppendBeveledBox(LeftArmSec, FTransform(FVector(0.0f, 0.0f, -16.0f)), FVector(5.5f, 5.0f, 16.0f), 2.0f, ShirtColor);
	FIstanbulMeshUtils::AppendBeveledBox(LeftArmSec, FTransform(FVector(0.0f, 0.0f, -36.0f)), FVector(4.5f, 4.0f, 5.0f), 1.5f, SkinColor);
	LeftArmMesh->SetRelativeLocation(FVector(0.0f, -23.0f, 34.0f));
	LeftArmSec.CommitToComponent(LeftArmMesh, 0, false, CommonMat);

	// 3. Right Arm (shoulder at Y = +23.0f, Z = +34.0f)
	FIstanbulMeshSectionData RightArmSec;
	FIstanbulMeshUtils::AppendBeveledBox(RightArmSec, FTransform(FVector(0.0f, 0.0f, -16.0f)), FVector(5.5f, 5.0f, 16.0f), 2.0f, ShirtColor);
	FIstanbulMeshUtils::AppendBeveledBox(RightArmSec, FTransform(FVector(0.0f, 0.0f, -36.0f)), FVector(4.5f, 4.0f, 5.0f), 1.5f, SkinColor);
	RightArmMesh->SetRelativeLocation(FVector(0.0f, 23.0f, 34.0f));
	RightArmSec.CommitToComponent(RightArmMesh, 0, false, CommonMat);

	// 4. Left Leg (hip at Y = -10.0f, Z = -10.0f)
	FIstanbulMeshSectionData LeftLegSec;
	FIstanbulMeshUtils::AppendBeveledBox(LeftLegSec, FTransform(FVector(0.0f, 0.0f, -22.0f)), FVector(7.0f, 6.5f, 22.0f), 2.0f, PantsColor);
	FIstanbulMeshUtils::AppendBeveledBox(LeftLegSec, FTransform(FVector(3.0f, 0.0f, -46.0f)), FVector(10.0f, 6.0f, 4.0f), 1.5f, ShoeColor);
	LeftLegMesh->SetRelativeLocation(FVector(0.0f, -10.0f, -10.0f));
	LeftLegSec.CommitToComponent(LeftLegMesh, 0, false, CommonMat);

	// 5. Right Leg (hip at Y = +10.0f, Z = -10.0f)
	FIstanbulMeshSectionData RightLegSec;
	FIstanbulMeshUtils::AppendBeveledBox(RightLegSec, FTransform(FVector(0.0f, 0.0f, -22.0f)), FVector(7.0f, 6.5f, 22.0f), 2.0f, PantsColor);
	FIstanbulMeshUtils::AppendBeveledBox(RightLegSec, FTransform(FVector(3.0f, 0.0f, -46.0f)), FVector(10.0f, 6.0f, 4.0f), 1.5f, ShoeColor);
	RightLegMesh->SetRelativeLocation(FVector(0.0f, 10.0f, -10.0f));
	RightLegSec.CommitToComponent(RightLegMesh, 0, false, CommonMat);
}

void AIstanbulPedestrian::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bHasBoardedTaxi) return;

	CheckVehicleDodge(DeltaTime);

	// ---- Hailing Passenger Behavior ----
	if (Role == EPedestrianRole::HailingPassenger)
	{
		HailingArmWaveTime += DeltaTime * 4.5f;
		// Wave right arm up and down
		const float ArmPitch = 120.0f + FMath::Sin(HailingArmWaveTime) * 35.0f;
		RightArmMesh->SetRelativeRotation(FRotator(ArmPitch, 0.0f, 15.0f));
		LeftArmMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

		// Rotate marker slowly
		HailingMarkerMesh->AddRelativeRotation(FRotator(0.0f, DeltaTime * 90.0f, 0.0f));

		// Detect nearby player taxi to board
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(PlayerPawn))
		{
			const float DistToTaxi = FVector::Dist2D(GetActorLocation(), Taxi->GetActorLocation());
			const float TaxiSpeed = FMath::Abs(Taxi->GetSpeedKmh());

			if (!bIsWalkingToTaxi && DistToTaxi <= 650.0f && TaxiSpeed <= 8.0f)
			{
				bIsWalkingToTaxi = true;
				TargetTaxi = Taxi;
			}

			if (bIsWalkingToTaxi && TargetTaxi.IsValid())
			{
				// Walk toward taxi's rear right passenger door
				const FVector TargetDoor = TargetTaxi->GetActorLocation() - TargetTaxi->GetActorForwardVector() * 80.0f + TargetTaxi->GetActorRightVector() * 110.0f;
				const FVector MoveDir = (TargetDoor - GetActorLocation()).GetSafeNormal2D();
				const float StepDist = WalkSpeedCms * DeltaTime;

				SetActorRotation(FMath::RInterpTo(GetActorRotation(), MoveDir.Rotation(), DeltaTime, 10.0f));
				SetActorLocation(GetActorLocation() + MoveDir * StepDist);
				UpdateWalkAnimation(DeltaTime, 1.0f);

				if (FVector::Dist2D(GetActorLocation(), TargetDoor) <= 55.0f)
				{
					// Board into taxi cabin!
					bHasBoardedTaxi = true;
					SetActorHiddenInGame(true);
					SetActorEnableCollision(false);

					if (AIstanbulTaxiHUD* HUD = Cast<AIstanbulTaxiHUD>(UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD()))
					{
						HUD->PushToast(TEXT("Müşteri arka koltuğa bindi! \"İyi günler kaptan, gidelim!\""), FLinearColor(0.2f, 0.95f, 0.3f), 5.0f);
					}
				}
			}
		}
		return;
	}

	// ---- Sidewalk & Crosswalk Patrol AI ----
	if (PatrolWaypoints.Num() > 0)
	{
		const FVector TargetWP = PatrolWaypoints[CurrentWaypointIdx];
		const FVector ToWP = (TargetWP - GetActorLocation());
		const float DistToWP = ToWP.Size2D();

		if (DistToWP <= 90.0f)
		{
			CurrentWaypointIdx = (CurrentWaypointIdx + 1) % PatrolWaypoints.Num();
		}
		else
		{
			FVector MoveDir = ToWP.GetSafeNormal2D();
			if (bIsDodging)
			{
				MoveDir = DodgeDirection;
			}

			const float ActualSpeed = bIsDodging ? (WalkSpeedCms * 2.2f) : WalkSpeedCms;
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), MoveDir.Rotation(), DeltaTime, 8.0f));
			SetActorLocation(GetActorLocation() + MoveDir * (ActualSpeed * DeltaTime));
			UpdateWalkAnimation(DeltaTime, bIsDodging ? 2.2f : 1.0f);
		}
	}
}

void AIstanbulPedestrian::UpdateWalkAnimation(float DeltaTime, float SpeedRatio)
{
	WalkCycleTime += DeltaTime * 6.5f * SpeedRatio;
	const float LegSwing = FMath::Sin(WalkCycleTime) * 32.0f;
	const float ArmSwing = -LegSwing * 0.9f;

	LeftLegMesh->SetRelativeRotation(FRotator(LegSwing, 0.0f, 0.0f));
	RightLegMesh->SetRelativeRotation(FRotator(-LegSwing, 0.0f, 0.0f));
	LeftArmMesh->SetRelativeRotation(FRotator(ArmSwing, 0.0f, 0.0f));
	RightArmMesh->SetRelativeRotation(FRotator(-ArmSwing, 0.0f, 0.0f));
}

void AIstanbulPedestrian::CheckVehicleDodge(float DeltaTime)
{
	DodgeCooldown -= DeltaTime;
	if (bIsDodging && DodgeCooldown <= 0.0f)
	{
		bIsDodging = false;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn) return;

	const FVector TaxiLoc = PlayerPawn->GetActorLocation();
	const FVector PedLoc = GetActorLocation();
	const float DistToTaxi = FVector::Dist2D(PedLoc, TaxiLoc);

	if (DistToTaxi <= 550.0f && DodgeCooldown <= 0.0f)
	{
		ATaxiPawn* Taxi = Cast<ATaxiPawn>(PlayerPawn);
		const float TaxiSpeed = Taxi ? FMath::Abs(Taxi->GetSpeedKmh()) : 0.0f;

		if (TaxiSpeed >= 18.0f)
		{
			// Player vehicle is rushing into crosswalk/pedestrian without yielding!
			bIsDodging = true;
			DodgeCooldown = 1.4f;
			// Dodge away perpendicular to vehicle forward
			const FVector AwayFromTaxi = (PedLoc - TaxiLoc).GetSafeNormal2D();
			DodgeDirection = AwayFromTaxi;

			// Shout Turkish pedestrian reaction + trigger passenger patience penalty
			static const TCHAR* TurkishShouts[4] = {
				TEXT("Yaya: \"Yavaş be hemşerim, insan var burada!\""),
				TEXT("Yaya: \"Kör müsün be taksici, yaya geçidi burası!\""),
				TEXT("Yaya: \"Aman dur! Çiğneyecektin az kalsın!\""),
				TEXT("Yaya: \"Hooop kaptan nereye, frene bas!\"")
			};
			const FString Shout = TurkishShouts[FMath::RandRange(0, 3)];

			if (AIstanbulTaxiHUD* HUD = Cast<AIstanbulTaxiHUD>(UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD()))
			{
				HUD->PushToast(Shout, FLinearColor(1.0f, 0.35f, 0.2f), 4.0f);
			}

			if (Taxi && Taxi->GetPassengerInteraction())
			{
				Taxi->GetPassengerInteraction()->NotifyHardJoltOrCollision(0.45f, true);
			}
		}
	}
}
