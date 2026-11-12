// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#include "Environment/WeatherSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Core/IstanbulTaxiHUD.h"
#include "IstanbulTaxi.h"

void UWeatherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentWeather = EIstanbulWeatherState::Clear;
	CurrentWetness = 0.0f;
	TargetWetness = 0.0f;
	AutoCycleTimer = 0.0f;
}

void UWeatherSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Interpolate wetness smoothly
	CurrentWetness = FMath::FInterpTo(CurrentWetness, TargetWetness, DeltaTime, 0.75f);

	// Slow atmospheric auto-progression every 180 seconds if untouched
	AutoCycleTimer += DeltaTime;
	if (AutoCycleTimer >= 180.0f)
	{
		AutoCycleTimer = 0.0f;
		const uint8 NextIdx = (static_cast<uint8>(CurrentWeather) + 1) % 4;
		SetWeather(static_cast<EIstanbulWeatherState>(NextIdx));
	}

	ApplyWeatherToScene(DeltaTime);
}

void UWeatherSubsystem::SetWeather(EIstanbulWeatherState NewWeather)
{
	if (CurrentWeather == NewWeather) return;

	CurrentWeather = NewWeather;
	AutoCycleTimer = 0.0f;

	switch (CurrentWeather)
	{
	case EIstanbulWeatherState::Clear:
		TargetWetness = 0.0f;
		break;
	case EIstanbulWeatherState::Overcast:
		TargetWetness = 0.08f;
		break;
	case EIstanbulWeatherState::IstanbulRain:
		TargetWetness = 1.0f;
		break;
	case EIstanbulWeatherState::BosphorusFog:
		TargetWetness = 0.35f;
		break;
	}

	OnWeatherChanged.Broadcast(CurrentWeather);

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AIstanbulTaxiHUD* HUD = Cast<AIstanbulTaxiHUD>(PC->GetHUD()))
			{
				FString ToastMsg = FString::Printf(TEXT("Hava Durumu Değişti: %s"), *GetWeatherDisplayName());
				HUD->PushToast(ToastMsg, FLinearColor(0.35f, 0.75f, 1.0f), 4.5f);
			}
		}
	}
}

void UWeatherSubsystem::CycleWeather()
{
	const uint8 NextIdx = (static_cast<uint8>(CurrentWeather) + 1) % 4;
	SetWeather(static_cast<EIstanbulWeatherState>(NextIdx));
}

FString UWeatherSubsystem::GetWeatherDisplayName() const
{
	switch (CurrentWeather)
	{
	case EIstanbulWeatherState::Clear:        return TEXT("Açık / Güneşli");
	case EIstanbulWeatherState::Overcast:     return TEXT("Bulutlu / Kapalı");
	case EIstanbulWeatherState::IstanbulRain: return TEXT("İstanbul Yağmuru (Islak Zemin & Yansımalar)");
	case EIstanbulWeatherState::BosphorusFog: return TEXT("Boğaz Sisi (Yoğun Sis)");
	}
	return TEXT("Bilinmeyen");
}

void UWeatherSubsystem::RegisterDynamicRoadMaterials(UMaterialInstanceDynamic* InAsphaltMat, UMaterialInstanceDynamic* InSidewalkMat, UMaterialInstanceDynamic* InCobbleMat)
{
	AsphaltMID = InAsphaltMat;
	SidewalkMID = InSidewalkMat;
	CobbleMID = InCobbleMat;
}

void UWeatherSubsystem::ApplyWeatherToScene(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Update road & sidewalk roughness & specular for wet asphalt reflections
	// Wet asphalt: Roughness drops to 0.12 - 0.22, Specular rises to 0.85
	const float BaseAsphaltRough = 0.78f;
	const float WetAsphaltRough  = 0.14f;
	const float CurrentRough = FMath::Lerp(BaseAsphaltRough, WetAsphaltRough, CurrentWetness);
	const float CurrentSpec  = FMath::Lerp(0.25f, 0.88f, CurrentWetness);

	if (AsphaltMID.IsValid())
	{
		AsphaltMID->SetScalarParameterValue(TEXT("Roughness"), CurrentRough);
		AsphaltMID->SetScalarParameterValue(TEXT("Specular"), CurrentSpec);
	}
	if (SidewalkMID.IsValid())
	{
		SidewalkMID->SetScalarParameterValue(TEXT("Roughness"), FMath::Lerp(0.85f, 0.22f, CurrentWetness));
		SidewalkMID->SetScalarParameterValue(TEXT("Specular"), FMath::Lerp(0.20f, 0.75f, CurrentWetness));
	}
	if (CobbleMID.IsValid())
	{
		CobbleMID->SetScalarParameterValue(TEXT("Roughness"), FMath::Lerp(0.72f, 0.16f, CurrentWetness));
		CobbleMID->SetScalarParameterValue(TEXT("Specular"), FMath::Lerp(0.35f, 0.85f, CurrentWetness));
	}

	// Adjust ExponentialHeightFog density dynamically for Bosphorus fog and rain
	if (AExponentialHeightFog* FogActor = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(World, AExponentialHeightFog::StaticClass())))
	{
		if (UExponentialHeightFogComponent* FogComp = FogActor->GetComponent())
		{
			float TargetFogDensity = 0.02f;
			if (CurrentWeather == EIstanbulWeatherState::BosphorusFog)
			{
				TargetFogDensity = 0.12f;
			}
			else if (CurrentWeather == EIstanbulWeatherState::IstanbulRain)
			{
				TargetFogDensity = 0.048f;
			}
			else if (CurrentWeather == EIstanbulWeatherState::Overcast)
			{
				TargetFogDensity = 0.032f;
			}

			const float NewDensity = FMath::FInterpTo(FogComp->FogDensity, TargetFogDensity, DeltaTime, 0.5f);
			FogComp->SetFogDensity(NewDensity);
		}
	}
}
