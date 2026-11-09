#include "Vehicle/TaxiSurfaceComponent.h"
#include "IstanbulTaxi.h"
#include "Core/IstanbulTaxiSettings.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UTaxiSurfaceComponent::UTaxiSurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UTaxiSurfaceComponent::BeginPlay()
{
	Super::BeginPlay();

	DominantSurfaceTag = IstanbulTaxiTags::Surface_Asphalt;
	ActiveResponses = SurfaceResponses;
	if (!ActiveResponses)
	{
		ActiveResponses = UIstanbulTaxiSettings::Get()->DefaultSurfaceResponses.LoadSynchronous();
	}
	if (!ActiveResponses)
	{
		// Build a runtime default surface table so surface vibration/impacts work with zero .uasset setup
		UTaxiSurfaceResponseDataAsset* BuiltIn = NewObject<UTaxiSurfaceResponseDataAsset>(this);
		auto AddRow = [BuiltIn](EPhysicalSurface Surf, FGameplayTag Tag, float Grip, float WetGrip, float Vib, float Freq, int32 AudioIdx, float Impact, float Wear)
		{
			FTaxiSurfaceResponse R;
			R.SurfaceTag = Tag;
			R.GripMultiplier = Grip;
			R.WetGripMultiplier = WetGrip;
			R.VibrationIntensity = Vib;
			R.VibrationFrequency = Freq;
			R.AudioSurfaceIndex = AudioIdx;
			R.EntryImpactSeverity = Impact;
			R.SuspensionWearPerKm = Wear;
			BuiltIn->Responses.Add(Surf, R);
			if (Surf == SurfaceType1) BuiltIn->Fallback = R;
		};
		AddRow(SurfaceType1, IstanbulTaxiTags::Surface_Asphalt,        1.00f, 0.78f, 0.02f,  8.0f, 0, 0.00f, 0.0003f);
		AddRow(SurfaceType2, IstanbulTaxiTags::Surface_PatchedAsphalt, 0.95f, 0.72f, 0.16f,  6.5f, 1, 0.00f, 0.0008f);
		AddRow(SurfaceType3, IstanbulTaxiTags::Surface_Cobblestone,    0.85f, 0.55f, 0.65f, 19.0f, 2, 0.12f, 0.0025f);
		AddRow(SurfaceType4, IstanbulTaxiTags::Surface_TramRail,       0.70f, 0.40f, 0.30f,  5.0f, 3, 0.25f, 0.0010f);
		AddRow(SurfaceType5, IstanbulTaxiTags::Surface_Pothole,        0.90f, 0.70f, 0.50f,  6.0f, 4, 0.85f, 0.0020f);
		AddRow(SurfaceType6, IstanbulTaxiTags::Surface_Wet_Asphalt,    0.75f, 0.70f, 0.03f,  8.0f, 5, 0.00f, 0.0003f);
		AddRow(SurfaceType7, IstanbulTaxiTags::Surface_Wet_Cobblestone,0.55f, 0.50f, 0.65f, 19.0f, 6, 0.12f, 0.0025f);
		AddRow(SurfaceType8, IstanbulTaxiTags::Surface_Wet_TramRail,   0.40f, 0.35f, 0.30f,  5.0f, 7, 0.25f, 0.0010f);
		ActiveResponses = BuiltIn;
	}

	CacheWheels();
}

void UTaxiSurfaceComponent::CacheWheels()
{
	Movement = GetOwner() ? GetOwner()->FindComponentByClass<UChaosWheeledVehicleMovementComponent>() : nullptr;
	WheelStates.Reset();
	if (!Movement)
	{
		UE_LOG(LogIstanbulTaxi, Warning, TEXT("%s: TaxiSurfaceComponent needs a ChaosWheeledVehicleMovementComponent."), *GetNameSafe(GetOwner()));
		return;
	}

	WheelStates.SetNum(Movement->WheelSetups.Num());
	for (int32 i = 0; i < WheelStates.Num(); ++i)
	{
		const TSubclassOf<UChaosVehicleWheel> WheelClass = Movement->WheelSetups[i].WheelClass;
		WheelStates[i].BaseFriction = WheelClass ? WheelClass->GetDefaultObject<UChaosVehicleWheel>()->FrictionForceMultiplier : 2.5f;
	}
}

void UTaxiSurfaceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Movement || !ActiveResponses || WheelStates.Num() == 0 || Movement->Wheels.Num() < WheelStates.Num())
	{
		return;
	}

	const float SpeedKmh = FMath::Abs(Movement->GetForwardSpeed()) * 0.036f;
	const float DistanceKm = SpeedKmh * (DeltaTime / 3600.0f);
	const float SpeedVibFactor = FMath::Clamp(SpeedKmh / FullVibrationSpeedKmh, 0.0f, 1.0f);
	const float Now = GetWorld()->GetTimeSeconds();

	float VibSum = 0.0f, FreqSum = 0.0f, RoughestVib = -1.0f;
	int32 Grounded = 0;

	for (int32 i = 0; i < WheelStates.Num(); ++i)
	{
		FTaxiWheelSurfaceState& State = WheelStates[i];
		const FWheelStatus& Status = Movement->GetWheelState(i);

		EPhysicalSurface NewSurface = SurfaceType_Default;
		if (Status.bInContact && Status.PhysMaterial.IsValid())
		{
			NewSurface = Status.PhysMaterial->SurfaceType;
		}
		State.bInContact = Status.bInContact;

		const FTaxiSurfaceResponse& Resp = ActiveResponses->FindResponse(NewSurface);

		// Surface change -> event + possible impact
		if (NewSurface != State.Surface.GetValue() || !State.SurfaceTag.IsValid())
		{
			State.Surface = NewSurface;
			State.SurfaceTag = Resp.SurfaceTag;
			OnWheelSurfaceChanged.Broadcast(i, Resp.SurfaceTag);

			if (Status.bInContact && Resp.EntryImpactSeverity > 0.0f && SpeedKmh >= MinImpactSpeedKmh
				&& (Now - State.LastImpactTime) >= ImpactCooldown)
			{
				const float SpeedScale = FMath::Clamp(SpeedKmh / FullImpactSpeedKmh, 0.0f, 1.0f);
				const float Severity = Resp.EntryImpactSeverity * SpeedScale;
				State.LastImpactTime = Now;
				State.SuspensionWear = FMath::Clamp(State.SuspensionWear + Severity * WearPerImpactSeverity, 0.0f, 1.0f);
				OnWheelSurfaceImpact.Broadcast(i, Resp.SurfaceTag, Severity);
			}
		}

		// Grip (blended toward wet value by global wetness), smoothed to avoid pops
		const float TargetGrip = FMath::Lerp(Resp.GripMultiplier, Resp.WetGripMultiplier, Wetness);
		State.CurrentGrip = FMath::FInterpTo(State.CurrentGrip, TargetGrip, DeltaTime, GripInterpSpeed);
		Movement->SetWheelFrictionMultiplier(i, State.BaseFriction * State.CurrentGrip);

		if (Status.bInContact)
		{
			++Grounded;
			VibSum += Resp.VibrationIntensity;
			FreqSum += Resp.VibrationFrequency;
			if (Resp.VibrationIntensity > RoughestVib)
			{
				RoughestVib = Resp.VibrationIntensity;
				DominantAudioIndex = Resp.AudioSurfaceIndex;
				DominantSurfaceTag = Resp.SurfaceTag;
			}
			State.SuspensionWear = FMath::Clamp(State.SuspensionWear + Resp.SuspensionWearPerKm * DistanceKm, 0.0f, 1.0f);
		}
	}

	VibrationIntensity = Grounded > 0 ? (VibSum / Grounded) * SpeedVibFactor : 0.0f;
	VibrationFrequency = Grounded > 0 ? (FreqSum / Grounded) * FMath::Lerp(0.5f, 1.0f, SpeedVibFactor) : VibrationFrequency;
}

void UTaxiSurfaceComponent::NotifyGroundTraceHit(const FHitResult& GroundHit, float SpeedKmh)
{
	if (!ActiveResponses) return;

	EPhysicalSurface Detected = SurfaceType1; // Asphalt default
	if (GroundHit.PhysMaterial.IsValid())
	{
		Detected = GroundHit.PhysMaterial->SurfaceType;
	}
	if (const AActor* HitActor = GroundHit.GetActor())
	{
		if (HitActor->ActorHasTag(TEXT("Surface_Cobblestone")))        Detected = SurfaceType3;
		else if (HitActor->ActorHasTag(TEXT("Surface_TramRail")))      Detected = SurfaceType4;
		else if (HitActor->ActorHasTag(TEXT("Surface_Pothole")))       Detected = SurfaceType5;
		else if (HitActor->ActorHasTag(TEXT("Surface_PatchedAsphalt")))Detected = SurfaceType2;
	}

	const FTaxiSurfaceResponse& Resp = ActiveResponses->FindResponse(Detected);
	const float SpeedVibFactor = FMath::Clamp(FMath::Abs(SpeedKmh) / FullVibrationSpeedKmh, 0.0f, 1.0f);

	if (Resp.SurfaceTag != DominantSurfaceTag)
	{
		DominantSurfaceTag = Resp.SurfaceTag;
		OnWheelSurfaceChanged.Broadcast(0, DominantSurfaceTag);

		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (Resp.EntryImpactSeverity > 0.0f && FMath::Abs(SpeedKmh) >= MinImpactSpeedKmh
			&& (Now - LastTraceImpactTime) >= ImpactCooldown)
		{
			LastTraceImpactTime = Now;
			const float Severity = Resp.EntryImpactSeverity * FMath::Clamp(FMath::Abs(SpeedKmh) / FullImpactSpeedKmh, 0.0f, 1.0f);
			OnWheelSurfaceImpact.Broadcast(0, DominantSurfaceTag, Severity);
		}
	}

	VibrationIntensity = Resp.VibrationIntensity * SpeedVibFactor;
	VibrationFrequency = Resp.VibrationFrequency * FMath::Lerp(0.5f, 1.0f, SpeedVibFactor);
	DominantAudioIndex = Resp.AudioSurfaceIndex;
}

float UTaxiSurfaceComponent::GetAverageSuspensionWear() const
{
	if (WheelStates.Num() == 0) return 0.0f;
	float Sum = 0.0f;
	for (const FTaxiWheelSurfaceState& S : WheelStates) Sum += S.SuspensionWear;
	return Sum / WheelStates.Num();
}

void UTaxiSurfaceComponent::SetSuspensionWear(int32 WheelIndex, float Wear)
{
	if (WheelStates.IsValidIndex(WheelIndex))
	{
		WheelStates[WheelIndex].SuspensionWear = FMath::Clamp(Wear, 0.0f, 1.0f);
	}
}
