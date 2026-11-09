#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestSceneBuilder.generated.h"

class AIstanbulTrafficManager;

/**
 * Automated Istanbul Urban Environment & Prototype Builder.
 * Spawns automatically via AIstanbulTaxiGameMode::BeginPlay (zero manual Blueprint setup required):
 *  1. Permanent Locked Exposure (EV100 = 1.0) & Balanced Lux Lighting via APostProcessVolume + UTimeOfDaySubsystem.
 *  2. Multi-Zone Authentic Istanbul Street Topology:
 *     - Coastal Main Boulevard (Sahil Yolu / Ana Arter): 4-lane divided asphalt road alongside the Bosphorus with
 *       high curbstones, yellow-black bollards, İETT bus stops, streetlamps, T1 Tram rails, and potholes.
 *     - Steep Cobblestone Side Streets (Sultanahmet / Balat Yokuşu & Ara Sokaklar): 11° uphill/downhill inclines
 *       with Arnavut kaldırımı surface vibration, closely packed 4–6 story bitişik nizam apartment blocks,
 *       ground-floor shop fronts/awnings, overhanging cumba bay windows, overhead electrical cables,
 *       and cars parked half-on-the-sidewalk narrowing the drivable width.
 *     - Hilltop Mosque Complex (ドーム + 2 Minarets) & Bosphorus Bridge Approach with HGS/OGS Toll Canopy.
 *  3. AIstanbulTrafficManager: Moving Dolmuş, İETT Buses, Sarı Taxis, Sedans, Delivery Vans, and double-parked hazard cars.
 */
UCLASS()
class ISTANBULTAXI_API ATestSceneBuilder : public AActor
{
	GENERATED_BODY()

public:
	ATestSceneBuilder();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prototype")
	bool bBuildTestEnvironment = true;

	UFUNCTION(BlueprintCallable, Category = "Prototype")
	void BuildPrototypeScene();

protected:
	virtual void BeginPlay() override;

private:
	AActor* SpawnShape(const TCHAR* ShapePath, const FString& Label, const FVector& Location,
		const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color,
		bool bCollidable = false, FName SurfaceActorTag = NAME_None);

	AActor* SpawnBox(const FString& Label, const FVector& Location, const FVector& Scale,
		const FLinearColor& Color, bool bCollidable = false, const FRotator& Rotation = FRotator::ZeroRotator,
		FName SurfaceActorTag = NAME_None);

	void SpawnIstanbulApartmentBlock(const FString& NamePrefix, const FVector& BaseLocation, float YawDeg,
		int32 Floors, float WidthM, float DepthM, const FLinearColor& FacadeColor, bool bHasCumba, bool bHasShopAwning);

	void SpawnParkedCurbCar(const FString& Label, const FVector& Location, const FRotator& Rotation, const FLinearColor& BodyColor);
	void SpawnIETTBusStop(const FString& Label, const FVector& Location, float YawDeg);
	void SpawnStreetLamp(const FString& Label, const FVector& Location, float YawDeg);
	void SpawnDetailedMosqueComplex(const FVector& HilltopBase);
	void SpawnHGSBridgeCanopy(const FVector& GateCenter);

	bool bBuilt = false;
};
