#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Audio/PrayerTimeCalculator.h"
#include "AudioAmbienceSubsystem.generated.h"

class UAudioAmbienceConfigDataAsset;
class UDistrictAmbienceDataAsset;
class ADistrictAmbienceVolume;
class AWaterfrontSplineActor;
class AMosqueActor;
class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEzanTriggered, FGameplayTag, PrayerTag, float, PrayerHour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDistrictAmbienceChanged, const UDistrictAmbienceDataAsset*, NewProfile);

/**
 * Soundscape & Audio Ambience Subsystem (M4 v1):
 *  - Computes daily Diyanet prayer schedule via UPrayerTimeCalculator and triggers AMosqueActor ezan
 *    across the city with staggered start delays and reverb.
 *  - Blends district MetaSound ambience beds (traffic murmur, tea garden clatter, crowd chatter, seagulls).
 *  - Modulates seagull intensity by real-time distance to Bosphorus/Golden Horn shoreline splines.
 *  - Applies interior/exterior window-occlusion LPF and volume attenuation based on camera mode & window open ratio.
 */
UCLASS()
class ISTANBULTAXI_API UAudioAmbienceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAudioAmbienceSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	// ---- Registration (called automatically by world actors) ----
	void RegisterMosque(AMosqueActor* Mosque);
	void UnregisterMosque(AMosqueActor* Mosque);
	void RegisterWaterfrontSpline(AWaterfrontSplineActor* Spline);
	void UnregisterWaterfrontSpline(AWaterfrontSplineActor* Spline);
	void PushAmbienceVolume(ADistrictAmbienceVolume* Volume);
	void PopAmbienceVolume(ADistrictAmbienceVolume* Volume);

	// ---- Controls ----

	/** Called by ATaxiPawn when the driver rolls the window up/down or switches interior/exterior camera. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Occlusion")
	void SetCabinState(bool bInInteriorCamera, float InWindowOpenRatio);

	/** Debug / manual trigger to test the staggered multi-mosque ezan immediately. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Prayer")
	void TriggerEzanNow(FGameplayTag PrayerTag);

	// ---- Queries ----
	UFUNCTION(BlueprintPure, Category = "Audio|Prayer")
	const FDiyanetPrayerSchedule& GetTodayPrayerSchedule() const { return TodaySchedule; }

	UFUNCTION(BlueprintPure, Category = "Audio|Ambience")
	const UDistrictAmbienceDataAsset* GetActiveDistrictProfile() const { return ActiveProfile; }

	UFUNCTION(BlueprintPure, Category = "Audio|Ambience")
	float GetCurrentSeagullIntensity() const { return CurrentSeagullIntensity; }

	UFUNCTION(BlueprintPure, Category = "Audio|Occlusion")
	float GetCurrentExteriorLPFHz() const { return CurrentLPFHz; }

	UPROPERTY(BlueprintAssignable, Category = "Audio|Prayer")
	FOnEzanTriggered OnEzanTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Audio|Ambience")
	FOnDistrictAmbienceChanged OnDistrictAmbienceChanged;

private:
	UFUNCTION()
	void HandleNewDay(int32 Month, int32 Day);

	void RecomputePrayerSchedule(int32 DayOfYear);
	void CheckPrayerTimes(float PreviousHour, float CurrentHour);
	void ResolveActiveDistrictProfile();
	float ComputeWaterfrontSeagullFactor(const FVector& ListenerLocation) const;
	void UpdateAmbienceBed(float DeltaTime, float CurrentHour, const FVector& ListenerLocation);
	void UpdateWindowOcclusion(float DeltaTime);

	UPROPERTY(Transient) TObjectPtr<const UAudioAmbienceConfigDataAsset> Config;
	UPROPERTY(Transient) TObjectPtr<const UDistrictAmbienceDataAsset> ActiveProfile;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> AmbienceBedAudio;

	TArray<TWeakObjectPtr<AMosqueActor>> Mosques;
	TArray<TWeakObjectPtr<AWaterfrontSplineActor>> WaterfrontSplines;
	TArray<TWeakObjectPtr<ADistrictAmbienceVolume>> OverlappingVolumes;

	FDiyanetPrayerSchedule TodaySchedule;
	TSet<FGameplayTag> TriggeredPrayersToday;
	float LastCheckedHour = -1.0f;
	bool bBoundToTimeSubsystem = false;

	// Smoothed layer states
	float SmoothedTraffic = 0.5f;
	float SmoothedTeaGarden = 0.2f;
	float SmoothedCrowd = 0.3f;
	float CurrentSeagullIntensity = 0.1f;

	// Cabin / window occlusion
	bool bInteriorCamera = false;
	float WindowOpenRatio = 0.0f;
	float CurrentLPFHz = 20000.0f;
	float CurrentExteriorVolumeScale = 1.0f;
};
