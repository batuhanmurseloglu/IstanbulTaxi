#include "Traffic/IstanbulTrafficManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

ATrafficVehicle::ATrafficVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (CubeFinder.Succeeded()) BodyMesh->SetStaticMesh(CubeFinder.Object);

	CabinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinMesh"));
	CabinMesh->SetupAttachment(Root);
	CabinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) CabinMesh->SetStaticMesh(CubeFinder.Object);

	RoofStripeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoofStripeMesh"));
	RoofStripeMesh->SetupAttachment(Root);
	RoofStripeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) RoofStripeMesh->SetStaticMesh(CubeFinder.Object);

	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel_%d"), i));
		W->SetupAttachment(Root);
		W->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
		W->SetRelativeScale3D(FVector(0.62f, 0.62f, 0.22f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CylFinder.Succeeded()) W->SetStaticMesh(CylFinder.Object);
		WheelMeshes.Add(W);
	}

	for (int32 i = 0; i < 2; ++i)
	{
		UStaticMeshComponent* H = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HazardLamp_%d"), i));
		H->SetupAttachment(Root);
		H->SetRelativeScale3D(FVector(0.18f, 0.28f, 0.14f));
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

void ATrafficVehicle::BuildVisualsForType(ETrafficVehicleType InType)
{
	VehicleType = InType;
	float LengthM = 4.3f, WidthM = 1.78f, BodyH = 0.56f, CabinH = 0.50f;
	FLinearColor BodyCol(0.72f, 0.75f, 0.78f);
	FLinearColor CabinCol(0.15f, 0.18f, 0.22f);
	FLinearColor StripeCol(0.95f, 0.85f, 0.15f);
	bool bShowStripe = false;

	switch (InType)
	{
	case ETrafficVehicleType::Sedan:
	{
		static const FLinearColor SedanPalette[5] = {
			FLinearColor(0.82f, 0.84f, 0.88f), // White/Silver Egea/Megane
			FLinearColor(0.14f, 0.16f, 0.20f), // Charcoal
			FLinearColor(0.08f, 0.22f, 0.48f), // Bosphorus Blue
			FLinearColor(0.55f, 0.10f, 0.10f), // Burgundy
			FLinearColor(0.32f, 0.34f, 0.36f)  // Titanium Grey
		};
		BodyCol = SedanPalette[FMath::RandRange(0, 4)];
		break;
	}
	case ETrafficVehicleType::YellowTaxi:
		BodyCol = FLinearColor(0.95f, 0.70f, 0.02f);
		CabinCol = FLinearColor(0.85f, 0.62f, 0.02f);
		bShowStripe = true;
		break;
	case ETrafficVehicleType::Dolmus:
		LengthM = 5.4f; WidthM = 2.02f; BodyH = 0.85f; CabinH = 0.72f;
		BodyCol = FLinearColor(0.92f, 0.86f, 0.62f); // Cream/Yellow Minibüs
		CabinCol = FLinearColor(0.12f, 0.45f, 0.78f); // Iconic Istanbul Mavi/Turkuaz Minibüs stripe
		StripeCol = FLinearColor(0.08f, 0.38f, 0.75f);
		bShowStripe = true;
		break;
	case ETrafficVehicleType::IETTBus:
		LengthM = 11.2f; WidthM = 2.48f; BodyH = 1.35f; CabinH = 1.15f;
		BodyCol = FLinearColor(0.95f, 0.74f, 0.04f); // Iconic Yellow/Red İETT Bus
		CabinCol = FLinearColor(0.78f, 0.12f, 0.10f);
		StripeCol = FLinearColor(0.90f, 0.92f, 0.95f);
		bShowStripe = true;
		break;
	case ETrafficVehicleType::DeliveryVan:
		LengthM = 5.1f; WidthM = 1.96f; BodyH = 0.95f; CabinH = 0.82f;
		BodyCol = FLinearColor(0.88f, 0.90f, 0.92f);
		CabinCol = FLinearColor(0.20f, 0.22f, 0.26f);
		break;
	}

	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f + BodyH * 50.0f));
	BodyMesh->SetRelativeScale3D(FVector(LengthM, WidthM, BodyH));
	TintMesh(BodyMesh, BodyCol);

	CabinMesh->SetRelativeLocation(FVector(-LengthM * 5.0f, 0.0f, 32.0f + (BodyH + CabinH * 0.48f) * 100.0f));
	CabinMesh->SetRelativeScale3D(FVector(LengthM * 0.58f, WidthM * 0.88f, CabinH));
	TintMesh(CabinMesh, CabinCol);

	RoofStripeMesh->SetVisibility(bShowStripe);
	RoofStripeMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f + (BodyH + CabinH) * 100.0f));
	RoofStripeMesh->SetRelativeScale3D(FVector(0.45f, WidthM * 0.5f, 0.18f));
	TintMesh(RoofStripeMesh, StripeCol);

	const float AxleX = LengthM * 32.0f;
	const float AxleY = WidthM * 48.0f;
	const FVector WPos[4] = {
		FVector( AxleX, -AxleY, 32.0f),
		FVector( AxleX,  AxleY, 32.0f),
		FVector(-AxleX, -AxleY, 32.0f),
		FVector(-AxleX,  AxleY, 32.0f)
	};
	for (int32 i = 0; i < 4; ++i)
	{
		WheelMeshes[i]->SetRelativeLocation(WPos[i]);
		TintMesh(WheelMeshes[i], FLinearColor(0.06f, 0.06f, 0.07f));
	}

	// Rear/Front hazard indicator bars
	if (HazardLampMeshes.Num() >= 2)
	{
		HazardLampMeshes[0]->SetRelativeLocation(FVector(-LengthM * 50.0f - 4.0f, -WidthM * 36.0f, 55.0f));
		HazardLampMeshes[1]->SetRelativeLocation(FVector(-LengthM * 50.0f - 4.0f,  WidthM * 36.0f, 55.0f));
		TintMesh(HazardLampMeshes[0], FLinearColor(1.0f, 0.45f, 0.0f));
		TintMesh(HazardLampMeshes[1], FLinearColor(1.0f, 0.45f, 0.0f));
	}
}

void ATrafficVehicle::InitializeTrafficVehicle(ETrafficVehicleType InType, const TArray<FVector>& InWaypoints,
	float InCruiseSpeedKmh, float InLateralOffsetCm, bool bInStationaryHazard, int32 StartWaypointIdx)
{
	BuildVisualsForType(InType);
	Waypoints = InWaypoints;
	CruiseSpeedCms = InCruiseSpeedKmh / 0.036f;
	CurrentSpeedCms = bInStationaryHazard ? 0.0f : CruiseSpeedCms * 0.8f;
	CurrentLateralOffsetCm = InLateralOffsetCm;
	TargetLateralOffsetCm = InLateralOffsetCm;
	bStationaryDoubleParked = bInStationaryHazard;
	bHazardLightsActive = bInStationaryHazard;
	CurrentWaypointIdx = (Waypoints.Num() > 0) ? (StartWaypointIdx % Waypoints.Num()) : 0;
	NextAdHocStopCooldown = FMath::FRandRange(10.0f, 22.0f);
	LaneChangeCooldown = FMath::FRandRange(7.0f, 16.0f);
}

FString ATrafficVehicle::GetDisplayLabel() const
{
	switch (VehicleType)
	{
	case ETrafficVehicleType::YellowTaxi:  return TEXT("Sarı Taksi");
	case ETrafficVehicleType::Dolmus:      return TEXT("Minibüs / Dolmuş");
	case ETrafficVehicleType::IETTBus:     return TEXT("İETT Otobüsü");
	case ETrafficVehicleType::DeliveryVan: return TEXT("Kargo Panelvan");
	default:                               return TEXT("Araç");
	}
}

void ATrafficVehicle::TriggerHorn(const FString& Reason)
{
	if (HornCooldown > 0.0f) return;
	HornCooldown = FMath::FRandRange(3.5f, 6.5f);
	HornVisualTimer = 1.6f;

	if (USoundBase* HornCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1")))
	{
		const float Pitch = (VehicleType == ETrafficVehicleType::IETTBus) ? 0.48f
			: (VehicleType == ETrafficVehicleType::Dolmus) ? 0.62f : 0.82f;
		UGameplayStatics::PlaySoundAtLocation(this, HornCue, GetActorLocation(), 0.85f, Pitch);
	}
	OnHornHonked.Broadcast(this, GetActorLocation(), Reason);
}

void ATrafficVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HornCooldown > 0.0f)    HornCooldown -= DeltaSeconds;
	if (HornVisualTimer > 0.0f) HornVisualTimer -= DeltaSeconds;

	// Blink orange hazard lights (dörtlüler) when double-parked or making a dolmuş curb stop
	if (bHazardLightsActive)
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

	// Stationary double-parked vehicles in narrow side streets honk if the player stops right in front of them
	if (bStationaryDoubleParked || Waypoints.Num() < 2)
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			const float DistCm = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
			if (DistCm < 650.0f && Player->GetVelocity().Size() < 80.0f)
			{
				BlockedDuration += DeltaSeconds;
				if (BlockedDuration > 2.2f)
				{
					TriggerHorn(TEXT("Çift sıra park / Dar sokak geçişi!"));
					BlockedDuration = 0.0f;
				}
			}
			else
			{
				BlockedDuration = 0.0f;
			}
		}
		return;
	}

	// Dolmuş ad-hoc curb stop behavior ("Sağda müsait bir yerde ineyim kaptan!")
	if (VehicleType == ETrafficVehicleType::Dolmus)
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

	// Forward obstacle / stop-and-go congestion check
	float DesiredSpeedCms = (AdHocStopTimer > 0.0f) ? 0.0f : CruiseSpeedCms;
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TrafficHeadway), false, this);
		const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 75.0f) + GetActorForwardVector() * 280.0f;
		const FVector End   = Start + GetActorForwardVector() * 1450.0f; // 14.5m lookahead
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(75.0f), Params))
		{
			const AActor* Blocker = Hit.GetActor();
			if (Blocker && (Blocker->IsA<APawn>() || Blocker->IsA<ATrafficVehicle>()))
			{
				const float GapCm = Hit.Distance;
				if (GapCm < 480.0f)
				{
					DesiredSpeedCms = 0.0f;
					BlockedDuration += DeltaSeconds;
					// Authentic Istanbul impatience: honk and attempt nose-in lane change!
					if (BlockedDuration > 1.35f)
					{
						TriggerHorn(FString::Printf(TEXT("%s kornaya bastı (Dur-kalk trafik)"), *GetDisplayLabel()));
						BlockedDuration = 0.0f;
						if (FMath::Abs(TargetLateralOffsetCm) > 100.0f && LaneChangeCooldown <= 0.0f)
						{
							TargetLateralOffsetCm = -TargetLateralOffsetCm;
							LaneChangeCooldown = 8.0f;
						}
					}
				}
				else
				{
					DesiredSpeedCms *= FMath::Clamp((GapCm - 480.0f) / 950.0f, 0.15f, 1.0f);
					BlockedDuration = FMath::Max(0.0f, BlockedDuration - DeltaSeconds);
				}
			}
		}
		else
		{
			BlockedDuration = 0.0f;
		}
	}

	// Spontaneous Istanbul lane-weaving on multi-lane boulevards
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
	CurrentLateralOffsetCm = FMath::FInterpTo(CurrentLateralOffsetCm, TargetLateralOffsetCm, DeltaSeconds, 2.4f);

	// Advance toward current waypoint
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

	// Ground snap so traffic climbs the Sultanahmet cobblestone hill and Bosphorus bridge ramp smoothly
	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(TrafficGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, NewLoc + FVector(0, 0, 250.0f), NewLoc - FVector(0, 0, 350.0f), ECC_Visibility, GroundParams))
	{
		NewLoc.Z = FMath::FInterpTo(GetActorLocation().Z, GroundHit.ImpactPoint.Z + 2.0f, DeltaSeconds, 14.0f);
	}

	const FRotator DesiredRot = MoveDir.IsNearlyZero() ? GetActorRotation() : MoveDir.Rotation();
	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaSeconds, 6.0f);
	SetActorLocationAndRotation(NewLoc, NewRot, false);

	// Spin wheels
	WheelSpinDeg = FMath::Fmod(WheelSpinDeg + (CurrentSpeedCms * DeltaSeconds / 31.0f) * (180.0f / PI), 360.0f);
	for (UStaticMeshComponent* W : WheelMeshes)
	{
		if (W) W->SetRelativeRotation(FRotator(WheelSpinDeg, 0.0f, 90.0f));
	}
}

// =============================================================================
// AIstanbulTrafficManager
// =============================================================================

AIstanbulTrafficManager::AIstanbulTrafficManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AIstanbulTrafficManager::ForwardHornEvent(AActor* HonkingVehicle, FVector WorldLocation, FString ReasonText)
{
	OnAnyVehicleHonked.Broadcast(HonkingVehicle, WorldLocation, ReasonText);
}

void AIstanbulTrafficManager::PopulateIstanbulTraffic(const FVector& Origin,
	const TArray<FVector>& BoulevardLoopRight, const TArray<FVector>& BoulevardLoopLeft,
	const TArray<FVector>& SideStreetLoop)
{
	UWorld* W = GetWorld();
	if (!W) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto SpawnFlowVehicle = [&](ETrafficVehicleType Type, const TArray<FVector>& Path, int32 StartWP, float SpeedKmh, float LatOffset)
	{
		if (Path.Num() < 2) return;
		const int32 Idx = StartWP % Path.Num();
		const int32 NextIdx = (Idx + 1) % Path.Num();
		const FVector Dir = (Path[NextIdx] - Path[Idx]).GetSafeNormal2D();
		const FVector Right(-Dir.Y, Dir.X, 0.0f);
		const FVector Pos = Path[Idx] + Right * LatOffset + FVector(0, 0, 10.0f);
		ATrafficVehicle* V = W->SpawnActor<ATrafficVehicle>(Pos, Dir.Rotation(), Params);
		if (V)
		{
			V->InitializeTrafficVehicle(Type, Path, SpeedKmh, LatOffset, false, NextIdx);
			V->OnHornHonked.AddDynamic(this, &AIstanbulTrafficManager::ForwardHornEvent);
			Vehicles.Add(V);
		}
	};

	// 1. Main Coastal Boulevard (Sahil Yolu -> 15 Temmuz Bridge Approach) — Rightbound & Leftbound lanes
	SpawnFlowVehicle(ETrafficVehicleType::Dolmus,      BoulevardLoopRight, 0, 38.0f,  260.0f);
	SpawnFlowVehicle(ETrafficVehicleType::YellowTaxi,  BoulevardLoopRight, 1, 52.0f, -240.0f);
	SpawnFlowVehicle(ETrafficVehicleType::IETTBus,     BoulevardLoopRight, 2, 34.0f,  280.0f);
	SpawnFlowVehicle(ETrafficVehicleType::Sedan,       BoulevardLoopRight, 3, 56.0f, -240.0f);
	SpawnFlowVehicle(ETrafficVehicleType::DeliveryVan, BoulevardLoopRight, 4, 44.0f,  260.0f);
	SpawnFlowVehicle(ETrafficVehicleType::YellowTaxi,  BoulevardLoopRight, 5, 50.0f, -240.0f);

	// Opposing boulevard flow
	SpawnFlowVehicle(ETrafficVehicleType::IETTBus,     BoulevardLoopLeft,  0, 36.0f,  260.0f);
	SpawnFlowVehicle(ETrafficVehicleType::Dolmus,      BoulevardLoopLeft,  2, 42.0f, -230.0f);
	SpawnFlowVehicle(ETrafficVehicleType::Sedan,       BoulevardLoopLeft,  4, 54.0f,  250.0f);
	SpawnFlowVehicle(ETrafficVehicleType::YellowTaxi,  BoulevardLoopLeft,  5, 48.0f, -230.0f);

	// 2. Narrow Cobblestone Side Street Loop (Sultanahmet / Balat Yokuşu)
	SpawnFlowVehicle(ETrafficVehicleType::Dolmus,      SideStreetLoop, 0, 24.0f, 0.0f);
	SpawnFlowVehicle(ETrafficVehicleType::YellowTaxi,  SideStreetLoop, 2, 28.0f, 0.0f);
	SpawnFlowVehicle(ETrafficVehicleType::Sedan,       SideStreetLoop, 4, 26.0f, 0.0f);

	// 3. Double-parked hazard vehicles blocking half a lane in the narrow side streets (blinking dörtlüler!)
	auto SpawnDoubleParked = [&](ETrafficVehicleType Type, const FVector& RelPos, float YawDeg)
	{
		TArray<FVector> Dummy;
		Dummy.Add(Origin + RelPos);
		ATrafficVehicle* V = W->SpawnActor<ATrafficVehicle>(Origin + RelPos, FRotator(0.0f, YawDeg, 0.0f), Params);
		if (V)
		{
			V->InitializeTrafficVehicle(Type, Dummy, 0.0f, 0.0f, true, 0);
			V->OnHornHonked.AddDynamic(this, &AIstanbulTrafficManager::ForwardHornEvent);
			Vehicles.Add(V);
		}
	};

	// Delivery van & hazard sedan double-parked in the narrow cobblestone alley (player must squeeze through!)
	SpawnDoubleParked(ETrafficVehicleType::DeliveryVan, FVector(4500.0f, 3850.0f, 15.0f),  88.0f);
	SpawnDoubleParked(ETrafficVehicleType::Sedan,       FVector(12500.0f, 7680.0f, 420.0f), 4.0f);
}
