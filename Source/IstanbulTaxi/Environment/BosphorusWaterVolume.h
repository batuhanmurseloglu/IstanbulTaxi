#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BosphorusWaterVolume.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Procedural Bosphorus Strait Water Volume, Animated Waves, Maiden's Tower (Kız Kulesi),
 * and Steaming Şehir Hatları Ferry (Vapur).
 * Spawns automatically via ATestSceneBuilder to give the coastal boulevard and bridge approach
 * an unmistakable, living Bosphorus view with zero external asset dependencies.
 */
UCLASS()
class ISTANBULTAXI_API ABosphorusWaterVolume : public AActor
{
	GENERATED_BODY()

public:
	ABosphorusWaterVolume();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<USceneComponent> Root;

	/** Deep Bosphorus turquoise-navy water plane. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<UStaticMeshComponent> DeepWaterMesh;

	/** Animated secondary wave crest strips that oscillate in Z, Y, and color shimmer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TArray<TObjectPtr<UStaticMeshComponent>> WaveCrestStrips;

	/** Classic White/Yellow-Funnel Istanbul Şehir Hatları Ferry (Vapur) cruising along the Bosphorus. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TObjectPtr<USceneComponent> FerryRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bosphorus")
	TArray<TObjectPtr<UStaticMeshComponent>> FerryParts;

private:
	UStaticMeshComponent* AddPart(const FName& Name, USceneComponent* Parent, const UStaticMesh* Mesh,
		const FVector& RelLoc, const FVector& RelScale, const FRotator& RelRot = FRotator::ZeroRotator);

	UMaterialInstanceDynamic* TintPart(UStaticMeshComponent* Comp, const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DeepWaterMID;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WaveStripMIDs;

	float WaveTime = 0.0f;
	float FerryProgress = 0.0f;
};
