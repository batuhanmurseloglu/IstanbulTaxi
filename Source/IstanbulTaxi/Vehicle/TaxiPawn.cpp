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
#include "Core/IstanbulTaxiGameplayTags.h"
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

	// --- Chaos setup: Egea-class 1350 kg FWD sedan, comfy and honest ---
	UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	Move->Mass = 1350.0f;
	Move->DragCoefficient = 0.31f;
	Move->ChassisWidth = 179.0f;
	Move->ChassisHeight = 145.0f;
	Move->bEnableCenterOfMassOverride = true;
	Move->CenterOfMassOverride = FVector(15.0f, 0.0f, -10.0f); // slightly nose-heavy, low

	Move->EngineSetup.MaxTorque = 320.0f;     // 1.6 Multijet diesel feel
	Move->EngineSetup.MaxRPM = 4800.0f;
	Move->EngineSetup.EngineIdleRPM = 850.0f;
	Move->EngineSetup.EngineBrakeEffect = 0.15f;
	Move->EngineSetup.EngineRevUpMOI = 5.0f;
	Move->EngineSetup.EngineRevDownRate = 600.0f;

	Move->DifferentialSetup.DifferentialType = EVehicleDifferential::FrontWheelDrive;
	Move->TransmissionSetup.bUseAutomaticGears = true;
	Move->TransmissionSetup.bUseAutoReverse = true;
	Move->TransmissionSetup.GearChangeTime = 0.35f;

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

	PlaceholderChassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderChassis"));
	PlaceholderChassis->SetupAttachment(GetMesh());
	PlaceholderChassis->SetRelativeLocation(FVector(0.0f, 0.0f, 52.0f));
	PlaceholderChassis->SetRelativeScale3D(FVector(4.4f, 1.8f, 0.55f)); // ~4.4m x 1.8m sedan body
	PlaceholderChassis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderChassis->SetStaticMesh(CubeFinder.Object);

	PlaceholderCabin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderCabin"));
	PlaceholderCabin->SetupAttachment(GetMesh());
	PlaceholderCabin->SetRelativeLocation(FVector(-25.0f, 0.0f, 102.0f));
	PlaceholderCabin->SetRelativeScale3D(FVector(2.2f, 1.55f, 0.52f));
	PlaceholderCabin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderCabin->SetStaticMesh(CubeFinder.Object);

	PlaceholderRoofSign = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderRoofSign"));
	PlaceholderRoofSign->SetupAttachment(GetMesh());
	PlaceholderRoofSign->SetRelativeLocation(FVector(-10.0f, 0.0f, 134.0f));
	PlaceholderRoofSign->SetRelativeScale3D(FVector(0.35f, 0.85f, 0.16f));
	PlaceholderRoofSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeFinder.Succeeded()) PlaceholderRoofSign->SetStaticMesh(CubeFinder.Object);

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
		W->SetRelativeScale3D(FVector(0.64f, 0.64f, 0.22f));
		W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CylFinder.Succeeded()) W->SetStaticMesh(CylFinder.Object);
		PlaceholderWheels.Add(W);
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
		if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			auto TintComp = [this, BaseMat](UStaticMeshComponent* Comp, const FLinearColor& Color)
			{
				if (!Comp) return;
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
				MID->SetVectorParameterValue(TEXT("Color"), Color);
				Comp->SetMaterial(0, MID);
			};
			TintComp(PlaceholderChassis,  FLinearColor(0.96f, 0.72f, 0.02f)); // Istanbul Sarı Taksi yellow
			TintComp(PlaceholderCabin,    FLinearColor(0.85f, 0.62f, 0.02f));
			TintComp(PlaceholderRoofSign, FLinearColor(1.00f, 0.92f, 0.15f)); // Bright yellow TAKSİ sign
			for (UStaticMeshComponent* W : PlaceholderWheels)
			{
				TintComp(W, FLinearColor(0.06f, 0.06f, 0.07f)); // Dark rubber tires
			}
		}
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
	// Credible ~1350kg Egea kinematic model with real gravity slope resistance on 12°-18° Cihangir/Galata hills!
	const float AccelCms2    = 680.0f;  // 1.6 Multijet diesel torque
	const float BrakeCms2    = 1550.0f;
	const float ReverseCms2  = 440.0f;
	const float DragPerSec   = 0.42f;
	const float MaxFwdCms    = 3888.0f; // ~140 km/h
	const float MaxRevCms    = -972.0f; // ~35 km/h

	// Gravity component along current road pitch: G * sin(Pitch) -> challenges uphill starts unless Space handbrake or throttle is held!
	const float PitchRad = FMath::DegreesToRadians(GetActorRotation().Pitch);
	const float SlopeGravityAccelCms2 = -980.0f * FMath::Sin(PitchRad) * 0.65f;

	if (bHandbrakeActive)
	{
		// Handbrake holds the car stationary on steep hills (Spacebar hill-start!)
		PlaceholderForwardSpeedCms = FMath::FInterpConstantTo(PlaceholderForwardSpeedCms, 0.0f, DeltaSeconds, BrakeCms2 * 1.4f);
	}
	else if (RawThrottleInput > 0.01f)
	{
		if (PlaceholderForwardSpeedCms < -25.0f)
		{
			PlaceholderForwardSpeedCms = FMath::FInterpConstantTo(PlaceholderForwardSpeedCms, 0.0f, DeltaSeconds, BrakeCms2);
		}
		else
		{
			PlaceholderForwardSpeedCms += (RawThrottleInput * AccelCms2 + SlopeGravityAccelCms2) * DeltaSeconds;
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
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, NewLoc + FVector(0, 0, 220.0f), NewLoc - FVector(0, 0, 320.0f), ECC_Visibility, Params))
	{
		NewLoc.Z = FMath::FInterpTo(GetActorLocation().Z, GroundHit.ImpactPoint.Z + 2.0f, DeltaSeconds, 18.0f);
		const FVector FwdXY = FRotator(0.0f, NewRot.Yaw, 0.0f).Vector();
		GroundSlopePitchDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-FVector::DotProduct(GroundHit.ImpactNormal, FwdXY), -0.5f, 0.5f)));
		if (SurfaceComponent)
		{
			SurfaceComponent->NotifyGroundTraceHit(GroundHit, PlaceholderForwardSpeedCms * 0.036f);
		}
	}
	NewRot.Pitch = FMath::FInterpTo(NewRot.Pitch, GroundSlopePitchDeg, DeltaSeconds, 10.0f);
	NewRot.Roll = 0.0f;

	FHitResult SweepHit;
	SetActorLocationAndRotation(NewLoc, NewRot, true, &SweepHit);
	if (SweepHit.bBlockingHit)
	{
		const float ImpactSpeedKmh = FMath::Abs(PlaceholderForwardSpeedCms) * 0.036f;
		if (ImpactSpeedKmh > 12.0f && PassengerInteraction)
		{
			PassengerInteraction->NotifyHardJoltOrCollision(FMath::Clamp(ImpactSpeedKmh / 65.0f, 0.25f, 1.0f), true);
		}
		// Slide along curbstones/parked cars and damp speed on collision
		PlaceholderForwardSpeedCms *= 0.45f;
	}

	// Set component velocity so UTaximeterComponent and ATollGateVolume read accurate GetVelocity()!
	if (USkeletalMeshComponent* RootSkel = GetMesh())
	{
		RootSkel->ComponentVelocity = GetActorForwardVector() * PlaceholderForwardSpeedCms;
	}

	// Visual body pitch/roll + spinning/steering wheels
	const float LongPitch = (RawBrakeInput - RawThrottleInput) * 1.8f;
	const float LatRoll   = -SmoothedSteer * FMath::Clamp(FMath::Abs(PlaceholderForwardSpeedCms) / 1500.0f, 0.0f, 1.0f) * 3.2f;
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
	TriggerPlayerHorn();
}

void ATaxiPawn::TriggerPlayerHorn()
{
	if (USoundBase* HornCue = LoadObject<USoundBase>(nullptr, TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1")))
	{
		UGameplayStatics::PlaySoundAtLocation(this, HornCue, GetActorLocation(), 1.0f, 0.72f);
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
