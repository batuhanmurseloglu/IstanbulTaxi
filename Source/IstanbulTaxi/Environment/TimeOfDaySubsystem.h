#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Environment/TimeOfDayPresetDataAsset.h"
#include "TimeOfDaySubsystem.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimePhaseChanged, FGameplayTag, NewPhase, FGameplayTag, PreviousPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNewDayStarted, int32, Month, int32, Day);

/**
 * 24-hour world clock & real-latitude (41.01N, 28.97E) solar position solver for Istanbul.
 * Broadcasts 8 named phases as GameplayTags, updates MPC_Environment every tick,
 * and registers with FTaxiGameClock::Provider so M2 tolls and M4 prayer times stay decoupled.
 */
UCLASS()
class ISTANBULTAXI_API UTimeOfDaySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTimeOfDaySubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** Called by ATimeOfDayActor in BeginPlay to register scene lighting actors. */
	void RegisterSceneActors(ADirectionalLight* InSun, ASkyLight* InSkyLight,
		AExponentialHeightFog* InFog, APostProcessVolume* InPostProcess,
		const UTimeOfDayPresetDataAsset* OverridePreset = nullptr);

	// ---- Controls ----
	UFUNCTION(BlueprintCallable, Category = "TimeOfDay")
	void SetTimeOfDayHours(float NewHour);

	UFUNCTION(BlueprintCallable, Category = "TimeOfDay")
	void SetCalendarDate(int32 NewMonth, int32 NewDay);

	UFUNCTION(BlueprintCallable, Category = "TimeOfDay")
	void SetTimeScale(float GameMinutesPerRealSecond) { TimeScaleMinutesPerSec = FMath::Max(0.0f, GameMinutesPerRealSecond); }

	UFUNCTION(BlueprintCallable, Category = "TimeOfDay")
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }

	// ---- Queries ----
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") float GetTimeOfDayHours() const { return CurrentHour; }
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") int32 GetMonth() const { return Month; }
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") int32 GetDay() const { return Day; }
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") int32 GetDayOfYear() const;
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") FGameplayTag GetCurrentPhaseTag() const { return CurrentPhaseTag; }
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") FRotator GetSunRotation() const { return CachedSunRotation; }
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") float GetSunAltitudeDegrees() const { return CachedSunAltitudeDeg; }
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") const FTimeOfDayPhaseSample& GetCurrentSample() const { return CurrentSample; }

	/** Formats CurrentHour as "HH:MM" (24h). */
	UFUNCTION(BlueprintPure, Category = "TimeOfDay") FText GetFormattedClockText() const;

	/** Exact NOAA-style solar altitude & azimuth (degrees, North=0, East=90) for a given DayOfYear and local hour. */
	static void ComputeSolarAngles(int32 DayOfYear, float LocalHour, float LatitudeDeg, float LongitudeDeg,
		float UtcOffsetHours, float& OutAltitudeDeg, float& OutAzimuthDeg);

	static int32 ToDayOfYear(int32 InMonth, int32 InDay);

	UPROPERTY(BlueprintAssignable, Category = "TimeOfDay") FOnTimePhaseChanged OnTimePhaseChanged;
	UPROPERTY(BlueprintAssignable, Category = "TimeOfDay") FOnNewDayStarted OnNewDayStarted;

private:
	void UpdateSolarAndEnvironment();
	void AdvanceCalendarDay();

	UPROPERTY(Transient) TObjectPtr<const UTimeOfDayPresetDataAsset> Preset;
	TWeakObjectPtr<ADirectionalLight> SunLight;
	TWeakObjectPtr<ASkyLight> SkyLight;
	TWeakObjectPtr<AExponentialHeightFog> HeightFog;
	TWeakObjectPtr<APostProcessVolume> PostProcess;

	float CurrentHour = 8.5f;
	float TimeScaleMinutesPerSec = 1.0f;
	int32 Month = 5;
	int32 Day = 15;
	bool bPaused = false;

	FGameplayTag CurrentPhaseTag;
	FTimeOfDayPhaseSample CurrentSample;
	FRotator CachedSunRotation = FRotator(-45.0f, 180.0f, 0.0f);
	float CachedSunAltitudeDeg = 45.0f;
};
