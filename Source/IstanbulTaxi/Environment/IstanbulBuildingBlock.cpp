#include "Environment/IstanbulBuildingBlock.h"
#include "Core/IstanbulMeshUtils.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AIstanbulBuildingBlock::AIstanbulBuildingBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	BatchedFacadeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BatchedFacadeMesh"));
	BatchedFacadeMesh->SetupAttachment(Root);
	BatchedFacadeMesh->bUseComplexAsSimpleCollision = true;
	BatchedFacadeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BatchedFacadeMesh->SetCollisionProfileName(TEXT("BlockAll"));
	BatchedFacadeMesh->SetCastShadow(true);
	BatchedFacadeMesh->LDMaxDrawDistance = 48000.0f;

	DetailCubeHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DetailCubeHISM"));
	DetailCubeHISM->SetupAttachment(Root);
	DetailCubeHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetailCubeHISM->SetCullDistances(0, 16000);

	RooftopCylinderHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RooftopCylinderHISM"));
	RooftopCylinderHISM->SetupAttachment(Root);
	RooftopCylinderHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RooftopCylinderHISM->SetCullDistances(0, 18000);

	if (UStaticMesh* Cube = FIstanbulMeshUtils::LoadBasicShapeSafe(TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		DetailCubeHISM->SetStaticMesh(Cube);
	}
	if (UStaticMesh* Cyl = FIstanbulMeshUtils::LoadBasicShapeSafe(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		RooftopCylinderHISM->SetStaticMesh(Cyl);
	}
}

void AIstanbulBuildingBlock::BuildBlockFromSpecs(const TArray<FIstanbulBuildingSpec>& InSpecs)
{
	BatchedFacadeMesh->ClearAllMeshSections();
	DetailCubeHISM->ClearInstances();
	RooftopCylinderHISM->ClearInstances();

	for (UBoxComponent* Box : SimpleCollisionBoxes)
	{
		if (Box) Box->DestroyComponent();
	}
	SimpleCollisionBoxes.Reset();

	for (UTextRenderComponent* Txt : ShopSignTexts)
	{
		if (Txt) Txt->DestroyComponent();
	}
	ShopSignTexts.Reset();
	BuildingCollisionBoxes.Reset();

	GeneratedBuildingCount = 0;
	GeneratedCumbaCount = 0;
	GeneratedHISMPropCount = 0;

	if (UMaterialInstanceDynamic* CubeMID = FIstanbulMeshUtils::CreatePBRMaterial(this, FLinearColor(0.76f, 0.78f, 0.80f), 0.25f, 0.45f))
	{
		DetailCubeHISM->SetMaterial(0, CubeMID);
	}
	if (UMaterialInstanceDynamic* CylMID = FIstanbulMeshUtils::CreatePBRMaterial(this, FLinearColor(0.62f, 0.66f, 0.70f), 0.65f, 0.30f))
	{
		RooftopCylinderHISM->SetMaterial(0, CylMID);
	}

	FIstanbulMeshSectionData FacadeSection;   // Slot 0: Walls, stone plinths, recessed frames, shutters, cumbas, awnings, signs, satellite dishes
	FIstanbulMeshSectionData GlassSection;    // Slot 1: Dark reflective recessed window panes & shopfront glass
	FIstanbulMeshSectionData RoofTileSection; // Slot 2: Pitched terracotta tile roofs, cumba caps, brick chimneys

	static const FLinearColor PlasterPalette[8] = {
		FLinearColor(0.78f, 0.58f, 0.34f), // Ochre
		FLinearColor(0.84f, 0.80f, 0.70f), // Cream Limestone
		FLinearColor(0.68f, 0.34f, 0.26f), // Faded Terracotta
		FLinearColor(0.56f, 0.58f, 0.60f), // Pera Warm Grey
		FLinearColor(0.46f, 0.58f, 0.68f), // Balat Muted Blue
		FLinearColor(0.56f, 0.64f, 0.48f), // Muted Pistachio
		FLinearColor(0.74f, 0.64f, 0.52f), // Sand Plaster
		FLinearColor(0.64f, 0.46f, 0.42f)  // Dusty Rose
	};

	static const TCHAR* TurkishShopNames[7] = {
		TEXT("TEKEL BAYİ"),
		TEXT("SİMİT SARAYI"),
		TEXT("ECZANE"),
		TEXT("DÖNERCİ"),
		TEXT("KIRAATHANE"),
		TEXT("TAŞ FIRIN"),
		TEXT("BAKKAL")
	};

	static const FLinearColor ShopBoardColors[7] = {
		FLinearColor(0.88f, 0.68f, 0.04f), // Tekel Bayi Yellow
		FLinearColor(0.72f, 0.12f, 0.10f), // Simit Sarayı Crimson
		FLinearColor(0.06f, 0.58f, 0.22f), // Eczane Green
		FLinearColor(0.84f, 0.22f, 0.08f), // Dönerci Red-Orange
		FLinearColor(0.16f, 0.38f, 0.24f), // Kıraathane Classic Green
		FLinearColor(0.82f, 0.48f, 0.10f), // Taş Fırın Warm Ochre
		FLinearColor(0.10f, 0.36f, 0.76f)  // Bakkal Blue
	};

	static const FLinearColor ShutterPalette[4] = {
		FLinearColor(0.16f, 0.34f, 0.22f), // Classic Ottoman Green
		FLinearColor(0.36f, 0.22f, 0.13f), // Walnut Wood
		FLinearColor(0.18f, 0.28f, 0.42f), // Bosphorus Slate Blue
		FLinearColor(0.48f, 0.20f, 0.16f)  // Anatolian Burgundy
	};

	for (int32 Idx = 0; Idx < InSpecs.Num(); ++Idx)
	{
		const FIstanbulBuildingSpec& Spec = InSpecs[Idx];
		FRandomStream Rng(Spec.StyleSeed + Idx * 37);

		const float WidthCm = FMath::Max(Spec.FrontageWidthM * 100.0f, 450.0f);
		const float DepthCm = FMath::Max(Spec.DepthM * 100.0f, 500.0f);
		const int32 Floors  = FMath::Clamp(Spec.Floors, 2, 8);
		const float FloorHCm = 320.0f;
		const float TotalHCm = Floors * FloorHCm;

		const FRotator Rot(0.0f, Spec.YawDegrees, 0.0f);
		const FTransform BldgXf(Rot, Spec.BaseLocation);

		const FLinearColor PlasterCol  = PlasterPalette[Rng.RandRange(0, 7)] * Rng.FRandRange(0.92f, 1.06f);
		const FLinearColor StoneCol    = FLinearColor(0.46f, 0.43f, 0.39f) * Rng.FRandRange(0.90f, 1.08f);
		const FLinearColor WoodCumba   = FLinearColor(0.38f, 0.24f, 0.14f) * Rng.FRandRange(0.90f, 1.10f);
		const FLinearColor CorniceCol  = PlasterCol * 1.14f;
		const FLinearColor FrameCol    = FLinearColor(0.86f, 0.84f, 0.78f);
		const FLinearColor ShutterCol  = ShutterPalette[Rng.RandRange(0, 3)];
		const FLinearColor Terracotta  = FLinearColor(0.64f, 0.24f, 0.14f) * Rng.FRandRange(0.92f, 1.08f);
		const FLinearColor BrickChimney(0.52f, 0.22f, 0.16f);

		// 1. Solid Blocking UBoxComponent per building (BlockAll) so vehicles NEVER clip through any building!
		UBoxComponent* ColBox = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("BldgCol_%d"), Idx));
		if (ColBox)
		{
			ColBox->SetupAttachment(Root);
			ColBox->RegisterComponent();
			ColBox->SetWorldLocationAndRotation(Spec.BaseLocation + FVector(0.0f, 0.0f, TotalHCm * 0.5f), Rot);
			ColBox->SetBoxExtent(FVector(DepthCm * 0.5f + 12.0f, WidthCm * 0.5f + 6.0f, TotalHCm * 0.5f));
			ColBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			ColBox->SetCollisionObjectType(ECC_WorldStatic);
			ColBox->SetCollisionProfileName(TEXT("BlockAll"));
			SimpleCollisionBoxes.Add(ColBox);
			BuildingCollisionBoxes.Add(ColBox->Bounds.GetBox());
		}

		// 2. Ground-Floor Stone Plinth + Upper Plaster/Brick Mass
		FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
			FTransform(FVector(0.0f, 0.0f, FloorHCm * 0.5f)) * BldgXf,
			FVector(DepthCm * 0.5f + 4.0f, WidthCm * 0.5f + 2.0f, FloorHCm * 0.5f), 4.0f, StoneCol);

		const float UpperHCm = (Floors - 1) * FloorHCm;
		FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
			FTransform(FVector(0.0f, 0.0f, FloorHCm + UpperHCm * 0.5f)) * BldgXf,
			FVector(DepthCm * 0.5f, WidthCm * 0.5f, UpperHCm * 0.5f), 5.0f, PlasterCol);

		// 3. Horizontal String Courses (Kat Silmeleri), Dentil Molding & Corner Vertical Quoin Pilasters
		for (int32 f = 1; f <= Floors; ++f)
		{
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 6.0f, 0.0f, f * FloorHCm)) * BldgXf,
				FVector(9.0f, WidthCm * 0.5f + 8.0f, 9.5f), 2.5f, CorniceCol);
		}
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 4.5f, Side * (WidthCm * 0.5f - 18.0f), TotalHCm * 0.5f)) * BldgXf,
				FVector(7.0f, 18.0f, TotalHCm * 0.5f), 2.5f, CorniceCol);
		}

		// 4. Ground-Floor Turkish Shopfront (Esnaf Dükkânı), 3D Striped Fabric Awning (Tente) & Illuminated Signboard
		if (Spec.GroundFloorType == EIstanbulGroundFloorType::ShopfrontAwning)
		{
			const int32 ShopTypeIdx = (Idx + FMath::Abs(Spec.StyleSeed)) % 7;
			const FLinearColor BoardCol = ShopBoardColors[ShopTypeIdx];
			const FLinearColor CreamStripe(0.95f, 0.92f, 0.84f);

			// Recessed Shop Entryway Door on Left + Large Display Window on Right
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f - 6.0f, -WidthCm * 0.30f, 118.0f)) * BldgXf,
				FVector(12.0f, 52.0f, 115.0f), 2.5f, WoodCumba * 0.85f);
			FIstanbulMeshUtils::AppendBeveledBox(GlassSection,
				FTransform(FVector(DepthCm * 0.5f - 8.0f, -WidthCm * 0.30f, 122.0f)) * BldgXf,
				FVector(3.0f, 38.0f, 95.0f), 1.0f, FLinearColor(0.04f, 0.08f, 0.14f));

			// Large Recessed Display Window + Window Sill + Interior Warm Counter Glow
			FIstanbulMeshUtils::AppendBeveledBox(GlassSection,
				FTransform(FVector(DepthCm * 0.5f - 4.0f, WidthCm * 0.08f, 132.0f)) * BldgXf,
				FVector(4.0f, WidthCm * 0.28f, 92.0f), 1.5f, FLinearColor(0.05f, 0.10f, 0.16f));
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 6.0f, WidthCm * 0.08f, 36.0f)) * BldgXf,
				FVector(10.0f, WidthCm * 0.30f, 8.0f), 2.0f, StoneCol * 1.15f);

			// 3D Striped Fabric Awning (Tente) with Scalloped Valance & Metal Support Arms
			// Transform maps Awning local +Y to Building +X (outward) and Awning local X to Building Y (frontage)
			const FTransform AwningWallXf = FTransform(
				FRotator(0.0f, -90.0f, 0.0f),
				FVector(DepthCm * 0.5f + 6.0f, 0.0f, 248.0f)) * BldgXf;
			const float AwningProjectionCm = FMath::Min(92.0f, FMath::Max(Spec.MaxAllowedProjectionCm, 68.0f));
			FIstanbulMeshUtils::AppendStripedShopAwning(
				FacadeSection, AwningWallXf,
				WidthCm * 0.82f, AwningProjectionCm, 34.0f,
				BoardCol, CreamStripe);

			// Illuminated Shop Signboard Box + Frame Above the Awning
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 10.0f, 0.0f, 278.0f)) * BldgXf,
				FVector(8.5f, WidthCm * 0.42f, 24.0f), 2.5f, BoardCol);
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 16.5f, 0.0f, 278.0f)) * BldgXf,
				FVector(2.5f, WidthCm * 0.39f, 18.0f), 1.0f, BoardCol * 1.22f);

			// Distance-culled Turkish Shop Sign Text ("TEKEL BAYİ", "SİMİT SARAYI", "ECZANE", "DÖNERCİ", "KIRAATHANE")
			UTextRenderComponent* SignText = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("ShopSign_%d"), Idx));
			if (SignText)
			{
				SignText->SetupAttachment(Root);
				SignText->RegisterComponent();
				SignText->SetWorldLocationAndRotation(
					BldgXf.TransformPosition(FVector(DepthCm * 0.5f + 21.0f, 0.0f, 278.0f)),
					Rot);
				SignText->SetText(FText::FromString(TurkishShopNames[ShopTypeIdx]));
				SignText->SetHorizontalAlignment(EHTA_Center);
				SignText->SetVerticalAlignment(EVRTA_TextCenter);
				SignText->SetWorldSize(30.0f);
				SignText->SetTextRenderColor(FColor(255, 250, 230));
				SignText->SetCullDistance(8500.0f);
				ShopSignTexts.Add(SignText);
			}
		}
		else
		{
			// Recessed Arched / Rusticated Historic Stone Portal Door + Flanking Ground Windows
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 10.0f, 0.0f, 132.0f)) * BldgXf,
				FVector(10.0f, 96.0f, 132.0f), 4.0f, CorniceCol);
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f - 6.0f, 0.0f, 120.0f)) * BldgXf,
				FVector(8.0f, 76.0f, 120.0f), 3.0f, WoodCumba);
		}

		// 5. Upper Floors: 3D Cumbas with Corbels & Recessed 15cm Windows with Stone Sills & Shutters
		const int32 NumBays = FMath::Clamp(FMath::RoundToInt(WidthCm / 360.0f), 2, 4);
		const float BayStepY = WidthCm / (float)NumBays;
		const float RawProjectionCm = Rng.FRandRange(72.0f, 98.0f);
		const float SafeProjectionCm = FMath::Clamp(RawProjectionCm, 0.0f, Spec.MaxAllowedProjectionCm);
		const bool bBuildingHasShutters = (Rng.FRand() < 0.65f);

		for (int32 Bay = 0; Bay < NumBays; ++Bay)
		{
			const float BayY = -WidthCm * 0.5f + (Bay + 0.5f) * BayStepY;
			const bool bBayIsCumba = Spec.bHasCumba && (Floors >= 3) && (SafeProjectionCm >= 55.0f) &&
				((NumBays == 3 && Bay == 1) || (NumBays != 3 && (Bay + Idx) % 2 == 0));

			if (bBayIsCumba)
			{
				++GeneratedCumbaCount;
				const float CumbaHCm = (Floors - 1) * FloorHCm * 0.82f;
				const float CumbaZCenter = FloorHCm + CumbaHCm * 0.5f + 10.0f;
				const FLinearColor CumbaTone = (Rng.FRand() < 0.48f) ? WoodCumba : (PlasterCol * 0.95f);

				// Transform with local +Y pointing along Building +X (outward from facade)
				const FTransform CumbaWallXf = FTransform(
					FRotator(0.0f, -90.0f, 0.0f),
					FVector(DepthCm * 0.5f, BayY, CumbaZCenter)) * BldgXf;

				FIstanbulMeshUtils::AppendCumbaWithCorbels(
					FacadeSection, GlassSection, RoofTileSection,
					CumbaWallXf,
					BayStepY * 0.78f, CumbaHCm, SafeProjectionCm,
					CumbaTone, WoodCumba);

				// Additional recessed windows on each floor of the Cumba face
				for (int32 f = 1; f < Floors; ++f)
				{
					const float WinZ = f * FloorHCm + 162.0f;
					const FTransform WinXf = FTransform(
						FRotator(0.0f, -90.0f, 0.0f),
						FVector(DepthCm * 0.5f + SafeProjectionCm + 2.0f, BayY, WinZ)) * BldgXf;
					FIstanbulMeshUtils::AppendRecessedWindow(
						FacadeSection, GlassSection, WinXf,
						BayStepY * 0.50f, 138.0f, 15.0f, FrameCol, CorniceCol, false, ShutterCol);
				}
			}
			else
			{
				// Recessed 15cm Facade Windows with Stone Sills, Lintels, Sash Muntins, and Louvered Shutters
				for (int32 f = 1; f < Floors; ++f)
				{
					const float WinZ = f * FloorHCm + 162.0f;
					const FTransform WinXf = FTransform(
						FRotator(0.0f, -90.0f, 0.0f),
						FVector(DepthCm * 0.5f + 2.0f, BayY, WinZ)) * BldgXf;

					FIstanbulMeshUtils::AppendRecessedWindow(
						FacadeSection, GlassSection, WinXf,
						FMath::Min(BayStepY * 0.44f, 125.0f), 146.0f, 15.0f,
						FrameCol, CorniceCol, bBuildingHasShutters, ShutterCol);

					// Optional iron Juliet balcony railing on 2nd/3rd floor windows
					if (f == 2 && (Bay + Idx) % 2 == 1)
					{
						FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
							FTransform(FVector(DepthCm * 0.5f + 18.0f, BayY, WinZ - 76.0f)) * BldgXf,
							FVector(16.0f, BayStepY * 0.32f, 5.0f), 1.5f, CorniceCol);
						FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
							FTransform(FVector(DepthCm * 0.5f + 30.0f, BayY, WinZ - 42.0f)) * BldgXf,
							FVector(2.5f, BayStepY * 0.30f, 32.0f), 0.8f, FLinearColor(0.08f, 0.09f, 0.10f));
					}

					// HISM Split-AC outdoor unit on 30% of flat bays
					if (DetailCubeHISM && Rng.FRand() < 0.30f)
					{
						DetailCubeHISM->AddInstance(
							FTransform(Rot, BldgXf.TransformPosition(FVector(DepthCm * 0.5f + 22.0f, BayY + 62.0f, WinZ - 46.0f)),
								FVector(0.32f, 0.65f, 0.45f)), true);
						++GeneratedHISMPropCount;
					}
				}
			}
		}

		// 6. Roofline Details: Overhanging Cornice, Pitched Red-Tile Roof (Kiremit Çatı), Brick Chimneys, Water Tanks & Satellite Dishes
		FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
			FTransform(FVector(0.0f, 0.0f, TotalHCm + 14.0f)) * BldgXf,
			FVector(DepthCm * 0.5f + 28.0f, WidthCm * 0.5f + 22.0f, 14.0f), 4.0f, CorniceCol);

		if (Spec.bPitchedRoof)
		{
			// 3D Pitched Hipped Terracotta Roof (Kiremit Çatı)
			const float RoofHeightCm = 145.0f;
			FIstanbulMeshUtils::AppendTaperedPrism(RoofTileSection,
				FTransform(FVector(0.0f, 0.0f, TotalHCm + 28.0f + RoofHeightCm * 0.5f)) * BldgXf,
				FVector2D(DepthCm * 0.5f + 24.0f, WidthCm * 0.5f + 18.0f),
				FVector2D(DepthCm * 0.16f, WidthCm * 0.28f),
				FVector::ZeroVector,
				RoofHeightCm * 0.5f, Terracotta);
		}
		else
		{
			// Recessed flat roof parapet wall
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(0.0f, 0.0f, TotalHCm + 42.0f)) * BldgXf,
				FVector(DepthCm * 0.5f + 12.0f, WidthCm * 0.5f + 10.0f, 22.0f), 3.0f, PlasterCol * 0.90f);
		}

		// Brick Chimney (Baca) with Stone Cap + Terracotta Chimney Pipe
		const float ChimneyBaseZ = TotalHCm + (Spec.bPitchedRoof ? 115.0f : 45.0f);
		const FTransform ChimneyXf = FTransform(FVector(DepthCm * 0.18f, -WidthCm * 0.26f, ChimneyBaseZ)) * BldgXf;
		FIstanbulMeshUtils::AppendBeveledBox(RoofTileSection,
			FTransform(FVector(0.0f, 0.0f, 55.0f)) * ChimneyXf,
			FVector(28.0f, 28.0f, 55.0f), 2.5f, BrickChimney);
		FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
			FTransform(FVector(0.0f, 0.0f, 114.0f)) * ChimneyXf,
			FVector(34.0f, 34.0f, 6.0f), 1.5f, CorniceCol);
		FIstanbulMeshUtils::AppendCylinder(RoofTileSection,
			FTransform(FVector(0.0f, 0.0f, 132.0f)) * ChimneyXf,
			10.0f, 16.0f, 8, Terracotta * 1.1f, true);

		// Rooftop Satellite Dish (Çanak Anten) facing South-East
		const FTransform DishBaseXf = FTransform(
			FRotator(0.0f, 35.0f, 0.0f),
			FVector(DepthCm * 0.24f, WidthCm * 0.24f, TotalHCm + (Spec.bPitchedRoof ? 95.0f : 42.0f))) * BldgXf;
		FIstanbulMeshUtils::AppendSatelliteDish(FacadeSection, DishBaseXf, 42.0f);

		// Galvanized Rooftop Water Tank (Su Deposu)
		if (RooftopCylinderHISM)
		{
			RooftopCylinderHISM->AddInstance(
				FTransform(FRotator::ZeroRotator,
					BldgXf.TransformPosition(FVector(-DepthCm * 0.18f, WidthCm * 0.18f, TotalHCm + (Spec.bPitchedRoof ? 120.0f : 95.0f))),
					FVector(1.1f, 1.1f, 1.25f)), true);
			++GeneratedHISMPropCount;
		}

		// 7. Optional Arched Passage / Gateway spanning across a narrow alley (>= 3.9m vertical drivable clearance!)
		if (Spec.bSpanAlleyGateway)
		{
			const float SpanCm = Spec.AlleySpanWidthM * 100.0f;
			const float ClearanceZCm = 390.0f;
			const float BridgeHCm = 280.0f;
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + SpanCm * 0.5f, 0.0f, ClearanceZCm + BridgeHCm * 0.5f)) * BldgXf,
				FVector(SpanCm * 0.5f, 210.0f, BridgeHCm * 0.5f), 6.0f, StoneCol);
		}

		++GeneratedBuildingCount;
	}

	UMaterialInterface* VtxMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	UMaterialInterface* GlassMat = FIstanbulMeshUtils::CreatePBRMaterial(this, FLinearColor(0.035f, 0.065f, 0.115f, 1.0f), 0.25f, 0.06f, 0.90f);
	UMaterialInterface* RoofMat  = FIstanbulMeshUtils::CreatePBRMaterial(this, FLinearColor(0.62f, 0.23f, 0.13f, 1.0f), 0.05f, 0.65f, 0.35f);

	FacadeSection.CommitToComponent(BatchedFacadeMesh, 0, true, VtxMat);
	GlassSection.CommitToComponent(BatchedFacadeMesh,  1, false, GlassMat ? GlassMat : VtxMat);
	RoofTileSection.CommitToComponent(BatchedFacadeMesh, 2, false, RoofMat ? RoofMat : VtxMat);
}
