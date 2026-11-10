// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#include "Environment/DestructibleStreetProp.h"
#include "Core/IstanbulMeshUtils.h"
#include "Core/IstanbulTaxiHUD.h"
#include "Missions/PassengerInteractionComponent.h"
#include "Traffic/IstanbulTrafficManager.h"
#include "Vehicle/TaxiPawn.h"
#include "IstanbulTaxi.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

ADestructibleStreetProp::ADestructibleStreetProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->InitBoxExtent(FVector(22.0f, 22.0f, 180.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionBox->SetNotifyRigidBodyCollision(true);
	CollisionBox->bDynamicObstacle = true;

	PropMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PropMesh"));
	PropMesh->SetupAttachment(CollisionBox);
	PropMesh->bUseComplexAsSimpleCollision = false;
	PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PropMesh->SetCastShadow(true);

	StreetLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("StreetLightComponent"));
	StreetLightComponent->SetupAttachment(CollisionBox);
	StreetLightComponent->SetRelativeLocation(FVector(85.0f, 0.0f, 595.0f));
	StreetLightComponent->SetIntensity(4200.0f);
	StreetLightComponent->SetAttenuationRadius(1850.0f);
	StreetLightComponent->SetLightColor(FLinearColor(1.0f, 0.82f, 0.52f));
	StreetLightComponent->SetCastShadows(false);

	SignTextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignTextComponent"));
	SignTextComponent->SetupAttachment(CollisionBox);
	SignTextComponent->SetHorizontalAlignment(EHTA_Center);
	SignTextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	SignTextComponent->SetWorldSize(18.0f);
	SignTextComponent->SetTextRenderColor(FColor::White);
	SignTextComponent->SetVisibility(false);
	SignTextComponent->LDMaxDrawDistance = 7500.0f;
}

void ADestructibleStreetProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildPropMesh();
}

void ADestructibleStreetProp::BeginPlay()
{
	Super::BeginPlay();
	InitialWorldLocation = GetActorLocation();
	InitialWorldRotation = GetActorRotation();

	if (CollisionBox)
	{
		CollisionBox->OnComponentHit.RemoveDynamic(this, &ADestructibleStreetProp::OnPropCollisionHit);
		CollisionBox->OnComponentHit.AddDynamic(this, &ADestructibleStreetProp::OnPropCollisionHit);
	}
	RebuildPropMesh();
}

void ADestructibleStreetProp::InitializeProp(EStreetPropType InType, bool bEnableLight, const FString& InSignText)
{
	PropType = InType;
	bEmitStreetLight = bEnableLight;
	SignLabelText = InSignText;
	InitialWorldLocation = GetActorLocation();
	InitialWorldRotation = GetActorRotation();
	RebuildPropMesh();
}

FString ADestructibleStreetProp::GetTurkishPropName() const
{
	switch (PropType)
	{
	case EStreetPropType::StreetLightPole: return TEXT("Sokak Lambası Direği");
	case EStreetPropType::StripedBollard:  return TEXT("Sarı-Siyah Duba");
	case EStreetPropType::TrafficSign:     return TEXT("Trafik Tabelası");
	case EStreetPropType::TrashBin:        return TEXT("Çöp Konteyneri");
	default:                               return TEXT("Sokak Direği");
	}
}

void ADestructibleStreetProp::RebuildPropMesh()
{
	if (!PropMesh || !CollisionBox)
	{
		return;
	}

	PropMesh->ClearAllMeshSections();

	FIstanbulMeshSectionData MetalSection;   // Slot 0: Metallic pole / cast iron
	FIstanbulMeshSectionData PaintedSection; // Slot 1: Painted body / stripes / sign face
	FIstanbulMeshSectionData TrimSection;    // Slot 2: Rubber / base collar
	FIstanbulMeshSectionData GlowSection;    // Slot 3: Emissive lamp / reflector strips

	const FLinearColor CastIronCol(0.14f, 0.16f, 0.18f, 1.0f);
	const FLinearColor GalvanizedCol(0.56f, 0.59f, 0.63f, 1.0f);
	const FLinearColor DarkBaseCol(0.06f, 0.06f, 0.07f, 1.0f);
	const FLinearColor WarmLampGlow(1.0f, 0.88f, 0.52f, 1.0f);

	if (PropType == EStreetPropType::StreetLightPole)
	{
		CollisionBox->SetBoxExtent(FVector(24.0f, 24.0f, 300.0f));
		PropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -300.0f));

		// Fluted cast-iron octagonal base pedestal + decorative collar rings
		FIstanbulMeshUtils::AppendCone(MetalSection, FTransform(FVector(0.0f, 0.0f, 48.0f)),
			20.0f, 13.0f, 48.0f, 10, CastIronCol, true);
		FIstanbulMeshUtils::AppendCylinder(TrimSection, FTransform(FVector(0.0f, 0.0f, 98.0f)),
			15.5f, 6.0f, 10, DarkBaseCol, true);

		// Tapered main steel shaft rising to 5.8m
		FIstanbulMeshUtils::AppendCone(MetalSection, FTransform(FVector(0.0f, 0.0f, 340.0f)),
			11.5f, 7.5f, 240.0f, 10, CastIronCol * 1.15f, false);

		// Curved overhanging luminaire mast arm + decorative scroll bracket
		FIstanbulMeshUtils::AppendBeveledBox(MetalSection,
			FTransform(FRotator(16.0f, 0.0f, 0.0f), FVector(46.0f, 0.0f, 592.0f)),
			FVector(52.0f, 4.5f, 4.5f), 1.2f, CastIronCol * 1.15f);
		FIstanbulMeshUtils::AppendBeveledBox(MetalSection,
			FTransform(FRotator(34.0f, 0.0f, 0.0f), FVector(26.0f, 0.0f, 566.0f)),
			FVector(28.0f, 3.0f, 3.0f), 0.8f, CastIronCol);

		// Cobra-head / Historic Istanbul Lantern Housing + Glowing Sodium/LED Lens
		FIstanbulMeshUtils::AppendBeveledBox(MetalSection,
			FTransform(FVector(92.0f, 0.0f, 608.0f)),
			FVector(22.0f, 14.0f, 6.5f), 2.5f, CastIronCol);
		FIstanbulMeshUtils::AppendBeveledBox(GlowSection,
			FTransform(FVector(92.0f, 0.0f, 600.5f)),
			FVector(18.0f, 11.5f, 2.8f), 0.8f, WarmLampGlow);

		// Yellow-black reflective hazard band near bumper height
		FIstanbulMeshUtils::AppendCylinder(GlowSection, FTransform(FVector(0.0f, 0.0f, 135.0f)),
			11.6f, 12.0f, 10, FLinearColor(0.96f, 0.76f, 0.04f), false);

		if (StreetLightComponent)
		{
			StreetLightComponent->SetRelativeLocation(FVector(92.0f, 0.0f, 285.0f));
			StreetLightComponent->SetVisibility(bEmitStreetLight);
			StreetLightComponent->SetIntensity(bEmitStreetLight ? InitialLightIntensity : 0.0f);
		}
		if (SignTextComponent)
		{
			SignTextComponent->SetVisibility(false);
		}
	}
	else if (PropType == EStreetPropType::StripedBollard)
	{
		CollisionBox->SetBoxExtent(FVector(18.0f, 18.0f, 48.0f));
		PropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -48.0f));

		// Rubber floor flange + yellow/black striped bollard cylinder + reflective red top collar
		FIstanbulMeshUtils::AppendCylinder(TrimSection, FTransform(FVector(0.0f, 0.0f, 4.0f)),
			17.0f, 4.0f, 12, DarkBaseCol, true);
		FIstanbulMeshUtils::AppendStripedBollard(GlowSection, FVector(0.0f, 0.0f, 6.0f), 12.0f, 86.0f, 12);
		FIstanbulMeshUtils::AppendCylinder(GlowSection, FTransform(FVector(0.0f, 0.0f, 82.0f)),
			12.5f, 5.0f, 12, FLinearColor(0.95f, 0.12f, 0.08f), true);

		if (StreetLightComponent) StreetLightComponent->SetVisibility(false);
		if (SignTextComponent) SignTextComponent->SetVisibility(false);
	}
	else if (PropType == EStreetPropType::TrafficSign)
	{
		CollisionBox->SetBoxExtent(FVector(20.0f, 20.0f, 145.0f));
		PropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -145.0f));

		// Galvanized steel post + concrete base collar
		FIstanbulMeshUtils::AppendCylinder(TrimSection, FTransform(FVector(0.0f, 0.0f, 10.0f)),
			14.0f, 10.0f, 10, DarkBaseCol, true);
		FIstanbulMeshUtils::AppendCylinder(MetalSection, FTransform(FVector(0.0f, 0.0f, 135.0f)),
			4.5f, 135.0f, 10, GalvanizedCol, true);

		// Circular/Rectangular Turkish Traffic Sign Board at Z = 245cm
		const FLinearColor SignBorderRed(0.86f, 0.08f, 0.08f, 1.0f);
		const FLinearColor SignFaceBlue(0.05f, 0.28f, 0.78f, 1.0f);
		const FLinearColor SignFaceWhite(0.94f, 0.94f, 0.94f, 1.0f);

		FIstanbulMeshUtils::AppendBeveledBox(PaintedSection, FTransform(FVector(5.5f, 0.0f, 245.0f)),
			FVector(2.5f, 38.0f, 38.0f), 4.0f, SignBorderRed);
		FIstanbulMeshUtils::AppendBeveledBox(GlowSection, FTransform(FVector(7.2f, 0.0f, 245.0f)),
			FVector(1.2f, 30.0f, 30.0f), 2.5f, SignLabelText.IsEmpty() ? SignFaceWhite : SignFaceBlue);

		if (StreetLightComponent) StreetLightComponent->SetVisibility(false);
		if (SignTextComponent)
		{
			SignTextComponent->SetRelativeLocation(FVector(10.0f, 0.0f, 100.0f));
			SignTextComponent->SetRelativeRotation(FRotator::ZeroRotator);
			SignTextComponent->SetText(FText::FromString(SignLabelText.IsEmpty() ? TEXT("30") : SignLabelText));
			SignTextComponent->SetTextRenderColor(SignLabelText.IsEmpty() ? FColor(20, 20, 20) : FColor::White);
			SignTextComponent->SetVisibility(true);
		}
	}
	else // TrashBin (Belediye Çöp Konteyneri)
	{
		CollisionBox->SetBoxExtent(FVector(36.0f, 48.0f, 56.0f));
		PropMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -56.0f));

		const FLinearColor BinGreen(0.10f, 0.34f, 0.18f, 1.0f);
		FIstanbulMeshUtils::AppendBeveledBox(PaintedSection, FTransform(FVector(0.0f, 0.0f, 54.0f)),
			FVector(32.0f, 44.0f, 44.0f), 4.0f, BinGreen);
		// Curved dark plastic lid + reflective yellow municipal stripe
		FIstanbulMeshUtils::AppendBeveledBox(TrimSection, FTransform(FVector(0.0f, 0.0f, 102.0f)),
			FVector(34.0f, 46.0f, 6.0f), 2.5f, DarkBaseCol);
		FIstanbulMeshUtils::AppendBeveledBox(GlowSection, FTransform(FVector(33.0f, 0.0f, 62.0f)),
			FVector(1.0f, 36.0f, 6.0f), 0.4f, FLinearColor(0.95f, 0.76f, 0.05f));

		if (StreetLightComponent) StreetLightComponent->SetVisibility(false);
		if (SignTextComponent) SignTextComponent->SetVisibility(false);
	}

	UMaterialInterface* VtxMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	MetalSection.CommitToComponent(PropMesh, 0, false, VtxMat);
	PaintedSection.CommitToComponent(PropMesh, 1, false, VtxMat);
	TrimSection.CommitToComponent(PropMesh, 2, false, VtxMat);
	GlowSection.CommitToComponent(PropMesh, 3, false, VtxMat);
}

void ADestructibleStreetProp::OnPropCollisionHit(
	UPrimitiveComponent* /*HitComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (bIsKnockedOver || !OtherActor || OtherActor == this)
	{
		return;
	}

	FVector ImpactVel = OtherActor->GetVelocity();
	if (const ATaxiPawn* Taxi = Cast<ATaxiPawn>(OtherActor))
	{
		ImpactVel = Taxi->GetActorForwardVector() * (Taxi->GetSpeedKmh() / 0.036f);
	}
	else if (ImpactVel.IsNearlyZero() && !NormalImpulse.IsNearlyZero())
	{
		ImpactVel = NormalImpulse.GetSafeNormal() * 600.0f;
	}

	TriggerKnockOver(Hit.ImpactPoint, ImpactVel, OtherActor);
}

bool ADestructibleStreetProp::TriggerKnockOver(const FVector& HitLocation, const FVector& ImpactVelocity, AActor* InstigatorActor)
{
	if (bIsKnockedOver)
	{
		return false;
	}

	const float ImpactSpeedKmh = ImpactVelocity.Size() * 0.036f;
	if (ImpactSpeedKmh < KnockOverSpeedThresholdKmh)
	{
		return false;
	}

	bIsKnockedOver = true;
	KnockOverElapsedSeconds = 0.0f;
	InitialWorldLocation = GetActorLocation();
	InitialWorldRotation = GetActorRotation();

	// Compute push direction away from impactor
	FVector PushDir2D = FVector(ImpactVelocity.X, ImpactVelocity.Y, 0.0f).GetSafeNormal();
	if (PushDir2D.IsNearlyZero() && InstigatorActor)
	{
		PushDir2D = (GetActorLocation() - InstigatorActor->GetActorLocation());
		PushDir2D.Z = 0.0f;
		PushDir2D.Normalize();
	}
	if (PushDir2D.IsNearlyZero())
	{
		PushDir2D = GetActorForwardVector();
	}

	// Rotation axis is perpendicular to push direction in the horizontal plane: Cross(Up, PushDir2D)
	FallAxisWorld = FVector::CrossProduct(FVector::UpVector, PushDir2D).GetSafeNormal();
	if (FallAxisWorld.IsNearlyZero())
	{
		FallAxisWorld = FVector::RightVector;
	}

	CurrentTiltDegrees = 0.0f;
	TiltAngularVelocityDegPerSec = FMath::Clamp(ImpactSpeedKmh * 5.2f, 110.0f, 340.0f);
	SlideVelocityWorld = PushDir2D * FMath::Clamp(ImpactVelocity.Size() * 0.32f, 140.0f, 520.0f);

	// Disable blocking collision once knocked over so the fallen prop never traps the vehicle chassis
	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Append bright electrical/impact spark burst quads to Slot 3
	if (PropMesh)
	{
		FIstanbulMeshSectionData SparkBurstSection;
		const FLinearColor SparkOrange(1.0f, 0.65f, 0.08f, 1.0f);
		const FLinearColor SparkWhite(1.0f, 0.96f, 0.75f, 1.0f);
		for (int32 s = 0; s < 8; ++s)
		{
			const float Angle = s * 45.0f;
			const FRotator RayRot(25.0f + (s % 3) * 18.0f, Angle, 0.0f);
			FIstanbulMeshUtils::AppendBeveledBox(SparkBurstSection,
				FTransform(RayRot, FVector(0.0f, 0.0f, 65.0f)),
				FVector(24.0f, 2.2f, 2.2f), 0.4f, (s % 2 == 0) ? SparkWhite : SparkOrange);
		}
		SparkBurstSection.CommitToComponent(PropMesh, 4, false, FIstanbulMeshUtils::LoadVertexColorMaterialSafe());
	}

	SetActorTickEnabled(true);

	// Notify nearby AI traffic to honk in alarm
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AIstanbulTrafficManager> It(World); It; ++It)
		{
			if (AIstanbulTrafficManager* TrafficMgr = *It)
			{
				TrafficMgr->BroadcastPlayerHorn(GetActorLocation(), 0.35f);
				break;
			}
		}

		// If the player taxi knocked it over, trigger passenger panic line + HUD warning toast
		if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(InstigatorActor))
		{
			if (UPassengerInteractionComponent* Passenger = Taxi->GetPassengerInteraction())
			{
				Passenger->NotifyStreetPropKnockedOver(GetTurkishPropName());
			}
			if (APlayerController* PC = Cast<APlayerController>(Taxi->GetController()))
			{
				if (AIstanbulTaxiHUD* TaxiHUD = Cast<AIstanbulTaxiHUD>(PC->GetHUD()))
				{
					TaxiHUD->PushToast(
						FString::Printf(TEXT("DİKKAT: %s devrildi! (Aman kaptan direği devirdin!)"), *GetTurkishPropName()),
						FLinearColor(1.0f, 0.42f, 0.12f), 4.0f);
				}
			}
		}
	}

	UE_LOG(LogIstanbulTaxi, Log, TEXT("ADestructibleStreetProp '%s' (%s) knocked over at %.1f km/h by %s"),
		*GetName(), *GetTurkishPropName(), ImpactSpeedKmh, *GetNameSafe(InstigatorActor));

	return true;
}

void ADestructibleStreetProp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsKnockedOver)
	{
		SetActorTickEnabled(false);
		return;
	}

	KnockOverElapsedSeconds += DeltaTime;

	// 1. Flicker & extinguish streetlight over 0.85s
	if (StreetLightComponent && StreetLightComponent->IsVisible())
	{
		if (KnockOverElapsedSeconds < 0.85f)
		{
			const float Flicker = (FMath::Sin(KnockOverElapsedSeconds * 58.0f) > 0.0f) ? 1.35f : 0.12f;
			StreetLightComponent->SetIntensity(InitialLightIntensity * Flicker * (1.0f - KnockOverElapsedSeconds / 0.85f));
		}
		else
		{
			StreetLightComponent->SetIntensity(0.0f);
			StreetLightComponent->SetVisibility(false);
		}
	}

	// Hide impact spark burst after 0.35s
	if (PropMesh && KnockOverElapsedSeconds > 0.35f)
	{
		PropMesh->SetMeshSectionVisible(4, false);
	}

	// 2. Integrate tip-over angular physics with gravity acceleration + ground bounce at 88 degrees
	TiltAngularVelocityDegPerSec += 280.0f * DeltaTime;
	CurrentTiltDegrees += TiltAngularVelocityDegPerSec * DeltaTime;

	if (CurrentTiltDegrees >= 88.0f)
	{
		CurrentTiltDegrees = 88.0f;
		if (FMath::Abs(TiltAngularVelocityDegPerSec) > 35.0f)
		{
			TiltAngularVelocityDegPerSec = -TiltAngularVelocityDegPerSec * 0.28f;
		}
		else
		{
			TiltAngularVelocityDegPerSec = 0.0f;
		}
	}

	// Slide displacement with ground friction
	if (!SlideVelocityWorld.IsNearlyZero(5.0f))
	{
		AddActorWorldOffset(SlideVelocityWorld * DeltaTime, false);
		SlideVelocityWorld = FMath::VInterpTo(SlideVelocityWorld, FVector::ZeroVector, DeltaTime, 4.5f);
	}

	// Rotate around base pivot
	const FQuat BaseQuat = InitialWorldRotation.Quaternion();
	const FQuat TiltQuat(FallAxisWorld, FMath::DegreesToRadians(CurrentTiltDegrees));
	SetActorRotation((TiltQuat * BaseQuat).Rotator());

	if (KnockOverElapsedSeconds > 2.2f && FMath::IsNearlyZero(TiltAngularVelocityDegPerSec, 1.0f))
	{
		SetActorTickEnabled(false);
	}
}
