#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Core/IstanbulMeshUtils.h"
#include "IstanbulTrafficManager.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;
class ATrafficVehicle;

/**
 * Canonical Istanbul vehicle visual archetypes (Task 2.1, including 5th ScooterCourier archetype).
 */
UENUM(BlueprintType)
enum class EIstanbulVehicleType : uint8
{
	YellowTaxi     UMETA(DisplayName = "Sarı Taksi (#F5C400)"),
	IETTBus        UMETA(DisplayName = "İETT Belediye Otobüsü (#C8102E / Beyaz)"),
	Dolmus         UMETA(DisplayName = "Sarı Minibüs / Dolmuş (#F2C618)"),
	DeliveryVan    UMETA(DisplayName = "Kargo / Panelvan (Çift Sıra Park)"),
	ScooterCourier UMETA(DisplayName = "Motorlu Kurye (Scooter)")
};

using ETrafficVehicleType = EIstanbulVehicleType;

/**
 * AI driver personality mood used by UHornResponseComponent (Task 2.3).
 */
UENUM(BlueprintType)
enum class EIstanbulDriverMood : uint8
{
	PoliteYielding      UMETA(DisplayName = "Nazik / Yol Veren"),
	StoicNormal         UMETA(DisplayName = "Sakin / Normal"),
	ImpatientHonker     UMETA(DisplayName = "Sabırsız / Kornacı"),
	AggressiveTailgater UMETA(DisplayName = "Agresif / Tampon Takipçisi")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTrafficHornHonked, AActor*, HonkingVehicle, FVector, WorldLocation, FString, ReasonText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTrafficHornResponse, AActor*, ResponderVehicle, bool, bLongAggressiveHorn, FString, ReactionSummary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChokepointEntered, FName, ChokepointId, float, FreeGapWidthM);

/**
 * Data-driven specification for an active Istanbul street chokepoint (Task 2.3).
 */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FIstanbulChokepointSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chokepoint")
	FName ChokepointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chokepoint")
	FVector Location = FVector::ZeroVector;

	/** Remaining drivable gap width in meters (2.4 - 3.0m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chokepoint")
	float FreeGapWidthM = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chokepoint")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chokepoint")
	float WaitDurationSec = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chokepoint")
	float ResetRuleSec = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chokepoint")
	bool bIsGatewayChokepoint = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Chokepoint")
	TArray<TObjectPtr<ATrafficVehicle>> BlockingVehicles;
};

/**
 * Horn reaction component attached to every ATrafficVehicle (Task 2.3).
 * Evaluates driver Patience (0..1) and Mood when the player taps or holds the horn within 25m.
 */
UCLASS(ClassGroup = (IstanbulTraffic), meta = (BlueprintSpawnableComponent))
class ISTANBULTAXI_API UHornResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHornResponseComponent();

	/** Driver patience (0.0 = aggressive/impatient, 1.0 = calm/polite). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Patience = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Personality")
	EIstanbulDriverMood DriverMood = EIstanbulDriverMood::PoliteYielding;

	/** Called when the player honks within 25m. HoldDurationSec < 0.35s = polite tap ("bip"), >= 0.35s = aggressive ("biiip"). */
	bool OnPlayerHornHeard(float HoldDurationSec, const FVector& HornWorldLocation, FString& OutReactionSummary);
};

/**
 * Mid-poly procedural Istanbul AI Traffic Vehicle with 80m LOD swap, license plates,
 * chokepoint yield/nudge AI, and UHornResponseComponent (Task 2.1 - 2.3).
 */
UCLASS()
class ISTANBULTAXI_API ATrafficVehicle : public AActor
{
	GENERATED_BODY()

public:
	ATrafficVehicle();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Shared procedural vehicle mesh builder used by both ATrafficVehicle and ATaxiPawn.
	 * Generates high-detail beveled geometry (hood, cabin, trunk, bumpers, mirrors, wheel arches, license plates)
	 * and a low-poly 80m LOD mesh section.
	 */
	static void BuildIstanbulVehicleMeshSections(EIstanbulVehicleType InType, int32 StyleSeed,
		FIstanbulMeshSectionData& OutHighDetail, FIstanbulMeshSectionData& OutLowLod,
		float& OutLengthCm, float& OutWidthCm);

	/** Builds the mid-poly beveled procedural mesh + 80m LOD mesh for the specified Istanbul vehicle archetype. */
	UFUNCTION(BlueprintCallable, Category = "Traffic|Visuals")
	void BuildVisualMesh(EIstanbulVehicleType InType);

	void InitializeTrafficVehicle(EIstanbulVehicleType InType, const TArray<FVector>& InWaypoints,
		float InCruiseSpeedKmh, float InLateralOffsetCm, bool bInStationaryHazard = false, int32 StartWaypointIdx = 0);

	void SetTargetLateralOffset(float NewLateralOffsetCm) { TargetLateralOffsetCm = NewLateralOffsetCm; }
	float GetTargetLateralOffset() const { return TargetLateralOffsetCm; }

	/** Nudges a stationary or slow blocker aside by PullAsideCm so the player can pass a chokepoint. */
	void NudgeAsideForPlayer(float PullAsideCm = 65.0f, float ForwardNudgeCm = 90.0f);

	void TriggerHorn(const FString& Reason);

	UFUNCTION(BlueprintPure, Category = "Traffic")
	EIstanbulVehicleType GetVehicleType() const { return VehicleType; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	UHornResponseComponent* GetHornResponse() const { return HornResponse; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool IsHonkingNow() const { return HornVisualTimer > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool AreHazardsBlinking() const { return bHazardLightsActive; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	bool IsUsingLowLOD() const { return bUsingLowLOD; }

	UFUNCTION(BlueprintPure, Category = "Traffic")
	FString GetDisplayLabel() const;

	UPROPERTY(BlueprintAssignable, Category = "Traffic")
	FOnTrafficHornHonked OnHornHonked;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** High-detail (< 80m) single-draw-call merged beveled body mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> VehicleProcMesh;

	/** Simplified (> 80m) low-poly box+glass LOD mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> VehicleLodMesh;

	/** Horn reaction & driver patience component (Task 2.3). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHornResponseComponent> HornResponse;

	/** Near-player distance-culled front/rear license plate text ("34 T 1453") and İETT route board text. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> FrontPlateText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> RearPlateText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> RouteSignText;

	/** Rotating & front-steering wheel cylinders (disabled beyond 80m LOD distance). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	/** Blinking hazard lamps toggled by timer for double-parked delivery vans and dolmuş stops. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> HazardLampMeshes;

private:
	void TintMesh(UStaticMeshComponent* Comp, const FLinearColor& Color);
	void UpdateDistanceLOD(float DistToPlayerCm);

	EIstanbulVehicleType VehicleType = EIstanbulVehicleType::YellowTaxi;
	int32 InstanceStyleSeed = 1453;
	TArray<FVector> Waypoints;
	int32 CurrentWaypointIdx = 0;

	float CruiseSpeedCms = 1100.0f;
	float CurrentSpeedCms = 0.0f;
	float CurrentLateralOffsetCm = 0.0f;
	float TargetLateralOffsetCm = 0.0f;
	float CurrentSteerYawDeg = 0.0f;

	bool bStationaryDoubleParked = false;
	bool bHazardLightsActive = false;
	bool bUsingLowLOD = false;
	bool bHasNudgedForPlayer = false;

	// Stationary pull-aside interpolation
	FVector StationaryBaseLocation = FVector::ZeroVector;
	FVector StationaryTargetOffset = FVector::ZeroVector;
	float StationaryResetTimer = 0.0f;

	// Dolmuş ad-hoc stop state
	float AdHocStopTimer = 0.0f;
	float NextAdHocStopCooldown = 14.0f;

	// Chokepoint wait (8-15s), deadlock recovery, and global stuck detector
	float LaneChangeCooldown = 9.0f;
	float BlockedDuration = 0.0f;
	float ChokepointWaitThresholdSec = 9.5f;
	float DeadlockTimer = 0.0f;
	float GlobalStuckTimer = 0.0f;
	FVector LastProgressCheckLoc = FVector::ZeroVector;
	float ReverseEscapeTimer = 0.0f;
	float HornCooldown = 0.0f;
	float HornVisualTimer = 0.0f;
	float HazardBlinkAccum = 0.0f;
	float WheelSpinDeg = 0.0f;
};

/**
 * Coordinates the city's active traffic loops, 4 active chokepoints, horn broadcasts, and debug visualization.
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulTrafficManager : public AActor
{
	GENERATED_BODY()

public:
	AIstanbulTrafficManager();

	virtual void Tick(float DeltaSeconds) override;

	void PopulateIstanbulTraffic(const FVector& Origin, const TArray<FVector>& BoulevardLoopRight,
		const TArray<FVector>& BoulevardLoopLeft, const TArray<FVector>& SideStreetLoop);

	/** Spawns additional live traffic vehicles of the requested type (used by Istanbul.SpawnTraffic). */
	UFUNCTION(BlueprintCallable, Category = "Traffic")
	int32 SpawnExtraTrafficByType(EIstanbulVehicleType Type, int32 Count);

	/** Broadcasts a player horn event (short tap vs. long press) to all traffic vehicles within 25m. */
	UFUNCTION(BlueprintCallable, Category = "Traffic|Horn")
	int32 BroadcastPlayerHorn(const FVector& PlayerLocation, float HoldDurationSec);

	UFUNCTION(BlueprintPure, Category = "Traffic")
	const TArray<ATrafficVehicle*>& GetSpawnedVehicles() const { return Vehicles; }

	UFUNCTION(BlueprintPure, Category = "Traffic|Chokepoints")
	const TArray<FIstanbulChokepointSpec>& GetChokepoints() const { return Chokepoints; }

	void SetChokepointDebugDraw(bool bEnable) { bDebugDrawChokepoints = bEnable; }
	bool IsChokepointDebugDrawEnabled() const { return bDebugDrawChokepoints; }

	UPROPERTY(BlueprintAssignable, Category = "Traffic")
	FOnTrafficHornHonked OnAnyVehicleHonked;

	UPROPERTY(BlueprintAssignable, Category = "Traffic")
	FOnTrafficHornResponse OnHornResponse;

	UPROPERTY(BlueprintAssignable, Category = "Traffic")
	FOnChokepointEntered OnChokepointEntered;

private:
	UFUNCTION()
	void ForwardHornEvent(AActor* HonkingVehicle, FVector WorldLocation, FString ReasonText);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATrafficVehicle>> Vehicles;

	UPROPERTY(EditAnywhere, Category = "Traffic|Chokepoints")
	TArray<FIstanbulChokepointSpec> Chokepoints;

	TArray<FVector> CachedBoulevardRight;
	TArray<FVector> CachedBoulevardLeft;
	TArray<FVector> CachedSideStreetLoop;
	FVector CachedOrigin = FVector::ZeroVector;
	int32 LastEnteredChokepointIdx = INDEX_NONE;
	bool bDebugDrawChokepoints = false;
};
