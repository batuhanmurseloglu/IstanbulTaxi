#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "IstanbulTaxiPlayerController.generated.h"

class UInputMappingContext;

/**
 * Player controller. Owns UI-level / global input contexts (menus, map, phone app later).
 * Driving input lives on the TaxiPawn so it is removed automatically when unpossessed.
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulTaxiPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void IstanbulRebuild();

	UFUNCTION(Exec)
	void IstanbulTeleportToSlope();

	UFUNCTION(Exec)
	void IstanbulTeleportToBosphorus();

	UFUNCTION(Exec)
	void IstanbulToggleDebugSlope();

protected:
	virtual void BeginPlay() override;

	/** Global (non-driving) mapping context, e.g. pause / map. Optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> GlobalMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	int32 GlobalMappingPriority = 0;
};
