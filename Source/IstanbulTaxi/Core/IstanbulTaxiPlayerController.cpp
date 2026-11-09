#include "Core/IstanbulTaxiPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

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
