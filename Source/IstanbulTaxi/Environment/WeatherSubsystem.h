// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WeatherSubsystem.generated.h"

class AExponentialHeightFog;
class ASkyLight;
class ADirectionalLight;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EIstanbulWeatherState : uint8
{
	Clear        UMETA(DisplayName = "Açık / Güneşli"),
	Overcast     UMETA(DisplayName = "Bulutlu / Kapalı"),
	IstanbulRain UMETA(DisplayName = "İstanbul Yağmuru (Islak Zemin)"),
	BosphorusFog UMETA(DisplayName = "Boğaz Sisi")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeatherChanged, EIstanbulWeatherState, NewWeather);

/**
 * World Subsystem handling authentic Istanbul dynamic weather states, wet asphalt reflections,
 * fog parameters, and road surface roughness modulation.
 */
UCLASS()
class ISTANBULTAXI_API UWeatherSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWeatherSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	UFUNCTION(BlueprintCallable, Category = "Istanbul|Weather")
	void SetWeather(EIstanbulWeatherState NewWeather);

	UFUNCTION(BlueprintCallable, Category = "Istanbul|Weather")
	void CycleWeather();

	UFUNCTION(BlueprintPure, Category = "Istanbul|Weather")
	EIstanbulWeatherState GetCurrentWeather() const { return CurrentWeather; }

	UFUNCTION(BlueprintPure, Category = "Istanbul|Weather")
	FString GetWeatherDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Istanbul|Weather")
	float GetWetness() const { return CurrentWetness; }

	UFUNCTION(BlueprintPure, Category = "Istanbul|Weather")
	bool IsRaining() const { return CurrentWeather == EIstanbulWeatherState::IstanbulRain; }

	void RegisterDynamicRoadMaterials(UMaterialInstanceDynamic* InAsphaltMat, UMaterialInstanceDynamic* InSidewalkMat, UMaterialInstanceDynamic* InCobbleMat);

	UPROPERTY(BlueprintAssignable, Category = "Istanbul|Weather")
	FOnWeatherChanged OnWeatherChanged;

private:
	UPROPERTY()
	EIstanbulWeatherState CurrentWeather = EIstanbulWeatherState::Clear;

	float TargetWetness = 0.0f;
	float CurrentWetness = 0.0f;
	float AutoCycleTimer = 0.0f;

	TWeakObjectPtr<UMaterialInstanceDynamic> AsphaltMID;
	TWeakObjectPtr<UMaterialInstanceDynamic> SidewalkMID;
	TWeakObjectPtr<UMaterialInstanceDynamic> CobbleMID;

	void ApplyWeatherToScene(float DeltaTime);
};
