#include "Environment/BosphorusWaterVolume.h"
#include "Core/IstanbulMeshUtils.h"
#include "Vehicle/TaxiPawn.h"
#include "IstanbulTaxi.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ABosphorusWaterVolume::ABosphorusWaterVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	WaterProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterProcMesh"));
	WaterProcMesh->SetupAttachment(Root);
	WaterProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterProcMesh->SetCastShadow(false);

	QuayAndSilhouetteProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("QuayAndSilhouetteProcMesh"));
	QuayAndSilhouetteProcMesh->SetupAttachment(Root);
	QuayAndSilhouetteProcMesh->bUseComplexAsSimpleCollision = true;
	QuayAndSilhouetteProcMesh->SetCollisionProfileName(TEXT("BlockAll"));

	QuayBollardHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("QuayBollardHISM"));
	QuayBollardHISM->SetupAttachment(Root);
	QuayBollardHISM->SetCollisionProfileName(TEXT("BlockAll"));

	// Water Kill / Respawn Trigger Box spanning the Golden Horn & Bosphorus below quay level
	WaterKillTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterKillTrigger"));
	WaterKillTrigger->SetupAttachment(Root);
	WaterKillTrigger->SetRelativeLocation(FVector(45000.0f, -21000.0f, -230.0f));
	WaterKillTrigger->SetBoxExtent(FVector(120000.0f, 19500.0f, 110.0f));
	WaterKillTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WaterKillTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	WaterKillTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	WaterKillTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
	WaterKillTrigger->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	const UStaticMesh* Cube = CubeFinder.Object;
	const UStaticMesh* Cyl  = CylFinder.Object;
	const UStaticMesh* Cone = ConeFinder.Object;

	if (Cyl)
	{
		QuayBollardHISM->SetStaticMesh(const_cast<UStaticMesh*>(Cyl));
	}

	// Iconic Şehir Hatları Bosphorus Ferry (Vapur) cruising parallel to the coast
	FerryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FerryRoot"));
	FerryRoot->SetupAttachment(Root);
	FerryRoot->SetRelativeLocation(FVector(4000.0f, -6200.0f, -110.0f));

	FerryParts.Add(AddPart(TEXT("Ferry_Hull"),      FerryRoot, Cube, FVector(0.0f, 0.0f,  90.0f), FVector(34.0f, 7.6f, 1.8f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Deck1"),     FerryRoot, Cube, FVector(0.0f, 0.0f, 250.0f), FVector(28.0f, 6.8f, 1.5f)));
	FerryParts.Add(AddPart(TEXT("Ferry_WinBand1"),  FerryRoot, Cube, FVector(0.0f, 0.0f, 260.0f), FVector(26.5f, 7.0f, 0.65f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Deck2"),     FerryRoot, Cube, FVector(-150.0f, 0.0f, 395.0f), FVector(20.0f, 5.8f, 1.3f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Bridge"),    FerryRoot, Cube, FVector(720.0f, 0.0f, 495.0f), FVector(4.2f, 6.2f, 1.1f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Funnel"),    FerryRoot, Cyl,  FVector(-120.0f, 0.0f, 590.0f), FVector(1.8f, 1.8f, 3.2f)));
	FerryParts.Add(AddPart(TEXT("Ferry_FunnelTip"), FerryRoot, Cyl,  FVector(-120.0f, 0.0f, 745.0f), FVector(1.85f, 1.85f, 0.65f)));

	// Kız Kulesi (Maiden's Tower) islet & lighthouse clearly visible off the Eminönü-Galata Bosphorus mouth
	const FVector IsletBase(24000.0f, -14500.0f, -118.0f);
	FerryParts.Add(AddPart(TEXT("KizKulesi_Islet"),   Root, Cyl,  IsletBase + FVector(0, 0,   55.0f), FVector(26.0f, 26.0f, 1.2f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Bastion"), Root, Cube, IsletBase + FVector(0, 0,  260.0f), FVector(10.5f, 10.5f, 3.2f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Tower"),   Root, Cube, IsletBase + FVector(0, 0,  780.0f), FVector(4.5f, 4.5f, 8.2f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Balcony"), Root, Cyl,  IsletBase + FVector(0, 0, 1210.0f), FVector(5.8f, 5.8f, 0.65f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Spire"),   Root, Cone, IsletBase + FVector(0, 0, 1490.0f), FVector(4.8f, 4.8f, 4.8f)));

	KizKulesiBeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KizKulesiBeaconLight"));
	KizKulesiBeaconLight->SetupAttachment(Root);
	KizKulesiBeaconLight->SetRelativeLocation(IsletBase + FVector(0.0f, 0.0f, 1310.0f));
	KizKulesiBeaconLight->SetIntensity(8500.0f);
	KizKulesiBeaconLight->SetAttenuationRadius(6500.0f);
	KizKulesiBeaconLight->SetLightColor(FLinearColor(1.0f, 0.82f, 0.32f));
	KizKulesiBeaconLight->SetCastShadows(false);

	LastSafeDryTransform = FTransform(FRotator(0.0f, -38.0f, 0.0f), FVector(5200.0f, -380.0f, 45.0f));
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
		Comp->SetStaticMesh(const_cast<UStaticMesh*>(Mesh));
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

	WaterKillTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABosphorusWaterVolume::OnWaterKillBoxBeginOverlap);

	BuildWaterGridMesh();
	BuildQuayWallAndDistantSilhouettes();

	if (FerryParts.Num() >= 12)
	{
		TintPart(FerryParts[0],  FLinearColor(0.06f, 0.10f, 0.14f));
		TintPart(FerryParts[1],  FLinearColor(0.92f, 0.92f, 0.90f));
		TintPart(FerryParts[2],  FLinearColor(0.10f, 0.16f, 0.24f));
		TintPart(FerryParts[3],  FLinearColor(0.92f, 0.92f, 0.90f));
		TintPart(FerryParts[4],  FLinearColor(0.92f, 0.92f, 0.90f));
		TintPart(FerryParts[5],  FLinearColor(0.95f, 0.72f, 0.05f));
		TintPart(FerryParts[6],  FLinearColor(0.05f, 0.05f, 0.06f));
		TintPart(FerryParts[7],  FLinearColor(0.38f, 0.36f, 0.33f));
		TintPart(FerryParts[8],  FLinearColor(0.76f, 0.72f, 0.64f));
		TintPart(FerryParts[9],  FLinearColor(0.86f, 0.84f, 0.78f));
		TintPart(FerryParts[10], FLinearColor(0.22f, 0.24f, 0.28f));
		TintPart(FerryParts[11], FLinearColor(0.32f, 0.36f, 0.42f));
	}
}

void ABosphorusWaterVolume::BuildWaterGridMesh()
{
	const int32 N = FMath::Clamp(GridResolution, 16, 128);
	const int32 VertsPerRow = N + 1;
	const int32 TotalVerts = VertsPerRow * VertsPerRow;

	BaseWaterVertices.SetNumUninitialized(TotalVerts);
	AnimatedWaterVertices.SetNumUninitialized(TotalVerts);
	WaterNormals.Init(FVector::UpVector, TotalVerts);
	BaseWaterUVs.SetNumUninitialized(TotalVerts);
	AnimatedWaterUVs.SetNumUninitialized(TotalVerts);
	WaterVertexColors.SetNumUninitialized(TotalVerts);
	WaterTangents.Init(FProcMeshTangent(FVector::ForwardVector, false), TotalVerts);
	WaterTriangles.Reset(N * N * 6);

	// Nearshore animated 64x64 grid: covers Eminönü -> Beşiktaş shoreline (Y=-1240cm) out across the entire Golden Horn & Bosphorus strait
	const float NearStartX  = -25000.0f;
	const float NearExtentX = 170000.0f; // 1.7 km animated coastal strip
	const float StartY      = -1240.0f;  // Directly hugs the coastal promenade kordon
	const float NearExtentY = 85000.0f;  // 850m animated bay width covering Galata Bridge & Karaköy bay
	const float BaseZ       = -120.0f;   // Unmistakably right below road elevation (Z = 0.0f)

	for (int32 iy = 0; iy <= N; ++iy)
	{
		const float V = (float)iy / (float)N;
		// Depth gradient: bright shallow coastal turquoise near shores -> deep Bosphorus navy out in channel
		const float ShoreBlend = FMath::SmoothStep(0.0f, 0.45f, V);
		const FLinearColor RowColor = FMath::Lerp(ShallowTurquoiseColor, DeepBosphorusNavyColor, ShoreBlend);

		for (int32 ix = 0; ix <= N; ++ix)
		{
			const float U = (float)ix / (float)N;
			const int32 Idx = iy * VertsPerRow + ix;

			const FVector Pos(
				NearStartX + U * NearExtentX,
				StartY - V * NearExtentY,
				BaseZ);

			BaseWaterVertices[Idx] = Pos;
			AnimatedWaterVertices[Idx] = Pos;
			BaseWaterUVs[Idx] = FVector2D(U * 32.0f, V * 32.0f);
			AnimatedWaterUVs[Idx] = BaseWaterUVs[Idx];
			WaterVertexColors[Idx] = RowColor;
		}
	}

	for (int32 iy = 0; iy < N; ++iy)
	{
		for (int32 ix = 0; ix < N; ++ix)
		{
			const int32 I00 = iy * VertsPerRow + ix;
			const int32 I10 = iy * VertsPerRow + (ix + 1);
			const int32 I01 = (iy + 1) * VertsPerRow + ix;
			const int32 I11 = (iy + 1) * VertsPerRow + (ix + 1);

			WaterTriangles.Add(I00);
			WaterTriangles.Add(I01);
			WaterTriangles.Add(I11);

			WaterTriangles.Add(I00);
			WaterTriangles.Add(I11);
			WaterTriangles.Add(I10);
		}
	}

	// Vivid glossy PBR water material (Roughness=0.08, Specular=0.92) for bright Bosphorus sun/sky reflections
	UMaterialInstanceDynamic* WaterPBRMat = FIstanbulMeshUtils::CreatePBRMaterial(
		this, FLinearColor::White, 0.05f, 0.08f, 0.92f);

	WaterProcMesh->CreateMeshSection_LinearColor(
		0, AnimatedWaterVertices, WaterTriangles, WaterNormals,
		AnimatedWaterUVs, WaterVertexColors, WaterTangents, false);
	if (WaterPBRMat)
	{
		WaterProcMesh->SetMaterial(0, WaterPBRMat);
	}

	// Section 1: Massive 5000m x 5000m Horizon Sea Surface so water stretches seamlessly to the far horizon
	{
		const float HalfW = FMath::Max(WaterExtentCm.X * 0.5f, 250000.0f);
		const float DepthH = FMath::Max(WaterExtentCm.Y, 500000.0f);
		const float OuterZ = -128.0f;

		TArray<FVector> OuterVerts = {
			FVector(-HalfW, StartY,         OuterZ),
			FVector( HalfW, StartY,         OuterZ),
			FVector(-HalfW, StartY - DepthH, OuterZ),
			FVector( HalfW, StartY - DepthH, OuterZ)
		};
		TArray<int32> OuterTris = { 0, 2, 3, 0, 3, 1 };
		TArray<FVector> OuterNorms;
		OuterNorms.Init(FVector::UpVector, 4);
		TArray<FVector2D> OuterUVs = { FVector2D(0, 0), FVector2D(100, 0), FVector2D(0, 100), FVector2D(100, 100) };
		TArray<FLinearColor> OuterCols = { ShallowTurquoiseColor, ShallowTurquoiseColor, DeepBosphorusNavyColor, DeepBosphorusNavyColor };
		TArray<FProcMeshTangent> OuterTangents;
		OuterTangents.Init(FProcMeshTangent(FVector::ForwardVector, false), 4);

		WaterProcMesh->CreateMeshSection_LinearColor(
			1, OuterVerts, OuterTris, OuterNorms, OuterUVs, OuterCols, OuterTangents, false);
		if (WaterPBRMat)
		{
			WaterProcMesh->SetMaterial(1, WaterPBRMat);
		}
	}

	// Section 2: White Shoreline Foam & Surf Strip hugging Eminönü/Beşiktaş seawall + Karaköy quay + Kız Kulesi islet
	{
		FIstanbulMeshSectionData FoamSection;
		const FLinearColor FoamWhite(0.86f, 0.96f, 1.0f, 1.0f);
		// Eminönü - Kabataş - Beşiktaş sea-wall surf strip
		FIstanbulMeshUtils::AppendBeveledBox(FoamSection,
			FTransform(FRotator::ZeroRotator, FVector(45000.0f, -1380.0f, -114.0f)),
			FVector(68000.0f, 150.0f, 6.0f), 2.0f, FoamWhite);
		// Karaköy North shore surf strip (Y = -40760cm)
		FIstanbulMeshUtils::AppendBeveledBox(FoamSection,
			FTransform(FRotator::ZeroRotator, FVector(12000.0f, -40740.0f, -114.0f)),
			FVector(32000.0f, 150.0f, 6.0f), 2.0f, FoamWhite);
		// Surf collar around Kız Kulesi rocky islet
		FIstanbulMeshUtils::AppendCylinder(FoamSection,
			FTransform(FRotator::ZeroRotator, FVector(24000.0f, -14500.0f, -114.0f)),
			1620.0f, 6.0f, 24, FoamWhite);

		UMaterialInstanceDynamic* FoamMat = FIstanbulMeshUtils::CreatePBRMaterial(
			this, FLinearColor::White, 0.0f, 0.35f, 0.65f);
		FoamSection.CommitToComponent(WaterProcMesh, 2, false, FoamMat);
	}
}

void ABosphorusWaterVolume::UpdateWaterWaveVertices(float TimeSec)
{
	if (BaseWaterVertices.Num() == 0) return;

	const int32 TotalVerts = BaseWaterVertices.Num();
	const FVector2D UVScroll(TimeSec * 0.12f, TimeSec * 0.08f);

	for (int32 i = 0; i < TotalVerts; ++i)
	{
		const FVector& Base = BaseWaterVertices[i];
		// Two layered sine waves (primary Bosphorus current swell + secondary chop)
		const float Wave1 = FMath::Sin(Base.X * 0.0014f + TimeSec * 1.9f) * FMath::Cos(Base.Y * 0.0018f + TimeSec * 1.3f);
		const float Wave2 = FMath::Sin((Base.X - Base.Y) * 0.0032f - TimeSec * 2.6f) * 0.45f;
		const float DispZ = (Wave1 + Wave2) * WaveAmplitudeCm;

		AnimatedWaterVertices[i] = FVector(Base.X, Base.Y, Base.Z + DispZ);
		AnimatedWaterUVs[i] = BaseWaterUVs[i] + UVScroll;
	}

	WaterProcMesh->UpdateMeshSection_LinearColor(
		0, AnimatedWaterVertices, WaterNormals, AnimatedWaterUVs, WaterVertexColors, WaterTangents);
}

void ABosphorusWaterVolume::BuildQuayWallAndDistantSilhouettes()
{
	FIstanbulMeshSectionData QuaySection;
	FIstanbulMeshSectionData SilhouetteSection;

	// 1. Low-profile Seaside Quay Wall & Stone Coping along Y = -1240cm (split at Galata Bridge X = 8800cm)
	// Kept low (Z <= +22cm) so the sea surface at Z = -120cm stays 100% unobstructed from driver eye level!
	const FLinearColor QuayStoneCol(0.48f, 0.46f, 0.42f);
	const FLinearColor CopingCol(0.66f, 0.64f, 0.58f);

	// West Promenade Quay: X = -16000 .. +7600 (Center = -4200, HalfExtent = 11800)
	FIstanbulMeshUtils::AppendBeveledBox(QuaySection,
		FTransform(FRotator::ZeroRotator, FVector(-4200.0f, -1240.0f, -55.0f)),
		FVector(11800.0f, 55.0f, 68.0f), 4.0f, QuayStoneCol);
	FIstanbulMeshUtils::AppendBeveledBox(QuaySection,
		FTransform(FRotator::ZeroRotator, FVector(-4200.0f, -1230.0f, 16.0f)),
		FVector(11800.0f, 64.0f, 6.0f), 2.0f, CopingCol);

	// East Promenade Quay: X = +10000 .. +105000 (Center = +57500, HalfExtent = 47500)
	FIstanbulMeshUtils::AppendBeveledBox(QuaySection,
		FTransform(FRotator::ZeroRotator, FVector(57500.0f, -1240.0f, -55.0f)),
		FVector(47500.0f, 55.0f, 68.0f), 4.0f, QuayStoneCol);
	FIstanbulMeshUtils::AppendBeveledBox(QuaySection,
		FTransform(FRotator::ZeroRotator, FVector(57500.0f, -1230.0f, 16.0f)),
		FVector(47500.0f, 64.0f, 6.0f), 2.0f, CopingCol);

	// Quay HISM low bollards spaced every 15m (leaving Galata Bridge gap at X=7600..10000)
	if (QuayBollardHISM && QuayBollardHISM->GetStaticMesh())
	{
		QuayBollardHISM->ClearInstances();
		if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* BollardMID = UMaterialInstanceDynamic::Create(BaseMat, this);
			BollardMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.72f, 0.04f));
			QuayBollardHISM->SetMaterial(0, BollardMID);
		}
		for (int32 b = 0; b < 68; ++b)
		{
			const float BX = -14000.0f + b * 1700.0f;
			if (BX > 7400.0f && BX < 10200.0f) continue;
			QuayBollardHISM->AddInstance(FTransform(
				FRotator::ZeroRotator,
				FVector(BX, -1200.0f, 44.0f),
				FVector(0.24f, 0.24f, 0.68f)));
		}
	}

	// 2. Distant Extruded Silhouettes at 2.2 - 3.0 km distance across the water (built purely from 2D point arrays!)
	// 2a) Distant Süleymaniye Mosque Silhouette (4 Minarets + Cascading Ottoman Domes at Y = -2200m / 2.2 km)
	const TArray<FVector2D> SuleymaniyeProfile = {
		FVector2D(-18000.0f,  1800.0f),
		FVector2D(-15000.0f,  2200.0f),
		FVector2D(-14500.0f, 11500.0f), // Outer Minaret 1
		FVector2D(-14000.0f, 13200.0f), // Minaret Spire
		FVector2D(-13500.0f, 11500.0f),
		FVector2D(-13000.0f,  2600.0f),
		FVector2D( -9500.0f,  3200.0f),
		FVector2D( -9000.0f, 14800.0f), // Inner Tall Minaret 2 (3 Şerefe)
		FVector2D( -8500.0f, 16800.0f),
		FVector2D( -8000.0f, 14800.0f),
		FVector2D( -7500.0f,  4200.0f),
		FVector2D( -5200.0f,  6800.0f), // Semi-dome W
		FVector2D( -3200.0f,  9800.0f), // Main Dome Shoulder
		FVector2D(     0.0f, 12600.0f), // Süleymaniye Central Dome Crown
		FVector2D(  3200.0f,  9800.0f),
		FVector2D(  5200.0f,  6800.0f), // Semi-dome E
		FVector2D(  7500.0f,  4200.0f),
		FVector2D(  8000.0f, 14800.0f), // Inner Tall Minaret 3
		FVector2D(  8500.0f, 16800.0f),
		FVector2D(  9000.0f, 14800.0f),
		FVector2D(  9500.0f,  3200.0f),
		FVector2D( 13000.0f,  2600.0f),
		FVector2D( 13500.0f, 11500.0f), // Outer Minaret 4
		FVector2D( 14000.0f, 13200.0f),
		FVector2D( 14500.0f, 11500.0f),
		FVector2D( 15000.0f,  2200.0f),
		FVector2D( 18000.0f,  1800.0f)
	};
	FIstanbulMeshUtils::AppendExtrudedProfileXZ(
		SilhouetteSection,
		FTransform(FRotator::ZeroRotator, FVector(4000.0f, -220000.0f, -40.0f)),
		SuleymaniyeProfile, 400.0f, DistantSilhouetteColor);

	// 2b) Distant Galata / Asian Hill Tower Silhouette (at Y = -2400m / 2.4 km)
	const TArray<FVector2D> DistantTowerProfile = {
		FVector2D(-9000.0f,  1400.0f),
		FVector2D(-3200.0f,  2800.0f),
		FVector2D(-2200.0f, 11200.0f),
		FVector2D(-2600.0f, 11800.0f), // Corbelled Balcony
		FVector2D(-2000.0f, 12800.0f),
		FVector2D(    0.0f, 16500.0f), // Conical Spire Peak
		FVector2D( 2000.0f, 12800.0f),
		FVector2D( 2600.0f, 11800.0f),
		FVector2D( 2200.0f, 11200.0f),
		FVector2D( 3200.0f,  2800.0f),
		FVector2D( 9000.0f,  1400.0f)
	};
	FIstanbulMeshUtils::AppendExtrudedProfileXZ(
		SilhouetteSection,
		FTransform(FRotator::ZeroRotator, FVector(22000.0f, -240000.0f, -40.0f)),
		DistantTowerProfile, 400.0f, DistantSilhouetteColor * 0.94f);

	// 2c) Distant Bosphorus Suspension Bridge Silhouette (Two Towers + Parabolic Main Cable Curve at 2.8 km)
	TArray<FVector2D> BridgeSilhouetteProfile;
	BridgeSilhouetteProfile.Add(FVector2D(-48000.0f, 3800.0f));
	BridgeSilhouetteProfile.Add(FVector2D(-28000.0f, 4400.0f));
	// Tower 1
	BridgeSilhouetteProfile.Add(FVector2D(-27200.0f, 19500.0f));
	BridgeSilhouetteProfile.Add(FVector2D(-25600.0f, 19500.0f));
	// Parabolic catenary cable sag between Tower 1 (-25600) and Tower 2 (+25600)
	for (int32 s = 1; s < 16; ++s)
	{
		const float T = (float)s / 16.0f;
		const float X = FMath::Lerp(-25600.0f, 25600.0f, T);
		const float Sag = 4.0f * T * (1.0f - T) * 13200.0f;
		BridgeSilhouetteProfile.Add(FVector2D(X, 19500.0f - Sag));
	}
	// Tower 2
	BridgeSilhouetteProfile.Add(FVector2D( 25600.0f, 19500.0f));
	BridgeSilhouetteProfile.Add(FVector2D( 27200.0f, 19500.0f));
	BridgeSilhouetteProfile.Add(FVector2D( 28000.0f,  4400.0f));
	BridgeSilhouetteProfile.Add(FVector2D( 48000.0f,  3800.0f));

	FIstanbulMeshUtils::AppendExtrudedProfileXZ(
		SilhouetteSection,
		FTransform(FRotator::ZeroRotator, FVector(34000.0f, -280000.0f, -40.0f)),
		BridgeSilhouetteProfile, 500.0f, DistantSilhouetteColor * 0.88f);

	UMaterialInterface* ProcMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	QuaySection.CommitToComponent(QuayAndSilhouetteProcMesh, 0, true, ProcMat);
	SilhouetteSection.CommitToComponent(QuayAndSilhouetteProcMesh, 1, false, ProcMat);
}

void ABosphorusWaterVolume::OnWaterKillBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* PlayerPawn = Cast<APawn>(OtherActor);
	if (!PlayerPawn || !PlayerPawn->IsPlayerControlled())
	{
		return;
	}

	UE_LOG(LogIstanbulTaxi, Warning,
		TEXT("ABosphorusWaterVolume: Player vehicle entered Bosphorus water at %s! Respawning at last dry checkpoint %s."),
		*PlayerPawn->GetActorLocation().ToString(), *LastSafeDryTransform.GetLocation().ToString());

	if (ATaxiPawn* Taxi = Cast<ATaxiPawn>(PlayerPawn))
	{
		Taxi->ResetVehicle();
		if (USkeletalMeshComponent* Skel = Taxi->GetMesh())
		{
			Skel->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Skel->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
	PlayerPawn->SetActorTransform(LastSafeDryTransform, false, nullptr, ETeleportType::TeleportPhysics);
}

void ABosphorusWaterVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	WaveTime += DeltaSeconds;
	WaveUpdateAccum += DeltaSeconds;
	CheckpointSampleAccum += DeltaSeconds;

	// Update 64x64 water mesh at capped rate (~15 Hz) for cheap frame time
	if (WaveUpdateAccum >= WaveUpdateInterval)
	{
		WaveUpdateAccum = 0.0f;
		UpdateWaterWaveVertices(WaveTime);
	}

	// Periodically record player's last safe dry-land checkpoint transform (Eminönü, Bridges, Karaköy)
	if (CheckpointSampleAccum >= 1.2f)
	{
		CheckpointSampleAccum = 0.0f;
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			const FVector Loc = Player->GetActorLocation();
			const bool bOnDryShore = (Loc.Y > -1150.0f) || (Loc.Y < -40800.0f) || (Loc.Z > 150.0f);
			if (bOnDryShore && Loc.Z > -10.0f)
			{
				LastSafeDryTransform = FTransform(
					FRotator(0.0f, Player->GetActorRotation().Yaw, 0.0f),
					Loc + FVector(0.0f, 0.0f, 35.0f));
			}
		}
	}

	// Cruise the built-in Şehir Hatları Ferry along the Golden Horn & Bosphorus mouth
	if (FerryRoot)
	{
		FerryProgress = FMath::Fmod(FerryProgress + DeltaSeconds * 480.0f, 80000.0f);
		const float FerryX = -10000.0f + FerryProgress;
		const float Pitch = FMath::Sin(WaveTime * 1.4f) * 1.2f;
		const float Roll  = FMath::Cos(WaveTime * 1.1f) * 1.8f;
		FerryRoot->SetRelativeLocationAndRotation(
			FVector(FerryX, -7500.0f, -110.0f + FMath::Sin(WaveTime * 1.4f) * 12.0f),
			FRotator(Pitch, 0.0f, Roll));
	}
}
