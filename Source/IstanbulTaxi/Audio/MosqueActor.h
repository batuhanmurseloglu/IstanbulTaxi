#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "MosqueActor.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class USoundSubmix;

/**
 * Mosque emitter (AMosqueActor). Registers with UAudioAmbienceSubsystem.
 * When a prayer time arrives, starts playing the makam-appropriate ezan after a randomized
 * stagger delay [MinStaggerDelaySeconds, MaxStaggerDelaySeconds] so multiple mosques across
 * Sultanahmet / Üsküdar create an authentic overlapping acoustic canon.
 */
UCLASS()
class ISTANBULTAXI_API AMosqueActor : public AActor
{
	GENERATED_BODY()

public:
	AMosqueActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque")
	FText MosqueName;

	/** Optional per-mosque ezan recordings (overrides UAudioAmbienceConfigDataAsset::DefaultEzanSounds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Audio")
	TMap<FGameplayTag, TObjectPtr<USoundBase>> EzanSoundOverrides;

	/** Fallback ezan sound if neither the map nor the global config has an entry for the prayer tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Audio")
	TObjectPtr<USoundBase> FallbackEzanSound;

	/** Minimum stagger delay (s) before this muezzin starts after the astronomical prayer time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Stagger", meta = (ClampMin = "0"))
	float MinStaggerDelaySeconds = 0.5f;

	/** Maximum stagger delay (s). Imperial mosques (Selatin) can use 0..3s; neighborhood mosques 2..18s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Stagger", meta = (ClampMin = "0"))
	float MaxStaggerDelaySeconds = 14.0f;

	/** Subtle pitch variation (e.g. 0.02 = +/-2%) so two mosques sharing a wave asset never phase-cancel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Stagger", meta = (ClampMin = "0", ClampMax = "0.15"))
	float PitchVariation = 0.02f;

	/** Spatial attenuation asset (long throw across rooftops, distance LPF + reverb send). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Reverb")
	TObjectPtr<USoundAttenuation> EzanAttenuation;

	/** Optional exterior reverb submix (e.g. Bosphorus / stone courtyard tail). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Reverb")
	TObjectPtr<USoundSubmix> ReverbSubmix;

	/** Wet send level (0..1) to ReverbSubmix. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosque|Reverb", meta = (ClampMin = "0", ClampMax = "1"))
	float ReverbSendLevel = 0.65f;

	/** Triggered by UAudioAmbienceSubsystem when a prayer time is reached. */
	UFUNCTION(BlueprintCallable, Category = "Mosque")
	void TriggerEzan(FGameplayTag PrayerTag, USoundBase* DefaultSoundFromSubsystem = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Mosque")
	void StopEzan();

	/** Called by UAudioAmbienceSubsystem to apply window-occlusion LPF/volume to live 3D ezan audio. */
	void ApplyExteriorOcclusion(float CutoffHz, float VolumeScale);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Positioned at the minaret balcony (şerefe) in the viewport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> MinaretAudio;

private:
	void StartEzanPlayback(TObjectPtr<USoundBase> SoundToPlay, FGameplayTag PrayerTag);

	FTimerHandle StaggerTimer;
	float BaseVolume = 1.0f;
};
