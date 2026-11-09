#include "Environment/BosphorusWaterVolume.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABosphorusWaterVolume::ABosphorusWaterVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	const UStaticMesh* Cube = CubeFinder.Object;
	const UStaticMesh* Cyl  = CylFinder.Object;
	const UStaticMesh* Cone = ConeFinder.Object;

	// 1. Main Bosphorus Sea Surface (520m x 160m deep turquoise-blue strait)
	DeepWaterMesh = AddPart(TEXT("DeepWaterMesh"), Root, Cube,
		FVector(17000.0f, -8800.0f, -55.0f), FVector(520.0f, 160.0f, 0.4f));

	// 2. 6 Animated Wave Crest & Foam Bands parallel to the Eminönü-Beşiktaş shoreline
	for (int32 i = 0; i < 6; ++i)
	{
		const float YOffset = -1950.0f - i * 1650.0f;
		UStaticMeshComponent* Strip = AddPart(*FString::Printf(TEXT("WaveStrip_%d"), i), Root, Cube,
			FVector(17000.0f, YOffset, -34.0f), FVector(500.0f, (i == 0) ? 1.8f : 4.2f, 0.08f));
		WaveCrestStrips.Add(Strip);
	}

	// 3. Iconic Şehir Hatları Bosphorus Ferry (Vapur) cruising parallel to the coast
	FerryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FerryRoot"));
	FerryRoot->SetupAttachment(Root);
	FerryRoot->SetRelativeLocation(FVector(4000.0f, -4200.0f, -20.0f));

	// Ferry Hull (Black/Dark Green waterline + White main deck)
	FerryParts.Add(AddPart(TEXT("Ferry_Hull"),      FerryRoot, Cube, FVector(0.0f, 0.0f,  90.0f), FVector(34.0f, 7.6f, 1.8f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Deck1"),     FerryRoot, Cube, FVector(0.0f, 0.0f, 250.0f), FVector(28.0f, 6.8f, 1.5f)));
	FerryParts.Add(AddPart(TEXT("Ferry_WinBand1"),  FerryRoot, Cube, FVector(0.0f, 0.0f, 260.0f), FVector(26.5f, 7.0f, 0.65f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Deck2"),     FerryRoot, Cube, FVector(-150.0f, 0.0f, 395.0f), FVector(20.0f, 5.8f, 1.3f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Bridge"),    FerryRoot, Cube, FVector(720.0f, 0.0f, 495.0f), FVector(4.2f, 6.2f, 1.1f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Funnel"),    FerryRoot, Cyl,  FVector(-120.0f, 0.0f, 590.0f), FVector(1.8f, 1.8f, 3.2f)));
	FerryParts.Add(AddPart(TEXT("Ferry_FunnelTip"), FerryRoot, Cyl,  FVector(-120.0f, 0.0f, 745.0f), FVector(1.85f, 1.85f, 0.65f)));

	// 4. Kız Kulesi (Maiden's Tower) islet & lighthouse silhouette out in the Bosphorus strait
	const FVector IsletBase(14500.0f, -9600.0f, -25.0f);
	FerryParts.Add(AddPart(TEXT("KizKulesi_Islet"),   Root, Cyl,  IsletBase + FVector(0, 0,  45.0f), FVector(18.0f, 18.0f, 0.9f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Bastion"), Root, Cube, IsletBase + FVector(0, 0, 210.0f), FVector(7.5f, 7.5f, 2.6f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Tower"),   Root, Cube, IsletBase + FVector(0, 0, 620.0f), FVector(3.4f, 3.4f, 6.2f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Balcony"), Root, Cyl,  IsletBase + FVector(0, 0, 960.0f), FVector(4.4f, 4.4f, 0.55f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Spire"),   Root, Cone, IsletBase + FVector(0, 0, 1180.0f), FVector(3.6f, 3.6f, 3.8f)));
}

UStaticMeshComponent* ABosphorusWaterVolume::AddPart(const FName& Name, USceneComponent* Parent, const UStaticMesh* Mesh,
	const FVector& RelLoc, const FVector& RelScale, const FRotator& RelRot)
{
	UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Comp->SetupAttachment(Parent);
	Comp->SetRelativeLocation(RelLoc);
	Comp->SetRelativeRotation(RelRot);
	Comp->SetRelativeScale3D(RelScale);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Mesh)
	{
		Comp->SetStaticMesh( const_cast<UStaticMesh*>(Mesh));
	}
	return Comp;
}

UMaterialInstanceDynamic* ABosphorusWaterVolume::TintPart(UStaticMeshComponent* Comp, const FLinearColor& Color)
{
	if (!Comp) return nullptr;
	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Comp->SetMaterial(0, MID);
		return MID;
	}
	return nullptr;
}

void ABosphorusWaterVolume::BeginPlay()
{
	Super::BeginPlay();

	// Deep Bosphorus teal-blue base
	DeepWaterMID = TintPart(DeepWaterMesh, FLinearColor(0.015f, 0.14f, 0.28f));

	// Wave crests (shoreline whitecap foam + turquoise Bosphorus swells)
	WaveStripMIDs.Reset();
	for (int32 i = 0; i < WaveCrestStrips.Num(); ++i)
	{
		const FLinearColor Col = (i == 0)
			? FLinearColor(0.58f, 0.78f, 0.90f) // Seafront seawall whitecap foam
			: FLinearColor(0.03f, 0.22f, 0.40f);
		WaveStripMIDs.Add(TintPart(WaveCrestStrips[i], Col));
	}

	// Tint Şehir Hatları Ferry & Kız Kulesi parts
	if (FerryParts.Num() >= 12)
	{
		TintPart(FerryParts[0],  FLinearColor(0.06f, 0.10f, 0.14f)); // Hull
		TintPart(FerryParts[1],  FLinearColor(0.92f, 0.92f, 0.90f)); // White Deck 1
		TintPart(FerryParts[2],  FLinearColor(0.10f, 0.16f, 0.24f)); // Cabin Windows
		TintPart(FerryParts[3],  FLinearColor(0.92f, 0.92f, 0.90f)); // White Deck 2
		TintPart(FerryParts[4],  FLinearColor(0.92f, 0.92f, 0.90f)); // Bridge
		TintPart(FerryParts[5],  FLinearColor(0.95f, 0.72f, 0.05f)); // Iconic Yellow Şehir Hatları Funnel
		TintPart(FerryParts[6],  FLinearColor(0.05f, 0.05f, 0.06f)); // Black Funnel Cap
		TintPart(FerryParts[7],  FLinearColor(0.38f, 0.36f, 0.33f)); // Kız Kulesi Rock Islet
		TintPart(FerryParts[8],  FLinearColor(0.76f, 0.72f, 0.64f)); // Bastion Stone
		TintPart(FerryParts[9],  FLinearColor(0.86f, 0.84f, 0.78f)); // White Tower
		TintPart(FerryParts[10], FLinearColor(0.22f, 0.24f, 0.28f)); // Balcony
		TintPart(FerryParts[11], FLinearColor(0.32f, 0.36f, 0.42f)); // Lead Spire
	}
}

void ABosphorusWaterVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	WaveTime += DeltaSeconds;

	// Subtle Bosphorus color shimmer on the main water plane
	if (DeepWaterMID)
	{
		const float Shimmer = 0.5f + 0.5f * FMath::Sin(WaveTime * 1.1f);
		const FLinearColor WaterCol = FMath::Lerp(
			FLinearColor(0.012f, 0.12f, 0.25f),
			FLinearColor(0.025f, 0.19f, 0.35f),
			Shimmer);
		DeepWaterMID->SetVectorParameterValue(TEXT("Color"), WaterCol);
	}

	// Animate wave crest strips (rolling toward the seafront promenade + vertical swell)
	for (int32 i = 0; i < WaveCrestStrips.Num(); ++i)
	{
		if (UStaticMeshComponent* Strip = WaveCrestStrips[i])
		{
			const float Phase = WaveTime * 1.8f + i * 1.15f;
			const float BaseY = -1950.0f - i * 1650.0f;
			const float SwellY = FMath::Sin(Phase) * ((i == 0) ? 45.0f : 120.0f);
			const float SwellZ = -34.0f + FMath::Cos(Phase * 1.3f) * 8.0f;
			Strip->SetRelativeLocation(FVector(17000.0f, BaseY + SwellY, SwellZ));

			if (WaveStripMIDs.IsValidIndex(i) && WaveStripMIDs[i])
			{
				const float CrestGlow = 0.5f + 0.5f * FMath::Sin(Phase);
				const FLinearColor BaseCol = (i == 0)
					? FLinearColor(0.55f, 0.75f, 0.88f)
					: FLinearColor(0.03f, 0.20f, 0.38f);
				const FLinearColor PeakCol = (i == 0)
					? FLinearColor(0.82f, 0.92f, 0.98f)
					: FLinearColor(0.08f, 0.34f, 0.54f);
				WaveStripMIDs[i]->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(BaseCol, PeakCol, CrestGlow));
			}
		}
	}

	// Gently cruise the Şehir Hatları Ferry along the Bosphorus (Eminönü <-> Beşiktaş / Üsküdar) with wave pitch/roll
	if (FerryRoot)
	{
		FerryProgress = FMath::Fmod(FerryProgress + DeltaSeconds * 420.0f, 32000.0f);
		const float FerryX = -2000.0f + FerryProgress;
		const float Pitch = FMath::Sin(WaveTime * 1.4f) * 1.2f;
		const float Roll  = FMath::Cos(WaveTime * 1.1f) * 1.8f;
		FerryRoot->SetRelativeLocationAndRotation(
			FVector(FerryX, -4200.0f, -20.0f + FMath::Sin(WaveTime * 1.4f) * 10.0f),
			FRotator(Pitch, 0.0f, Roll));
	}
}
