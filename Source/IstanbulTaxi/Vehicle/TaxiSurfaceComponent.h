#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Vehicle/TaxiSurfaceResponseDataAsset.h"
#include "TaxiSurfaceComponent.generated.h"

class UChaosWheeledVehicleMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWheelSurfaceChanged, int32, WheelIndex, FGameplayTag, NewSurface);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWheelSurfaceImpact, int32, WheelIndex, FGameplayTag, Surface, float, Severity);

/** Runtime state for one wheel (read-only to Blueprints for debug HUDs). */
USTRUCT(BlueprintType)
struct FTaxiWheelSurfaceState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType_Default;

	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	FGameplayTag SurfaceTag;

	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	bool bInContact = false;

	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	float CurrentGrip = 1.0f;

	/** Accumulated suspension wear for this corner, 0 = new, 1 = worn out. Economy reads/writes this (M7). */
	UPROPERTY(BlueprintReadOnly, Category = "Surface")
	float SuspensionWear = 0.0f;

	float BaseFriction = 2.5f;
	float LastImpactTime = -100.0f;
};

/**
 * Per-wheel surface detection for Chaos vehicles.
 * Reads each wheel's contact PhysicalMaterial every physics-ish tick and drives:
 *  - grip (SetWheelFrictionMultiplier, blended with global wetness)
 *  - vibration intensity/frequency (consumed by the pawn for camera/FFB)
 *  - audio surface index (consumed by the pawn's road-noise MetaSound)
 *  - impact events + suspension wear (consumed by comfort score/economy later via delegates)
 */
UCLASS(ClassGroup = (IstanbulTaxi), meta = (BlueprintSpawnableComponent))
class ISTANBULTAXI_API UTaxiSurfaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTaxiSurfaceComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Tunables ----

	/** Overrides Project Settings default if set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	TObjectPtr<UTaxiSurfaceResponseDataAsset> SurfaceResponses;

	/** How fast grip changes when crossing surfaces (1/s). Prevents physics pops on rail edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.5", ClampMax = "50"))
	float GripInterpSpeed = 8.0f;

	/** Speed (km/h) at which vibration reaches full intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "1"))
	float FullVibrationSpeedKmh = 50.0f;

	/** Below this speed (km/h) entering a pothole produces no impact event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0"))
	float MinImpactSpeedKmh = 8.0f;

	/** Speed (km/h) where impact severity hits its surface max. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "1"))
	float FullImpactSpeedKmh = 60.0f;

	/** Per-wheel cooldown between impact events (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0"))
	float ImpactCooldown = 0.35f;

	/** Extra wear per unit impact severity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0"))
	float WearPerImpactSeverity = 0.01f;

	// ---- Runtime API ----

	/** 0..1 global wetness. Set by WeatherSubsystem (M4); exposed for testing now. */
	UFUNCTION(BlueprintCallable, Category = "Surface")
	void SetWetness(float InWetness) { Wetness = FMath::Clamp(InWetness, 0.0f, 1.0f); }

	UFUNCTION(BlueprintPure, Category = "Surface")
	float GetWetness() const { return Wetness; }

	/** Speed-scaled 0..1 rumble, averaged across grounded wheels. */
	UFUNCTION(BlueprintPure, Category = "Surface")
	float GetVibrationIntensity() const { return VibrationIntensity; }

	UFUNCTION(BlueprintPure, Category = "Surface")
	float GetVibrationFrequency() const { return VibrationFrequency; }

	/** Audio index of the roughest surface currently under the car. */
	UFUNCTION(BlueprintPure, Category = "Surface")
	int32 GetDominantAudioSurfaceIndex() const { return DominantAudioIndex; }

	UFUNCTION(BlueprintPure, Category = "Surface")
	FGameplayTag GetDominantSurfaceTag() const { return DominantSurfaceTag; }

	/** Called by ATaxiPawn ground trace when running in zero-asset placeholder drive mode. */
	void NotifyGroundTraceHit(const FHitResult& GroundHit, float SpeedKmh);

	UFUNCTION(BlueprintPure, Category = "Surface")
	const TArray<FTaxiWheelSurfaceState>& GetWheelStates() const { return WheelStates; }

	UFUNCTION(BlueprintPure, Category = "Surface")
	float GetAverageSuspensionWear() const;

	/** For economy repairs / save-load (M7). */
	UFUNCTION(BlueprintCallable, Category = "Surface")
	void SetSuspensionWear(int32 WheelIndex, float Wear);

	UPROPERTY(BlueprintAssignable, Category = "Surface")
	FOnWheelSurfaceChanged OnWheelSurfaceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Surface")
	FOnWheelSurfaceImpact OnWheelSurfaceImpact;

private:
	void CacheWheels();

	UPROPERTY(Transient)
	TObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;

	UPROPERTY(Transient)
	TObjectPtr<const UTaxiSurfaceResponseDataAsset> ActiveResponses;

	UPROPERTY(Transient)
	TArray<FTaxiWheelSurfaceState> WheelStates;

	float Wetness = 0.0f;
	float VibrationIntensity = 0.0f;
	float VibrationFrequency = 10.0f;
	int32 DominantAudioIndex = 0;
	FGameplayTag DominantSurfaceTag;
	float LastTraceImpactTime = -100.0f;
};
