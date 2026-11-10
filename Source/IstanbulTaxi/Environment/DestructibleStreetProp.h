// Copyright Istanbul Taxi Simulator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructibleStreetProp.generated.h"

class UBoxComponent;
class UProceduralMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EStreetPropType : uint8
{
	StreetLightPole UMETA(DisplayName = "Sokak Lambasi Diregi"),
	StripedBollard  UMETA(DisplayName = "Sari-Siyah Duba"),
	TrafficSign     UMETA(DisplayName = "Trafik Tabelasi"),
	TrashBin        UMETA(DisplayName = "Sokak Cop Kutusu")
};

/**
 * Interactive physical street furniture (streetlights, bollards/dubalar, traffic signs, trash bins).
 * Anchored upright with solid collision by default; tips over with spark effects, light flicker,
 * nearby AI horn reactions, and passenger panic dialogue when struck above 15 km/h.
 */
UCLASS()
class ISTANBULTAXI_API ADestructibleStreetProp : public AActor
{
	GENERATED_BODY()

public:
	ADestructibleStreetProp();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Configure prop archetype, height/scale, and optional streetlamp illumination. */
	UFUNCTION(BlueprintCallable, Category = "Istanbul|StreetProp")
	void InitializeProp(EStreetPropType InType, bool bEnableLight = true, const FString& InSignText = TEXT(""));

	/** Trigger physical knock-over from a vehicle impact (> 15 km/h). Returns true if newly knocked over. */
	UFUNCTION(BlueprintCallable, Category = "Istanbul|StreetProp")
	bool TriggerKnockOver(const FVector& HitLocation, const FVector& ImpactVelocity, AActor* InstigatorActor);

	/** Whether this prop has already been knocked down. */
	UFUNCTION(BlueprintPure, Category = "Istanbul|StreetProp")
	bool IsKnockedOver() const { return bIsKnockedOver; }

	/** Turkish display name for HUD / passenger reaction ("Sokak Direği", "Duba", "Trafik Tabelası"). */
	UFUNCTION(BlueprintPure, Category = "Istanbul|StreetProp")
	FString GetTurkishPropName() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Istanbul|StreetProp")
	EStreetPropType PropType = EStreetPropType::StreetLightPole;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Istanbul|StreetProp")
	bool bEmitStreetLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Istanbul|StreetProp")
	FString SignLabelText;

	/** Minimum impact speed in km/h required to knock over this prop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Istanbul|StreetProp")
	float KnockOverSpeedThresholdKmh = 15.0f;

protected:
	UFUNCTION()
	void OnPropCollisionHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void RebuildPropMesh();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Istanbul|StreetProp")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Istanbul|StreetProp")
	TObjectPtr<UProceduralMeshComponent> PropMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Istanbul|StreetProp")
	TObjectPtr<UPointLightComponent> StreetLightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Istanbul|StreetProp")
	TObjectPtr<UTextRenderComponent> SignTextComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SlotMaterials;

private:
	bool bIsKnockedOver = false;
	float KnockOverElapsedSeconds = 0.0f;
	float InitialLightIntensity = 4200.0f;
	FVector InitialWorldLocation = FVector::ZeroVector;
	FRotator InitialWorldRotation = FRotator::ZeroRotator;
	FVector FallAxisWorld = FVector::RightVector;
	FVector SlideVelocityWorld = FVector::ZeroVector;
	float CurrentTiltDegrees = 0.0f;
	float TiltAngularVelocityDegPerSec = 0.0f;
};
