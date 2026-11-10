#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "Missions/PassengerMissionSubsystem.h"
#include "IstanbulTaxiHUD.generated.h"

/**
 * Authentic Turkish Digital Taximeter (Mirror/Dashboard LCD Unit) & Istanbul Telemetry HUD.
 * Renders out-of-the-box in C++ Canvas with zero UMG .uasset dependencies:
 *  - Mirror/Dashboard Digital Taximeter Bezel: backlit amber/red 7-segment-style Fare readout (TL),
 *    LED status pills (BOŞ / DOLU / BEKLEME), UKOME Tariff Segment badge (Sarı / Turkuaz / 8+1 / Siyah),
 *    Wait-billing indicator, Distance counter (km), and İndi-Bindi floor.
 *  - Instrument Cluster: Digital Speedometer (km/h), Gear Indicator (R/N/1..6), and segmented RPM Tachometer bar.
 *  - Compact Istanbul Ambience Bar: 24h Clock, Solar Phase, Active District, Surface Tag & Rumble,
 *    Seagull %, Window LPF Hz, and Diyanet Ezan timetable.
 *  - 3D Viewport Overlays: Passenger Pickup/Drop-off waypoints + AI Traffic Horn "[KORNA!]" callouts.
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulTaxiHUD : public AHUD
{
	GENERATED_BODY()

public:
	AIstanbulTaxiHUD();

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	void PushToast(const FString& Message, const FLinearColor& Color, float Duration = 6.0f);

private:
	UFUNCTION() void HandleRideCompleted(const FTaxiRideResult& Result);
	UFUNCTION() void HandlePassengerRemark(const FTaxiRideRequest& Ride, FGameplayTag RemarkTag);
	UFUNCTION() void HandleTollCharged(FGameplayTag Crossing, int32 FeeKurus, bool bAddedToFare);
	UFUNCTION() void HandleEzanTriggered(FGameplayTag PrayerTag, float PrayerHour);
	UFUNCTION() void HandleTrafficHorn(AActor* HonkingVehicle, FVector WorldLocation, FString ReasonText);

	void DrawBezelBox(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border, float BorderThickness = 1.5f);

	struct FToastEntry
	{
		FString Text;
		FLinearColor Color;
		float ExpireTime = 0.0f;
	};
	TArray<FToastEntry> Toasts;
	bool bBoundToPawnDelegates = false;
	bool bBoundToTrafficDelegates = false;
};
