#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DistrictAmbienceVolume.generated.h"

class UBoxComponent;
class USplineComponent;
class UDistrictAmbienceDataAsset;

/**
 * Defines a district's acoustic zone (e.g. Sultanahmet, Eminönü, Kadıköy).
 * Higher Priority wins when volumes overlap (e.g. a small Çay Bahçesi pocket inside Sultanahmet).
 */
UCLASS()
class ISTANBULTAXI_API ADistrictAmbienceVolume : public AActor
{
	GENERATED_BODY()

public:
	ADistrictAmbienceVolume();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UDistrictAmbienceDataAsset> AmbienceProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambience")
	int32 Priority = 0;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Bounds;

	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
};

/**
 * Spline traced along the Bosphorus, Golden Horn (Haliç), or Marmara shoreline.
 * UAudioAmbienceSubsystem queries distance to the nearest shoreline spline to drive seagull intensity.
 */
UCLASS()
class ISTANBULTAXI_API AWaterfrontSplineActor : public AActor
{
	GENERATED_BODY()

public:
	AWaterfrontSplineActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waterfront")
	TObjectPtr<USplineComponent> ShorelineSpline;

	float GetDistanceToLocationCm(const FVector& WorldLocation) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
