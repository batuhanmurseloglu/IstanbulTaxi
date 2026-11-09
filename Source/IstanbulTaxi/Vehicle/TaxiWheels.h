#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "TaxiWheels.generated.h"

/** Front axle: steered + driven (FWD Egea-class). Soft, comfort-tuned suspension. */
UCLASS()
class ISTANBULTAXI_API UTaxiWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UTaxiWheelFront();
};

/** Rear axle: handbrake only. */
UCLASS()
class ISTANBULTAXI_API UTaxiWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()
public:
	UTaxiWheelRear();
};
