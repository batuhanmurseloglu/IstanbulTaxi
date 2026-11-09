#include "Audio/PrayerTimeCalculator.h"
#include "Core/IstanbulTaxiGameplayTags.h"

void FDiyanetPrayerSchedule::GetEzanTimes(TArray<TPair<FGameplayTag, float>>& OutEzans) const
{
	OutEzans.Reset(5);
	OutEzans.Emplace(IstanbulTaxiTags::Prayer_Sabah,  Sabah);
	OutEzans.Emplace(IstanbulTaxiTags::Prayer_Ogle,   Ogle);
	OutEzans.Emplace(IstanbulTaxiTags::Prayer_Ikindi, Ikindi);
	OutEzans.Emplace(IstanbulTaxiTags::Prayer_Aksam,  Aksam);
	OutEzans.Emplace(IstanbulTaxiTags::Prayer_Yatsi,  Yatsi);
}

double UPrayerTimeCalculator::ComputeHourAngleHours(double AltitudeDeg, double LatRad, double DeclRad)
{
	const double AltRad = FMath::DegreesToRadians(AltitudeDeg);
	const double Numer = FMath::Sin(AltRad) - FMath::Sin(LatRad) * FMath::Sin(DeclRad);
	const double Denom = FMath::Max(FMath::Cos(LatRad) * FMath::Cos(DeclRad), 1e-6);
	const double CosH = FMath::Clamp(Numer / Denom, -1.0, 1.0);
	return FMath::RadiansToDegrees(FMath::Acos(CosH)) / 15.0;
}

FDiyanetPrayerSchedule UPrayerTimeCalculator::ComputeDailySchedule(
	int32 DayOfYear, float LatitudeDeg, float LongitudeDeg, float UtcOffsetHours)
{
	const int32 DOY = FMath::Clamp(DayOfYear, 1, 366);
	const double Gamma = (2.0 * DOUBLE_PI / 365.0) * (DOY - 1);

	// Solar declination (radians) and equation of time (hours)
	const double EqTimeHours = (229.18 * (0.000075 + 0.001868 * FMath::Cos(Gamma) - 0.032077 * FMath::Sin(Gamma)
		- 0.014615 * FMath::Cos(2.0 * Gamma) - 0.040849 * FMath::Sin(2.0 * Gamma))) / 60.0;
	const double DeclRad = 0.006918 - 0.399912 * FMath::Cos(Gamma) + 0.070257 * FMath::Sin(Gamma)
		- 0.006758 * FMath::Cos(2.0 * Gamma) + 0.000907 * FMath::Sin(2.0 * Gamma)
		- 0.002697 * FMath::Cos(3.0 * Gamma) + 0.00148 * FMath::Sin(3.0 * Gamma);

	const double LatRad = FMath::DegreesToRadians((double)LatitudeDeg);

	// Astronomical solar noon (local clock hours)
	const double DhuhrNoon = 12.0 + UtcOffsetHours - (LongitudeDeg / 15.0) - EqTimeHours;

	// Diyanet angles: Fajr = -18 deg, Sunrise/Sunset = -0.833 deg, Isha = -17 deg
	const double HaSunrise = ComputeHourAngleHours(-0.833, LatRad, DeclRad);
	const double HaFajr    = ComputeHourAngleHours(-18.0,  LatRad, DeclRad);
	const double HaIsha    = ComputeHourAngleHours(-17.0,  LatRad, DeclRad);

	// Asr-i Evvel (Diyanet standard: shadow ratio = 1 + tan(|Lat - Decl|))
	const double AsrAltRad = FMath::Atan(1.0 / (1.0 + FMath::Tan(FMath::Abs(LatRad - DeclRad))));
	const double HaAsr     = ComputeHourAngleHours(FMath::RadiansToDegrees(AsrAltRad), LatRad, DeclRad);

	// Diyanet temkin (precautionary offsets in minutes):
	// Gunes -7m, Ogle +5m, Ikindi +4m, Aksam +7m
	FDiyanetPrayerSchedule S;
	S.Sabah  = (float)(DhuhrNoon - HaFajr);
	S.Gunes  = (float)(DhuhrNoon - HaSunrise - 7.0 / 60.0);
	S.Ogle   = (float)(DhuhrNoon + 5.0 / 60.0);
	S.Ikindi = (float)(DhuhrNoon + HaAsr + 4.0 / 60.0);
	S.Aksam  = (float)(DhuhrNoon + HaSunrise + 7.0 / 60.0);
	S.Yatsi  = (float)(DhuhrNoon + HaIsha);
	return S;
}

FText UPrayerTimeCalculator::FormatPrayerHour(float Hour)
{
	const int32 TotalMin = FMath::RoundToInt(FMath::Fmod(FMath::Max(Hour, 0.0f), 24.0f) * 60.0f) % 1440;
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), TotalMin / 60, TotalMin % 60));
}
