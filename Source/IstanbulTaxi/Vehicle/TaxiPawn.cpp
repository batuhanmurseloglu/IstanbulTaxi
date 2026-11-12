#include "Vehicle/TaxiPawn.h"
#include "IstanbulTaxi.h"
#include "Vehicle/TaxiWheels.h"
#include "Vehicle/TaxiSurfaceComponent.h"
#include "Taximeter/TaximeterComponent.h"
#include "Taximeter/TaximeterDisplayWidget.h"
#include "Missions/PassengerMissionSubsystem.h"
#include "Missions/PassengerInteractionComponent.h"
#include "Environment/TimeOfDaySubsystem.h"
#include "Environment/WeatherSubsystem.h"
#include "Environment/DestructibleStreetProp.h"
#include "Audio/AudioAmbienceSubsystem.h"
#include "Traffic/IstanbulTrafficManager.h"
#include "Core/IstanbulMeshUtils.h"
#include "Core/IstanbulTaxiHUD.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "ProceduralMeshComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ATaxiPawn::ATaxiPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SkelMesh->BodyInstance.bNotifyRigidBodyCollision = true;
		SkelMesh->SetNotifyRigidBodyCollision(true);
		SkelMesh->SetGenerateOverlapEvents(true);
	}

	// Solid 3D Chassis Collision Box (BlockAllDynamic) so the player taxi never clips through walls or AI traffic
	SolidChassisCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SolidChassisCollision"));
	SolidChassisCollision->SetupAttachment(GetMesh());
	SolidChassisCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 74.0f));
	SolidChassisCollision->InitBoxExtent(FVector(210.0f, 86.0f, 48.0f));
	SolidChassisCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	SolidChassisCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SolidChassisCollision->SetCollisionObjectType(ECC_Vehicle);
	SolidChassisCollision->SetNotifyRigidBodyCollision(true);
	SolidChassisCollision->SetGenerateOverlapEvents(true);

	// Real Front Projector Headlight Cones + Rear Brake Glow Lights
	LeftHeadlightSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("LeftHeadlightSpot"));
	LeftHeadlightSpot->SetupAttachment(GetMesh());
	LeftHeadlightSpot->SetRelativeLocation(FVector(212.0f, -64.0f, 62.0f));
	LeftHeadlightSpot->SetRelativeRotation(FRotator(-6.0f, 0.0f, 0.0f));
	LeftHeadlightSpot->SetIntensity(8500.0f);
	LeftHeadlightSpot->SetAttenuationRadius(3600.0f);
	LeftHeadlightSpot->SetInnerConeAngle(18.0f);
	LeftHeadlightSpot->SetOuterConeAngle(38.0f);
	LeftHeadlightSpot->SetLightColor(FLinearColor(1.0f, 0.94f, 0.80f));
	LeftHeadlightSpot->SetCastShadows(false);

	RightHeadlightSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("RightHeadlightSpot"));
	RightHeadlightSpot->SetupAttachment(GetMesh());
	RightHeadlightSpot->SetRelativeLocation(FVector(212.0f, 64.0f, 62.0f));
	RightHeadlightSpot->SetRelativeRotation(FRotator(-6.0f, 0.0f, 0.0f));
	RightHeadlightSpot->SetIntensity(8500.0f);
	RightHeadlightSpot->SetAttenuationRadius(3600.0f);
	RightHeadlightSpot->SetInnerConeAngle(18.0f);
	RightHeadlightSpot->SetOuterConeAngle(38.0f);
	RightHeadlightSpot->SetLightColor(FLinearColor(1.0f, 0.94f, 0.80f));
	RightHeadlightSpot->SetCastShadows(false);

	LeftBrakeLightPoint = CreateDefaultSubobject<UPointLightComponent>(TEXT("LeftBrakeLightPoint"));
	LeftBrakeLightPoint->SetupAttachment(GetMesh());
	LeftBrakeLightPoint->SetRelativeLocation(FVector(-212.0f, -64.0f, 64.0f));
	LeftBrakeLightPoint->SetIntensity(220.0f);
	LeftBrakeLightPoint->SetAttenuationRadius(360.0f);
	LeftBrakeLightPoint->SetLightColor(FLinearColor(1.0f, 0.06f, 0.04f));
	LeftBrakeLightPoint->SetCastShadows(false);

	RightBrakeLightPoint = CreateDefaultSubobject<UPointLightComponent>(TEXT("RightBrakeLightPoint"));
	RightBrakeLightPoint->SetupAttachment(GetMesh());
	RightBrakeLightPoint->SetRelativeLocation(FVector(-212.0f, 64.0f, 64.0f));
	RightBrakeLightPoint->SetIntensity(220.0f);
	RightBrakeLightPoint->SetAttenuationRadius(360.0f);
	RightBrakeLightPoint->SetLightColor(FLinearColor(1.0f, 0.06f, 0.04f));
	RightBrakeLightPoint->SetCastShadows(false);

	// --- Chaos setup: Egea-class 1350 kg sedan tuned for 18° Cihangir/Galata hill-starts ---
	UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	Move->Mass = 1350.0f;
	Move->DragCoefficient = 0.31f;
	Move->ChassisWidth = 179.0f;
	Move->ChassisHeight = 145.0f;
	Move->bEnableCenterOfMassOverride = true;
	Move->CenterOfMassOverride = FVector(15.0f, 0.0f, -15.0f);

	Move->EngineSetup.MaxTorque = 420.0f;
	Move->EngineSetup.MaxRPM = 4800.0f;
	Move->EngineSetup.EngineIdleRPM = 850.0f;
	Move->EngineSetup.EngineBrakeEffect = 0.22f;
	Move->EngineSetup.EngineRevUpMOI = 4.5f;
	Move->EngineSetup.EngineRevDownRate = 600.0f;
	Move->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
	Move->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(800.0f,  345.0f);
	Move->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(1500.0f, 420.0f);
	Move->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(2750.0f, 420.0f);
	Move->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(3800.0f, 365.0f);
	Move->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(4800.0f, 270.0f);

	Move->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;
	Move->DifferentialSetup.FrontRearSplit = 0.58f;
	Move->TransmissionSetup.bUseAutomaticGears = true;
	Move->TransmissionSetup.bUseAutoReverse = true;
	Move->TransmissionSetup.FinalRatio = 4.10f;
	Move->TransmissionSetup.GearChangeTime = 0.28f;

	Move->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	Move->SteeringSetup.AngleRatio = 0.7f;

	Move->WheelSetups.SetNum(4);
	Move->WheelSetups[0].WheelClass = UTaxiWheelFront::StaticClass(); Move->WheelSetups[0].BoneName = TEXT("Phys_Wheel_FL");
	Move->WheelSetups[1].WheelClass = UTaxiWheelFront::StaticClass(); Move->WheelSetups[1].BoneName = TEXT("Phys_Wheel_FR");
	Move->WheelSetups[2].WheelClass = UTaxiWheelRear::StaticClass();  Move->WheelSetups[2].BoneName = TEXT("Phys_Wheel_BL");
	Move->WheelSetups[3].WheelClass = UTaxiWheelRear::StaticClass();  Move->WheelSetups[3].BoneName = TEXT("Phys_Wheel_BR");

	// --- Procedural 4-Slot Contoured Vehicle Body & 5-Spoke Alloy Wheels ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	TaxiBodyProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TaxiBodyProcMesh"));
	TaxiBodyProcMesh->SetupAttachment(GetMesh());
	TaxiBodyProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TaxiBodyProcMesh->SetCastShadow(true);

	const FVector WheelOffsets[4] = {
		FVector( 130.0f, -86.0f, 35.0f), // FL
		FVector( 130.0f,  86.0f, 35.0f), // FR
		FVector(-130.0f, -86.0f, 35.0f), // BL
		FVector(-130.0f,  86.0f, 35.0f)  // BR
	};
	for (int32 i = 0; i < 4; ++i)
	{
		UProceduralMeshComponent* WP = CreateDefaultSubobject<UProceduralMeshComponent>(*FString::Printf(TEXT("VisualWheelProc_%d"), i));
		WP->SetupAttachment(GetMesh());
		WP->SetRelativeLocation(WheelOffsets[i]);
		WP->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WP->SetCastShadow(true);
		VisualWheelProcMeshes.Add(WP);
	}

	PlaceholderChassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderChassis"));
	PlaceholderChassis->SetupAttachment(GetMesh());
	PlaceholderChassis->SetRelativeLocation(FVector(0.0f, 0.0f, 52.0f));
	PlaceholderChassis->SetRelativeScale3D(FVector(4.45f, 1.80f, 0.54f));
	PlaceholderChassis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderChassis->SetVisibility(false);
	if (CubeFinder.Succeeded()) PlaceholderChassis->SetStaticMesh(CubeFinder.Object);

	PlaceholderCabin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderCabin"));
	PlaceholderCabin->SetupAttachment(PlaceholderChassis);
	PlaceholderCabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderCabin->SetVisibility(false);
	if (CubeFinder.Succeeded()) PlaceholderCabin->SetStaticMesh(CubeFinder.Object);

	PlaceholderWindshield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderWindshield"));
	PlaceholderWindshield->SetupAttachment(PlaceholderChassis);
	PlaceholderWindshield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderWindshield->SetVisibility(false);
	if (CubeFinder.Succeeded()) PlaceholderWindshield->SetStaticMesh(CubeFinder.Object);

	PlaceholderSideBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderSideBand"));
	PlaceholderSideBand->SetupAttachment(PlaceholderChassis);
	PlaceholderSideBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderSideBand->SetVisibility(false);
	if (CubeFinder.Succeeded()) PlaceholderSideBand->SetStaticMesh(CubeFinder.Object);

	PlaceholderFrontGrille = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderFrontGrille"));
	PlaceholderFrontGrille->SetupAttachment(PlaceholderChassis);
	PlaceholderFrontGrille->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderFrontGrille->SetVisibility(false);
	if (CubeFinder.Succeeded()) PlaceholderFrontGrille->SetStaticMesh(CubeFinder.Object);

	PlaceholderRoofSign = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderRoofSign"));
	PlaceholderRoofSign->SetupAttachment(PlaceholderChassis);
	PlaceholderRoofSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderRoofSign->SetVisibility(false);
	if (CubeFinder.Succeeded()) PlaceholderRoofSign->SetStaticMesh(CubeFinder.Object);

	for (int32 i = 0; i < 2; ++i)
	{
		UStaticMeshComponent* HL = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderHeadlight_%d"), i));
		HL->SetupAttachment(PlaceholderChassis);
		HL->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HL->SetVisibility(false);
		PlaceholderHeadlights.Add(HL);

		UStaticMeshComponent* TL = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderTaillight_%d"), i));
		TL->SetupAttachment(PlaceholderChassis);
		TL->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TL->SetVisibility(false);
		PlaceholderTaillights.Add(TL);
	}

	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderWheel_%d"), i));
		W->SetupAttachment(GetMesh());
		W->SetRelativeLocation(WheelOffsets[i]);
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		W->SetVisibility(false);
		if (CylFinder.Succeeded()) W->SetStaticMesh(CylFinder.Object);
		PlaceholderWheels.Add(W);

		UStaticMeshComponent* Hub = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderHubcap_%d"), i));
		Hub->SetupAttachment(W);
		Hub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hub->SetVisibility(false);
		PlaceholderHubcaps.Add(Hub);
	}

	// --- Cameras ---
	ChaseSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseSpringArm"));
	ChaseSpringArm->SetupAttachment(GetMesh());
	ChaseSpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	ChaseSpringArm->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
	ChaseSpringArm->TargetArmLength = 620.0f;
	ChaseSpringArm->bEnableCameraLag = true;
	ChaseSpringArm->CameraLagSpeed = 6.0f;
	ChaseSpringArm->bEnableCameraRotationLag = true;
	ChaseSpringArm->CameraRotationLagSpeed = 3.0f;
	ChaseSpringArm->bInheritRoll = false;
	ChaseSpringArm->bInheritPitch = false;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(ChaseSpringArm);
	ChaseCamera->FieldOfView = 80.0f;

	InteriorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InteriorCamera"));
	InteriorCamera->SetupAttachment(GetMesh());
	InteriorCamera->SetRelativeLocation(FVector(-10.0f, -36.0f, 112.0f));
	InteriorCamera->FieldOfView = 85.0f;
	InteriorCamera->SetAutoActivate(false);

	HoodCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("HoodCamera"));
	HoodCamera->SetupAttachment(GetMesh());
	HoodCamera->SetRelativeLocation(FVector(145.0f, 0.0f, 85.0f));
	HoodCamera->FieldOfView = 85.0f;
	HoodCamera->SetAutoActivate(false);

	// Cockpit / Interior Dashboard & Controls
	InteriorDashboardProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("InteriorDashboardProcMesh"));
	InteriorDashboardProcMesh->SetupAttachment(GetMesh());
	InteriorDashboardProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SteeringWheelProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SteeringWheelProcMesh"));
	SteeringWheelProcMesh->SetupAttachment(GetMesh());
	SteeringWheelProcMesh->SetRelativeLocation(FVector(22.0f, -36.0f, 96.0f));
	SteeringWheelProcMesh->SetRelativeRotation(FRotator(-22.0f, 0.0f, 0.0f));
	SteeringWheelProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LeftWiperProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("LeftWiperProcMesh"));
	LeftWiperProcMesh->SetupAttachment(GetMesh());
	LeftWiperProcMesh->SetRelativeLocation(FVector(65.0f, -38.0f, 91.0f));
	LeftWiperProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RightWiperProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RightWiperProcMesh"));
	RightWiperProcMesh->SetupAttachment(GetMesh());
	RightWiperProcMesh->SetRelativeLocation(FVector(65.0f, 12.0f, 91.0f));
	RightWiperProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// --- Systems ---
	SurfaceComponent = CreateDefaultSubobject<UTaxiSurfaceComponent>(TEXT("SurfaceComponent"));
	Taximeter = CreateDefaultSubobject<UTaximeterComponent>(TEXT("Taximeter"));
	PassengerInteraction = CreateDefaultSubobject<UPassengerInteractionComponent>(TEXT("PassengerInteraction"));

	TaximeterDisplay = CreateDefaultSubobject<UWidgetComponent>(TEXT("TaximeterDisplay"));
	TaximeterDisplay->SetupAttachment(GetMesh());
	TaximeterDisplay->SetWidgetClass(UTaximeterDisplayWidget::StaticClass());
	TaximeterDisplay->SetRelativeLocation(FVector(38.0f, 0.0f, 102.0f));
	TaximeterDisplay->SetRelativeRotation(FRotator(-10.0f, 180.0f, 0.0f));
	TaximeterDisplay->SetDrawSize(FVector2D(320.0f, 120.0f));
	TaximeterDisplay->SetRelativeScale3D(FVector(0.05f));
	TaximeterDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RoadNoiseAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RoadNoiseAudio"));
	RoadNoiseAudio->SetupAttachment(GetMesh());
	RoadNoiseAudio->bAutoActivate = true;
}

UChaosWheeledVehicleMovementComponent* ATaxiPawn::GetChaosMovement() const
{
	return Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

void ATaxiPawn::BeginPlay()
{
	Super::BeginPlay();

	const bool bHasSkeletalVehicle = (GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr);
	bUsingPlaceholderDrive = !bHasSkeletalVehicle;

	if (bUsingPlaceholderDrive)
	{
		// Build the 4-Slot Contoured YellowTaxi procedural mesh (Slot 0 Glossy Paint, Slot 1 Dark Glass,
		// Slot 2 Rubber/Trim, Slot 3 Emissive Lights & TAKSI Roof Dome) on TaxiBodyProcMesh!
		if (TaxiBodyProcMesh)
		{
			FIstanbulMeshSectionData PaintSection, GlassSection, TrimSection, LightsSection, LodSection;
			FLinearColor PrimaryPaintColor;
			float LengthCm = 445.0f, WidthCm = 182.0f;
			ATrafficVehicle::BuildIstanbulVehicleMultiSlotSections(
				EIstanbulVehicleType::YellowTaxi, 1453, true,
				PaintSection, GlassSection, TrimSection, LightsSection, LodSection,
				PrimaryPaintColor, LengthCm, WidthCm);

			ATrafficVehicle::CommitMultiSlotVehicleMesh(
				this, TaxiBodyProcMesh, false, PrimaryPaintColor,
				PaintSection, GlassSection, TrimSection, LightsSection);

			TaxiBodyProcMesh->SetVisibility(true);
		}

		// Build 5-Spoke Sculpted Alloy Rim + Rubber Tire Wheel Meshes on VisualWheelProcMeshes
		FIstanbulMeshSectionData TireSection, RimSection;
		FIstanbulMeshUtils::AppendDetailedWheelMesh(TireSection, RimSection, 35.0f, 13.0f, 18);
		UMaterialInterface* RubberMat = FIstanbulMeshUtils::CreatePBRMaterial(this, FLinearColor(0.035f, 0.035f, 0.04f), 0.05f, 0.85f, 0.2f);
		UMaterialInterface* AlloyMat  = FIstanbulMeshUtils::CreatePBRMaterial(this, FLinearColor(0.76f, 0.79f, 0.84f), 0.78f, 0.20f, 0.9f);

		for (UProceduralMeshComponent* WP : VisualWheelProcMeshes)
		{
			if (WP)
			{
				WP->ClearAllMeshSections();
				TireSection.CommitToComponent(WP, 0, false, RubberMat);
				RimSection.CommitToComponent(WP, 1, false, AlloyMat);
				WP->SetVisibility(true);
			}
		}

		// Procedural 3D Interior Dashboard, Steering Wheel & Animated Wipers
		if (InteriorDashboardProcMesh)
		{
			FIstanbulMeshSectionData DashSec;
			const FLinearColor DashDark(0.08f, 0.08f, 0.09f);
			const FLinearColor MeterBoxCol(0.12f, 0.12f, 0.14f);
			const FLinearColor MirrorFrame(0.15f, 0.15f, 0.17f);
			const FLinearColor MirrorGlass(0.75f, 0.85f, 0.95f);

			// Main curved dashboard fascia
			FIstanbulMeshUtils::AppendBeveledBox(DashSec, FTransform(FVector(32.0f, 0.0f, 88.0f)), FVector(26.0f, 74.0f, 15.0f), 4.0f, DashDark);
			// Instrument binnacle hood over steering column
			FIstanbulMeshUtils::AppendBeveledBox(DashSec, FTransform(FVector(26.0f, -36.0f, 100.0f)), FVector(16.0f, 22.0f, 7.0f), 2.5f, DashDark);
			// Center console descending between driver and passenger seats
			FIstanbulMeshUtils::AppendBeveledBox(DashSec, FTransform(FVector(0.0f, 0.0f, 62.0f)), FVector(38.0f, 14.0f, 20.0f), 3.0f, DashDark);
			// 3D physical taximeter casing on center dash
			FIstanbulMeshUtils::AppendBeveledBox(DashSec, FTransform(FVector(38.0f, 0.0f, 98.0f)), FVector(4.5f, 14.0f, 6.0f), 1.0f, MeterBoxCol);
			// Rearview mirror mounted from windshield header
			FIstanbulMeshUtils::AppendBeveledBox(DashSec, FTransform(FVector(18.0f, 0.0f, 134.0f)), FVector(2.0f, 12.0f, 3.2f), 0.8f, MirrorFrame);
			FIstanbulMeshUtils::AppendBeveledBox(DashSec, FTransform(FVector(16.8f, 0.0f, 134.0f)), FVector(0.8f, 11.2f, 2.6f), 0.5f, MirrorGlass);

			UMaterialInterface* DashMat = FIstanbulMeshUtils::CreatePBRMaterial(this, DashDark, 0.05f, 0.88f, 0.2f);
			DashSec.CommitToComponent(InteriorDashboardProcMesh, 0, false, DashMat);
		}

		if (SteeringWheelProcMesh)
		{
			FIstanbulMeshSectionData WheelSec;
			const FLinearColor WheelRimCol(0.05f, 0.05f, 0.06f);
			const FLinearColor SpokesCol(0.22f, 0.22f, 0.25f);
			// Steering wheel rim (34cm diameter outer torus)
			FIstanbulMeshUtils::AppendCylinder(WheelSec, FTransform(FRotator(90.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f)), 17.0f, 1.8f, 16, WheelRimCol);
			// Center boss / horn pad
			FIstanbulMeshUtils::AppendCylinder(WheelSec, FTransform(FRotator(90.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -1.0f)), 6.0f, 2.2f, 12, SpokesCol);
			// 3 Spokes
			FIstanbulMeshUtils::AppendBeveledBox(WheelSec, FTransform(FVector(0.0f, 0.0f, 0.0f)), FVector(1.2f, 15.0f, 1.2f), 0.4f, SpokesCol);
			FIstanbulMeshUtils::AppendBeveledBox(WheelSec, FTransform(FVector(0.0f, 0.0f, -8.0f)), FVector(1.2f, 2.0f, 7.5f), 0.4f, SpokesCol);

			UMaterialInterface* WheelMat = FIstanbulMeshUtils::CreatePBRMaterial(this, WheelRimCol, 0.05f, 0.80f, 0.2f);
			WheelSec.CommitToComponent(SteeringWheelProcMesh, 0, false, WheelMat);
		}

		// Windshield Wipers
		FIstanbulMeshSectionData WiperSec;
		const FLinearColor WiperBladeCol(0.06f, 0.06f, 0.07f);
		FIstanbulMeshUtils::AppendBeveledBox(WiperSec, FTransform(FRotator(-40.0f, 0.0f, 0.0f), FVector(18.0f, 0.0f, 16.0f)), FVector(20.0f, 1.2f, 0.8f), 0.3f, WiperBladeCol);
		UMaterialInterface* WiperMat = FIstanbulMeshUtils::CreatePBRMaterial(this, WiperBladeCol, 0.1f, 0.7f, 0.3f);

		if (LeftWiperProcMesh)
		{
			WiperSec.CommitToComponent(LeftWiperProcMesh, 0, false, WiperMat);
		}
		if (RightWiperProcMesh)
		{
			WiperSec.CommitToComponent(RightWiperProcMesh, 0, false, WiperMat);
		}

		// Ensure legacy blocky static mesh placeholders remain hidden
		if (PlaceholderChassis)     PlaceholderChassis->SetVisibility(false);
		if (PlaceholderCabin)       PlaceholderCabin->SetVisibility(false);
		if (PlaceholderWindshield)  PlaceholderWindshield->SetVisibility(false);
		if (PlaceholderSideBand)    PlaceholderSideBand->SetVisibility(false);
		if (PlaceholderFrontGrille) PlaceholderFrontGrille->SetVisibility(false);
		if (PlaceholderRoofSign)    PlaceholderRoofSign->SetVisibility(false);
		for (UStaticMeshComponent* HL : PlaceholderHeadlights) if (HL) HL->SetVisibility(false);
		for (UStaticMeshComponent* TL : PlaceholderTaillights) if (TL) TL->SetVisibility(false);
		for (UStaticMeshComponent* W  : PlaceholderWheels)     if (W)  W->SetVisibility(false);
		for (UStaticMeshComponent* H  : PlaceholderHubcaps)    if (H)  H->SetVisibility(false);
	}

	if (PassengerInteraction)
	{
		OnNearMiss.AddUniqueDynamic(PassengerInteraction, &UPassengerInteractionComponent::NotifyNearMiss);
		OnScrape.AddUniqueDynamic(PassengerInteraction, &UPassengerInteractionComponent::NotifyVehicleScrape);
	}

	InteriorCameraBaseLocation = InteriorCamera->GetRelativeLocation();
	bInteriorCamera = bStartInInterior;
	ApplyCameraMode();

	SurfaceComponent->OnWheelSurfaceImpact.AddDynamic(this, &ATaxiPawn::HandleSurfaceImpact);

	if (TaximeterDisplay)
	{
		TaximeterDisplay->InitWidget();
		if (UTaximeterDisplayWidget* DisplayWidget = Cast<UTaximeterDisplayWidget>(TaximeterDisplay->GetUserWidgetObject()))
		{
			DisplayWidget->BindToTaximeter(Taximeter);
		}
	}
}

void ATaxiPawn::NotifyHit(
	UPrimitiveComponent* MyComp,
	AActor* Other,
	UPrimitiveComponent* OtherComp,
	bool bSelfMoved,
	FVector HitLocation,
	FVector HitNormal,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	if (!Other || Other == this)
	{
		return;
	}

	const float CurrentSpeedKmh = FMath::Abs(GetSpeedKmh());
	if (ADestructibleStreetProp* Prop = Cast<ADestructibleStreetProp>(Other))
	{
		Prop->TriggerKnockOver(HitLocation, GetActorForwardVector() * (CurrentSpeedKmh / 0.036f), this);
		return;
	}

	if (!bUsingPlaceholderDrive && CurrentSpeedKmh > 10.0f)
	{
		const float HeadOnDot = FMath::Abs(FVector::DotProduct(HitNormal, GetActorForwardVector()));
		HandleSolidObstacleCollision(Other, HitLocation, HitNormal, CurrentSpeedKmh, HeadOnDot >= 0.55f);
	}
}

void ATaxiPawn::HandleSolidObstacleCollision(
	AActor* HitActor,
	const FVector& /*ImpactPoint*/,
	const FVector& /*ImpactNormal*/,
	float ImpactSpeedKmh,
	bool bHeadOnImpact)
{
	const float Severity01 = FMath::Clamp(ImpactSpeedKmh / 65.0f, 0.25f, 1.0f);
	JoltOffset = FMath::Max(JoltOffset, Severity01 * ImpactJoltAmplitude * (bHeadOnImpact ? 1.35f : 0.65f));

	if (bHeadOnImpact)
	{
		if (ImpactSpeedKmh > 10.0f)
		{
			if (PassengerInteraction)
			{
				PassengerInteraction->NotifyHardJoltOrCollision(Severity01, true);
			}
			if (USoundBase* CrashCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_grab.VR_grab")))
			{
				UGameplayStatics::PlaySoundAtLocation(this, CrashCue, GetActorLocation(), FMath::Clamp(Severity01 * 1.1f, 0.5f, 1.0f), 0.78f);
			}
			if (APlayerController* PC = Cast<APlayerController>(Controller))
			{
				PC->PlayDynamicForceFeedback(Severity01, 0.28f, true, true, true, true);
				if (CollisionWarningCooldownSec <= 0.0f)
				{
					CollisionWarningCooldownSec = 2.4f;
					if (AIstanbulTaxiHUD* TaxiHUD = Cast<AIstanbulTaxiHUD>(PC->GetHUD()))
					{
						TaxiHUD->PushToast(
							TEXT("DİKKAT: Kaza / Çarpma! Yolcu sabrı ve konforu azaldı!"),
							FLinearColor(1.0f, 0.22f, 0.16f), 3.6f);
					}
				}
			}
		}
	}
	else if (ScrapeCooldownSec <= 0.0f && ImpactSpeedKmh > 8.0f)
	{
		ScrapeCooldownSec = 1.6f;
		if (USoundBase* ScrapeCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_grab.VR_grab")))
		{
			UGameplayStatics::PlaySoundAtLocation(this, ScrapeCue, GetActorLocation(), 0.55f, 1.25f);
		}
		OnScrape.Broadcast(HitActor, ImpactSpeedKmh);
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			if (CollisionWarningCooldownSec <= 0.0f)
			{
				CollisionWarningCooldownSec = 2.4f;
				if (AIstanbulTaxiHUD* TaxiHUD = Cast<AIstanbulTaxiHUD>(PC->GetHUD()))
				{
					TaxiHUD->PushToast(
						TEXT("DİKKAT: Araç / Duvar sürtmesi! Yolcu sabrı azaldı!"),
						FLinearColor(1.0f, 0.55f, 0.12f), 3.0f);
				}
			}
		}
	}
}

void ATaxiPawn::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DrivingMappingContext)
			{
				Sub->AddMappingContext(DrivingMappingContext, DrivingMappingPriority);
			}
		}
	}
}

void ATaxiPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent) return;

	// 1. Legacy DefaultInput.ini bindings: work immediately out of the box with zero .uasset setup
	PlayerInputComponent->BindAxis(TEXT("Throttle"),  this, &ATaxiPawn::LegacyThrottle);
	PlayerInputComponent->BindAxis(TEXT("Brake"),     this, &ATaxiPawn::LegacyBrake);
	PlayerInputComponent->BindAxis(TEXT("Steering"),  this, &ATaxiPawn::LegacySteering);
	PlayerInputComponent->BindAxis(TEXT("LookYaw"),   this, &ATaxiPawn::LegacyLookYaw);
	PlayerInputComponent->BindAxis(TEXT("LookPitch"), this, &ATaxiPawn::LegacyLookPitch);

	PlayerInputComponent->BindAction(TEXT("Handbrake"),       IE_Pressed,  this, &ATaxiPawn::LegacyHandbrakePressed);
	PlayerInputComponent->BindAction(TEXT("Handbrake"),       IE_Released, this, &ATaxiPawn::LegacyHandbrakeReleased);
	PlayerInputComponent->BindAction(TEXT("ToggleCamera"),    IE_Pressed,  this, &ATaxiPawn::LegacyToggleCamera);
	PlayerInputComponent->BindAction(TEXT("ToggleWeather"),   IE_Pressed,  this, &ATaxiPawn::LegacyToggleWeather);
	PlayerInputComponent->BindAction(TEXT("ResetVehicle"),    IE_Pressed,  this, &ATaxiPawn::LegacyReset);
	PlayerInputComponent->BindAction(TEXT("ToggleTaximeter"), IE_Pressed,  this, &ATaxiPawn::LegacyTaximeterToggle);
	PlayerInputComponent->BindAction(TEXT("PauseTaximeter"),  IE_Pressed,  this, &ATaxiPawn::LegacyTaximeterPause);
	PlayerInputComponent->BindAction(TEXT("CycleTariff"),     IE_Pressed,  this, &ATaxiPawn::LegacyCycleTariff);
	PlayerInputComponent->BindAction(TEXT("InteractMission"), IE_Pressed,  this, &ATaxiPawn::LegacyInteractMission);
	PlayerInputComponent->BindAction(TEXT("ToggleWindow"),    IE_Pressed,  this, &ATaxiPawn::LegacyWindowToggle);
	PlayerInputComponent->BindAction(TEXT("TriggerTestEzan"), IE_Pressed,  this, &ATaxiPawn::LegacyTriggerEzan);
	PlayerInputComponent->BindAction(TEXT("AdvanceTimeHour"), IE_Pressed,  this, &ATaxiPawn::LegacyAdvanceTime);
	PlayerInputComponent->BindAction(TEXT("Horn"),            IE_Pressed,  this, &ATaxiPawn::LegacyHornPressed);
	PlayerInputComponent->BindAction(TEXT("Horn"),            IE_Released, this, &ATaxiPawn::LegacyHornReleased);
	PlayerInputComponent->BindAction(TEXT("DialogueChoice1"), IE_Pressed,  this, &ATaxiPawn::LegacyDialogueChoice1);
	PlayerInputComponent->BindAction(TEXT("DialogueChoice2"), IE_Pressed,  this, &ATaxiPawn::LegacyDialogueChoice2);
	PlayerInputComponent->BindAction(TEXT("DialogueChoice3"), IE_Pressed,  this, &ATaxiPawn::LegacyDialogueChoice3);

	// 2. Enhanced Input bindings
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (ThrottleAction)
		{
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ATaxiPawn::OnThrottle);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ATaxiPawn::OnThrottle);
		}
		if (BrakeAction)
		{
			EIC->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ATaxiPawn::OnBrake);
			EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ATaxiPawn::OnBrake);
		}
		if (SteeringAction)
		{
			EIC->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ATaxiPawn::OnSteering);
			EIC->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ATaxiPawn::OnSteering);
		}
		if (HandbrakeAction)
		{
			EIC->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ATaxiPawn::OnHandbrakePressed);
			EIC->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ATaxiPawn::OnHandbrakeReleased);
		}
		if (LookAction)            EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATaxiPawn::OnLook);
		if (ToggleCameraAction)    EIC->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &ATaxiPawn::OnToggleCamera);
		if (ResetAction)           EIC->BindAction(ResetAction, ETriggerEvent::Started, this, &ATaxiPawn::OnReset);
		if (TaximeterToggleAction) EIC->BindAction(TaximeterToggleAction, ETriggerEvent::Started, this, &ATaxiPawn::OnTaximeterToggle);
		if (TaximeterPauseAction)  EIC->BindAction(TaximeterPauseAction, ETriggerEvent::Started, this, &ATaxiPawn::OnTaximeterPause);
		if (WindowToggleAction)    EIC->BindAction(WindowToggleAction, ETriggerEvent::Started, this, &ATaxiPawn::OnWindowToggle);
	}
}

float ATaxiPawn::GetSpeedKmh() const
{
	if (bUsingPlaceholderDrive)
	{
		return PlaceholderForwardSpeedCms * 0.036f;
	}
	const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	return Move ? Move->GetForwardSpeed() * 0.036f : 0.0f;
}

float ATaxiPawn::GetEngineRPM() const
{
	if (!bUsingPlaceholderDrive)
	{
		if (const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement())
		{
			return Move->GetEngineRotationSpeed();
		}
	}
	const float AbsKmh = FMath::Abs(GetSpeedKmh());
	if (AbsKmh < 1.0f)
	{
		return 850.0f + RawThrottleInput * 1600.0f;
	}
	static const float GearMaxKmh[6] = { 24.0f, 44.0f, 68.0f, 96.0f, 125.0f, 160.0f };
	const int32 GearIdx = FMath::Clamp(GetCurrentGear() - 1, 0, 5);
	const float PrevMax = GearIdx > 0 ? GearMaxKmh[GearIdx - 1] * 0.62f : 0.0f;
	const float GearAlpha = FMath::Clamp((AbsKmh - PrevMax) / FMath::Max(GearMaxKmh[GearIdx] - PrevMax, 10.0f), 0.0f, 1.0f);
	return FMath::Clamp(FMath::Lerp(1350.0f, 3950.0f, GearAlpha) + RawThrottleInput * 350.0f, 850.0f, 4600.0f);
}

int32 ATaxiPawn::GetCurrentGear() const
{
	if (!bUsingPlaceholderDrive)
	{
		if (const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement())
		{
			return Move->GetCurrentGear();
		}
	}
	const float Kmh = GetSpeedKmh();
	if (Kmh < -1.0f) return -1; // Reverse (R)
	if (Kmh < 1.0f && RawThrottleInput < 0.05f) return 0; // Neutral (N)
	if (Kmh < 22.0f) return 1;
	if (Kmh < 42.0f) return 2;
	if (Kmh < 65.0f) return 3;
	if (Kmh < 92.0f) return 4;
	if (Kmh < 120.0f) return 5;
	return 6;
}

void ATaxiPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Speed-sensitive, smoothed steering
	const float SpeedAlpha = FMath::Clamp(FMath::Abs(GetSpeedKmh()) / HighSpeedSteerKmh, 0.0f, 1.0f);
	const float TargetSteer = RawSteerInput * FMath::Lerp(1.0f, HighSpeedSteerScale, SpeedAlpha);
	SmoothedSteer = FMath::FInterpTo(SmoothedSteer, TargetSteer, DeltaSeconds, SteeringInterpSpeed);
	if (UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement())
	{
		Move->SetSteeringInput(SmoothedSteer);
	}

	// Dynamic rear brake light intensity when braking or holding handbrake
	const float TargetBrakeLightIntensity = (RawBrakeInput > 0.05f || bHandbrakeActive) ? 1650.0f : 220.0f;
	if (LeftBrakeLightPoint)  LeftBrakeLightPoint->SetIntensity(TargetBrakeLightIntensity);
	if (RightBrakeLightPoint) RightBrakeLightPoint->SetIntensity(TargetBrakeLightIntensity);

	if (bUsingPlaceholderDrive)
	{
		UpdatePlaceholderDriveAndVisuals(DeltaSeconds);
	}

	if (!FMath::IsNearlyEqual(WindowOpenRatio, TargetWindowOpenRatio, 1e-3f))
	{
		WindowOpenRatio = FMath::FInterpConstantTo(WindowOpenRatio, TargetWindowOpenRatio, DeltaSeconds, WindowRollSpeed);
		SyncCabinAudioState();
	}

	UpdateFeel(DeltaSeconds);
}

void ATaxiPawn::UpdatePlaceholderDriveAndVisuals(float DeltaSeconds)
{
	const float AccelCms2    = 820.0f;  // 420 Nm diesel launch authority
	const float BrakeCms2    = 1750.0f;
	const float ReverseCms2  = 520.0f;
	const float DragPerSec   = 0.42f;
	const float MaxFwdCms    = 3888.0f; // ~140 km/h
	const float MaxRevCms    = -972.0f; // ~35 km/h

	const float PitchRad = FMath::DegreesToRadians(GetActorRotation().Pitch);
	const float SlopeGravityAccelCms2 = -980.0f * FMath::Sin(PitchRad) * 0.72f;

	if (bHandbrakeActive)
	{
		PlaceholderForwardSpeedCms = FMath::FInterpConstantTo(PlaceholderForwardSpeedCms, 0.0f, DeltaSeconds, BrakeCms2 * 2.2f);
		if (FMath::Abs(PlaceholderForwardSpeedCms) < 35.0f)
		{
			PlaceholderForwardSpeedCms = 0.0f;
		}
	}
	else if (RawThrottleInput > 0.01f)
	{
		if (PlaceholderForwardSpeedCms < -25.0f)
		{
			PlaceholderForwardSpeedCms = FMath::FInterpConstantTo(PlaceholderForwardSpeedCms, 0.0f, DeltaSeconds, BrakeCms2 * 1.4f);
		}
		else
		{
			const float HillStartAssist = (PlaceholderForwardSpeedCms < 180.0f && GetActorRotation().Pitch > 6.0f) ? 1.22f : 1.0f;
			PlaceholderForwardSpeedCms += (RawThrottleInput * AccelCms2 * HillStartAssist + SlopeGravityAccelCms2) * DeltaSeconds;
		}
	}
	else if (RawBrakeInput > 0.01f)
	{
		if (PlaceholderForwardSpeedCms > 25.0f)
		{
			PlaceholderForwardSpeedCms = FMath::FInterpConstantTo(PlaceholderForwardSpeedCms, 0.0f, DeltaSeconds, RawBrakeInput * BrakeCms2);
		}
		else
		{
			PlaceholderForwardSpeedCms += (-RawBrakeInput * ReverseCms2 + SlopeGravityAccelCms2) * DeltaSeconds;
		}
	}
	else
	{
		PlaceholderForwardSpeedCms += SlopeGravityAccelCms2 * DeltaSeconds;
		PlaceholderForwardSpeedCms -= PlaceholderForwardSpeedCms * DragPerSec * DeltaSeconds;
		if (FMath::Abs(SlopeGravityAccelCms2) < 15.0f && FMath::Abs(PlaceholderForwardSpeedCms) < 8.0f)
		{
			PlaceholderForwardSpeedCms = 0.0f;
		}
	}

	PlaceholderForwardSpeedCms = FMath::Clamp(PlaceholderForwardSpeedCms, MaxRevCms, MaxFwdCms);

	// Bicycle-model yaw rate (wheelbase ~ 270 cm, max front steer 36 deg)
	const float SteerRad = FMath::DegreesToRadians(SmoothedSteer * 36.0f);
	const float YawRateDegPerSec = FMath::RadiansToDegrees((PlaceholderForwardSpeedCms / 270.0f) * FMath::Tan(SteerRad));

	const FVector OldLoc = GetActorLocation();
	FRotator NewRot = GetActorRotation();
	NewRot.Yaw += YawRateDegPerSec * DeltaSeconds;

	FVector DesiredLoc = OldLoc + GetActorForwardVector() * (PlaceholderForwardSpeedCms * DeltaSeconds);

	if (ScrapeCooldownSec > 0.0f)
	{
		ScrapeCooldownSec = FMath::Max(0.0f, ScrapeCooldownSec - DeltaSeconds);
	}
	if (CollisionWarningCooldownSec > 0.0f)
	{
		CollisionWarningCooldownSec = FMath::Max(0.0f, CollisionWarningCooldownSec - DeltaSeconds);
	}
	if (bHornKeyDown)
	{
		HornHoldDurationSec += DeltaSeconds;
		if (HornHoldDurationSec >= 0.42f && HornHoldDurationSec - DeltaSeconds < 0.42f)
		{
			TriggerPlayerHorn(HornHoldDurationSec);
		}
	}

	// =========================================================================
	// PILLAR 1: Full Solid 3D Chassis Box Sweep (Zero Clipping Through Buildings,
	// Walls, Bridge Parapets, AI Traffic, or Street Props)
	// =========================================================================
	FVector ResolvedLoc = DesiredLoc;
	if (UWorld* World = GetWorld())
	{
		// Box height center at Z + 76cm with HalfExtent.Z = 44cm (bottom at +32cm, top at +120cm):
		// Easily clears 18cm sidewalk curbs and road slope transitions while solidly colliding with
		// every building, stone wall, bridge parapet (85cm), traffic vehicle, bollard (90cm), and pole!
		const FVector SweepLift(0.0f, 0.0f, 76.0f);
		const FCollisionShape ChassisSweepBox = FCollisionShape::MakeBox(FVector(202.0f, 84.0f, 44.0f));
		const FQuat YawQuat = FRotator(0.0f, NewRot.Yaw, 0.0f).Quaternion();
		FCollisionQueryParams SweepParams(SCENE_QUERY_STAT(TaxiSolidChassisSweep), false, this);

		FHitResult SweepHit;
		const bool bHit = World->SweepSingleByChannel(
			SweepHit,
			OldLoc + SweepLift,
			DesiredLoc + SweepLift,
			YawQuat,
			ECC_Pawn,
			ChassisSweepBox,
			SweepParams);

		if (bHit)
		{
			AActor* HitActor = SweepHit.GetActor();
			const float ImpactSpeedKmh = FMath::Abs(PlaceholderForwardSpeedCms) * 0.036f;

			// 1. Check if we struck a breakable street prop (streetlight, bollard/duba, traffic sign, trash bin)
			if (ADestructibleStreetProp* Prop = Cast<ADestructibleStreetProp>(HitActor))
			{
				if (Prop->TriggerKnockOver(SweepHit.ImpactPoint, GetActorForwardVector() * PlaceholderForwardSpeedCms, this))
				{
					// Prop knocked over! Slight momentum loss + jolt, continue moving through fallen pole
					PlaceholderForwardSpeedCms *= 0.82f;
					JoltOffset = FMath::Max(JoltOffset, 1.8f);
				}
				else
				{
					// Low-speed nudge (< 15 km/h): stop against the pole without knocking it down
					FVector PushNormal2D(SweepHit.ImpactNormal.X, SweepHit.ImpactNormal.Y, 0.0f);
					PushNormal2D = PushNormal2D.GetSafeNormal();
					ResolvedLoc = OldLoc + (DesiredLoc - OldLoc) * FMath::Max(0.0f, SweepHit.Time - 0.02f) + PushNormal2D * 3.5f;
					PlaceholderForwardSpeedCms = 0.0f;
				}
			}
			else
			{
				// 2. Solid Building, Wall, Bridge Parapet, or AI Traffic Vehicle -> ZERO CLIPPING!
				FVector WallNormal2D(SweepHit.ImpactNormal.X, SweepHit.ImpactNormal.Y, 0.0f);
				if (WallNormal2D.IsNearlyZero())
				{
					WallNormal2D = -GetActorForwardVector();
					WallNormal2D.Z = 0.0f;
				}
				WallNormal2D.Normalize();

				const float HeadOnDot = FMath::Abs(FVector::DotProduct(WallNormal2D, GetActorForwardVector()));
				const bool bHeadOnImpact = (HeadOnDot >= 0.55f);

				if (SweepHit.bStartPenetrating)
				{
					// Depenetrate immediately out of any overlapping wall/vehicle
					const float DepenDist = FMath::Clamp(SweepHit.PenetrationDepth + 6.0f, 6.0f, 65.0f);
					ResolvedLoc = OldLoc + WallNormal2D * DepenDist;
				}
				else
				{
					// Move to safe contact point + slide along the wall tangent
					const FVector MoveDelta = DesiredLoc - OldLoc;
					const FVector SafeContactLoc = OldLoc + MoveDelta * FMath::Max(0.0f, SweepHit.Time - 0.02f) + WallNormal2D * 3.5f;
					const FVector RemainingDelta = MoveDelta * (1.0f - SweepHit.Time);
					FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, WallNormal2D);
					SlideDelta.Z = 0.0f;

					ResolvedLoc = SafeContactLoc;
					if (!SlideDelta.IsNearlyZero(0.5f))
					{
						FHitResult SlideHit;
						if (!World->SweepSingleByChannel(SlideHit, SafeContactLoc + SweepLift, SafeContactLoc + SlideDelta + SweepLift, YawQuat, ECC_Pawn, ChassisSweepBox, SweepParams))
						{
							ResolvedLoc = SafeContactLoc + SlideDelta;
						}
					}
				}

				HandleSolidObstacleCollision(HitActor, SweepHit.ImpactPoint, WallNormal2D, ImpactSpeedKmh, bHeadOnImpact);

				if (bHeadOnImpact)
				{
					// Realistic bounce-back / deceleration on head-on collision
					if (ImpactSpeedKmh > 18.0f)
					{
						PlaceholderForwardSpeedCms = -PlaceholderForwardSpeedCms * 0.24f;
					}
					else
					{
						PlaceholderForwardSpeedCms = 0.0f;
					}
				}
				else
				{
					// Wall/vehicle side-scrape speed damping
					PlaceholderForwardSpeedCms *= 0.80f;
				}
			}
		}
	}

	// Ground snap + road slope pitch + surface detection (Cobblestone / TramRail / Pothole / Asphalt)
	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(TaxiGroundSnap), false, this);
	float GroundSlopePitchDeg = 0.0f;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, ResolvedLoc + FVector(0, 0, 220.0f), ResolvedLoc - FVector(0, 0, 360.0f), ECC_Visibility, GroundParams))
	{
		// Only snap to driveable road/sidewalk/bridge deck surfaces (prevent snapping onto roof of a tall building or vehicle!)
		const float HeightDiff = GroundHit.ImpactPoint.Z - OldLoc.Z;
		if (HeightDiff <= 65.0f && GroundHit.ImpactNormal.Z >= 0.72f)
		{
			ResolvedLoc.Z = FMath::FInterpTo(OldLoc.Z, GroundHit.ImpactPoint.Z + 2.0f, DeltaSeconds, 20.0f);
			const FVector FwdXY = FRotator(0.0f, NewRot.Yaw, 0.0f).Vector();
			GroundSlopePitchDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-FVector::DotProduct(GroundHit.ImpactNormal, FwdXY), -0.6f, 0.6f)));
			if (SurfaceComponent)
			{
				SurfaceComponent->NotifyGroundTraceHit(GroundHit, PlaceholderForwardSpeedCms * 0.036f);
			}
		}
	}
	NewRot.Pitch = FMath::FInterpTo(NewRot.Pitch, GroundSlopePitchDeg, DeltaSeconds, 12.0f);
	NewRot.Roll = 0.0f;
	CurrentSlopeAngleDeg = NewRot.Pitch;

	// Periodic debug log of slope angle under the vehicle
	SlopeDebugLogAccumulator += DeltaSeconds;
	if (SlopeDebugLogAccumulator >= 1.5f && (bDebugSlopeTelemetry || FMath::Abs(CurrentSlopeAngleDeg) >= 4.0f))
	{
		SlopeDebugLogAccumulator = 0.0f;
		const float GradePct = FMath::Tan(FMath::DegreesToRadians(FMath::Abs(CurrentSlopeAngleDeg))) * 100.0f;
		UE_LOG(LogIstanbulTaxi, Log,
			TEXT("TaxiSlopeTelemetry: Slope=%.2f deg (Grade=%.1f%%), Speed=%.1f km/h, Handbrake=%s, Throttle=%.2f"),
			CurrentSlopeAngleDeg, GradePct, GetSpeedKmh(),
			bHandbrakeActive ? TEXT("HOLD") : TEXT("OFF"), RawThrottleInput);
	}

	SetActorLocationAndRotation(ResolvedLoc, NewRot, false);

	// Periodic near-miss check when squeezing past traffic vehicles at > 20 km/h
	NearMissCheckTimer += DeltaSeconds;
	if (NearMissCheckTimer >= 0.35f && FMath::Abs(GetSpeedKmh()) > 20.0f)
	{
		NearMissCheckTimer = 0.0f;
		if (AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AIstanbulTrafficManager::StaticClass())))
		{
			for (ATrafficVehicle* V : TM->GetSpawnedVehicles())
			{
				if (!V) continue;
				const float CenterDistCm = FVector::Dist2D(GetActorLocation(), V->GetActorLocation());
				if (CenterDistCm > 185.0f && CenterDistCm < 245.0f)
				{
					OnNearMiss.Broadcast(V, CenterDistCm - 180.0f);
					break;
				}
			}
		}
	}

	// Set component velocity so UTaximeterComponent and ATollGateVolume read accurate GetVelocity()!
	if (USkeletalMeshComponent* RootSkel = GetMesh())
	{
		RootSkel->ComponentVelocity = GetActorForwardVector() * PlaceholderForwardSpeedCms;
	}

	// Visual body pitch/roll + 5-spoke spinning/steering alloy wheels
	const float LongPitch = (RawBrakeInput - RawThrottleInput) * 1.8f;
	const float LatRoll   = -SmoothedSteer * FMath::Clamp(FMath::Abs(PlaceholderForwardSpeedCms) / 1500.0f, 0.0f, 1.0f) * 3.2f;
	if (TaxiBodyProcMesh) TaxiBodyProcMesh->SetRelativeRotation(FRotator(LongPitch, 0.0f, LatRoll));

	// Real-time animated steering wheel rotation with A/D input
	if (SteeringWheelProcMesh)
	{
		SteeringWheelProcMesh->SetRelativeRotation(FRotator(-22.0f, 0.0f, -SmoothedSteer * 110.0f));
	}

	// Dynamic animated windshield wipers during rain or when weather is wet
	bool bShouldWipe = false;
	if (UWorld* W = GetWorld())
	{
		if (UWeatherSubsystem* WeatherSub = W->GetSubsystem<UWeatherSubsystem>())
		{
			bShouldWipe = WeatherSub->IsRaining() || (WeatherSub->GetWetness() > 0.4f);
		}
	}
	if (bShouldWipe)
	{
		WiperCycleTime += DeltaSeconds * 5.2f;
		const float WiperAngle = FMath::Abs(FMath::Sin(WiperCycleTime)) * 68.0f;
		if (LeftWiperProcMesh)  LeftWiperProcMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, WiperAngle));
		if (RightWiperProcMesh) RightWiperProcMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, WiperAngle));
	}
	else
	{
		if (LeftWiperProcMesh)  LeftWiperProcMesh->SetRelativeRotation(FRotator::ZeroRotator);
		if (RightWiperProcMesh) RightWiperProcMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}

	PlaceholderWheelSpinDeg = FMath::Fmod(PlaceholderWheelSpinDeg - (PlaceholderForwardSpeedCms * DeltaSeconds / 35.0f) * (180.0f / PI), 360.0f);
	for (int32 i = 0; i < VisualWheelProcMeshes.Num(); ++i)
	{
		if (UProceduralMeshComponent* WP = VisualWheelProcMeshes[i])
		{
			const float WheelYaw = (i < 2) ? (SmoothedSteer * 36.0f) : 0.0f;
			WP->SetRelativeRotation(FRotator(PlaceholderWheelSpinDeg, WheelYaw, 0.0f));
		}
	}
}

void ATaxiPawn::UpdateFeel(float DeltaSeconds)
{
	FeelTime += DeltaSeconds;
	JoltOffset = FMath::FInterpTo(JoltOffset, 0.0f, DeltaSeconds, ImpactJoltDecay);

	const float Intensity = SurfaceComponent->GetVibrationIntensity();
	const float Freq = SurfaceComponent->GetVibrationFrequency();
	const float T = FeelTime * Freq;

	const FVector Rumble(
		0.0f,
		FMath::PerlinNoise1D(T * 0.7f + 31.7f) * 0.4f,
		FMath::PerlinNoise1D(T));
	const FVector Offset = Rumble * (InteriorVibrationAmplitude * Intensity) + FVector(0.0f, 0.0f, -JoltOffset);

	if (bInteriorCamera)
	{
		InteriorCamera->SetRelativeLocation(InteriorCameraBaseLocation + Offset);
		InteriorCamera->SetRelativeRotation(InteriorLook);
	}
	else
	{
		ChaseCamera->SetRelativeLocation(Offset * ChaseVibrationScale);
	}

	if (RoadNoiseAudio && RoadNoiseAudio->IsPlaying())
	{
		RoadNoiseAudio->SetFloatParameter(TEXT("Speed"), FMath::Abs(GetSpeedKmh()));
		RoadNoiseAudio->SetIntParameter(TEXT("SurfaceIndex"), SurfaceComponent->GetDominantAudioSurfaceIndex());
		RoadNoiseAudio->SetFloatParameter(TEXT("Roughness"), Intensity);
		RoadNoiseAudio->SetFloatParameter(TEXT("Wetness"), SurfaceComponent->GetWetness());
	}
}

void ATaxiPawn::HandleSurfaceImpact(int32 WheelIndex, FGameplayTag Surface, float Severity)
{
	JoltOffset = FMath::Max(JoltOffset, Severity * ImpactJoltAmplitude);
	if (PassengerInteraction && Severity > 0.2f)
	{
		PassengerInteraction->NotifyHardJoltOrCollision(Severity, false);
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation(), FMath::Lerp(0.4f, 1.0f, Severity));
	}
	if (ImpactForceFeedback)
	{
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			FForceFeedbackParameters Params;
			PC->ClientPlayForceFeedback(ImpactForceFeedback, Params);
		}
	}
	UE_LOG(LogIstanbulTaxi, Verbose, TEXT("Impact wheel %d on %s, severity %.2f"), WheelIndex, *Surface.ToString(), Severity);
}

void ATaxiPawn::ApplyCameraMode()
{
	ChaseCamera->SetActive(CameraMode == ETaxiCameraViewMode::ThirdPersonChase);
	HoodCamera->SetActive(CameraMode == ETaxiCameraViewMode::HoodBumper);
	InteriorCamera->SetActive(CameraMode == ETaxiCameraViewMode::InteriorDriver);

	bInteriorCamera = (CameraMode == ETaxiCameraViewMode::InteriorDriver);
	InteriorLook = FRotator::ZeroRotator;
	SyncCabinAudioState();
}

void ATaxiPawn::SetWindowOpenRatio(float NewRatio)
{
	TargetWindowOpenRatio = FMath::Clamp(NewRatio, 0.0f, 1.0f);
	WindowOpenRatio = TargetWindowOpenRatio;
	SyncCabinAudioState();
}

void ATaxiPawn::ToggleDriverWindow()
{
	TargetWindowOpenRatio = (TargetWindowOpenRatio < 0.5f) ? 1.0f : 0.0f;
}

void ATaxiPawn::SyncCabinAudioState()
{
	if (UWorld* W = GetWorld())
	{
		if (UAudioAmbienceSubsystem* AudioSub = W->GetSubsystem<UAudioAmbienceSubsystem>())
		{
			AudioSub->SetCabinState(bInteriorCamera, WindowOpenRatio);
		}
	}
	if (RoadNoiseAudio && RoadNoiseAudio->IsPlaying())
	{
		const float EffectiveOpen = bInteriorCamera ? WindowOpenRatio : 1.0f;
		RoadNoiseAudio->SetLowPassFilterEnabled(EffectiveOpen < 0.99f);
		RoadNoiseAudio->SetLowPassFilterFrequency(FMath::Lerp(1200.0f, 20000.0f, EffectiveOpen));
	}
}

// ---------------- Enhanced Input Callbacks ----------------

void ATaxiPawn::OnThrottle(const FInputActionValue& Value)
{
	LegacyThrottle(Value.Get<float>());
}

void ATaxiPawn::OnBrake(const FInputActionValue& Value)
{
	LegacyBrake(Value.Get<float>());
}

void ATaxiPawn::OnSteering(const FInputActionValue& Value)
{
	LegacySteering(Value.Get<float>());
}

void ATaxiPawn::OnHandbrakePressed(const FInputActionValue&)  { LegacyHandbrakePressed(); }
void ATaxiPawn::OnHandbrakeReleased(const FInputActionValue&) { LegacyHandbrakeReleased(); }

void ATaxiPawn::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	LegacyLookYaw(Axis.X);
	LegacyLookPitch(Axis.Y);
}

void ATaxiPawn::OnToggleCamera(const FInputActionValue&)    { LegacyToggleCamera(); }
void ATaxiPawn::OnReset(const FInputActionValue&)           { LegacyReset(); }
void ATaxiPawn::OnTaximeterToggle(const FInputActionValue&) { LegacyTaximeterToggle(); }
void ATaxiPawn::OnTaximeterPause(const FInputActionValue&)  { LegacyTaximeterPause(); }
void ATaxiPawn::OnWindowToggle(const FInputActionValue&)    { LegacyWindowToggle(); }

// ---------------- Legacy Axis / Action Callbacks ----------------

void ATaxiPawn::LegacyThrottle(float Value)
{
	RawThrottleInput = FMath::Clamp(Value, 0.0f, 1.0f);
	if (UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement()) Move->SetThrottleInput(RawThrottleInput);
}

void ATaxiPawn::LegacyBrake(float Value)
{
	RawBrakeInput = FMath::Clamp(Value, 0.0f, 1.0f);
	if (UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement()) Move->SetBrakeInput(RawBrakeInput);
}

void ATaxiPawn::LegacySteering(float Value)
{
	RawSteerInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void ATaxiPawn::LegacyLookYaw(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;
	if (bInteriorCamera)
	{
		InteriorLook.Yaw = FMath::Clamp(InteriorLook.Yaw + Value, -InteriorYawLimit, InteriorYawLimit);
	}
	else
	{
		FRotator R = ChaseSpringArm->GetRelativeRotation();
		R.Yaw += Value;
		ChaseSpringArm->SetRelativeRotation(R);
	}
}

void ATaxiPawn::LegacyLookPitch(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;
	if (bInteriorCamera)
	{
		InteriorLook.Pitch = FMath::Clamp(InteriorLook.Pitch + Value, -InteriorPitchLimit, InteriorPitchLimit);
	}
	else
	{
		FRotator R = ChaseSpringArm->GetRelativeRotation();
		R.Pitch = FMath::Clamp(R.Pitch + Value, -60.0f, 10.0f);
		ChaseSpringArm->SetRelativeRotation(R);
	}
}

void ATaxiPawn::LegacyHandbrakePressed()
{
	bHandbrakeActive = true;
	if (UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement()) Move->SetHandbrakeInput(true);
}

void ATaxiPawn::LegacyHandbrakeReleased()
{
	bHandbrakeActive = false;
	if (UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement()) Move->SetHandbrakeInput(false);
}

void ATaxiPawn::LegacyToggleCamera()
{
	const uint8 NextMode = (static_cast<uint8>(CameraMode) + 1) % 3;
	CameraMode = static_cast<ETaxiCameraViewMode>(NextMode);
	ApplyCameraMode();

	if (AIstanbulTaxiHUD* HUD = Cast<AIstanbulTaxiHUD>(UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetHUD()))
	{
		static const TCHAR* CameraModeNames[3] = {
			TEXT("Kamera: Dış Takip (Chase Cam)"),
			TEXT("Kamera: Kaput / Tampon Görünümü (Hood Cam)"),
			TEXT("Kamera: Sürücü Kokpit / İç Görünüm (Cockpit View)")
		};
		HUD->PushToast(CameraModeNames[NextMode], FLinearColor(0.95f, 0.85f, 0.35f), 2.5f);
	}
}

void ATaxiPawn::LegacyToggleWeather()
{
	if (UWorld* W = GetWorld())
	{
		if (UWeatherSubsystem* WeatherSub = W->GetSubsystem<UWeatherSubsystem>())
		{
			WeatherSub->CycleWeather();
		}
	}
}

void ATaxiPawn::LegacyReset()
{
	ResetVehicle();
}

void ATaxiPawn::LegacyTaximeterToggle()
{
	if (Taximeter) Taximeter->ToggleMeter();
}

void ATaxiPawn::LegacyTaximeterPause()
{
	if (Taximeter) Taximeter->TogglePause();
}

void ATaxiPawn::LegacyCycleTariff()
{
	if (Taximeter) Taximeter->CycleTariffPreset();
}

void ATaxiPawn::LegacyInteractMission()
{
	if (UWorld* W = GetWorld())
	{
		if (UPassengerMissionSubsystem* Missions = W->GetSubsystem<UPassengerMissionSubsystem>())
		{
			if (Missions->GetPhase() == ETaxiRidePhase::None)
			{
				Missions->StartDebugRide(FVector(3000.0f, 0.0f, 0.0f), FVector(18000.0f, 0.0f, 0.0f));
			}
			else
			{
				Missions->CancelRide();
			}
		}
	}
}

void ATaxiPawn::LegacyWindowToggle()
{
	ToggleDriverWindow();
}

void ATaxiPawn::LegacyTriggerEzan()
{
	if (UWorld* W = GetWorld())
	{
		if (UAudioAmbienceSubsystem* AudioSub = W->GetSubsystem<UAudioAmbienceSubsystem>())
		{
			AudioSub->TriggerEzanNow(IstanbulTaxiTags::Prayer_Aksam);
		}
	}
}

void ATaxiPawn::LegacyAdvanceTime()
{
	if (UWorld* W = GetWorld())
	{
		if (UTimeOfDaySubsystem* TimeSub = W->GetSubsystem<UTimeOfDaySubsystem>())
		{
			TimeSub->SetTimeOfDayHours(TimeSub->GetTimeOfDayHours() + 1.5f);
		}
	}
}

void ATaxiPawn::LegacyHornPressed()
{
	bHornKeyDown = true;
	HornHoldDurationSec = 0.0f;
	TriggerPlayerHorn(0.15f);
}

void ATaxiPawn::LegacyHornReleased()
{
	if (bHornKeyDown && HornHoldDurationSec >= 0.35f)
	{
		TriggerPlayerHorn(HornHoldDurationSec);
	}
	bHornKeyDown = false;
	HornHoldDurationSec = 0.0f;
}

void ATaxiPawn::TriggerPlayerHorn(float HoldDurationSec)
{
	const bool bLongAggressive = (HoldDurationSec >= 0.35f);
	if (USoundBase* HornCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1")))
	{
		UGameplayStatics::PlaySoundAtLocation(this, HornCue, GetActorLocation(),
			bLongAggressive ? 1.15f : 0.85f,
			bLongAggressive ? 0.64f : 0.78f);
	}

	if (UWorld* W = GetWorld())
	{
		if (AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(W, AIstanbulTrafficManager::StaticClass())))
		{
			TM->BroadcastPlayerHorn(GetActorLocation(), HoldDurationSec);
		}
	}
}

void ATaxiPawn::LegacyDialogueChoice1()
{
	if (PassengerInteraction) PassengerInteraction->SelectDialogueChoice(1);
}

void ATaxiPawn::LegacyDialogueChoice2()
{
	if (PassengerInteraction) PassengerInteraction->SelectDialogueChoice(2);
}

void ATaxiPawn::LegacyDialogueChoice3()
{
	if (PassengerInteraction) PassengerInteraction->SelectDialogueChoice(3);
}

void ATaxiPawn::ResetVehicle()
{
	PlaceholderForwardSpeedCms = 0.0f;
	const FVector NewLoc = GetActorLocation() + FVector(0.0f, 0.0f, ResetLiftHeight);
	const FRotator NewRot(0.0f, GetActorRotation().Yaw, 0.0f);
	SetActorLocationAndRotation(NewLoc, NewRot, false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	UE_LOG(LogIstanbulTaxi, Log, TEXT("Taxi reset."));
}
