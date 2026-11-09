#include "Audio/MosqueActor.h"
#include "Audio/AudioAmbienceSubsystem.h"
#include "IstanbulTaxi.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundSubmix.h"
#include "TimerManager.h"

AMosqueActor::AMosqueActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	MinaretAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MinaretAudio"));
	MinaretAudio->SetupAttachment(Root);
	MinaretAudio->SetRelativeLocation(FVector(0.0f, 0.0f, 3200.0f)); // ~32m up on the şerefe by default
	MinaretAudio->bAutoActivate = false;
	MinaretAudio->bAllowSpatialization = true;
	MinaretAudio->bOverrideAttenuation = true;
	// Sensible long-throw defaults if no EzanAttenuation asset is assigned yet:
	MinaretAudio->AttenuationOverrides.bAttenuate = true;
	MinaretAudio->AttenuationOverrides.bSpatialize = true;
	MinaretAudio->AttenuationOverrides.FalloffDistance = 85000.0f; // 850 m audible radius across the district
	MinaretAudio->AttenuationOverrides.bAttenuateWithLPF = true;
	MinaretAudio->AttenuationOverrides.LPFRadiusMin = 5000.0f;
	MinaretAudio->AttenuationOverrides.LPFRadiusMax = 80000.0f;
	MinaretAudio->AttenuationOverrides.LPFFrequencyAtMin = 18000.0f;
	MinaretAudio->AttenuationOverrides.LPFFrequencyAtMax = 1400.0f;
}

void AMosqueActor::BeginPlay()
{
	Super::BeginPlay();

	if (EzanAttenuation)
	{
		MinaretAudio->bOverrideAttenuation = false;
		MinaretAudio->AttenuationSettings = EzanAttenuation;
	}
	if (ReverbSubmix)
	{
		MinaretAudio->SetSubmixSend(ReverbSubmix, ReverbSendLevel);
	}

	if (UAudioAmbienceSubsystem* Sub = GetWorld()->GetSubsystem<UAudioAmbienceSubsystem>())
	{
		Sub->RegisterMosque(this);
	}
}

void AMosqueActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StaggerTimer);
		if (UAudioAmbienceSubsystem* Sub = W->GetSubsystem<UAudioAmbienceSubsystem>())
		{
			Sub->UnregisterMosque(this);
		}
	}
	Super::EndPlay(Reason);
}

void AMosqueActor::TriggerEzan(FGameplayTag PrayerTag, USoundBase* DefaultSoundFromSubsystem)
{
	USoundBase* Chosen = nullptr;
	if (const TObjectPtr<USoundBase>* Override = EzanSoundOverrides.Find(PrayerTag))
	{
		Chosen = *Override;
	}
	if (!Chosen) Chosen = DefaultSoundFromSubsystem;
	if (!Chosen) Chosen = FallbackEzanSound;
	if (!Chosen || !GetWorld()) return;

	const float Delay = FMath::FRandRange(MinStaggerDelaySeconds, FMath::Max(MinStaggerDelaySeconds, MaxStaggerDelaySeconds));
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		StartEzanPlayback(Chosen, PrayerTag);
	}
	else
	{
		FTimerDelegate Del;
		Del.BindUObject(this, &AMosqueActor::StartEzanPlayback, TObjectPtr<USoundBase>(Chosen), PrayerTag);
		GetWorld()->GetTimerManager().SetTimer(StaggerTimer, Del, Delay, false);
	}
}

void AMosqueActor::StartEzanPlayback(TObjectPtr<USoundBase> SoundToPlay, FGameplayTag PrayerTag)
{
	if (!SoundToPlay || !MinaretAudio) return;

	MinaretAudio->Stop();
	MinaretAudio->SetSound(SoundToPlay);
	MinaretAudio->SetPitchMultiplier(1.0f + FMath::FRandRange(-PitchVariation, PitchVariation));
	MinaretAudio->Play();

	// Also pass a MetaSound parameter in case SoundToPlay is a procedural/layered MetaSound
	MinaretAudio->SetFloatParameter(TEXT("ReverbWet"), ReverbSendLevel);

	UE_LOG(LogIstanbulTaxi, Log, TEXT("Mosque '%s' started %s ezan."),
		*MosqueName.ToString(), *PrayerTag.ToString());
}

void AMosqueActor::StopEzan()
{
	if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(StaggerTimer);
	if (MinaretAudio && MinaretAudio->IsPlaying())
	{
		MinaretAudio->FadeOut(2.0f, 0.0f);
	}
}

void AMosqueActor::ApplyExteriorOcclusion(float CutoffHz, float VolumeScale)
{
	if (!MinaretAudio) return;
	MinaretAudio->SetLowPassFilterEnabled(CutoffHz < 19500.0f);
	MinaretAudio->SetLowPassFilterFrequency(CutoffHz);
	MinaretAudio->SetVolumeMultiplier(BaseVolume * VolumeScale);
}
