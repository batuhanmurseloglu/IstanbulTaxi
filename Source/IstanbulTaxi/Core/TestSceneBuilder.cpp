#include "Core/TestSceneBuilder.h"
#include "Environment/TimeOfDayActor.h"
#include "Environment/TimeOfDaySubsystem.h"
#include "Environment/BosphorusWaterVolume.h"
#include "Taximeter/TollGateVolume.h"
#include "Audio/DistrictAmbienceVolume.h"
#include "Audio/DistrictAmbienceDataAsset.h"
#include "Audio/MosqueActor.h"
#include "Traffic/IstanbulTrafficManager.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "IstanbulTaxi.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Atmosphere/AtmosphericFog.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

ATestSceneBuilder::ATestSceneBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATestSceneBuilder::BeginPlay()
{
	Super::BeginPlay();
	if (bBuildTestEnvironment && !bBuilt)
	{
		BuildPrototypeScene();
	}
}

AActor* ATestSceneBuilder::SpawnShape(const TCHAR* ShapePath, const FString& Label, const FVector& Location,
	const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color, bool bCollidable, FName SurfaceActorTag)
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* MeshActor = W->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (!MeshActor) return nullptr;

	if (!SurfaceActorTag.IsNone())
	{
		MeshActor->Tags.Add(SurfaceActorTag);
	}

	UStaticMeshComponent* SMC = MeshActor->GetStaticMeshComponent();
	SMC->SetMobility(EComponentMobility::Movable);
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, ShapePath))
	{
		SMC->SetStaticMesh(Mesh);
	}
	SMC->SetWorldScale3D(Scale);
	SMC->SetCollisionEnabled(bCollidable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollidable)
	{
		SMC->SetCollisionProfileName(TEXT("BlockAll"));
	}

	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, MeshActor);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		SMC->SetMaterial(0, MID);
	}
#if WITH_EDITOR
	MeshActor->SetActorLabel(Label);
#endif
	return MeshActor;
}

AActor* ATestSceneBuilder::SpawnBox(const FString& Label, const FVector& Location, const FVector& Scale,
	const FLinearColor& Color, bool bCollidable, const FRotator& Rotation, FName SurfaceActorTag)
{
	return SpawnShape(TEXT("/Engine/BasicShapes/Cube.Cube"), Label, Location, Rotation, Scale, Color, bCollidable, SurfaceActorTag);
}

void ATestSceneBuilder::SpawnIstanbulApartmentBlock(const FString& NamePrefix, const FVector& BaseLocation,
	float YawDeg, int32 Floors, float WidthM, float DepthM, const FLinearColor& FacadeColor, bool bHasCumba, bool bHasShopAwning)
{
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Fwd = Rot.Vector();
	const FVector Right = FRotationMatrix(Rot).GetScaledAxis(EAxis::Y);

	const float FloorH = 3.2f; // 3.2m per story
	const float TotalHM = Floors * FloorH;

	// Main building mass (collidable)
	SpawnBox(NamePrefix + TEXT("_Body"), BaseLocation + FVector(0.0f, 0.0f, TotalHM * 50.0f),
		FVector(DepthM, WidthM, TotalHM), FacadeColor, true, Rot);

	// Terracotta tiled roof cap
	SpawnBox(NamePrefix + TEXT("_Roof"), BaseLocation + FVector(0.0f, 0.0f, TotalHM * 100.0f + 25.0f),
		FVector(DepthM + 0.4f, WidthM + 0.3f, 0.45f), FLinearColor(0.58f, 0.22f, 0.14f), false, Rot);

	// Ground-floor dark shopfront glazing + colorful shop signboard (Döner / Tekel / Bakkal / Eczane)
	SpawnBox(NamePrefix + TEXT("_ShopFront"), BaseLocation + Fwd * (DepthM * 50.0f + 6.0f) + FVector(0.0f, 0.0f, 145.0f),
		FVector(0.12f, WidthM * 0.86f, 2.5f), FLinearColor(0.08f, 0.10f, 0.14f), false, Rot);

	if (bHasShopAwning)
	{
		static const FLinearColor AwningColors[4] = {
			FLinearColor(0.75f, 0.12f, 0.10f), // Dönerci / Büfe Red
			FLinearColor(0.06f, 0.48f, 0.22f), // Eczane / Manav Green
			FLinearColor(0.10f, 0.32f, 0.72f), // Tekel / Market Blue
			FLinearColor(0.88f, 0.58f, 0.06f)  // Fırın / Simit Sarayı Amber
		};
		const FLinearColor AwningCol = AwningColors[FMath::RandRange(0, 3)];
		SpawnBox(NamePrefix + TEXT("_ShopAwning"),
			BaseLocation + Fwd * (DepthM * 50.0f + 95.0f) + FVector(0.0f, 0.0f, 290.0f),
			FVector(1.8f, WidthM * 0.88f, 0.22f), AwningCol, false, FRotator(-14.0f, YawDeg, 0.0f));
	}

	// Overhanging Ottoman/Levantine Cumba (bay window) or concrete balconies on upper floors
	if (bHasCumba && Floors >= 3)
	{
		const float CumbaH = (Floors - 1) * FloorH * 0.78f;
		const float CumbaZ = 340.0f + CumbaH * 50.0f;
		SpawnBox(NamePrefix + TEXT("_Cumba"),
			BaseLocation + Fwd * (DepthM * 50.0f + 68.0f) + FVector(0.0f, 0.0f, CumbaZ),
			FVector(1.35f, WidthM * 0.56f, CumbaH), FacadeColor * 0.92f, false, Rot);
	}

	// Horizontal window bands per upper floor
	for (int32 f = 1; f < Floors; ++f)
	{
		const float WinZ = f * FloorH * 100.0f + 160.0f;
		const FLinearColor WinCol = (FMath::FRand() < 0.35f)
			? FLinearColor(0.95f, 0.78f, 0.38f) // Warm lit Istanbul apartment window
			: FLinearColor(0.14f, 0.20f, 0.28f);
		SpawnBox(FString::Printf(TEXT("%s_WinF%d"), *NamePrefix, f),
			BaseLocation + Fwd * (DepthM * 50.0f + (bHasCumba ? 138.0f : 8.0f)) + FVector(0.0f, 0.0f, WinZ),
			FVector(0.10f, WidthM * (bHasCumba ? 0.48f : 0.76f), 1.15f), WinCol, false, Rot);
	}

	// Rooftop satellite dish / water tank silhouette
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), NamePrefix + TEXT("_WaterTank"),
		BaseLocation + Right * (WidthM * 22.0f) + FVector(0.0f, 0.0f, TotalHM * 100.0f + 90.0f),
		FRotator::ZeroRotator, FVector(1.1f, 1.1f, 1.2f), FLinearColor(0.62f, 0.65f, 0.68f), false);
}

void ATestSceneBuilder::SpawnParkedCurbCar(const FString& Label, const FVector& Location,
	const FRotator& Rotation, const FLinearColor& BodyColor)
{
	// Kaldırıma yarı çıkmış park halinde araç (tilted in Rotation.Roll by caller)
	SpawnBox(Label + TEXT("_Body"), Location + FVector(0.0f, 0.0f, 55.0f),
		FVector(4.1f, 1.72f, 0.55f), BodyColor, true, Rotation);
	SpawnBox(Label + TEXT("_Cabin"), Location + FVector(0.0f, 0.0f, 102.0f),
		FVector(2.1f, 1.50f, 0.48f), FLinearColor(0.12f, 0.15f, 0.18f), false, Rotation);
}

void ATestSceneBuilder::SpawnIETTBusStop(const FString& Label, const FVector& Location, float YawDeg)
{
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Fwd = Rot.Vector();
	const FVector Right = FRotationMatrix(Rot).GetScaledAxis(EAxis::Y);

	// Back glass/frame panel
	SpawnBox(Label + TEXT("_BackWall"), Location - Fwd * 75.0f + FVector(0.0f, 0.0f, 125.0f),
		FVector(0.12f, 3.8f, 2.4f), FLinearColor(0.16f, 0.22f, 0.28f), true, Rot);
	// Roof canopy with yellow İETT band
	SpawnBox(Label + TEXT("_Canopy"), Location + FVector(0.0f, 0.0f, 250.0f),
		FVector(1.75f, 4.1f, 0.16f), FLinearColor(0.95f, 0.72f, 0.02f), false, Rot);
	// Bench
	SpawnBox(Label + TEXT("_Bench"), Location - Fwd * 45.0f + FVector(0.0f, 0.0f, 48.0f),
		FVector(0.45f, 2.4f, 0.12f), FLinearColor(0.45f, 0.28f, 0.14f), false, Rot);
	// Red/Blue İETT "D" sign totem pole
	SpawnBox(Label + TEXT("_SignPole"), Location + Right * 235.0f + FVector(0.0f, 0.0f, 145.0f),
		FVector(0.18f, 0.42f, 2.8f), FLinearColor(0.82f, 0.12f, 0.12f), false, Rot);
}

void ATestSceneBuilder::SpawnStreetLamp(const FString& Label, const FVector& Location, float YawDeg)
{
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Fwd = Rot.Vector();
	// Ornate cast-iron base plinth + tapered pole + curved bracket + warm sodium lantern head
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Label + TEXT("_Base"),
		Location + FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator,
		FVector(0.34f, 0.34f, 0.9f), FLinearColor(0.14f, 0.16f, 0.18f), false);
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Label + TEXT("_Pole"),
		Location + FVector(0.0f, 0.0f, 380.0f), FRotator::ZeroRotator,
		FVector(0.18f, 0.18f, 7.6f), FLinearColor(0.20f, 0.22f, 0.25f), false);
	SpawnBox(Label + TEXT("_Arm"), Location + Fwd * 95.0f + FVector(0.0f, 0.0f, 745.0f),
		FVector(1.9f, 0.16f, 0.14f), FLinearColor(0.20f, 0.22f, 0.25f), false, Rot);
	SpawnBox(Label + TEXT("_Bulb"), Location + Fwd * 175.0f + FVector(0.0f, 0.0f, 732.0f),
		FVector(0.48f, 0.34f, 0.14f), FLinearColor(1.0f, 0.85f, 0.45f), false, Rot);
}

void ATestSceneBuilder::SpawnHistoricalStoneWall(const FString& Label, const FVector& CenterLocation,
	const FRotator& Rotation, float LengthM, float HeightM)
{
	// Byzantine / Ottoman "Almaşık" stone & brick coursed retaining wall flanking steep Istanbul backstreets
	const FLinearColor LimestoneCol(0.52f, 0.49f, 0.44f);
	const FLinearColor KhorasanBrickCol(0.58f, 0.24f, 0.18f);
	const FLinearColor CopingStoneCol(0.64f, 0.61f, 0.56f);

	SpawnBox(Label + TEXT("_StoneCore"), CenterLocation,
		FVector(LengthM, 0.55f, HeightM), LimestoneCol, true, Rotation);

	// 2 Horizontal Khorasan brick courses (Horasan tuğla hatılları)
	const FVector UpVec = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Z);
	SpawnBox(Label + TEXT("_BrickBand1"), CenterLocation - UpVec * (HeightM * 18.0f),
		FVector(LengthM * 1.002f, 0.59f, 0.18f), KhorasanBrickCol, false, Rotation);
	SpawnBox(Label + TEXT("_BrickBand2"), CenterLocation + UpVec * (HeightM * 22.0f),
		FVector(LengthM * 1.002f, 0.59f, 0.18f), KhorasanBrickCol, false, Rotation);

	// Top weathered stone coping ledge
	SpawnBox(Label + TEXT("_Coping"), CenterLocation + UpVec * (HeightM * 50.0f + 10.0f),
		FVector(LengthM * 1.01f, 0.68f, 0.20f), CopingStoneCol, false, Rotation);
}

void ATestSceneBuilder::SpawnGalataTowerLandmark(const FVector& RidgeBase)
{
	const FLinearColor GenoeseStone(0.62f, 0.57f, 0.49f);
	const FLinearColor LeadConeCol(0.28f, 0.32f, 0.38f);

	// Iconic cylindrical Genoese stone shaft of Galata Kulesi
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_MainShaft"),
		RidgeBase + FVector(0.0f, 0.0f, 1850.0f), FRotator::ZeroRotator,
		FVector(14.5f, 14.5f, 37.0f), GenoeseStone, true);

	// Decorative stone belt moldings
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_Belt1"),
		RidgeBase + FVector(0.0f, 0.0f, 2400.0f), FRotator::ZeroRotator,
		FVector(15.2f, 15.2f, 0.6f), GenoeseStone * 1.1f, false);

	// Corbelled Observation Balcony (Seyir Terası)
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_Balcony"),
		RidgeBase + FVector(0.0f, 0.0f, 3650.0f), FRotator::ZeroRotator,
		FVector(16.8f, 16.8f, 1.1f), FLinearColor(0.76f, 0.72f, 0.64f), false);

	// Warm-lit arched arcade level under the roof cone
	SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), TEXT("Galata_Arcade"),
		RidgeBase + FVector(0.0f, 0.0f, 3920.0f), FRotator::ZeroRotator,
		FVector(13.8f, 13.8f, 4.5f), FLinearColor(0.95f, 0.78f, 0.38f), false);

	// Iconic Conical Lead Roof Cap (Külah) + Finial Alem
	SpawnShape(TEXT("/Engine/BasicShapes/Cone.Cone"), TEXT("Galata_ConicalRoof"),
		RidgeBase + FVector(0.0f, 0.0f, 4850.0f), FRotator::ZeroRotator,
		FVector(15.0f, 15.0f, 15.0f), LeadConeCol, false);
}

void ATestSceneBuilder::SpawnBosphorusScenicOverlook(const FVector& Origin)
{
	// Dedicated Bosphorus Coastal Ledge & Scenic Overlook Terrace (Sahil Kordonu & Boğaz Seyir Terası)
	// extending out toward the Bosphorus between X = +80m and X = +210m so the player has a clear view of the sea & bridge
	const FLinearColor GraniteCol(0.46f, 0.47f, 0.49f);
	const FLinearColor IronRailCol(0.12f, 0.14f, 0.16f);

	// Seafront stone seawall & overhanging scenic viewing deck
	SpawnBox(TEXT("Overlook_Seawall"), Origin + FVector(15000.0f, -1620.0f, -15.0f),
		FVector(420.0f, 0.8f, 0.85f), FLinearColor(0.34f, 0.35f, 0.37f), true);
	SpawnBox(TEXT("Overlook_BelvedereDeck"), Origin + FVector(18500.0f, -1820.0f, 14.0f),
		FVector(48.0f, 6.5f, 0.28f), GraniteCol, true);

	// Continuous low wrought-iron Bosphorus cornice railing (keeps the horizon & water completely unobstructed from the taxi!)
	SpawnBox(TEXT("Overlook_TopRail"), Origin + FVector(15000.0f, -1590.0f, 82.0f),
		FVector(360.0f, 0.08f, 0.06f), IronRailCol, false);

	// Seaside promenade wooden benches & cast-iron bollards facing Kız Kulesi and the Bosphorus Bridge
	for (int32 i = 0; i < 8; ++i)
	{
		const float BX = 6500.0f + i * 2400.0f;
		SpawnBox(FString::Printf(TEXT("Overlook_Bench_%d"), i), Origin + FVector(BX, -1480.0f, 45.0f),
			FVector(2.2f, 0.55f, 0.45f), FLinearColor(0.45f, 0.26f, 0.12f), false);
	}
}

void ATestSceneBuilder::SpawnDetailedMosqueComplex(const FVector& HilltopBase)
{
	const FLinearColor StoneCol(0.84f, 0.81f, 0.75f);
	const FLinearColor LeadDomeCol(0.36f, 0.40f, 0.45f);

	// Main prayer hall cube (Harim)
	SpawnBox(TEXT("Mosque_HarimHall"), HilltopBase + FVector(0.0f, 0.0f, 700.0f),
		FVector(22.0f, 22.0f, 14.0f), StoneCol, true);

	// Central Ottoman Lead Dome + 4 Semi-Domes
	SpawnShape(TEXT("/Engine/BasicShapes/Sphere.Sphere"), TEXT("Mosque_MainDome"),
		HilltopBase + FVector(0.0f, 0.0f, 1420.0f), FRotator::ZeroRotator,
		FVector(16.5f, 16.5f, 12.5f), LeadDomeCol, false);

	const FVector SemiOffsets[4] = {
		FVector(850.0f, 0.0f, 1180.0f), FVector(-850.0f, 0.0f, 1180.0f),
		FVector(0.0f, 850.0f, 1180.0f), FVector(0.0f, -850.0f, 1180.0f)
	};
	for (int32 i = 0; i < 4; ++i)
	{
		SpawnShape(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FString::Printf(TEXT("Mosque_SemiDome_%d"), i),
			HilltopBase + SemiOffsets[i], FRotator::ZeroRotator,
			FVector(9.5f, 9.5f, 7.5f), LeadDomeCol, false);
	}

	// Twin Minarets with Şerefe balconies and conical铅 (lead) spires
	const FVector MinaretBases[2] = {
		HilltopBase + FVector(1350.0f, -1350.0f, 0.0f),
		HilltopBase + FVector(1350.0f,  1350.0f, 0.0f)
	};
	for (int32 m = 0; m < 2; ++m)
	{
		const FString MName = FString::Printf(TEXT("Mosque_Minaret_%d"), m);
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), MName + TEXT("_Shaft"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 2100.0f), FRotator::ZeroRotator,
			FVector(2.2f, 2.2f, 42.0f), StoneCol, true);
		// Two Şerefe balconies
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), MName + TEXT("_Serefe1"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 2650.0f), FRotator::ZeroRotator,
			FVector(3.4f, 3.4f, 0.7f), FLinearColor(0.92f, 0.88f, 0.78f), false);
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), MName + TEXT("_Serefe2"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 3550.0f), FRotator::ZeroRotator,
			FVector(3.2f, 3.2f, 0.7f), FLinearColor(0.92f, 0.88f, 0.78f), false);
		// Conical spire (Külah)
		SpawnShape(TEXT("/Engine/BasicShapes/Cone.Cone"), MName + TEXT("_Spire"),
			MinaretBases[m] + FVector(0.0f, 0.0f, 4550.0f), FRotator::ZeroRotator,
			FVector(2.1f, 2.1f, 7.5f), LeadDomeCol, false);
	}
}

void ATestSceneBuilder::SpawnHGSBridgeCanopy(const FVector& GateCenter)
{
	// 4 Toll Booth Islands + Blue/Orange HGS/OGS Overhead Gantry + Bosphorus Bridge Suspension Towers ahead
	for (int32 Lane = -2; Lane <= 2; ++Lane)
	{
		const float Y = Lane * 580.0f;
		// Concrete crash-barrier island + yellow-black bullnose
		SpawnBox(FString::Printf(TEXT("HGS_Island_%d"), Lane), GateCenter + FVector(0.0f, Y, 45.0f),
			FVector(9.5f, 0.65f, 0.9f), FLinearColor(0.95f, 0.72f, 0.02f), true);
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FString::Printf(TEXT("HGS_Pillar_%d"), Lane),
			GateCenter + FVector(0.0f, Y, 340.0f), FRotator::ZeroRotator,
			FVector(0.65f, 0.65f, 6.8f), FLinearColor(0.18f, 0.22f, 0.28f), false);
	}

	// Overhead steel truss & blue/orange HGS/OGS electronic signboards
	SpawnBox(TEXT("HGS_MainCanopy"), GateCenter + FVector(0.0f, 0.0f, 680.0f),
		FVector(3.2f, 26.0f, 1.15f), FLinearColor(0.06f, 0.35f, 0.78f), false);
	SpawnBox(TEXT("HGS_SignStrip"), GateCenter + FVector(-165.0f, 0.0f, 680.0f),
		FVector(0.15f, 24.5f, 0.75f), FLinearColor(0.98f, 0.52f, 0.04f), false);

	// Iconic 15 Temmuz Bosphorus Bridge Red Suspension Towers + Catenary Main Cables & Vertical Suspenders ahead (+260m)
	const FVector BridgeTowerPos = GateCenter + FVector(8500.0f, 0.0f, 0.0f);
	const FLinearColor BridgeRed(0.76f, 0.14f, 0.12f);
	const FLinearColor CableSteel(0.72f, 0.75f, 0.80f);
	SpawnBox(TEXT("BosphorusBridge_TowerL"), BridgeTowerPos + FVector(0.0f, -1350.0f, 3200.0f),
		FVector(3.8f, 3.8f, 64.0f), BridgeRed, true);
	SpawnBox(TEXT("BosphorusBridge_TowerR"), BridgeTowerPos + FVector(0.0f,  1350.0f, 3200.0f),
		FVector(3.8f, 3.8f, 64.0f), BridgeRed, true);
	SpawnBox(TEXT("BosphorusBridge_CrossBeam1"), BridgeTowerPos + FVector(0.0f, 0.0f, 2800.0f),
		FVector(2.6f, 27.0f, 2.4f), BridgeRed, false);
	SpawnBox(TEXT("BosphorusBridge_CrossBeam2"), BridgeTowerPos + FVector(0.0f, 0.0f, 5400.0f),
		FVector(2.6f, 27.0f, 2.4f), BridgeRed, false);

	// Sloped main suspension cables + vertical suspender hangers
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float CableY = Side * 1350.0f;
		SpawnBox(FString::Printf(TEXT("Bridge_MainCableBack_%d"), Side),
			BridgeTowerPos + FVector(-3600.0f, CableY, 3100.0f),
			FVector(82.0f, 0.45f, 0.45f), CableSteel, false, FRotator(36.0f, 0.0f, 0.0f));
		SpawnBox(FString::Printf(TEXT("Bridge_MainCableSpan_%d"), Side),
			BridgeTowerPos + FVector(3800.0f, CableY, 3400.0f),
			FVector(86.0f, 0.45f, 0.45f), CableSteel, false, FRotator(-32.0f, 0.0f, 0.0f));

		for (int32 h = 1; h <= 5; ++h)
		{
			const float HX = -h * 1100.0f;
			const float HHeightM = FMath::Max(8.0f, 52.0f - h * 8.5f);
			SpawnBox(FString::Printf(TEXT("Bridge_Hanger_%d_%d"), Side, h),
				BridgeTowerPos + FVector(HX, CableY, HHeightM * 50.0f),
				FVector(0.12f, 0.12f, HHeightM), CableSteel, false);
		}
	}
}

void ATestSceneBuilder::BuildPrototypeScene()
{
	UWorld* W = GetWorld();
	if (!W || bBuilt) return;
	bBuilt = true;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// =========================================================================
	// 1. PERMANENT AUTO-EXPOSURE LOCK & BALANCED LIGHTING
	// =========================================================================
	ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(W, ADirectionalLight::StaticClass()));
	if (!Sun)
	{
		Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(-38.0f, 155.0f, 0.0f), SpawnParams);
	}
	if (Sun && Sun->GetLightComponent())
	{
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* DLC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			DLC->SetAtmosphereSunLight(true);
			DLC->SetIntensity(36.0f);
		}
	}

	ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(W, ASkyLight::StaticClass()));
	if (!Sky)
	{
		Sky = W->SpawnActor<ASkyLight>(FVector(0, 0, 1000), FRotator::ZeroRotator, SpawnParams);
	}
	if (Sky && Sky->GetLightComponent())
	{
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->bRealTimeCapture = true;
		Sky->GetLightComponent()->SetIntensity(4.5f);
	}

	if (!UGameplayStatics::GetActorOfClass(W, ASkyAtmosphere::StaticClass()))
	{
		W->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}

	AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(W, AExponentialHeightFog::StaticClass()));
	if (!Fog)
	{
		Fog = W->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 200), FRotator::ZeroRotator, SpawnParams);
	}

	APostProcessVolume* PP = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(W, APostProcessVolume::StaticClass()));
	if (!PP)
	{
		PP = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}
	if (PP)
	{
		PP->bUnbound = true;
		PP->Priority = 100.0f;
		// Permanent Manual Exposure Lock (AEM_Manual + Bias -4.0f) so the viewport never washes out to white!
		PP->Settings.bOverride_AutoExposureMethod = true;
		PP->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		PP->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		PP->Settings.AutoExposureApplyPhysicalCameraExposure = false;
		PP->Settings.bOverride_AutoExposureMinBrightness = true;
		PP->Settings.AutoExposureMinBrightness = 1.0f;
		PP->Settings.bOverride_AutoExposureMaxBrightness = true;
		PP->Settings.AutoExposureMaxBrightness = 1.0f;
		PP->Settings.bOverride_AutoExposureBias = true;
		PP->Settings.AutoExposureBias = -4.0f;
	}

	ATimeOfDayActor* TODActor = Cast<ATimeOfDayActor>(UGameplayStatics::GetActorOfClass(W, ATimeOfDayActor::StaticClass()));
	if (!TODActor)
	{
		TODActor = W->SpawnActor<ATimeOfDayActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}
	if (TODActor)
	{
		TODActor->SunLight = Sun;
		TODActor->SkyLight = Sky;
		TODActor->HeightFog = Fog;
		TODActor->PostProcessVolume = PP;
		if (UTimeOfDaySubsystem* TimeSub = W->GetSubsystem<UTimeOfDaySubsystem>())
		{
			TimeSub->RegisterSceneActors(Sun, Sky, Fog, PP, TODActor->PresetOverride);
		}
	}

	const FVector Origin = GetActorLocation();

	// =========================================================================
	// 2. ZONE A — COASTAL MAIN BOULEVARD (SAHİL YOLU / KENNEDY CADDESİ -> KÖPRÜ)
	// =========================================================================
	// 420m long x 22m wide multi-lane asphalt coastal artery (X = -40m to +380m, Y = 0)
	SpawnBox(TEXT("Road_CoastalBoulevard"), Origin + FVector(17000.0f, 0.0f, -25.0f),
		FVector(420.0f, 22.0f, 0.5f), FLinearColor(0.08f, 0.085f, 0.095f), true, FRotator::ZeroRotator, TEXT("Surface_Asphalt"));

	// T1 Tramway Raised Concrete Bed & Steel Rails along the median strip (triggers Surface_TramRail vibration!)
	SpawnBox(TEXT("Tram_RailBed"), Origin + FVector(8000.0f, -20.0f, 1.5f),
		FVector(180.0f, 2.8f, 0.05f), FLinearColor(0.28f, 0.29f, 0.31f), true, FRotator::ZeroRotator, TEXT("Surface_TramRail"));

	// Patched Asphalt & Pothole clusters along the boulevard (triggers Surface_Pothole impact jolts!)
	SpawnBox(TEXT("Surface_PatchedStrip1"), Origin + FVector(5200.0f, 450.0f, 1.0f),
		FVector(14.0f, 3.6f, 0.03f), FLinearColor(0.05f, 0.05f, 0.06f), true, FRotator::ZeroRotator, TEXT("Surface_PatchedAsphalt"));
	SpawnBox(TEXT("Surface_Pothole1"), Origin + FVector(6800.0f, 380.0f, 2.0f),
		FVector(1.8f, 1.6f, 0.06f), FLinearColor(0.03f, 0.03f, 0.035f), true, FRotator::ZeroRotator, TEXT("Surface_Pothole"));
	SpawnBox(TEXT("Surface_Pothole2"), Origin + FVector(14200.0f, -420.0f, 2.0f),
		FVector(2.1f, 1.8f, 0.06f), FLinearColor(0.03f, 0.03f, 0.035f), true, FRotator::ZeroRotator, TEXT("Surface_Pothole"));

	// High Istanbul Curbstones (Yüksek Kaldırım, 22cm high) + Pedestrian Promenades on both sides
	const FLinearColor SidewalkCol(0.38f, 0.39f, 0.41f);
	// Left (Bosphorus Seafront Corniche — Kordon)
	SpawnBox(TEXT("Sidewalk_Seafront"), Origin + FVector(17000.0f, -1350.0f, 11.0f),
		FVector(420.0f, 5.0f, 0.24f), SidewalkCol, true);
	// Right sidewalks (broken at X=+35m and X=+205m for the Side-Street Junctions!)
	SpawnBox(TEXT("Sidewalk_City_Seg1"), Origin + FVector(-250.0f, 1350.0f, 11.0f),
		FVector(65.0f, 5.0f, 0.24f), SidewalkCol, true);
	SpawnBox(TEXT("Sidewalk_City_Seg2"), Origin + FVector(12000.0f, 1350.0f, 11.0f),
		FVector(150.0f, 5.0f, 0.24f), SidewalkCol, true);
	SpawnBox(TEXT("Sidewalk_City_Seg3"), Origin + FVector(29500.0f, 1350.0f, 11.0f),
		FVector(165.0f, 5.0f, 0.24f), SidewalkCol, true);

	// Yellow-Black Striped Istanbul Concrete Curb Stones & Bollards + Streetlamps along the boulevard
	for (int32 i = 0; i < 20; ++i)
	{
		const float X = i * 2000.0f - 2000.0f;
		// Center lane dashes
		SpawnBox(FString::Printf(TEXT("Blvd_DashR_%d"), i), Origin + FVector(X,  520.0f, 1.5f),
			FVector(6.0f, 0.18f, 0.02f), FLinearColor(0.92f, 0.92f, 0.92f), false);
		SpawnBox(FString::Printf(TEXT("Blvd_DashL_%d"), i), Origin + FVector(X, -520.0f, 1.5f),
			FVector(6.0f, 0.18f, 0.02f), FLinearColor(0.92f, 0.92f, 0.92f), false);

		// Seafront bollards (Dubalar)
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FString::Printf(TEXT("SeaBollard_%d"), i),
			Origin + FVector(X, -1120.0f, 48.0f), FRotator::ZeroRotator,
			FVector(0.24f, 0.24f, 0.95f), (i % 2 == 0) ? FLinearColor(0.96f, 0.72f, 0.02f) : FLinearColor(0.08f, 0.08f, 0.09f), true);

		if (i % 2 == 0)
		{
			SpawnStreetLamp(FString::Printf(TEXT("Lamp_Sea_%d"), i),  Origin + FVector(X, -1180.0f, 20.0f),  90.0f);
			SpawnStreetLamp(FString::Printf(TEXT("Lamp_City_%d"), i), Origin + FVector(X,  1180.0f, 20.0f), -90.0f);

			// Overhead T1 Tramway catenary wire & cross-street support cable over the boulevard
			if (X >= 0.0f && X <= 18000.0f)
			{
				SpawnBox(FString::Printf(TEXT("Tram_CrossSpanCable_%d"), i),
					Origin + FVector(X, 0.0f, 680.0f),
					FVector(0.05f, 23.6f, 0.05f), FLinearColor(0.05f, 0.05f, 0.06f), false);
			}
		}
	}

	// Continuous T1 Tramway overhead copper contact wire along the median
	SpawnBox(TEXT("Tram_ContactWire"), Origin + FVector(8000.0f, -20.0f, 675.0f),
		FVector(180.0f, 0.05f, 0.05f), FLinearColor(0.08f, 0.08f, 0.09f), false);

	// Bosphorus Scenic Overlook Ledge & Corniche along the seafront
	SpawnBosphorusScenicOverlook(Origin);

	// Two İETT Bus Stops (Eminönü İskele Durağı at +25m & Kabataş Durağı at +155m)
	SpawnIETTBusStop(TEXT("IETT_Stop_Eminonu"), Origin + FVector(2500.0f, 1320.0f, 22.0f), -90.0f);
	SpawnIETTBusStop(TEXT("IETT_Stop_Kabatas"), Origin + FVector(15500.0f, 1320.0f, 22.0f), -90.0f);

	// Pickup Marker (+30m at Eminönü İETT Stop) & Drop-off Marker (+180m before HGS Bridge)
	SpawnBox(TEXT("Proto_PickupMarker_30m"), Origin + FVector(3000.0f, 1020.0f, 180.0f),
		FVector(0.75f, 0.75f, 3.6f), FLinearColor(0.15f, 0.95f, 0.35f), false);
	SpawnBox(TEXT("Proto_DropoffMarker_180m"), Origin + FVector(18000.0f, 1020.0f, 180.0f),
		FVector(0.75f, 0.75f, 3.6f), FLinearColor(1.0f, 0.68f, 0.1f), false);

	// HGS / OGS Toll Plaza at X = +235m + Bosphorus Bridge Towers at X = +320m
	const FVector TollPos = Origin + FVector(23500.0f, 0.0f, 180.0f);
	if (!UGameplayStatics::GetActorOfClass(W, ATollGateVolume::StaticClass()))
	{
		W->SpawnActor<ATollGateVolume>(TollPos, FRotator::ZeroRotator, SpawnParams);
		SpawnHGSBridgeCanopy(Origin + FVector(23500.0f, 0.0f, 0.0f));
	}

	// =========================================================================
	// 3. ZONE B — STEEP 12° COBBLESTONE NARROW SIDE STREETS (CİHANGİR / GALATA YOKUŞU)
	// =========================================================================
	// Side Street Loop turns right off the boulevard at X=+35m, climbs a steep ~12° cobblestone hill (Yokuş) to Z=+1180cm
	// at Y=+76m, runs along the upper historic ridge (X=+35m .. +205m, Y=+76m, Z=+1180cm) past the hilltop mosque,
	// and descends a steep ~12° cobblestone ramp back to the boulevard at X=+205m!
	const FLinearColor CobbleCol(0.26f, 0.25f, 0.24f);

	// 1) Steep Uphill Narrow Cobblestone Alley (Cihangir Yokuşu Yukarı, ~12° grade): X = +35m, Y = +11m -> +76m, Z = 0 -> +1180cm
	SpawnBox(TEXT("Cobble_UphillRamp"), Origin + FVector(3500.0f, 4350.0f, 570.0f),
		FVector(69.5f, 8.8f, 0.45f), CobbleCol, true, FRotator(12.0f, 90.0f, 0.0f), TEXT("Surface_Cobblestone"));

	// 2) Upper Historic Ridge Cobblestone Street (Cihangir / Galata / Sultanahmet Sokak): X = +35m -> +205m, Y = +76m, Z = +1180cm
	SpawnBox(TEXT("Cobble_UpperRidgeStreet"), Origin + FVector(12000.0f, 7600.0f, 1158.0f),
		FVector(182.0f, 9.2f, 0.45f), CobbleCol, true, FRotator::ZeroRotator, TEXT("Surface_Cobblestone"));

	// 3) Steep Downhill Narrow Cobblestone Alley (Beşiktaş / Tophane Yokuşu Aşağı, ~12° grade): X = +205m, Y = +76m -> +11m, Z = +1180cm -> 0
	SpawnBox(TEXT("Cobble_DownhillRamp"), Origin + FVector(20500.0f, 4350.0f, 570.0f),
		FVector(69.5f, 8.8f, 0.45f), CobbleCol, true, FRotator(12.0f, -90.0f, 0.0f), TEXT("Surface_Cobblestone"));

	// Historical Byzantine/Ottoman Stone Retaining Walls (Almaşık Taş İstinat Duvarları) flanking the steep hill alleys
	SpawnHistoricalStoneWall(TEXT("HistWall_Uphill_Left"),    Origin + FVector(3010.0f,  4350.0f, 710.0f), FRotator(12.0f,  90.0f, 0.0f), 64.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_Uphill_Right"),   Origin + FVector(3990.0f,  4350.0f, 710.0f), FRotator(12.0f,  90.0f, 0.0f), 64.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_Downhill_Left"),  Origin + FVector(20010.0f, 4350.0f, 710.0f), FRotator(12.0f, -90.0f, 0.0f), 64.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_Downhill_Right"), Origin + FVector(20990.0f, 4350.0f, 710.0f), FRotator(12.0f, -90.0f, 0.0f), 64.0f, 2.8f);
	SpawnHistoricalStoneWall(TEXT("HistWall_RidgeTerrace"),   Origin + FVector(12000.0f, 5420.0f, 920.0f), FRotator::ZeroRotator,         150.0f, 5.2f);

	// High narrow curbstones along the Upper Historic Street (Z = 1180cm + curb height)
	SpawnBox(TEXT("UpperCurb_South"), Origin + FVector(12000.0f, 7040.0f, 1192.0f),
		FVector(160.0f, 2.2f, 0.28f), SidewalkCol, true);
	SpawnBox(TEXT("UpperCurb_North"), Origin + FVector(12000.0f, 8160.0f, 1192.0f),
		FVector(160.0f, 2.2f, 0.28f), SidewalkCol, true);

	// Closely Packed 4-to-6 Story Bitişik Nizam Apartment Blocks flanking both the Boulevard and Narrow Streets
	static const FLinearColor IstanbulPalette[7] = {
		FLinearColor(0.76f, 0.56f, 0.36f), // Ochre / Mustard
		FLinearColor(0.68f, 0.34f, 0.28f), // Terracotta Red
		FLinearColor(0.74f, 0.72f, 0.66f), // Warm Limestone
		FLinearColor(0.45f, 0.58f, 0.68f), // Balat Pastel Blue
		FLinearColor(0.55f, 0.64f, 0.48f), // Pistachio Green
		FLinearColor(0.72f, 0.62f, 0.52f), // Sand Plaster
		FLinearColor(0.62f, 0.46f, 0.42f)  // Dusty Rose
	};

	// Row 1: Boulevard city-side apartment blocks (between X = +52m and X = +188m, facing the Bosphorus at Y = +22m)
	for (int32 b = 0; b < 9; ++b)
	{
		const float BX = 5400.0f + b * 1600.0f;
		const int32 Floors = 4 + (b % 3);
		SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_Blvd_%d"), b),
			Origin + FVector(BX, 2350.0f, 0.0f), -90.0f, Floors, 15.6f, 14.0f,
			IstanbulPalette[b % 7], (b % 2 == 0), true);
	}

	// Row 2: Narrow Uphill Street flanking apartments (X = +21m & X = +49m, climbing along Y = +24m .. +64m up the 12° hill)
	for (int32 b = 0; b < 3; ++b)
	{
		const float BY = 2600.0f + b * 1700.0f;
		const float BZ = 260.0f + b * 310.0f;
		SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UphillL_%d"), b),
			Origin + FVector(2150.0f, BY, BZ), 0.0f, 5, 16.5f, 13.0f,
			IstanbulPalette[(b + 2) % 7], true, true);
		SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UphillR_%d"), b),
			Origin + FVector(4850.0f, BY, BZ), 180.0f, 4 + (b % 2), 16.5f, 13.0f,
			IstanbulPalette[(b + 4) % 7], true, (b == 0));
	}

	// Row 3: Upper Historic Ridge tightly-packed bitişik nizam row houses (Y = +61m and Y = +91m, Z = +1180cm)
	for (int32 b = 0; b < 9; ++b)
	{
		const float BX = 5400.0f + b * 1600.0f;
		SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UpperSouth_%d"), b),
			Origin + FVector(BX, 6150.0f, 1180.0f), 90.0f, 4 + (b % 3), 15.8f, 13.0f,
			IstanbulPalette[(b + 1) % 7], true, true);

		// Leave a grand plaza opening at b == 4..5 for the Hilltop Mosque Complex!
		if (b < 3 || b > 5)
		{
			SpawnIstanbulApartmentBlock(FString::Printf(TEXT("Apt_UpperNorth_%d"), b),
				Origin + FVector(BX, 9050.0f, 1180.0f), -90.0f, 5 + (b % 2), 15.8f, 13.0f,
				IstanbulPalette[(b + 3) % 7], true, true);
		}

		// Yellow-black narrow alley bollards (Dubalar) on the upper ridge curbstones
		SpawnShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FString::Printf(TEXT("UpperBollard_%d"), b),
			Origin + FVector(BX, 7110.0f, 1225.0f), FRotator::ZeroRotator,
			FVector(0.22f, 0.22f, 0.85f), (b % 2 == 0) ? FLinearColor(0.96f, 0.72f, 0.02f) : FLinearColor(0.08f, 0.08f, 0.09f), true);

		// Overhead tangled electrical cables & diagonal crisscrossing utility lines across the narrow upper street
		SpawnBox(FString::Printf(TEXT("OverheadCable_%d"), b),
			Origin + FVector(BX, 7600.0f, 1940.0f),
			FVector(0.05f, 16.0f, 0.05f), FLinearColor(0.03f, 0.03f, 0.03f), false);
		SpawnBox(FString::Printf(TEXT("OverheadCrisscross_%d"), b),
			Origin + FVector(BX + 450.0f, 7600.0f, 1985.0f),
			FVector(0.05f, 17.5f, 0.05f), FLinearColor(0.04f, 0.04f, 0.05f), false, FRotator(0.0f, (b % 2 == 0) ? 24.0f : -24.0f, 0.0f));
		SpawnStreetLamp(FString::Printf(TEXT("UpperAlleyLamp_%d"), b),
			Origin + FVector(BX - 520.0f, 7090.0f, 1195.0f), 90.0f);
	}

	// Cars parked half-on-the-sidewalk (Kaldırıma yarı çıkmış park etmiş araçlar, tilted 9° Roll) in the narrow streets
	SpawnParkedCurbCar(TEXT("ParkedCar_Uphill1"), Origin + FVector(3120.0f, 3100.0f, 385.0f),  FRotator(12.0f, 90.0f,  9.0f), FLinearColor(0.75f, 0.78f, 0.82f));
	SpawnParkedCurbCar(TEXT("ParkedCar_Upper1"),  Origin + FVector(6800.0f, 7140.0f, 1195.0f), FRotator(0.0f,   2.0f, -9.0f), FLinearColor(0.16f, 0.22f, 0.35f));
	SpawnParkedCurbCar(TEXT("ParkedCar_Upper2"),  Origin + FVector(9800.0f, 8060.0f, 1195.0f), FRotator(0.0f, 178.0f,  9.0f), FLinearColor(0.58f, 0.12f, 0.12f));
	SpawnParkedCurbCar(TEXT("ParkedCar_Upper3"),  Origin + FVector(15600.0f, 7140.0f, 1195.0f), FRotator(0.0f,  0.0f, -9.0f), FLinearColor(0.82f, 0.82f, 0.80f));

	// =========================================================================
	// 4. HILLTOP MOSQUE SILHOUETTE, GALATA TOWER, BOSPHORUS WATER & AUDIO ZONES
	// =========================================================================
	const FVector MosqueHillPos = Origin + FVector(11800.0f, 10600.0f, 1180.0f);
	SpawnDetailedMosqueComplex(MosqueHillPos);

	// Iconic Galata Tower (Galata Kulesi) crowning the western ridge at the top of the Cihangir/Galata climb
	SpawnGalataTowerLandmark(Origin + FVector(3500.0f, 9600.0f, 1180.0f));

	if (!UGameplayStatics::GetActorOfClass(W, AMosqueActor::StaticClass()))
	{
		USoundBase* PlaceholderChime = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));

		AMosqueActor* MosqueHill = W->SpawnActor<AMosqueActor>(MosqueHillPos, FRotator::ZeroRotator, SpawnParams);
		if (MosqueHill)
		{
			MosqueHill->MosqueName = FText::FromString(TEXT("Sultanahmet Tepesi Camii"));
			MosqueHill->MinStaggerDelaySeconds = 0.4f;
			MosqueHill->MaxStaggerDelaySeconds = 2.0f;
			MosqueHill->FallbackEzanSound = PlaceholderChime;
		}

		AMosqueActor* MosqueCoast = W->SpawnActor<AMosqueActor>(Origin + FVector(19500.0f, -1800.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
		if (MosqueCoast)
		{
			MosqueCoast->MosqueName = FText::FromString(TEXT("Sahil Yeni Cami"));
			MosqueCoast->MinStaggerDelaySeconds = 3.5f;
			MosqueCoast->MaxStaggerDelaySeconds = 8.5f;
			MosqueCoast->FallbackEzanSound = PlaceholderChime;
		}
	}

	if (!UGameplayStatics::GetActorOfClass(W, ADistrictAmbienceVolume::StaticClass()))
	{
		// Coastal Boulevard Zone
		ADistrictAmbienceVolume* CoastZone = W->SpawnActor<ADistrictAmbienceVolume>(Origin + FVector(11000.0f, 0.0f, 200.0f), FRotator::ZeroRotator, SpawnParams);
		if (CoastZone)
		{
			UDistrictAmbienceDataAsset* EminonuProf = NewObject<UDistrictAmbienceDataAsset>(CoastZone);
			EminonuProf->DistrictTag = IstanbulTaxiTags::District_HistoricPeninsula;
			EminonuProf->DisplayName = FText::FromString(TEXT("Eminönü / Sahil Bulvarı"));
			EminonuProf->BaseTrafficMurmur = 0.72f;
			EminonuProf->TeaGardenClatter = 0.35f;
			EminonuProf->CrowdChatter = 0.65f;
			EminonuProf->BaseSeagullIntensity = 0.45f;
			CoastZone->AmbienceProfile = EminonuProf;
			CoastZone->Priority = 5;
		}

		// Upper Historic Cobblestone District Zone (higher priority)
		ADistrictAmbienceVolume* HillZone = W->SpawnActor<ADistrictAmbienceVolume>(Origin + FVector(12000.0f, 7600.0f, 1280.0f), FRotator::ZeroRotator, SpawnParams);
		if (HillZone)
		{
			UDistrictAmbienceDataAsset* SultanProf = NewObject<UDistrictAmbienceDataAsset>(HillZone);
			SultanProf->DistrictTag = IstanbulTaxiTags::District_HistoricPeninsula;
			SultanProf->DisplayName = FText::FromString(TEXT("Cihangir / Galata / Sultanahmet Yokuşu"));
			SultanProf->BaseTrafficMurmur = 0.35f;
			SultanProf->TeaGardenClatter = 0.80f;
			SultanProf->CrowdChatter = 0.55f;
			SultanProf->BaseSeagullIntensity = 0.20f;
			HillZone->AmbienceProfile = SultanProf;
			HillZone->Priority = 15;
		}
	}

	if (!UGameplayStatics::GetActorOfClass(W, AWaterfrontSplineActor::StaticClass()))
	{
		AWaterfrontSplineActor* Shore = W->SpawnActor<AWaterfrontSplineActor>(Origin + FVector(0.0f, -1650.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
		if (Shore && Shore->ShorelineSpline)
		{
			Shore->ShorelineSpline->ClearSplinePoints();
			Shore->ShorelineSpline->AddSplinePoint(Origin + FVector(-4000.0f, -1650.0f, 0.0f), ESplineCoordinateSpace::World);
			Shore->ShorelineSpline->AddSplinePoint(Origin + FVector(38000.0f, -1650.0f, 0.0f), ESplineCoordinateSpace::World);
			Shore->ShorelineSpline->UpdateSpline();
		}
	}

	if (!UGameplayStatics::GetActorOfClass(W, ABosphorusWaterVolume::StaticClass()))
	{
		W->SpawnActor<ABosphorusWaterVolume>(Origin, FRotator::ZeroRotator, SpawnParams);
	}

	// =========================================================================
	// 5. ISTANBUL AI TRAFFIC NETWORK (Boulevard + Cobblestone Hill + Double-Parked)
	// =========================================================================
	if (!UGameplayStatics::GetActorOfClass(W, AIstanbulTrafficManager::StaticClass()))
	{
		AIstanbulTrafficManager* TrafficMgr = W->SpawnActor<AIstanbulTrafficManager>(Origin, FRotator::ZeroRotator, SpawnParams);
		if (TrafficMgr)
		{
			TArray<FVector> BlvdRight = {
				Origin + FVector(-2000.0f,  480.0f, 5.0f),
				Origin + FVector( 6000.0f,  480.0f, 5.0f),
				Origin + FVector(14000.0f,  480.0f, 5.0f),
				Origin + FVector(22000.0f,  480.0f, 5.0f),
				Origin + FVector(32000.0f,  480.0f, 5.0f),
				Origin + FVector(32000.0f, -480.0f, 5.0f),
				Origin + FVector(-2000.0f, -480.0f, 5.0f)
			};

			TArray<FVector> BlvdLeft = {
				Origin + FVector(31000.0f, -480.0f, 5.0f),
				Origin + FVector(22000.0f, -480.0f, 5.0f),
				Origin + FVector(13000.0f, -480.0f, 5.0f),
				Origin + FVector( 4000.0f, -480.0f, 5.0f),
				Origin + FVector(-2000.0f, -480.0f, 5.0f),
				Origin + FVector(-2000.0f,  480.0f, 5.0f),
				Origin + FVector(31000.0f,  480.0f, 5.0f)
			};

			TArray<FVector> SideStreetLoop = {
				Origin + FVector( 3500.0f,  800.0f,    5.0f),
				Origin + FVector( 3500.0f, 7600.0f, 1185.0f),
				Origin + FVector(11500.0f, 7600.0f, 1185.0f),
				Origin + FVector(20500.0f, 7600.0f, 1185.0f),
				Origin + FVector(20500.0f,  800.0f,    5.0f),
				Origin + FVector(12000.0f,  480.0f,    5.0f)
			};

			TrafficMgr->PopulateIstanbulTraffic(Origin, BlvdRight, BlvdLeft, SideStreetLoop);
		}
	}

	UE_LOG(LogIstanbulTaxi, Log, TEXT("ATestSceneBuilder: Spawned multi-zone Istanbul urban environment (Exposure locked, Sahil Bulvarı, Cobblestone Yokuş & Ara Sokaklar, Hilltop Mosque, HGS Bridge, AI Traffic)."));
}
