#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "PrayerTimeCalculator.generated.h"

/** Daily Diyanet-compatible prayer schedule (local hours in [0, 24), e.g. 5.75 = 05:45). */
USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FDiyanetPrayerSchedule
{
	GENERATED_BODY()

	/** İmsak / Sabah Ezanı (Fajr, -18 deg + temkin). */
	UPROPERTY(BlueprintReadOnly, Category = "Prayer") float Sabah = 5.0f;
	/** Güneş doğuşu (Sunrise, -0.833 deg - 7 min temkin). Not an ezan, included for reference. */
	UPROPERTY(BlueprintReadOnly, Category = "Prayer") float Gunes = 6.5f;
	/** Öğle Ezanı (Dhuhr, solar noon + 5 min temkin). */
	UPROPERTY(BlueprintReadOnly, Category = "Prayer") float Ogle = 13.15f;
	/** İkindi Ezanı (Asr, Shafi/Hanafi standard shadow factor 1 + 4 min temkin). */
	UPROPERTY(BlueprintReadOnly, Category = "Prayer") float Ikindi = 16.85f;
	/** Akşam Ezanı (Maghrib, sunset -0.833 deg + 7 min temkin). */
	UPROPERTY(BlueprintReadOnly, Category = "Prayer") float Aksam = 20.1f;
	/** Yatsı Ezanı (Isha, -17 deg + temkin). */
	UPROPERTY(BlueprintReadOnly, Category = "Prayer") float Yatsi = 21.75f;

	/** Returns the 5 ezan times mapped to their GameplayTags (Prayer.Sabah, Ogle, Ikindi, Aksam, Yatsi). */
	void GetEzanTimes(TArray<TPair<FGameplayTag, float>>& OutEzans) const;
};

/**
 * Diyanet İşleri Başkanlığı-compatible astronomical prayer time calculator.
 * Uses Fajr = 18 deg, Isha = 17 deg, Asr = standard (shadow factor 1), and Diyanet temkin (precautionary) offsets.
 */
UCLASS()
class ISTANBULTAXI_API UPrayerTimeCalculator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Computes the full daily schedule for a given DayOfYear (1..365) and coordinates (defaults to Istanbul). */
	UFUNCTION(BlueprintPure, Category = "Audio|Prayer")
	static FDiyanetPrayerSchedule ComputeDailySchedule(
		int32 DayOfYear,
		float LatitudeDeg = 41.01f,
		float LongitudeDeg = 28.97f,
		float UtcOffsetHours = 3.0f);

	/** Formats a decimal hour (e.g. 13.25) as "13:15". */
	UFUNCTION(BlueprintPure, Category = "Audio|Prayer")
	static FText FormatPrayerHour(float Hour);

private:
	/** Hour angle (in hours) for a given solar altitude angle (degrees), latitude (radians), and declination (radians). */
	static double ComputeHourAngleHours(double AltitudeDeg, double LatRad, double DeclRad);
};
