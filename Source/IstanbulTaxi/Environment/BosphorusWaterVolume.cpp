#include "Environment/BosphorusWaterVolume.h"
#include "Core/IstanbulMeshUtils.h"
#include "Vehicle/TaxiPawn.h"
#include "IstanbulTaxi.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
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

	// Water Kill / Respawn Trigger Box spanning the Bosphorus below quay level
	WaterKillTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterKillTrigger"));
	WaterKillTrigger->SetupAttachment(Root);
	WaterKillTrigger->SetRelativeLocation(FVector(17000.0f, -13600.0f, -140.0f));
	WaterKillTrigger->SetBoxExtent(FVector(24000.0f, 12000.0f, 130.0f));
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
	FerryRoot->SetRelativeLocation(FVector(4000.0f, -4200.0f, -20.0f));

	FerryParts.Add(AddPart(TEXT("Ferry_Hull"),      FerryRoot, Cube, FVector(0.0f, 0.0f,  90.0f), FVector(34.0f, 7.6f, 1.8f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Deck1"),     FerryRoot, Cube, FVector(0.0f, 0.0f, 250.0f), FVector(28.0f, 6.8f, 1.5f)));
	FerryParts.Add(AddPart(TEXT("Ferry_WinBand1"),  FerryRoot, Cube, FVector(0.0f, 0.0f, 260.0f), FVector(26.5f, 7.0f, 0.65f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Deck2"),     FerryRoot, Cube, FVector(-150.0f, 0.0f, 395.0f), FVector(20.0f, 5.8f, 1.3f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Bridge"),    FerryRoot, Cube, FVector(720.0f, 0.0f, 495.0f), FVector(4.2f, 6.2f, 1.1f)));
	FerryParts.Add(AddPart(TEXT("Ferry_Funnel"),    FerryRoot, Cyl,  FVector(-120.0f, 0.0f, 590.0f), FVector(1.8f, 1.8f, 3.2f)));
	FerryParts.Add(AddPart(TEXT("Ferry_FunnelTip"), FerryRoot, Cyl,  FVector(-120.0f, 0.0f, 745.0f), FVector(1.85f, 1.85f, 0.65f)));

	// Kız Kulesi (Maiden's Tower) islet & lighthouse out in the Bosphorus strait
	const FVector IsletBase(14500.0f, -9600.0f, -25.0f);
	FerryParts.Add(AddPart(TEXT("KizKulesi_Islet"),   Root, Cyl,  IsletBase + FVector(0, 0,  45.0f), FVector(18.0f, 18.0f, 0.9f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Bastion"), Root, Cube, IsletBase + FVector(0, 0, 210.0f), FVector(7.5f, 7.5f, 2.6f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Tower"),   Root, Cube, IsletBase + FVector(0, 0, 620.0f), FVector(3.4f, 3.4f, 6.2f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Balcony"), Root, Cyl,  IsletBase + FVector(0, 0, 960.0f), FVector(4.4f, 4.4f, 0.55f)));
	FerryParts.Add(AddPart(TEXT("KizKulesi_Spire"),   Root, Cone, IsletBase + FVector(0, 0, 1180.0f), FVector(3.6f, 3.6f, 3.8f)));

	LastSafeDryTransform = FTransform(FRotator::ZeroRotator, FVector(1200.0f, 0.0f, 65.0f));
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

	const float StartX = -6000.0f;
	const float StartY = -1620.0f; // Hugs the coastal quay wall
	const float BaseZ  = -42.0f;

	for (int32 iy = 0; iy <= N; ++iy)
	{
		const float V = (float)iy / (float)N;
		// Depth gradient: V=0 near the Eminönü/Karaköy quay is shallow turquoise, V=1 out in the strait is deep Bosphorus navy
		const float DepthAlpha = FMath::SmoothStep(0.0f, 1.0f, FMath::Pow(V, 0.65f));
		const FLinearColor RowColor = FMath::Lerp(ShallowTurquoiseColor, DeepBosphorusNavyColor, DepthAlpha);

		for (int32 ix = 0; ix <= N; ++ix)
		{
			const float U = (float)ix / (float)N;
			const int32 Idx = iy * VertsPerRow + ix;

			const FVector Pos(
				StartX + U * WaterExtentCm.X,
				StartY - V * WaterExtentCm.Y,
				BaseZ);

			BaseWaterVertices[Idx] = Pos;
			AnimatedWaterVertices[Idx] = Pos;
			BaseWaterUVs[Idx] = FVector2D(U * 16.0f, V * 16.0f);
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

	UMaterialInterface* WaterMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	WaterProcMesh->CreateMeshSection_LinearColor(
		0, AnimatedWaterVertices, WaterTriangles, WaterNormals,
		AnimatedWaterUVs, WaterVertexColors, WaterTangents, false);
	if (WaterMat)
	{
		WaterProcMesh->SetMaterial(0, WaterMat);
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

	// 1. Coastal Quay Wall & Stone Coping Edge + Guard Rail along Y = -1580cm (X = -40m .. +380m)
	const FLinearColor QuayStoneCol(0.48f, 0.46f, 0.42f);
	const FLinearColor CopingCol(0.62f, 0.60f, 0.56f);
	const FLinearColor GuardRailCol(0.14f, 0.16f, 0.18f);

	FIstanbulMeshUtils::AppendBeveledBox(QuaySection,
		FTransform(FRotator::ZeroRotator, FVector(17000.0f, -1610.0f, -18.0f)),
		FVector(21000.0f, 45.0f, 42.0f), 4.0f, QuayStoneCol);
	FIstanbulMeshUtils::AppendBeveledBox(QuaySection,
		FTransform(FRotator::ZeroRotator, FVector(17000.0f, -1585.0f, 28.0f)),
		FVector(21000.0f, 52.0f, 8.0f), 2.0f, CopingCol);
	FIstanbulMeshUtils::AppendBeveledBox(QuaySection,
		FTransform(FRotator::ZeroRotator, FVector(17000.0f, -1575.0f, 86.0f)),
		FVector(21000.0f, 4.0f, 4.0f), 1.0f, GuardRailCol);

	// Quay HISM bollards every 12m
	if (QuayBollardHISM && QuayBollardHISM->GetStaticMesh())
	{
		QuayBollardHISM->ClearInstances();
		if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* BollardMID = UMaterialInstanceDynamic::Create(BaseMat, this);
			BollardMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.72f, 0.04f));
			QuayBollardHISM->SetMaterial(0, BollardMID);
		}
		for (int32 b = 0; b < 32; ++b)
		{
			const float BX = -2000.0f + b * 1200.0f;
			QuayBollardHISM->AddInstance(FTransform(
				FRotator::ZeroRotator,
				FVector(BX, -1520.0f, 52.0f),
				FVector(0.26f, 0.26f, 0.90f)));
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

	// Periodically record player's last safe dry-land checkpoint transform
	if (CheckpointSampleAccum >= 1.2f)
	{
		CheckpointSampleAccum = 0.0f;
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			const FVector Loc = Player->GetActorLocation();
			if (Loc.Y > -1350.0f && Loc.Z > -10.0f)
			{
				LastSafeDryTransform = FTransform(
					FRotator(0.0f, Player->GetActorRotation().Yaw, 0.0f),
					Loc + FVector(0.0f, 0.0f, 35.0f));
			}
		}
	}

	// Cruise the Şehir Hatları Ferry along the Bosphorus
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
