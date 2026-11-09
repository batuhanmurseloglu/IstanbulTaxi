#include "Core/IstanbulTaxiPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/Engine.h"

void AIstanbulTaxiPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (GlobalMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Sub->AddMappingContext(GlobalMappingContext, GlobalMappingPriority);
		}
	}
}

void AIstanbulTaxiPlayerController::IstanbulRebuild()
{
	ConsoleCommand(TEXT("Istanbul.Rebuild"));
}

void AIstanbulTaxiPlayerController::IstanbulTeleportToSlope()
{
	ConsoleCommand(TEXT("Istanbul.TeleportToSlope"));
}

void AIstanbulTaxiPlayerController::IstanbulTeleportToBosphorus()
{
	ConsoleCommand(TEXT("Istanbul.TeleportToBosphorus"));
}

void AIstanbulTaxiPlayerController::IstanbulToggleDebugSlope()
{
	ConsoleCommand(TEXT("Istanbul.ToggleDebugSlope"));
}
