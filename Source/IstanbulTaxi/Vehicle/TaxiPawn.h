#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "GameplayTagContainer.h"
#include "TaxiPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAudioComponent;
class UStaticMeshComponent;
class USoundBase;
class UInputMappingContext;
class UInputAction;
class UForceFeedbackEffect;
class UTaxiSurfaceComponent;
class UTaximeterComponent;
class UPassengerInteractionComponent;
class UWidgetComponent;
class UProceduralMeshComponent;
class UChaosWheeledVehicleMovementComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaxiNearMiss, AActor*, OtherVehicle, float, ClearanceCm);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaxiScrape, AActor*, ScrapedActor, float, ScrapeSpeedKmh);

/**
 * The player's taxi: ~1350 kg Egea-class FWD sedan on Chaos, soft suspension,
 * per-wheel surface response, chase + interior cameras.
 *
 * Zero-Asset Prototype Support:
 * If no SkeletalMesh is assigned yet, ATaxiPawn automatically builds a visible yellow taxi
 * placeholder out of Engine BasicShapes (chassis, cabin, roof "TAKSİ" sign, 4 rotating/steering wheels)
 * and falls back to simple physics propulsion when Chaos has no skeletal wheel bones to step.
 */
UCLASS()
class ISTANBULTAXI_API ATaxiPawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	ATaxiPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	UFUNCTION(BlueprintPure, Category = "Taxi")
	float GetSpeedKmh() const;

	UFUNCTION(BlueprintPure, Category = "Taxi")
	float GetEngineRPM() const;

	UFUNCTION(BlueprintPure, Category = "Taxi")
	int32 GetCurrentGear() const;

	/** Signed road pitch angle under the vehicle in degrees (+ = climbing uphill, - = descending). */
	UFUNCTION(BlueprintPure, Category = "Taxi|Slope")
	float GetCurrentSlopeAngleDegrees() const { return CurrentSlopeAngleDeg; }

	UFUNCTION(BlueprintCallable, Category = "Taxi|Slope")
	void SetDebugSlopeTelemetry(bool bEnabled) { bDebugSlopeTelemetry = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Taxi|Slope")
	bool IsDebugSlopeTelemetryEnabled() const { return bDebugSlopeTelemetry; }

	UFUNCTION(BlueprintCallable, Category = "Taxi")
	void TriggerPlayerHorn(float HoldDurationSec = 0.15f);

	UPROPERTY(BlueprintAssignable, Category = "Taxi|Gameplay")
	FOnTaxiNearMiss OnNearMiss;

	UPROPERTY(BlueprintAssignable, Category = "Taxi|Gameplay")
	FOnTaxiScrape OnScrape;

	UFUNCTION(BlueprintPure, Category = "Taxi")
	bool IsInteriorCamera() const { return bInteriorCamera; }

	/** 0 = driver window closed, 1 = fully rolled down. Drives interior/exterior audio occlusion filter. */
	UFUNCTION(BlueprintPure, Category = "Taxi|Audio")
	float GetWindowOpenRatio() const { return WindowOpenRatio; }

	UFUNCTION(BlueprintCallable, Category = "Taxi|Audio")
	void SetWindowOpenRatio(float NewRatio);

	UFUNCTION(BlueprintCallable, Category = "Taxi|Audio")
	void ToggleDriverWindow();

	UFUNCTION(BlueprintCallable, Category = "Taxi")
	void ResetVehicle();

	UFUNCTION(BlueprintPure, Category = "Taxi")
	UTaxiSurfaceComponent* GetSurfaceComponent() const { return SurfaceComponent; }

	UFUNCTION(BlueprintPure, Category = "Taxi")
	UTaximeterComponent* GetTaximeter() const { return Taximeter; }

	UFUNCTION(BlueprintPure, Category = "Taxi")
	UPassengerInteractionComponent* GetPassengerInteraction() const { return PassengerInteraction; }

	UFUNCTION(BlueprintPure, Category = "Taxi")
	UChaosWheeledVehicleMovementComponent* GetChaosMovement() const;

protected:
	virtual void BeginPlay() override;

	// ---- Components ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> ChaseSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> InteriorCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTaxiSurfaceComponent> SurfaceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTaximeterComponent> Taximeter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPassengerInteractionComponent> PassengerInteraction;

	/** Physical interior dashboard/mirror taximeter display (WBP_TaximeterDisplay or C++ fallback). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> TaximeterDisplay;

	/** Looping road/tyre noise MetaSound. Receives Speed (km/h), SurfaceIndex (int), Roughness (0..1), Wetness (0..1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> RoadNoiseAudio;

	// ---- Placeholder Visual Components (automatically hidden when a SkeletalMesh is assigned) ----
	/** Refined beveled Sarı Taksi procedural mesh (Task 2.1, shared with ATrafficVehicle::BuildIstanbulVehicleMeshSections). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TObjectPtr<UProceduralMeshComponent> TaxiBodyProcMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderChassis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderCabin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderWindshield;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderSideBand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderFrontGrille;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderRoofSign;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TArray<TObjectPtr<UStaticMeshComponent>> PlaceholderHeadlights;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TArray<TObjectPtr<UStaticMeshComponent>> PlaceholderTaillights;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TArray<TObjectPtr<UStaticMeshComponent>> PlaceholderWheels;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Placeholder")
	TArray<TObjectPtr<UStaticMeshComponent>> PlaceholderHubcaps;

	// ---- Input ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DrivingMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	int32 DrivingMappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ThrottleAction;        // Axis1D 0..1
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> BrakeAction;           // Axis1D 0..1 (reverse when stopped)
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> SteeringAction;        // Axis1D -1..1
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> HandbrakeAction;       // Bool
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> LookAction;            // Axis2D
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ToggleCameraAction;    // Bool
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> ResetAction;           // Bool
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> TaximeterToggleAction; // Bool (Free <-> Hired)
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> TaximeterPauseAction;  // Bool (Hired <-> Paused)
	UPROPERTY(EditDefaultsOnly, Category = "Input") TObjectPtr<UInputAction> WindowToggleAction;    // Bool (Roll window up/down)

	// ---- Handling tunables ----

	/** Steering input scale at HighSpeedSteerKmh (speed-sensitive steering keeps it calm, not twitchy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Handling", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float HighSpeedSteerScale = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Handling", meta = (ClampMin = "10"))
	float HighSpeedSteerKmh = 110.0f;

	/** How quickly steering follows input (1/s); smooths keyboard steering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Handling", meta = (ClampMin = "0.5", ClampMax = "30"))
	float SteeringInterpSpeed = 6.0f;

	/** Height (cm) to lift the car when resetting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Handling")
	float ResetLiftHeight = 100.0f;

	// ---- Feel tunables ----

	/** Max interior camera shake amplitude (cm) at vibration intensity 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Feel", meta = (ClampMin = "0"))
	float InteriorVibrationAmplitude = 0.6f;

	/** Chase cam gets a fraction of the rumble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float ChaseVibrationScale = 0.25f;

	/** Interior camera jolt (cm) per unit impact severity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Feel", meta = (ClampMin = "0"))
	float ImpactJoltAmplitude = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Taxi|Feel", meta = (ClampMin = "1"))
	float ImpactJoltDecay = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Taxi|Feel")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, Category = "Taxi|Feel")
	TObjectPtr<UForceFeedbackEffect> ImpactForceFeedback;

	/** Interior free-look limits (degrees). */
	UPROPERTY(EditAnywhere, Category = "Taxi|Camera") float InteriorYawLimit = 110.0f;
	UPROPERTY(EditAnywhere, Category = "Taxi|Camera") float InteriorPitchLimit = 45.0f;
	UPROPERTY(EditAnywhere, Category = "Taxi|Camera") bool bStartInInterior = false;

	/** Speed (1/s) at which the driver window rolls up/down when toggled. */
	UPROPERTY(EditAnywhere, Category = "Taxi|Audio", meta = (ClampMin = "0.2", ClampMax = "10.0"))
	float WindowRollSpeed = 1.2f;

private:
	// Enhanced Input callbacks
	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnSteering(const FInputActionValue& Value);
	void OnHandbrakePressed(const FInputActionValue& Value);
	void OnHandbrakeReleased(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnToggleCamera(const FInputActionValue& Value);
	void OnReset(const FInputActionValue& Value);
	void OnTaximeterToggle(const FInputActionValue& Value);
	void OnTaximeterPause(const FInputActionValue& Value);
	void OnWindowToggle(const FInputActionValue& Value);

	// Legacy DefaultInput.ini Axis / Action callbacks (work immediately without InputAction .uassets)
	void LegacyThrottle(float Value);
	void LegacyBrake(float Value);
	void LegacySteering(float Value);
	void LegacyLookYaw(float Value);
	void LegacyLookPitch(float Value);
	void LegacyHandbrakePressed();
	void LegacyHandbrakeReleased();
	void LegacyToggleCamera();
	void LegacyReset();
	void LegacyTaximeterToggle();
	void LegacyTaximeterPause();
	void LegacyCycleTariff();
	void LegacyInteractMission();
	void LegacyWindowToggle();
	void LegacyTriggerEzan();
	void LegacyAdvanceTime();
	void LegacyHornPressed();
	void LegacyHornReleased();
	void LegacyDialogueChoice1();
	void LegacyDialogueChoice2();
	void LegacyDialogueChoice3();

	UFUNCTION()
	void HandleSurfaceImpact(int32 WheelIndex, FGameplayTag Surface, float Severity);

	void ApplyCameraMode();
	void UpdateFeel(float DeltaSeconds);
	void UpdatePlaceholderDriveAndVisuals(float DeltaSeconds);
	void SyncCabinAudioState();

	float RawThrottleInput = 0.0f;
	float RawBrakeInput = 0.0f;
	float RawSteerInput = 0.0f;
	float SmoothedSteer = 0.0f;
	bool bHandbrakeActive = false;
	bool bUsingPlaceholderDrive = false;
	float PlaceholderForwardSpeedCms = 0.0f;
	float PlaceholderWheelSpinDeg = 0.0f;
	float CurrentSlopeAngleDeg = 0.0f;
	float SlopeDebugLogAccumulator = 0.0f;
	bool bDebugSlopeTelemetry = false;
	bool bHornKeyDown = false;
	float HornHoldDurationSec = 0.0f;
	float NearMissCheckTimer = 0.0f;
	float ScrapeCooldownSec = 0.0f;

	bool bInteriorCamera = false;
	float WindowOpenRatio = 0.0f;
	float TargetWindowOpenRatio = 0.0f;
	FVector InteriorCameraBaseLocation = FVector::ZeroVector;
	FRotator InteriorLook = FRotator::ZeroRotator;
	float JoltOffset = 0.0f;
	float FeelTime = 0.0f;
};
