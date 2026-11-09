#include "Vehicle/TaxiPawn.h"
#include "IstanbulTaxi.h"
#include "Vehicle/TaxiWheels.h"
#include "Vehicle/TaxiSurfaceComponent.h"
#include "Taximeter/TaximeterComponent.h"
#include "Taximeter/TaximeterDisplayWidget.h"
#include "Missions/PassengerMissionSubsystem.h"
#include "Missions/PassengerInteractionComponent.h"
#include "Environment/TimeOfDaySubsystem.h"
#include "Audio/AudioAmbienceSubsystem.h"
#include "Traffic/IstanbulTrafficManager.h"
#include "Core/IstanbulMeshUtils.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "ProceduralMeshComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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

	// --- Chaos setup: Egea-class 1350 kg sedan tuned for 18° Cihangir/Galata hill-starts ---
	// Physics Verification on 18° grade (tan 18° = 32.5%, sin 18° = 0.309):
	//   Gravity pullback force: F_g = 1350 kg * 9.81 m/s^2 * sin(18°) = 4,092 N
	//   Hold torque at wheel (R = 0.315 m): T_hold = 4,092 * 0.315 = 1,289 Nm
	//   With MaxTorque = 420 Nm, low-end diesel torque curve (0.82 @ 850 RPM),
	//   1st gear = 3.90, FinalRatio = 4.10 -> Wheel launch torque = 5,506 Nm (4.2x T_hold!).
	UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	Move->Mass = 1350.0f;
	Move->DragCoefficient = 0.31f;
	Move->ChassisWidth = 179.0f;
	Move->ChassisHeight = 145.0f;
	Move->bEnableCenterOfMassOverride = true;
	Move->CenterOfMassOverride = FVector(15.0f, 0.0f, -15.0f); // low CoM prevents rear weight-transfer wheelspin on 18°

	Move->EngineSetup.MaxTorque = 420.0f;     // Tuned 1.6 Multijet turbo-diesel torque (Nm) for 18° hill climbs
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

	// Bone names match the UE Vehicle Template skeleton; rename in BP if your mesh differs.
	Move->WheelSetups.SetNum(4);
	Move->WheelSetups[0].WheelClass = UTaxiWheelFront::StaticClass(); Move->WheelSetups[0].BoneName = TEXT("Phys_Wheel_FL");
	Move->WheelSetups[1].WheelClass = UTaxiWheelFront::StaticClass(); Move->WheelSetups[1].BoneName = TEXT("Phys_Wheel_FR");
	Move->WheelSetups[2].WheelClass = UTaxiWheelRear::StaticClass();  Move->WheelSetups[2].BoneName = TEXT("Phys_Wheel_BL");
	Move->WheelSetups[3].WheelClass = UTaxiWheelRear::StaticClass();  Move->WheelSetups[3].BoneName = TEXT("Phys_Wheel_BR");

	// --- Visible Placeholder Meshes (Engine BasicShapes) for immediate zero-asset playability ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	TaxiBodyProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TaxiBodyProcMesh"));
	TaxiBodyProcMesh->SetupAttachment(GetMesh());
	TaxiBodyProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PlaceholderChassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderChassis"));
	PlaceholderChassis->SetupAttachment(GetMesh());
	PlaceholderChassis->SetRelativeLocation(FVector(0.0f, 0.0f, 52.0f));
	PlaceholderChassis->SetRelativeScale3D(FVector(4.45f, 1.80f, 0.54f)); // ~4.45m x 1.80m Egea sedan body
	PlaceholderChassis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderChassis->SetStaticMesh(CubeFinder.Object);

	PlaceholderCabin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderCabin"));
	PlaceholderCabin->SetupAttachment(PlaceholderChassis);
	PlaceholderCabin->SetRelativeLocation(FVector(-5.0f, 0.0f, 88.0f));
	PlaceholderCabin->SetRelativeScale3D(FVector(0.54f, 0.86f, 0.92f));
	PlaceholderCabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderCabin->SetStaticMesh(CubeFinder.Object);

	PlaceholderWindshield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderWindshield"));
	PlaceholderWindshield->SetupAttachment(PlaceholderChassis);
	PlaceholderWindshield->SetRelativeLocation(FVector(-5.0f, 0.0f, 84.0f));
	PlaceholderWindshield->SetRelativeScale3D(FVector(0.57f, 0.89f, 0.64f));
	PlaceholderWindshield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderWindshield->SetStaticMesh(CubeFinder.Object);

	PlaceholderSideBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderSideBand"));
	PlaceholderSideBand->SetupAttachment(PlaceholderChassis);
	PlaceholderSideBand->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	PlaceholderSideBand->SetRelativeScale3D(FVector(1.005f, 1.02f, 0.22f));
	PlaceholderSideBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderSideBand->SetStaticMesh(CubeFinder.Object);

	PlaceholderFrontGrille = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderFrontGrille"));
	PlaceholderFrontGrille->SetupAttachment(PlaceholderChassis);
	PlaceholderFrontGrille->SetRelativeLocation(FVector(50.5f, 0.0f, -6.0f));
	PlaceholderFrontGrille->SetRelativeScale3D(FVector(0.02f, 0.62f, 0.45f));
	PlaceholderFrontGrille->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderFrontGrille->SetStaticMesh(CubeFinder.Object);

	PlaceholderRoofSign = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderRoofSign"));
	PlaceholderRoofSign->SetupAttachment(PlaceholderChassis);
	PlaceholderRoofSign->SetRelativeLocation(FVector(-3.0f, 0.0f, 146.0f));
	PlaceholderRoofSign->SetRelativeScale3D(FVector(0.085f, 0.48f, 0.28f));
	PlaceholderRoofSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderRoofSign->SetStaticMesh(CubeFinder.Object);

	for (int32 i = 0; i < 2; ++i)
	{
		const float SideSign = (i == 0) ? -1.0f : 1.0f;
		UStaticMeshComponent* HL = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderHeadlight_%d"), i));
		HL->SetupAttachment(PlaceholderChassis);
		HL->SetRelativeLocation(FVector(50.6f, SideSign * 36.0f, 8.0f));
		HL->SetRelativeScale3D(FVector(0.02f, 0.16f, 0.26f));
		HL->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CubeFinder.Succeeded()) HL->SetStaticMesh(CubeFinder.Object);
		PlaceholderHeadlights.Add(HL);

		UStaticMeshComponent* TL = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderTaillight_%d"), i));
		TL->SetupAttachment(PlaceholderChassis);
		TL->SetRelativeLocation(FVector(-50.6f, SideSign * 36.0f, 12.0f));
		TL->SetRelativeScale3D(FVector(0.02f, 0.18f, 0.28f));
		TL->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CubeFinder.Succeeded()) TL->SetStaticMesh(CubeFinder.Object);
		PlaceholderTaillights.Add(TL);
	}

	const FVector WheelOffsets[4] = {
		FVector(135.0f, -88.0f, 32.0f), // FL
		FVector(135.0f,  88.0f, 32.0f), // FR
		FVector(-135.0f, -88.0f, 32.0f), // BL
		FVector(-135.0f,  88.0f, 32.0f)  // BR
	};
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderWheel_%d"), i));
		W->SetupAttachment(GetMesh());
		W->SetRelativeLocation(WheelOffsets[i]);
		W->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f)); // Cylinder axis along Y (axle)
		W->SetRelativeScale3D(FVector(0.64f, 0.64f, 0.24f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CylFinder.Succeeded()) W->SetStaticMesh(CylFinder.Object);
		PlaceholderWheels.Add(W);

		UStaticMeshComponent* Hub = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PlaceholderHubcap_%d"), i));
		Hub->SetupAttachment(W);
		Hub->SetRelativeLocation(FVector(0.0f, 0.0f, (i % 2 == 0) ? -52.0f : 52.0f));
		Hub->SetRelativeScale3D(FVector(0.58f, 0.58f, 0.12f));
		Hub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CylFinder.Succeeded()) Hub->SetStaticMesh(CylFinder.Object);
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
	InteriorCamera->SetRelativeLocation(FVector(-10.0f, -36.0f, 112.0f)); // LHD driver's head
	InteriorCamera->FieldOfView = 85.0f;
	InteriorCamera->SetAutoActivate(false);

	// --- Systems ---
	SurfaceComponent = CreateDefaultSubobject<UTaxiSurfaceComponent>(TEXT("SurfaceComponent"));
	Taximeter = CreateDefaultSubobject<UTaximeterComponent>(TEXT("Taximeter"));
	PassengerInteraction = CreateDefaultSubobject<UPassengerInteractionComponent>(TEXT("PassengerInteraction"));

	TaximeterDisplay = CreateDefaultSubobject<UWidgetComponent>(TEXT("TaximeterDisplay"));
	TaximeterDisplay->SetupAttachment(GetMesh());
	// Default: center console / mirror area in front of LHD driver; uses C++ UTaximeterDisplayWidget out of the box.
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

	// If a real SkeletalMesh was assigned in BP_TaxiPawn, hide the BasicShapes placeholder.
	// Otherwise tint the placeholder Sarı Taksi yellow (#F5B800) and enable kinematic/fallback drive.
	const bool bHasSkeletalVehicle = (GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr);
	bUsingPlaceholderDrive = !bHasSkeletalVehicle;

	PlaceholderChassis->SetVisibility(!bHasSkeletalVehicle);
	PlaceholderCabin->SetVisibility(!bHasSkeletalVehicle);
	PlaceholderRoofSign->SetVisibility(!bHasSkeletalVehicle);
	for (UStaticMeshComponent* W : PlaceholderWheels)
	{
		if (W) W->SetVisibility(!bHasSkeletalVehicle);
	}

	if (bUsingPlaceholderDrive)
	{
		// Build the refined beveled YellowTaxi procedural mesh (Task 2.1: hood, raked windshield, cabin, trunk,
		// bumpers, wheel arches, side mirrors, license plates) on TaxiBodyProcMesh!
		if (TaxiBodyProcMesh)
		{
			FIstanbulMeshSectionData HighDetailSection;
			FIstanbulMeshSectionData LodSection;
			float LengthCm = 445.0f, WidthCm = 180.0f;
			ATrafficVehicle::BuildIstanbulVehicleMeshSections(
				EIstanbulVehicleType::YellowTaxi, 1453, HighDetailSection, LodSection, LengthCm, WidthCm);
			HighDetailSection.CommitToComponent(TaxiBodyProcMesh, 0, false, FIstanbulMeshUtils::LoadVertexColorMaterialSafe());
			TaxiBodyProcMesh->SetVisibility(true);
		}
		// Hide the old blocky static-mesh chassis cubes now that TaxiBodyProcMesh renders the beveled sedan body
		if (PlaceholderChassis)     PlaceholderChassis->SetVisibility(false);
		if (PlaceholderCabin)       PlaceholderCabin->SetVisibility(false);
		if (PlaceholderWindshield)  PlaceholderWindshield->SetVisibility(false);
		if (PlaceholderSideBand)    PlaceholderSideBand->SetVisibility(false);
		if (PlaceholderFrontGrille) PlaceholderFrontGrille->SetVisibility(false);
		for (UStaticMeshComponent* HL : PlaceholderHeadlights) if (HL) HL->SetVisibility(false);
		for (UStaticMeshComponent* TL : PlaceholderTaillights) if (TL) TL->SetVisibility(false);

		if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			auto TintComp = [this, BaseMat](UStaticMeshComponent* Comp, const FLinearColor& Color)
			{
				if (!Comp) return;
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
				MID->SetVectorParameterValue(TEXT("Color"), Color);
				Comp->SetMaterial(0, MID);
			};
			TintComp(PlaceholderRoofSign, FLinearColor(1.00f, 0.92f, 0.18f)); // Bright yellow TAKSİ roof sign
			for (UStaticMeshComponent* W  : PlaceholderWheels)  TintComp(W,  FLinearColor(0.05f, 0.05f, 0.06f));
			for (UStaticMeshComponent* H  : PlaceholderHubcaps) TintComp(H,  FLinearColor(0.72f, 0.75f, 0.78f));
		}
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

	// 2. Enhanced Input bindings (if InputAction assets are also assigned on a BP child)
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
	// Realistic 6-speed 1.6 Multijet diesel RPM curve (Idle 850 .. Redline 4600 RPM)
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
	// Credible ~1350kg Egea kinematic model tuned for 12°-18° Cihangir/Galata hills:
	// - At 18° (sin 18° = 0.309), SlopeGravityAccelCms2 = -980 * 0.309 * 0.72 = -218 cm/s^2.
	// - AccelCms2 = 820 cm/s^2 gives +602 cm/s^2 net uphill acceleration at full throttle (never stalls on 18°!).
	// - Handbrake clamps speed to 0.0 with zero creep at any angle up to 25°; releasing handbrake while on throttle
	//   performs a clean, rollback-free hill-start.
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
		// Handbrake holds the car on 18° slopes with zero creep!
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
			// Clean hill-start launch torque when starting from near-zero speed on a steep uphill grade
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

	FRotator NewRot = GetActorRotation();
	NewRot.Yaw += YawRateDegPerSec * DeltaSeconds;

	FVector NewLoc = GetActorLocation() + GetActorForwardVector() * (PlaceholderForwardSpeedCms * DeltaSeconds);

	// Ground snap + road slope pitch + surface detection (Cobblestone / TramRail / Pothole / Asphalt)
	FHitResult GroundHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TaxiGroundSnap), false, this);
	float GroundSlopePitchDeg = 0.0f;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, NewLoc + FVector(0, 0, 260.0f), NewLoc - FVector(0, 0, 360.0f), ECC_Visibility, Params))
	{
		NewLoc.Z = FMath::FInterpTo(GetActorLocation().Z, GroundHit.ImpactPoint.Z + 2.0f, DeltaSeconds, 20.0f);
		const FVector FwdXY = FRotator(0.0f, NewRot.Yaw, 0.0f).Vector();
		GroundSlopePitchDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-FVector::DotProduct(GroundHit.ImpactNormal, FwdXY), -0.6f, 0.6f)));
		if (SurfaceComponent)
		{
			SurfaceComponent->NotifyGroundTraceHit(GroundHit, PlaceholderForwardSpeedCms * 0.036f);
		}
	}
	NewRot.Pitch = FMath::FInterpTo(NewRot.Pitch, GroundSlopePitchDeg, DeltaSeconds, 12.0f);
	NewRot.Roll = 0.0f;
	CurrentSlopeAngleDeg = NewRot.Pitch;

	// Periodic debug log of slope angle under the vehicle when on an incline (> 4 deg) or when DebugSlopeTelemetry is toggled on
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

	if (ScrapeCooldownSec > 0.0f)
	{
		ScrapeCooldownSec = FMath::Max(0.0f, ScrapeCooldownSec - DeltaSeconds);
	}
	if (bHornKeyDown)
	{
		HornHoldDurationSec += DeltaSeconds;
		if (HornHoldDurationSec >= 0.42f && HornHoldDurationSec - DeltaSeconds < 0.42f)
		{
			// Fired once when hold crosses into long aggressive horn ("biiip")
			TriggerPlayerHorn(HornHoldDurationSec);
		}
	}

	FHitResult SweepHit;
	SetActorLocationAndRotation(NewLoc, NewRot, true, &SweepHit);
	if (SweepHit.bBlockingHit)
	{
		const float ImpactSpeedKmh = FMath::Abs(PlaceholderForwardSpeedCms) * 0.036f;
		const float HeadOnDot = FMath::Abs(FVector::DotProduct(SweepHit.ImpactNormal, GetActorForwardVector()));
		const bool bLightSideScrape = (ImpactSpeedKmh < 24.0f || HeadOnDot < 0.55f);

		if (bLightSideScrape)
		{
			// Task 2.3: Light side-scrape in tight chokepoints — gentle speed clamp, scrape sound, small comfort penalty
			PlaceholderForwardSpeedCms *= 0.85f;
			if (ScrapeCooldownSec <= 0.0f)
			{
				ScrapeCooldownSec = 1.6f;
				if (USoundBase* ScrapeCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_grab.VR_grab")))
				{
					UGameplayStatics::PlaySoundAtLocation(this, ScrapeCue, GetActorLocation(), 0.55f, 1.25f);
				}
				OnScrape.Broadcast(SweepHit.GetActor(), ImpactSpeedKmh);
			}
		}
		else
		{
			if (ImpactSpeedKmh > 12.0f && PassengerInteraction)
			{
				PassengerInteraction->NotifyHardJoltOrCollision(FMath::Clamp(ImpactSpeedKmh / 65.0f, 0.25f, 1.0f), true);
			}
			PlaceholderForwardSpeedCms *= 0.45f;
		}
	}

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

	// Visual body pitch/roll + spinning/steering wheels
	const float LongPitch = (RawBrakeInput - RawThrottleInput) * 1.8f;
	const float LatRoll   = -SmoothedSteer * FMath::Clamp(FMath::Abs(PlaceholderForwardSpeedCms) / 1500.0f, 0.0f, 1.0f) * 3.2f;
	if (TaxiBodyProcMesh)   TaxiBodyProcMesh->SetRelativeRotation(FRotator(LongPitch, 0.0f, LatRoll));
	if (PlaceholderChassis) PlaceholderChassis->SetRelativeRotation(FRotator(LongPitch, 0.0f, LatRoll));
	if (PlaceholderCabin)   PlaceholderCabin->SetRelativeRotation(FRotator(LongPitch, 0.0f, LatRoll));

	PlaceholderWheelSpinDeg = FMath::Fmod(PlaceholderWheelSpinDeg + (PlaceholderForwardSpeedCms * DeltaSeconds / 31.5f) * (180.0f / PI), 360.0f);
	for (int32 i = 0; i < PlaceholderWheels.Num(); ++i)
	{
		if (UStaticMeshComponent* W = PlaceholderWheels[i])
		{
			const float WheelYaw = (i < 2) ? (SmoothedSteer * 36.0f) : 0.0f;
			W->SetRelativeRotation(FRotator(PlaceholderWheelSpinDeg, WheelYaw, 90.0f));
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

	// Procedural rumble: vertical dominant, slight lateral, cheap Perlin.
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

	// Road noise MetaSound parameters
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
	ChaseCamera->SetActive(!bInteriorCamera);
	InteriorCamera->SetActive(bInteriorCamera);
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
	bInteriorCamera = !bInteriorCamera;
	ApplyCameraMode();
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
				// Spawn a test ride 30m ahead (pickup) to 180m ahead (drop-off) along the test boulevard
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
	// Immediate short polite tap ("bip")
	TriggerPlayerHorn(0.15f);
}

void ATaxiPawn::LegacyHornReleased()
{
	if (bHornKeyDown && HornHoldDurationSec >= 0.35f)
	{
		// If held long, ensure traffic vehicles received the long-press duration
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
