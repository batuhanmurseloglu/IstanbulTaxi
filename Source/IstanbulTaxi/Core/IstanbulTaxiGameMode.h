#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "IstanbulTaxiGameMode.generated.h"

/**
 * Default GameMode for İstanbul Taksi Simülatörü.
 * Uses ATaxiPawn, AIstanbulTaxiPlayerController, and AIstanbulTaxiHUD out of the box.
 * Automatically spawns ATestSceneBuilder in BeginPlay if bAutoSpawnPrototypeScene is true.
 */
UCLASS()
class ISTANBULTAXI_API AIstanbulTaxiGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIstanbulTaxiGameMode();

	virtual void BeginPlay() override;

	/** Automatically builds the playable test boulevard, lighting, HGS toll gate, and mosques on BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Prototype")
	bool bAutoSpawnPrototypeScene = true;
};
