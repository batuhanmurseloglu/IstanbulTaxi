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
	BatchedFacadeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Simple UBoxComponents handle collision!
	BatchedFacadeMesh->LDMaxDrawDistance = 42000.0f;

	DetailCubeHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DetailCubeHISM"));
	DetailCubeHISM->SetupAttachment(Root);
	DetailCubeHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetailCubeHISM->SetCullDistances(0, 14000);

	RooftopCylinderHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RooftopCylinderHISM"));
	RooftopCylinderHISM->SetupAttachment(Root);
	RooftopCylinderHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RooftopCylinderHISM->SetCullDistances(0, 16000);

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

	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* CubeMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		CubeMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.76f, 0.78f, 0.80f));
		DetailCubeHISM->SetMaterial(0, CubeMID);

		UMaterialInstanceDynamic* CylMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		CylMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.58f, 0.62f, 0.66f));
		RooftopCylinderHISM->SetMaterial(0, CylMID);
	}

	FIstanbulMeshSectionData FacadeSection;

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

	static const TCHAR* TurkishShopNames[5] = {
		TEXT("ECZANE"),
		TEXT("TAŞ FIRIN"),
		TEXT("BAKKAL"),
		TEXT("ÇAY OCAĞI"),
		TEXT("TEKEL BAYİİ")
	};

	static const FLinearColor ShopBoardColors[5] = {
		FLinearColor(0.06f, 0.56f, 0.22f), // Eczane Green
		FLinearColor(0.86f, 0.54f, 0.08f), // Fırın Amber
		FLinearColor(0.10f, 0.36f, 0.76f), // Bakkal Blue
		FLinearColor(0.76f, 0.14f, 0.12f), // Çay Ocağı Red
		FLinearColor(0.88f, 0.68f, 0.04f)  // Tekel Yellow
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
		const FVector Fwd = Rot.Vector();
		const FVector Right = FRotationMatrix(Rot).GetScaledAxis(EAxis::Y);

		const FLinearColor PlasterCol = PlasterPalette[Rng.RandRange(0, 7)] * Rng.FRandRange(0.92f, 1.06f);
		const FLinearColor StoneCol   = FLinearColor(0.46f, 0.43f, 0.39f) * Rng.FRandRange(0.90f, 1.08f);
		const FLinearColor WoodCumba  = FLinearColor(0.38f, 0.24f, 0.14f) * Rng.FRandRange(0.90f, 1.10f);
		const FLinearColor CorniceCol = PlasterCol * 1.14f;
		const FLinearColor DarkGlass(0.08f, 0.12f, 0.18f);

		// 1. Cheap Simple Box Collision per building
		UBoxComponent* ColBox = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("BldgCol_%d"), Idx));
		if (ColBox)
		{
			ColBox->SetupAttachment(Root);
			ColBox->RegisterComponent();
			ColBox->SetWorldLocationAndRotation(Spec.BaseLocation + FVector(0.0f, 0.0f, TotalHCm * 0.5f), Rot);
			ColBox->SetBoxExtent(FVector(DepthCm * 0.5f, WidthCm * 0.5f, TotalHCm * 0.5f));
			ColBox->SetCollisionProfileName(TEXT("BlockAll"));
			SimpleCollisionBoxes.Add(ColBox);
			BuildingCollisionBoxes.Add(ColBox->Bounds.GetBox());
		}

		// 2. Ground-Floor Stone Plinth + Upper Plaster Mass
		FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
			FTransform(FVector(0.0f, 0.0f, FloorHCm * 0.5f)) * BldgXf,
			FVector(DepthCm * 0.5f + 4.0f, WidthCm * 0.5f + 2.0f, FloorHCm * 0.5f), 4.0f, StoneCol);

		const float UpperHCm = (Floors - 1) * FloorHCm;
		FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
			FTransform(FVector(0.0f, 0.0f, FloorHCm + UpperHCm * 0.5f)) * BldgXf,
			FVector(DepthCm * 0.5f, WidthCm * 0.5f, UpperHCm * 0.5f), 5.0f, PlasterCol);

		// 3. Horizontal String Courses (kat silmeleri) & Corner Vertical Pilasters
		for (int32 f = 1; f <= Floors; ++f)
		{
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 6.0f, 0.0f, f * FloorHCm)) * BldgXf,
				FVector(8.0f, WidthCm * 0.5f + 8.0f, 9.0f), 2.0f, CorniceCol);
		}
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 4.0f, Side * (WidthCm * 0.5f - 18.0f), TotalHCm * 0.5f)) * BldgXf,
				FVector(6.0f, 18.0f, TotalHCm * 0.5f), 2.0f, CorniceCol);
		}

		// 4. Ground-floor Shopfront, Awning & Turkish Shop Signboard (ECZANE / FIRIN / BAKKAL / ÇAY OCAĞI)
		if (Spec.GroundFloorType == EIstanbulGroundFloorType::ShopfrontAwning)
		{
			const int32 ShopTypeIdx = Rng.RandRange(0, 4);
			const FLinearColor BoardCol = ShopBoardColors[ShopTypeIdx];

			// Dark shop glazing + colored signboard header + sloped awning
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 6.0f, 0.0f, 130.0f)) * BldgXf,
				FVector(4.0f, WidthCm * 0.40f, 105.0f), 1.5f, DarkGlass);
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 12.0f, 0.0f, 265.0f)) * BldgXf,
				FVector(8.0f, WidthCm * 0.42f, 26.0f), 2.0f, BoardCol);
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FRotator(-15.0f, 0.0f, 0.0f), FVector(DepthCm * 0.5f + 68.0f, 0.0f, 232.0f)) * BldgXf,
				FVector(65.0f, WidthCm * 0.42f, 6.0f), 1.5f, BoardCol * 0.9f);

			// Cheap distance-culled TextRender label on every 2nd shopfront
			if (Idx % 2 == 0)
			{
				UTextRenderComponent* SignText = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("ShopSign_%d"), Idx));
				if (SignText)
				{
					SignText->SetupAttachment(Root);
					SignText->RegisterComponent();
					SignText->SetWorldLocationAndRotation(
						BldgXf.TransformPosition(FVector(DepthCm * 0.5f + 22.0f, 0.0f, 252.0f)),
						Rot);
					SignText->SetText(FText::FromString(TurkishShopNames[ShopTypeIdx]));
					SignText->SetHorizontalAlignment(EHTA_Center);
					SignText->SetWorldSize(34.0f);
					SignText->SetTextRenderColor(FColor(255, 248, 220));
					SignText->SetCullDistance(6500.0f); // Only rendered within 65m of player!
					ShopSignTexts.Add(SignText);
				}
			}
		}
		else
		{
			// Arched / Rusticated stone portal door
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + 8.0f, 0.0f, 125.0f)) * BldgXf,
				FVector(6.0f, 85.0f, 125.0f), 4.0f, WoodCumba);
		}

		// 5. CUMBA (Bay Windows) with slanted corbel brackets, clamped to MaxAllowedProjectionCm so it never intrudes into the road!
		const int32 NumBays = FMath::Clamp(FMath::RoundToInt(WidthCm / 380.0f), 1, 4);
		const float BayStepY = WidthCm / (float)NumBays;
		const float RawProjectionCm = Rng.FRandRange(65.0f, 95.0f);
		const float SafeProjectionCm = FMath::Clamp(RawProjectionCm, 0.0f, Spec.MaxAllowedProjectionCm);

		for (int32 Bay = 0; Bay < NumBays; ++Bay)
		{
			const float BayY = -WidthCm * 0.5f + (Bay + 0.5f) * BayStepY;
			const bool bBayIsCumba = Spec.bHasCumba && (Floors >= 3) && (SafeProjectionCm >= 55.0f) && ((Bay + Idx) % 2 == 0);

			if (bBayIsCumba)
			{
				++GeneratedCumbaCount;
				const float CumbaHCm = (Floors - 1) * FloorHCm * 0.80f;
				const float CumbaZCenter = FloorHCm + CumbaHCm * 0.5f + 12.0f;
				const FLinearColor CumbaTone = (Rng.FRand() < 0.45f) ? WoodCumba : (PlasterCol * 0.94f);

				// Main projecting Cumba box
				FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
					FTransform(FVector(DepthCm * 0.5f + SafeProjectionCm * 0.5f, BayY, CumbaZCenter)) * BldgXf,
					FVector(SafeProjectionCm * 0.5f, BayStepY * 0.36f, CumbaHCm * 0.5f), 4.0f, CumbaTone);

				// Two slanted corbel/bracket supports (Payanda / Eli böğründe) under the Cumba
				for (int32 Brk = -1; Brk <= 1; Brk += 2)
				{
					FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
						FTransform(FRotator(38.0f, 0.0f, 0.0f),
							FVector(DepthCm * 0.5f + SafeProjectionCm * 0.42f, BayY + Brk * BayStepY * 0.26f, FloorHCm - 22.0f)) * BldgXf,
						FVector(SafeProjectionCm * 0.58f, 8.0f, 10.0f), 2.0f, WoodCumba);
				}

				// Pitched or flat Cumba cap
				FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
					FTransform(FRotator(-8.0f, 0.0f, 0.0f),
						FVector(DepthCm * 0.5f + SafeProjectionCm * 0.52f, BayY, FloorHCm + CumbaHCm + 18.0f)) * BldgXf,
					FVector(SafeProjectionCm * 0.56f, BayStepY * 0.40f, 9.0f), 2.0f, FLinearColor(0.56f, 0.22f, 0.14f));
			}

			// Upper-floor window bays + HISM AC units / shutters
			for (int32 f = 1; f < Floors; ++f)
			{
				const float WinZ = f * FloorHCm + 165.0f;
				const float WinX = DepthCm * 0.5f + (bBayIsCumba ? SafeProjectionCm + 3.0f : 5.0f);
				const FLinearColor WinCol = (Rng.FRand() < 0.32f)
					? FLinearColor(0.96f, 0.80f, 0.38f) // Warm lit Istanbul apartment window
					: DarkGlass;

				FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
					FTransform(FVector(WinX, BayY, WinZ)) * BldgXf,
					FVector(3.0f, BayStepY * 0.24f, 68.0f), 1.5f, WinCol);

				// HISM Split-AC outdoor unit on 30% of flat bays
				if (!bBayIsCumba && DetailCubeHISM && Rng.FRand() < 0.30f)
				{
					DetailCubeHISM->AddInstance(
						FTransform(Rot, BldgXf.TransformPosition(FVector(DepthCm * 0.5f + 22.0f, BayY + 55.0f, WinZ - 42.0f)),
							FVector(0.32f, 0.65f, 0.45f)), true);
					++GeneratedHISMPropCount;
				}
			}
		}

		// 6. Roof Cornice, Pitched Terracotta Cap, Chimneys & HISM Rooftop Water Tanks
		FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
			FTransform(FVector(0.0f, 0.0f, TotalHCm + 24.0f)) * BldgXf,
			FVector(DepthCm * 0.5f + 22.0f, WidthCm * 0.5f + 18.0f, 24.0f), 4.0f,
			Spec.bPitchedRoof ? FLinearColor(0.58f, 0.22f, 0.14f) : (CorniceCol * 0.9f));

		if (RooftopCylinderHISM)
		{
			RooftopCylinderHISM->AddInstance(
				FTransform(FRotator::ZeroRotator,
					BldgXf.TransformPosition(FVector(-DepthCm * 0.15f, WidthCm * 0.22f, TotalHCm + 95.0f)),
					FVector(1.1f, 1.1f, 1.25f)), true);
			++GeneratedHISMPropCount;
		}

		// 7. Optional Arched Passage / Gateway spanning across a narrow alley (>= 3.8m vertical drivable clearance!)
		if (Spec.bSpanAlleyGateway)
		{
			const float SpanCm = Spec.AlleySpanWidthM * 100.0f;
			const float ClearanceZCm = 390.0f; // 3.9m > 3.2m minimum requirement
			const float BridgeHCm = 280.0f;
			FIstanbulMeshUtils::AppendBeveledBox(FacadeSection,
				FTransform(FVector(DepthCm * 0.5f + SpanCm * 0.5f, 0.0f, ClearanceZCm + BridgeHCm * 0.5f)) * BldgXf,
				FVector(SpanCm * 0.5f, 210.0f, BridgeHCm * 0.5f), 6.0f, StoneCol);
		}

		++GeneratedBuildingCount;
	}

	UMaterialInterface* VtxMat = FIstanbulMeshUtils::LoadVertexColorMaterialSafe();
	FacadeSection.CommitToComponent(BatchedFacadeMesh, 0, false, VtxMat);
}
