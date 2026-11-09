#include "Traffic/IstanbulTrafficManager.h"
#include "Core/IstanbulMeshUtils.h"
#include "Core/TestSceneBuilder.h"
#include "ProceduralMeshComponent.h"
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

	VehicleProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("VehicleProcMesh"));
	VehicleProcMesh->SetupAttachment(Root);
	VehicleProcMesh->bUseComplexAsSimpleCollision = true;
	VehicleProcMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	VehicleProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

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

void ATrafficVehicle::BuildVisualMesh(EIstanbulVehicleType InType)
{
	VehicleType = InType;
	FIstanbulMeshSectionData BodySection;

	const FLinearColor DarkGlass(0.06f, 0.09f, 0.13f, 1.0f);
	const FLinearColor WheelArchCol(0.04f, 0.04f, 0.05f, 1.0f);
	const FLinearColor HeadlightCol(1.00f, 0.96f, 0.78f, 1.0f);
	const FLinearColor TaillightCol(0.92f, 0.06f, 0.05f, 1.0f);

	float LengthCm = 440.0f;
	float WidthCm  = 178.0f;

	switch (InType)
	{
	case EIstanbulVehicleType::YellowTaxi:
	{
		// 1. Yellow Taxi (#F5C400): separate beveled Hood, Cabin greenhouse, and Trunk deck + roof "TAKSİ" sign + wheel arches
		LengthCm = 440.0f;
		WidthCm  = 178.0f;
		const FLinearColor TaxiYellow = FLinearColor::FromSRGBColor(FColor(0xF5, 0xC4, 0x00));
		const FLinearColor CheckerBand(0.05f, 0.05f, 0.06f, 1.0f);
		const FLinearColor RoofSignYellow(1.0f, 0.92f, 0.18f, 1.0f);

		// Lower sill & bumper base
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(0.0f, 0.0f, 44.0f)),
			FVector(218.0f, 88.0f, 18.0f), 5.0f, TaxiYellow * 0.94f);
		// Front Hood box (beveled)
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(138.0f, 0.0f, 66.0f)),
			FVector(78.0f, 86.0f, 18.0f), 6.0f, TaxiYellow);
		// Center Passenger Cabin (beveled)
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-12.0f, 0.0f, 96.0f)),
			FVector(108.0f, 78.0f, 34.0f), 8.0f, TaxiYellow);
		// Dark Glass Window Band
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-12.0f, 0.0f, 98.0f)),
			FVector(111.0f, 80.0f, 20.0f), 4.0f, DarkGlass);
		// Rear Trunk Deck box (beveled)
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-156.0f, 0.0f, 68.0f)),
			FVector(62.0f, 85.0f, 20.0f), 6.0f, TaxiYellow);
		// Black Checkered Waist Band
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(0.0f, 0.0f, 62.0f)),
			FVector(219.0f, 90.0f, 4.5f), 1.0f, CheckerBand);
		// Roof-mounted "TAKSİ" light dome
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-5.0f, 0.0f, 136.0f)),
			FVector(18.0f, 42.0f, 8.5f), 2.5f, RoofSignYellow);
		break;
	}
	case EIstanbulVehicleType::IETTBus:
	{
		// 2. İETT Bus (~12m articulated-looking coach, #C8102E red lower / crisp white upper livery, dark window band, front LED route sign)
		LengthCm = 1180.0f;
		WidthCm  = 250.0f;
		const FLinearColor IETTRed   = FLinearColor::FromSRGBColor(FColor(0xC8, 0x10, 0x2E));
		const FLinearColor IETTWhite = FLinearColor(0.94f, 0.94f, 0.96f, 1.0f);
		const FLinearColor BellowsGrey(0.14f, 0.15f, 0.17f, 1.0f);
		const FLinearColor RouteAmber(1.00f, 0.58f, 0.04f, 1.0f);

		// Lower Red Hull (#C8102E)
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(0.0f, 0.0f, 78.0f)),
			FVector(585.0f, 124.0f, 46.0f), 10.0f, IETTRed);
		// Crisp White Upper Livery Band
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(0.0f, 0.0f, 134.0f)),
			FVector(586.0f, 125.0f, 16.0f), 4.0f, IETTWhite);
		// Upper Coach Crown & Panoramic Dark Window Band
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(0.0f, 0.0f, 205.0f)),
			FVector(582.0f, 122.0f, 58.0f), 12.0f, IETTWhite);
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(4.0f, 0.0f, 198.0f)),
			FVector(585.0f, 124.5f, 36.0f), 6.0f, DarkGlass);
		// Articulated Accordion Seam Band in the middle + Rooftop A/C pods
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-65.0f, 0.0f, 152.0f)),
			FVector(24.0f, 126.0f, 112.0f), 3.0f, BellowsGrey);
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(210.0f, 0.0f, 272.0f)),
			FVector(120.0f, 78.0f, 12.0f), 4.0f, IETTWhite * 0.9f);
		// Front Illuminated Destination Sign Panel ("30M BEŞİKTAŞ - EMİNÖNÜ")
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(586.0f, 0.0f, 244.0f)),
			FVector(3.0f, 78.0f, 12.0f), 1.0f, RouteAmber);
		break;
	}
	case EIstanbulVehicleType::Dolmus:
	{
		// 3. Dolmuş: Yellow Ford Transit-like boxy minibus (~5.5m), black waist stripe, sliding-door seam detail
		LengthCm = 550.0f;
		WidthCm  = 204.0f;
		const FLinearColor DolmusYellow = FLinearColor::FromSRGBColor(FColor(0xF2, 0xC6, 0x18));
		const FLinearColor BlackStripe(0.05f, 0.05f, 0.06f, 1.0f);

		// Main Transit Lower Van Box
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(0.0f, 0.0f, 74.0f)),
			FVector(272.0f, 100.0f, 42.0f), 8.0f, DolmusYellow);
		// Short Sloped Transit Nose / Hood
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(228.0f, 0.0f, 88.0f)),
			FVector(46.0f, 96.0f, 26.0f), 8.0f, DolmusYellow);
		// High-Roof Minibus Passenger Greenhouse
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-24.0f, 0.0f, 148.0f)),
			FVector(226.0f, 95.0f, 42.0f), 10.0f, DolmusYellow * 0.98f);
		// Dark Window Band
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-18.0f, 0.0f, 145.0f)),
			FVector(232.0f, 97.5f, 24.0f), 4.0f, DarkGlass);
		// Black Beltline Stripe
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(0.0f, 0.0f, 98.0f)),
			FVector(274.0f, 102.0f, 6.0f), 1.5f, BlackStripe);
		// Right-Side Passenger Sliding-Door Track & Vertical Seam Detail
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(35.0f, 102.5f, 112.0f)),
			FVector(58.0f, 1.5f, 56.0f), 0.5f, BlackStripe);
		// Illuminated Forehead Route Board ("TAKSiM - BEŞİKTAŞ")
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(195.0f, 0.0f, 194.0f)),
			FVector(12.0f, 52.0f, 8.0f), 2.0f, FLinearColor(1.0f, 0.78f, 0.12f, 1.0f));
		break;
	}
	case EIstanbulVehicleType::DeliveryVan:
	{
		// 4. Delivery Van: White/colored boxy van with double-parked logistics stripe & rear cargo doors
		LengthCm = 540.0f;
		WidthCm  = 202.0f;
		const FLinearColor VanWhite(0.92f, 0.94f, 0.96f, 1.0f);
		const FLinearColor CabDark(0.18f, 0.22f, 0.28f, 1.0f);
		const FLinearColor CargoStripe(0.08f, 0.38f, 0.78f, 1.0f);

		// Forward Driver Cab + Hood
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(172.0f, 0.0f, 86.0f)),
			FVector(88.0f, 94.0f, 52.0f), 8.0f, VanWhite);
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(184.0f, 0.0f, 108.0f)),
			FVector(78.0f, 96.0f, 22.0f), 4.0f, DarkGlass);
		// High-Cube Boxy Cargo Container (Kasa)
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-62.0f, 0.0f, 124.0f)),
			FVector(186.0f, 101.0f, 88.0f), 6.0f, VanWhite);
		// Colored Logistics Livery Band + Rear Step Bumper
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-62.0f, 0.0f, 105.0f)),
			FVector(187.5f, 102.5f, 12.0f), 2.0f, CargoStripe);
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-252.0f, 0.0f, 42.0f)),
			FVector(8.0f, 98.0f, 8.0f), 2.0f, CabDark);
		break;
	}
	}

	const float AxleX = LengthCm * 0.32f;
	const float AxleY = WidthCm * 0.48f;

	// Recessed dark wheel arches + emissive-tinted headlights & taillights in the single merged mesh
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector( AxleX, Side * (AxleY - 4.0f), 36.0f)),
			FVector(36.0f, 10.0f, 22.0f), 3.0f, WheelArchCol);
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-AxleX, Side * (AxleY - 4.0f), 36.0f)),
			FVector(36.0f, 10.0f, 22.0f), 3.0f, WheelArchCol);

		// Headlights
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(LengthCm * 0.5f + 1.0f, Side * WidthCm * 0.35f, 56.0f)),
			FVector(3.0f, 14.0f, 8.0f), 1.0f, HeadlightCol);
		// Taillights
		FIstanbulMeshUtils::AppendBeveledBox(BodySection, FTransform(FVector(-LengthCm * 0.5f - 1.0f, Side * WidthCm * 0.35f, 58.0f)),
			FVector(3.0f, 15.0f, 8.0f), 1.0f, TaillightCol);
	}

	UMaterialInterface* VtxMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	BodySection.CommitToComponent(VehicleProcMesh, 0, true, VtxMat);

	// Position rotating wheel cylinders & blinking hazard lamps
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
	CurrentWaypointIdx = (Waypoints.Num() > 0) ? (StartWaypointIdx % Waypoints.Num()) : 0;
	NextAdHocStopCooldown = FMath::FRandRange(10.0f, 22.0f);
	LaneChangeCooldown = FMath::FRandRange(7.0f, 16.0f);
}

FString ATrafficVehicle::GetDisplayLabel() const
{
	switch (VehicleType)
	{
	case EIstanbulVehicleType::YellowTaxi:  return TEXT("Sarı Taksi");
	case EIstanbulVehicleType::Dolmus:      return TEXT("Minibüs / Dolmuş");
	case EIstanbulVehicleType::IETTBus:     return TEXT("İETT Otobüsü");
	case EIstanbulVehicleType::DeliveryVan: return TEXT("Kargo Panelvan");
	default:                                return TEXT("Araç");
	}
}

void ATrafficVehicle::TriggerHorn(const FString& Reason)
{
	if (HornCooldown > 0.0f) return;
	HornCooldown = FMath::FRandRange(3.5f, 6.5f);
	HornVisualTimer = 1.6f;

	if (USoundBase* HornCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1")))
	{
		const float Pitch = (VehicleType == EIstanbulVehicleType::IETTBus) ? 0.48f
			: (VehicleType == EIstanbulVehicleType::Dolmus) ? 0.62f : 0.82f;
		UGameplayStatics::PlaySoundAtLocation(this, HornCue, GetActorLocation(), 0.85f, Pitch);
	}
	OnHornHonked.Broadcast(this, GetActorLocation(), Reason);
}

void ATrafficVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HornCooldown > 0.0f)    HornCooldown -= DeltaSeconds;
	if (HornVisualTimer > 0.0f) HornVisualTimer -= DeltaSeconds;

	// Blink orange hazard lights (dörtlüler) via timer when double-parked or making a dolmuş curb stop
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

	// Forward obstacle / stop-and-go congestion & anti-deadlock check
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
				if (GapCm < 480.0f)
				{
					DesiredSpeedCms = 0.0f;
					BlockedDuration += DeltaSeconds;
					DeadlockTimer += DeltaSeconds;
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
					// Anti-deadlock timeout in narrow alleys: reverse briefly and shift lateral offset to bypass
					if (DeadlockTimer > 5.0f)
					{
						DeadlockTimer = 0.0f;
						ReverseEscapeTimer = 1.6f;
						TargetLateralOffsetCm = (TargetLateralOffsetCm <= 0.0f) ? 145.0f : -145.0f;
					}
				}
				else
				{
					DesiredSpeedCms *= FMath::Clamp((GapCm - 480.0f) / 950.0f, 0.15f, 1.0f);
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
	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaSeconds, 6.0f);
	SetActorLocationAndRotation(NewLoc, NewRot, false);

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

	auto SpawnFlowVehicle = [&](EIstanbulVehicleType Type, const TArray<FVector>& Path, int32 StartWP, float SpeedKmh, float LatOffset)
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
			V->Tags.Add(ATestSceneBuilder::GeneratedActorTag);
			V->InitializeTrafficVehicle(Type, Path, SpeedKmh, LatOffset, false, NextIdx);
			V->OnHornHonked.AddDynamic(this, &AIstanbulTrafficManager::ForwardHornEvent);
			Vehicles.Add(V);
		}
	};

	// 1. Main Coastal Boulevard (Eminönü -> Galata Bridge / Beşiktaş Approach)
	SpawnFlowVehicle(EIstanbulVehicleType::Dolmus,      BoulevardLoopRight, 0, 38.0f,  260.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,  BoulevardLoopRight, 1, 52.0f, -240.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::IETTBus,     BoulevardLoopRight, 2, 34.0f,  280.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,  BoulevardLoopRight, 3, 56.0f, -240.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::DeliveryVan, BoulevardLoopRight, 4, 44.0f,  260.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,  BoulevardLoopRight, 5, 50.0f, -240.0f);

	// Opposing boulevard flow
	SpawnFlowVehicle(EIstanbulVehicleType::IETTBus,     BoulevardLoopLeft,  0, 36.0f,  260.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::Dolmus,      BoulevardLoopLeft,  2, 42.0f, -230.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::DeliveryVan, BoulevardLoopLeft,  4, 48.0f,  250.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,  BoulevardLoopLeft,  5, 50.0f, -230.0f);

	// 2. Narrow Cobblestone Side Street Loop (Galata / Cihangir Yokuşu)
	SpawnFlowVehicle(EIstanbulVehicleType::Dolmus,      SideStreetLoop, 0, 24.0f, 0.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::YellowTaxi,  SideStreetLoop, 2, 28.0f, 0.0f);
	SpawnFlowVehicle(EIstanbulVehicleType::DeliveryVan, SideStreetLoop, 4, 25.0f, 0.0f);

	// 3. Double-parked Delivery Vans & Parked Eminönü Taxi Rank
	auto SpawnStationary = [&](EIstanbulVehicleType Type, const FVector& RelPos, float YawDeg, bool bHazardBlink)
	{
		TArray<FVector> Dummy;
		Dummy.Add(Origin + RelPos);
		ATrafficVehicle* V = W->SpawnActor<ATrafficVehicle>(Origin + RelPos, FRotator(0.0f, YawDeg, 0.0f), Params);
		if (V)
		{
			V->Tags.Add(ATestSceneBuilder::GeneratedActorTag);
			V->InitializeTrafficVehicle(Type, Dummy, 0.0f, 0.0f, true, 0);
			if (!bHazardBlink)
			{
				// Parked taxi rank at Eminönü quay (stationary, hazards off)
			}
			V->OnHornHonked.AddDynamic(this, &AIstanbulTrafficManager::ForwardHornEvent);
			Vehicles.Add(V);
		}
	};

	// Double-parked delivery vans in narrow alleys leaving a ~2.6m drivable squeeze gap!
	SpawnStationary(EIstanbulVehicleType::DeliveryVan, FVector( 3730.0f, 4850.0f,  720.0f), 90.0f, true);
	SpawnStationary(EIstanbulVehicleType::DeliveryVan, FVector(12500.0f, 7740.0f, 1295.0f),  2.0f, true);
	SpawnStationary(EIstanbulVehicleType::DeliveryVan, FVector( 7330.0f, 2600.0f,    5.0f), 90.0f, true);

	// Parked Yellow Taxis in a Taxi Rank (Taksi Durağı) at the Eminönü quay
	SpawnStationary(EIstanbulVehicleType::YellowTaxi,  FVector( 1800.0f, -920.0f,    5.0f),  0.0f, false);
	SpawnStationary(EIstanbulVehicleType::YellowTaxi,  FVector( 1150.0f, -920.0f,    5.0f),  0.0f, false);
	SpawnStationary(EIstanbulVehicleType::YellowTaxi,  FVector(  500.0f, -920.0f,    5.0f),  0.0f, false);
}
