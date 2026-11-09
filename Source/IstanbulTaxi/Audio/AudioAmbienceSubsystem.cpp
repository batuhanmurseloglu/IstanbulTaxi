#include "Audio/AudioAmbienceSubsystem.h"
#include "Audio/DistrictAmbienceDataAsset.h"
#include "Audio/DistrictAmbienceVolume.h"
#include "Audio/MosqueActor.h"
#include "Environment/TimeOfDaySubsystem.h"
#include "Core/IstanbulTaxiSettings.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "Core/TaxiGameClock.h"
#include "IstanbulTaxi.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UAudioAmbienceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Config = UIstanbulTaxiSettings::Get()->DefaultAudioAmbienceConfig.LoadSynchronous();
	if (!Config)
	{
		Config = GetDefault<UAudioAmbienceConfigDataAsset>();
	}
	ActiveProfile = Config ? Config->DefaultDistrictAmbience.Get() : nullptr;

	RecomputePrayerSchedule(135); // May 15 fallback until UTimeOfDaySubsystem binds on first tick
}

void UAudioAmbienceSubsystem::Deinitialize()
{
	if (AmbienceBedAudio)
	{
		AmbienceBedAudio->Stop();
		AmbienceBedAudio = nullptr;
	}
	Super::Deinitialize();
}

void UAudioAmbienceSubsystem::RegisterMosque(AMosqueActor* Mosque)
{
	if (Mosque) Mosques.AddUnique(Mosque);
}

void UAudioAmbienceSubsystem::UnregisterMosque(AMosqueActor* Mosque)
{
	Mosques.Remove(Mosque);
}

void UAudioAmbienceSubsystem::RegisterWaterfrontSpline(AWaterfrontSplineActor* Spline)
{
	if (Spline) WaterfrontSplines.AddUnique(Spline);
}

void UAudioAmbienceSubsystem::UnregisterWaterfrontSpline(AWaterfrontSplineActor* Spline)
{
	WaterfrontSplines.Remove(Spline);
}

void UAudioAmbienceSubsystem::PushAmbienceVolume(ADistrictAmbienceVolume* Volume)
{
	if (!Volume) return;
	OverlappingVolumes.AddUnique(Volume);
	ResolveActiveDistrictProfile();
}

void UAudioAmbienceSubsystem::PopAmbienceVolume(ADistrictAmbienceVolume* Volume)
{
	OverlappingVolumes.Remove(Volume);
	ResolveActiveDistrictProfile();
}

void UAudioAmbienceSubsystem::ResolveActiveDistrictProfile()
{
	const UDistrictAmbienceDataAsset* Best = Config ? Config->DefaultDistrictAmbience.Get() : nullptr;
	int32 BestPriority = MIN_int32;

	for (int32 i = OverlappingVolumes.Num() - 1; i >= 0; --i)
	{
		const ADistrictAmbienceVolume* V = OverlappingVolumes[i].Get();
		if (!V) { OverlappingVolumes.RemoveAtSwap(i); continue; }
		if (V->AmbienceProfile && V->Priority >= BestPriority)
		{
			BestPriority = V->Priority;
			Best = V->AmbienceProfile;
		}
	}

	if (Best != ActiveProfile)
	{
		ActiveProfile = Best;
		if (AmbienceBedAudio && ActiveProfile && ActiveProfile->AmbienceMetaSound
			&& AmbienceBedAudio->GetSound() != ActiveProfile->AmbienceMetaSound)
		{
			AmbienceBedAudio->SetSound(ActiveProfile->AmbienceMetaSound);
			AmbienceBedAudio->Play();
		}
		OnDistrictAmbienceChanged.Broadcast(ActiveProfile);
	}
}

void UAudioAmbienceSubsystem::SetCabinState(bool bInInteriorCamera, float InWindowOpenRatio)
{
	bInteriorCamera = bInInteriorCamera;
	WindowOpenRatio = FMath::Clamp(InWindowOpenRatio, 0.0f, 1.0f);
}

void UAudioAmbienceSubsystem::HandleNewDay(int32 Month, int32 Day)
{
	TriggeredPrayersToday.Reset();
	RecomputePrayerSchedule(UTimeOfDaySubsystem::ToDayOfYear(Month, Day));
}

void UAudioAmbienceSubsystem::RecomputePrayerSchedule(int32 DayOfYear)
{
	const UIstanbulTaxiSettings* S = UIstanbulTaxiSettings::Get();
	TodaySchedule = UPrayerTimeCalculator::ComputeDailySchedule(DayOfYear, S->Latitude, S->Longitude, S->UtcOffsetHours);
	UE_LOG(LogIstanbulTaxi, Log, TEXT("Diyanet Schedule (DOY %d): Sabah %s | Ogle %s | Ikindi %s | Aksam %s | Yatsi %s"),
		DayOfYear,
		*UPrayerTimeCalculator::FormatPrayerHour(TodaySchedule.Sabah).ToString(),
		*UPrayerTimeCalculator::FormatPrayerHour(TodaySchedule.Ogle).ToString(),
		*UPrayerTimeCalculator::FormatPrayerHour(TodaySchedule.Ikindi).ToString(),
		*UPrayerTimeCalculator::FormatPrayerHour(TodaySchedule.Aksam).ToString(),
		*UPrayerTimeCalculator::FormatPrayerHour(TodaySchedule.Yatsi).ToString());
}

void UAudioAmbienceSubsystem::TriggerEzanNow(FGameplayTag PrayerTag)
{
	USoundBase* DefaultSound = nullptr;
	if (Config)
	{
		if (const TObjectPtr<USoundBase>* Found = Config->DefaultEzanSounds.Find(PrayerTag))
		{
			DefaultSound = *Found;
		}
	}

	int32 Count = 0;
	for (int32 i = Mosques.Num() - 1; i >= 0; --i)
	{
		if (AMosqueActor* M = Mosques[i].Get())
		{
			M->TriggerEzan(PrayerTag, DefaultSound);
			++Count;
		}
		else
		{
			Mosques.RemoveAtSwap(i);
		}
	}

	const float HourNow = FTaxiGameClock::GetHourOfDay(GetWorld());
	OnEzanTriggered.Broadcast(PrayerTag, HourNow);
	UE_LOG(LogIstanbulTaxi, Log, TEXT("Ezan triggered (%s) across %d mosques at %02.2fh."), *PrayerTag.ToString(), Count, HourNow);
}

void UAudioAmbienceSubsystem::CheckPrayerTimes(float PreviousHour, float CurrentHour)
{
	if (PreviousHour < 0.0f || CurrentHour < PreviousHour) return; // first frame or midnight wrap

	TArray<TPair<FGameplayTag, float>> Ezans;
	TodaySchedule.GetEzanTimes(Ezans);

	for (const TPair<FGameplayTag, float>& Pair : Ezans)
	{
		const FGameplayTag& Tag = Pair.Key;
		const float PrayerHour = Pair.Value;
		if (!TriggeredPrayersToday.Contains(Tag) && PreviousHour < PrayerHour && CurrentHour >= PrayerHour)
		{
			TriggeredPrayersToday.Add(Tag);
			TriggerEzanNow(Tag);
		}
	}
}

float UAudioAmbienceSubsystem::ComputeWaterfrontSeagullFactor(const FVector& ListenerLocation) const
{
	if (!Config || WaterfrontSplines.Num() == 0) return 0.0f;

	float MinDistCm = BIG_NUMBER;
	for (const TWeakObjectPtr<AWaterfrontSplineActor>& WeakSpline : WaterfrontSplines)
	{
		if (const AWaterfrontSplineActor* Spline = WeakSpline.Get())
		{
			MinDistCm = FMath::Min(MinDistCm, Spline->GetDistanceToLocationCm(ListenerLocation));
		}
	}

	const float FullCm = Config->SeagullFullIntensityDistanceCm;
	const float FadeCm = FMath::Max(FullCm + 1.0f, Config->SeagullFadeOutDistanceCm);
	return 1.0f - FMath::Clamp((MinDistCm - FullCm) / (FadeCm - FullCm), 0.0f, 1.0f);
}

void UAudioAmbienceSubsystem::Tick(float DeltaTime)
{
	UWorld* W = GetWorld();
	if (!W) return;

	if (!bBoundToTimeSubsystem)
	{
		if (UTimeOfDaySubsystem* TimeSub = W->GetSubsystem<UTimeOfDaySubsystem>())
		{
			TimeSub->OnNewDayStarted.AddDynamic(this, &UAudioAmbienceSubsystem::HandleNewDay);
			RecomputePrayerSchedule(TimeSub->GetDayOfYear());
			bBoundToTimeSubsystem = true;
		}
	}

	const float CurrentHour = FTaxiGameClock::GetHourOfDay(W);
	CheckPrayerTimes(LastCheckedHour, CurrentHour);
	LastCheckedHour = CurrentHour;

	FVector ListenerLoc = FVector::ZeroVector;
	if (const APlayerController* PC = W->GetFirstPlayerController())
	{
		FVector UnusedDir;
		PC->GetAudioListenerPosition(ListenerLoc, UnusedDir, UnusedDir);
	}

	UpdateAmbienceBed(DeltaTime, CurrentHour, ListenerLoc);
	UpdateWindowOcclusion(DeltaTime);
}

void UAudioAmbienceSubsystem::UpdateAmbienceBed(float DeltaTime, float CurrentHour, const FVector& ListenerLocation)
{
	if (!AmbienceBedAudio && ActiveProfile && ActiveProfile->AmbienceMetaSound)
	{
		AmbienceBedAudio = UGameplayStatics::CreateSound2D(GetWorld(), ActiveProfile->AmbienceMetaSound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		if (AmbienceBedAudio) AmbienceBedAudio->Play();
	}

	// Nighttime naturally quiets tea gardens, crowds, and seagulls
	const bool bDeepNight = (CurrentHour < 5.5f || CurrentHour >= 23.5f);
	const float TimeActivityScale = bDeepNight ? 0.2f : 1.0f;

	const float TargetTraffic   = (ActiveProfile ? ActiveProfile->BaseTrafficMurmur : 0.4f) * (bDeepNight ? 0.35f : 1.0f);
	const float TargetTea       = (ActiveProfile ? ActiveProfile->TeaGardenClatter  : 0.1f) * TimeActivityScale;
	const float TargetCrowd     = (ActiveProfile ? ActiveProfile->CrowdChatter      : 0.3f) * TimeActivityScale;
	const float BaseGulls       = (ActiveProfile ? ActiveProfile->BaseSeagullIntensity : 0.05f);
	const float CoastalGulls    = ComputeWaterfrontSeagullFactor(ListenerLocation);
	const float TargetSeagulls  = FMath::Clamp(BaseGulls + CoastalGulls, 0.0f, 1.0f) * (bDeepNight ? 0.1f : 1.0f);

	SmoothedTraffic          = FMath::FInterpTo(SmoothedTraffic, TargetTraffic, DeltaTime, 1.5f);
	SmoothedTeaGarden        = FMath::FInterpTo(SmoothedTeaGarden, TargetTea, DeltaTime, 1.5f);
	SmoothedCrowd            = FMath::FInterpTo(SmoothedCrowd, TargetCrowd, DeltaTime, 1.5f);
	CurrentSeagullIntensity  = FMath::FInterpTo(CurrentSeagullIntensity, TargetSeagulls, DeltaTime, 2.0f);

	if (AmbienceBedAudio && AmbienceBedAudio->IsPlaying())
	{
		AmbienceBedAudio->SetFloatParameter(TEXT("TrafficMurmur"), SmoothedTraffic);
		AmbienceBedAudio->SetFloatParameter(TEXT("TeaGardenClatter"), SmoothedTeaGarden);
		AmbienceBedAudio->SetFloatParameter(TEXT("CrowdChatter"), SmoothedCrowd);
		AmbienceBedAudio->SetFloatParameter(TEXT("SeagullIntensity"), CurrentSeagullIntensity);
		AmbienceBedAudio->SetFloatParameter(TEXT("TimeOfDay"), CurrentHour);
	}
}

void UAudioAmbienceSubsystem::UpdateWindowOcclusion(float DeltaTime)
{
	const float ClosedLPF = Config ? Config->InteriorWindowClosedLPFHz : 950.0f;
	const float OpenLPF   = Config ? Config->ExteriorOrWindowOpenLPFHz : 20000.0f;
	const float ClosedVol = Config ? Config->InteriorWindowClosedVolumeScale : 0.42f;

	// In exterior chase camera, no cabin occlusion is applied (equivalent to window 100% open).
	const float EffectiveOpen = bInteriorCamera ? WindowOpenRatio : 1.0f;

	// Exponential frequency mapping feels natural to human hearing
	const float TargetLPF = FMath::Lerp(ClosedLPF, OpenLPF, FMath::Pow(EffectiveOpen, 2.0f));
	const float TargetVol = FMath::Lerp(ClosedVol, 1.0f, EffectiveOpen);

	CurrentLPFHz = FMath::FInterpTo(CurrentLPFHz, TargetLPF, DeltaTime, 10.0f);
	CurrentExteriorVolumeScale = FMath::FInterpTo(CurrentExteriorVolumeScale, TargetVol, DeltaTime, 10.0f);

	if (AmbienceBedAudio)
	{
		AmbienceBedAudio->SetLowPassFilterEnabled(CurrentLPFHz < 19500.0f);
		AmbienceBedAudio->SetLowPassFilterFrequency(CurrentLPFHz);
		AmbienceBedAudio->SetVolumeMultiplier(CurrentExteriorVolumeScale);
		AmbienceBedAudio->SetFloatParameter(TEXT("WindowOpenRatio"), EffectiveOpen);
		AmbienceBedAudio->SetFloatParameter(TEXT("ExteriorLPFHz"), CurrentLPFHz);
	}

	for (const TWeakObjectPtr<AMosqueActor>& WeakMosque : Mosques)
	{
		if (AMosqueActor* M = WeakMosque.Get())
		{
			M->ApplyExteriorOcclusion(CurrentLPFHz, CurrentExteriorVolumeScale);
		}
	}
}
