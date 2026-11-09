#pragma once

#include "CoreMinimal.h"

/**
 * Tiny indirection for "what game hour is it?" so M2 (toll night rates) doesn't depend on M3.
 * M3's UTimeOfDaySubsystem assigns Provider in its Initialize() and clears it in Deinitialize().
 * Until then the real local clock is used.
 */
struct ISTANBULTAXI_API FTaxiGameClock
{
	/** Returns hour of day in [0, 24). */
	static float GetHourOfDay(const UWorld* World);

	static TFunction<float(const UWorld*)> Provider;
};
