#include "Core/IstanbulTaxiGameMode.h"
#include "Core/IstanbulTaxiPlayerController.h"
#include "Core/IstanbulTaxiHUD.h"
#include "Core/TestSceneBuilder.h"
#include "Vehicle/TaxiPawn.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AIstanbulTaxiGameMode::AIstanbulTaxiGameMode()
{
	PlayerControllerClass = AIstanbulTaxiPlayerController::StaticClass();
	DefaultPawnClass = ATaxiPawn::StaticClass();
	HUDClass = AIstanbulTaxiHUD::StaticClass();
}

void AIstanbulTaxiGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSpawnPrototypeScene && GetWorld())
	{
		if (!UGameplayStatics::GetActorOfClass(GetWorld(), ATestSceneBuilder::StaticClass()))
		{
			FVector Origin = FVector::ZeroVector;
			if (const APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
			{
				Origin = P->GetActorLocation();
				Origin.Z = 0.0f;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<ATestSceneBuilder>(Origin, FRotator::ZeroRotator, Params);
		}
	}
}
