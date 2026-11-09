#include "Environment/TimeOfDaySubsystem.h"
#include "Core/IstanbulTaxiSettings.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "Core/TaxiGameClock.h"
#include "IstanbulTaxi.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

void UTimeOfDaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Preset = UIstanbulTaxiSettings::Get()->DefaultTimeOfDayPreset.LoadSynchronous();
	if (!Preset)
	{
		Preset = GetDefault<UTimeOfDayPresetDataAsset>();
	}

	CurrentHour = Preset->StartHour;
	TimeScaleMinutesPerSec = Preset->GameMinutesPerRealSecond;
	Month = Preset->StartMonth;
	Day = Preset->StartDay;

	FTaxiGameClock::Provider = [](const UWorld* W) -> float
	{
		if (W)
		{
			if (const UTimeOfDaySubsystem* Sub = W->GetSubsystem<UTimeOfDaySubsystem>())
			{
				return Sub->GetTimeOfDayHours();
			}
		}
		return 12.0f;
	};

	UpdateSolarAndEnvironment();
}

void UTimeOfDaySubsystem::Deinitialize()
{
	FTaxiGameClock::Provider = nullptr;
	Super::Deinitialize();
}

void UTimeOfDaySubsystem::RegisterSceneActors(ADirectionalLight* InSun, ASkyLight* InSkyLight,
	AExponentialHeightFog* InFog, APostProcessVolume* InPostProcess,
	const UTimeOfDayPresetDataAsset* OverridePreset)
{
	SunLight = InSun;
	SkyLight = InSkyLight;
	HeightFog = InFog;
	PostProcess = InPostProcess;
	if (OverridePreset)
	{
		Preset = OverridePreset;
	}
	UpdateSolarAndEnvironment();
}

void UTimeOfDaySubsystem::SetTimeOfDayHours(float NewHour)
{
	CurrentHour = FMath::Fmod(NewHour, 24.0f);
	if (CurrentHour < 0.0f) CurrentHour += 24.0f;
	UpdateSolarAndEnvironment();
}

void UTimeOfDaySubsystem::SetCalendarDate(int32 NewMonth, int32 NewDay)
{
	Month = FMath::Clamp(NewMonth, 1, 12);
	Day = FMath::Clamp(NewDay, 1, 31);
	OnNewDayStarted.Broadcast(Month, Day);
	UpdateSolarAndEnvironment();
}

int32 UTimeOfDaySubsystem::GetDayOfYear() const
{
	return ToDayOfYear(Month, Day);
}

int32 UTimeOfDaySubsystem::ToDayOfYear(int32 InMonth, int32 InDay)
{
	static const int32 CumDays[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	const int32 M = FMath::Clamp(InMonth, 1, 12) - 1;
	return CumDays[M] + FMath::Clamp(InDay, 1, 31);
}

FText UTimeOfDaySubsystem::GetFormattedClockText() const
{
	const int32 TotalMinutes = FMath::FloorToInt(CurrentHour * 60.0f) % (24 * 60);
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), TotalMinutes / 60, TotalMinutes % 60));
}

void UTimeOfDaySubsystem::Tick(float DeltaTime)
{
	if (!bPaused && TimeScaleMinutesPerSec > 0.0f)
	{
		CurrentHour += (TimeScaleMinutesPerSec * DeltaTime) / 60.0f;
		if (CurrentHour >= 24.0f)
		{
			CurrentHour = FMath::Fmod(CurrentHour, 24.0f);
			AdvanceCalendarDay();
		}
	}
	UpdateSolarAndEnvironment();
}

void UTimeOfDaySubsystem::AdvanceCalendarDay()
{
	static const int32 DaysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	++Day;
	if (Day > DaysInMonth[FMath::Clamp(Month, 1, 12) - 1])
	{
		Day = 1;
		Month = (Month % 12) + 1;
	}
	OnNewDayStarted.Broadcast(Month, Day);
}

void UTimeOfDaySubsystem::ComputeSolarAngles(int32 DayOfYear, float LocalHour, float LatitudeDeg,
	float LongitudeDeg, float UtcOffsetHours, float& OutAltitudeDeg, float& OutAzimuthDeg)
{
	// NOAA fractional year (radians)
	const double Gamma = (2.0 * DOUBLE_PI / 365.0) * (DayOfYear - 1 + (LocalHour - 12.0) / 24.0);

	// Equation of time (minutes) and solar declination (radians)
	const double EqTimeMin = 229.18 * (0.000075 + 0.001868 * FMath::Cos(Gamma) - 0.032077 * FMath::Sin(Gamma)
		- 0.014615 * FMath::Cos(2.0 * Gamma) - 0.040849 * FMath::Sin(2.0 * Gamma));
	const double DeclRad = 0.006918 - 0.399912 * FMath::Cos(Gamma) + 0.070257 * FMath::Sin(Gamma)
		- 0.006758 * FMath::Cos(2.0 * Gamma) + 0.000907 * FMath::Sin(2.0 * Gamma)
		- 0.002697 * FMath::Cos(3.0 * Gamma) + 0.00148 * FMath::Sin(3.0 * Gamma);

	// True solar time and hour angle
	const double TimeOffsetMin = EqTimeMin + 4.0 * LongitudeDeg - 60.0 * UtcOffsetHours;
	const double TrueSolarTimeMin = FMath::Fmod(LocalHour * 60.0 + TimeOffsetMin + 1440.0, 1440.0);
	const double HourAngleDeg = (TrueSolarTimeMin / 4.0) - 180.0;
	const double HourAngleRad = FMath::DegreesToRadians(HourAngleDeg);
	const double LatRad = FMath::DegreesToRadians(LatitudeDeg);

	const double SinAlt = FMath::Sin(LatRad) * FMath::Sin(DeclRad)
		+ FMath::Cos(LatRad) * FMath::Cos(DeclRad) * FMath::Cos(HourAngleRad);
	const double AltRad = FMath::Asin(FMath::Clamp(SinAlt, -1.0, 1.0));
	OutAltitudeDeg = FMath::RadiansToDegrees(AltRad);

	const double CosAz = (FMath::Sin(DeclRad) - FMath::Sin(LatRad) * SinAlt)
		/ FMath::Max(FMath::Cos(LatRad) * FMath::Cos(AltRad), 1e-5);
	double AzDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAz, -1.0, 1.0)));
	if (HourAngleDeg > 0.0) AzDeg = 360.0 - AzDeg;
	OutAzimuthDeg = (float)AzDeg;
}

void UTimeOfDaySubsystem::UpdateSolarAndEnvironment()
{
	const UIstanbulTaxiSettings* Settings = UIstanbulTaxiSettings::Get();
	float AltDeg = 0.0f, AzDeg = 180.0f;
	ComputeSolarAngles(GetDayOfYear(), CurrentHour, Settings->Latitude, Settings->Longitude,
		Settings->UtcOffsetHours, AltDeg, AzDeg);

	CachedSunAltitudeDeg = AltDeg;
	// UE DirectionalLight points IN the direction of travel: Pitch = -Altitude, Yaw = Azimuth
	CachedSunRotation = FRotator(-AltDeg, AzDeg, 0.0f);

	FGameplayTag NewPhase;
	CurrentSample = Preset ? Preset->EvaluateAtHour(CurrentHour, NewPhase) : FTimeOfDayPhaseSample();

	if (NewPhase.IsValid() && NewPhase != CurrentPhaseTag)
	{
		const FGameplayTag Old = CurrentPhaseTag;
		CurrentPhaseTag = NewPhase;
		OnTimePhaseChanged.Broadcast(CurrentPhaseTag, Old);
		UE_LOG(LogIstanbulTaxi, Log, TEXT("TimeOfDay phase -> %s (%.2fh, sun alt %.1f deg)"),
			*CurrentPhaseTag.ToString(), CurrentHour, AltDeg);
	}

	// Apply to scene actors if registered
	if (ADirectionalLight* Sun = SunLight.Get())
	{
		Sun->SetActorRotation(CachedSunRotation);
		if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			LC->SetIntensity(CurrentSample.SunIntensityLux);
			LC->SetLightColor(CurrentSample.SunColor);
		}
	}
	if (ASkyLight* Sky = SkyLight.Get())
	{
		if (USkyLightComponent* SLC = Sky->GetLightComponent())
		{
			SLC->SetIntensity(CurrentSample.SkyLightIntensity);
		}
	}
	if (AExponentialHeightFog* Fog = HeightFog.Get())
	{
		if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
		{
			FC->SetFogDensity(CurrentSample.FogDensity);
			FC->SetFogInscatteringColor(CurrentSample.FogInscatteringColor);
		}
	}
	if (APostProcessVolume* PP = PostProcess.Get())
	{
		PP->bUnbound = true;
		PP->Settings.bOverride_AutoExposureMethod = true;
		PP->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		PP->Settings.bOverride_AutoExposureMinBrightness = true;
		PP->Settings.AutoExposureMinBrightness = 1.0f;
		PP->Settings.bOverride_AutoExposureMaxBrightness = true;
		PP->Settings.AutoExposureMaxBrightness = 1.0f;
		PP->Settings.bOverride_AutoExposureBias = true;
		PP->Settings.AutoExposureBias = CurrentSample.ExposureCompensation;
	}

	// Push to MPC_Environment
	if (Preset && Preset->EnvironmentMPC && GetWorld())
	{
		if (UMaterialParameterCollectionInstance* Inst = GetWorld()->GetParameterCollectionInstance(Preset->EnvironmentMPC))
		{
			Inst->SetScalarParameterValue(TEXT("TimeOfDay"), CurrentHour);
			Inst->SetScalarParameterValue(TEXT("SunAltitude"), CachedSunAltitudeDeg);
			Inst->SetScalarParameterValue(TEXT("StreetLightEmissive"), CurrentSample.StreetLightEmissive);
			Inst->SetScalarParameterValue(TEXT("WindowEmissive"), CurrentSample.WindowEmissive);
			Inst->SetScalarParameterValue(TEXT("BridgeLedIntensity"), CurrentSample.BridgeLedIntensity);
		}
	}
}
