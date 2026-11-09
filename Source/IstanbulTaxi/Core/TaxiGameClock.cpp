#include "Core/TaxiGameClock.h"

TFunction<float(const UWorld*)> FTaxiGameClock::Provider;

float FTaxiGameClock::GetHourOfDay(const UWorld* World)
{
	if (Provider)
	{
		return FMath::Fmod(FMath::Max(Provider(World), 0.0f), 24.0f);
	}
	const FDateTime Now = FDateTime::Now();
	return Now.GetHour() + Now.GetMinute() / 60.0f;
}
