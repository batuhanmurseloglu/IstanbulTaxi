#include "Core/IstanbulMeshUtils.h"
#include "IstanbulTaxi.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

void FIstanbulMeshSectionData::CommitToComponent(UProceduralMeshComponent* PMC, int32 SectionIndex, bool bCreateCollision, UMaterialInterface* Material) const
{
	if (!PMC || Vertices.Num() == 0 || Triangles.Num() == 0)
	{
		return;
	}
	PMC->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, bCreateCollision);
	if (Material)
	{
		PMC->SetMaterial(SectionIndex, Material);
	}
}

UStaticMesh* FIstanbulMeshUtils::LoadBasicShapeSafe(const TCHAR* PreferredPath)
{
	if (PreferredPath)
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, PreferredPath))
		{
			return Mesh;
		}
		UE_LOG(LogIstanbulTaxi, Warning, TEXT("FIstanbulMeshUtils: Failed to load StaticMesh '%s', falling back to /Engine/BasicShapes/Cube.Cube"), PreferredPath);
	}
	UStaticMesh* Fallback = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Fallback)
	{
		UE_LOG(LogIstanbulTaxi, Warning, TEXT("FIstanbulMeshUtils: Fallback Cube also returned nullptr!"));
	}
	return Fallback;
}

UMaterialInterface* FIstanbulMeshUtils::LoadVertexColorMaterialSafe()
{
	if (UMaterialInterface* VtxMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly")))
	{
		return VtxMat;
	}
	if (UMaterialInterface* VtxMat2 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial")))
	{
		return VtxMat2;
	}
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!BaseMat)
	{
		UE_LOG(LogIstanbulTaxi, Warning, TEXT("FIstanbulMeshUtils: Could not load BasicShapeMaterial fallback."));
	}
	return BaseMat;
}

UMaterialInstanceDynamic* FIstanbulMeshUtils::CreatePBRMaterial(UObject* Outer, const FLinearColor& BaseColor,
	float Metallic, float Roughness, float Specular)
{
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!BaseMat)
	{
		BaseMat = LoadVertexColorMaterialSafe();
	}
	if (!BaseMat)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Outer);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), BaseColor);
		MID->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
		MID->SetScalarParameterValue(TEXT("Metallic"), FMath::Clamp(Metallic, 0.0f, 1.0f));
		MID->SetScalarParameterValue(TEXT("Roughness"), FMath::Clamp(Roughness, 0.02f, 1.0f));
		MID->SetScalarParameterValue(TEXT("Specular"), FMath::Clamp(Specular, 0.0f, 1.0f));
	}
	return MID;
}

void FIstanbulMeshUtils::AppendQuad(FIstanbulMeshSectionData& OutSection, const FVector& V0, const FVector& V1,
	const FVector& V2, const FVector& V3, const FLinearColor& Color)
{
	const int32 BaseIdx = OutSection.Vertices.Num();
	const FVector Edge1 = (V1 - V0);
	const FVector Edge2 = (V3 - V0);
	const FVector Normal = FVector::CrossProduct(Edge2, Edge1).GetSafeNormal();
	const FProcMeshTangent Tangent(Edge1.GetSafeNormal(), false);

	OutSection.Vertices.Add(V0);
	OutSection.Vertices.Add(V1);
	OutSection.Vertices.Add(V2);
	OutSection.Vertices.Add(V3);

	for (int32 i = 0; i < 4; ++i)
	{
		OutSection.Normals.Add(Normal);
		OutSection.VertexColors.Add(Color);
		OutSection.Tangents.Add(Tangent);
	}
	OutSection.UVs.Add(FVector2D(0.0f, 0.0f));
	OutSection.UVs.Add(FVector2D(1.0f, 0.0f));
	OutSection.UVs.Add(FVector2D(1.0f, 1.0f));
	OutSection.UVs.Add(FVector2D(0.0f, 1.0f));

	// Two triangles (CCW winding in UE)
	OutSection.Triangles.Add(BaseIdx + 0);
	OutSection.Triangles.Add(BaseIdx + 3);
	OutSection.Triangles.Add(BaseIdx + 2);

	OutSection.Triangles.Add(BaseIdx + 0);
	OutSection.Triangles.Add(BaseIdx + 2);
	OutSection.Triangles.Add(BaseIdx + 1);
}

void FIstanbulMeshUtils::AppendBeveledBox(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
	const FVector& HalfExtentsCm, float BevelCm, const FLinearColor& Color)
{
	const FVector E(
		FMath::Max(HalfExtentsCm.X, 0.4f),
		FMath::Max(HalfExtentsCm.Y, 0.4f),
		FMath::Max(HalfExtentsCm.Z, 0.4f));
	const float B = FMath::Clamp(BevelCm, 0.0f, FMath::Min3(E.X, E.Y, E.Z) * 0.45f);
	const FVector I = E - FVector(B);

	auto Xf = [&Transform](float X, float Y, float Z) -> FVector
	{
		return Transform.TransformPosition(FVector(X, Y, Z));
	};

	const FLinearColor TopCol  = Color * 1.06f;
	const FLinearColor SideCol = Color;
	const FLinearColor BevCol  = Color * 0.94f;
	const FLinearColor BotCol  = Color * 0.80f;

	// 6 Principal Inner Faces
	// +Z Top
	AppendQuad(OutSection, Xf(-I.X, -I.Y,  E.Z), Xf( I.X, -I.Y,  E.Z), Xf( I.X,  I.Y,  E.Z), Xf(-I.X,  I.Y,  E.Z), TopCol);
	// -Z Bottom
	AppendQuad(OutSection, Xf(-I.X,  I.Y, -E.Z), Xf( I.X,  I.Y, -E.Z), Xf( I.X, -I.Y, -E.Z), Xf(-I.X, -I.Y, -E.Z), BotCol);
	// +X Front
	AppendQuad(OutSection, Xf( E.X, -I.Y, -I.Z), Xf( E.X,  I.Y, -I.Z), Xf( E.X,  I.Y,  I.Z), Xf( E.X, -I.Y,  I.Z), SideCol);
	// -X Back
	AppendQuad(OutSection, Xf(-E.X,  I.Y, -I.Z), Xf(-E.X, -I.Y, -I.Z), Xf(-E.X, -I.Y,  I.Z), Xf(-E.X,  I.Y,  I.Z), SideCol);
	// +Y Right
	AppendQuad(OutSection, Xf( I.X,  E.Y, -I.Z), Xf(-I.X,  E.Y, -I.Z), Xf(-I.X,  E.Y,  I.Z), Xf( I.X,  E.Y,  I.Z), SideCol);
	// -Y Left
	AppendQuad(OutSection, Xf(-I.X, -E.Y, -I.Z), Xf( I.X, -E.Y, -I.Z), Xf( I.X, -E.Y,  I.Z), Xf(-I.X, -E.Y,  I.Z), SideCol);

	if (B > 0.1f)
	{
		// 4 Top Bevel Chamfers
		AppendQuad(OutSection, Xf( I.X, -I.Y,  E.Z), Xf( E.X, -I.Y,  I.Z), Xf( E.X,  I.Y,  I.Z), Xf( I.X,  I.Y,  E.Z), BevCol);
		AppendQuad(OutSection, Xf(-I.X,  I.Y,  E.Z), Xf(-E.X,  I.Y,  I.Z), Xf(-E.X, -I.Y,  I.Z), Xf(-I.X, -I.Y,  E.Z), BevCol);
		AppendQuad(OutSection, Xf( I.X,  I.Y,  E.Z), Xf( E.X,  I.Y,  I.Z), Xf(-E.X,  I.Y,  I.Z), Xf(-I.X,  I.Y,  E.Z), BevCol);
		AppendQuad(OutSection, Xf(-I.X, -I.Y,  E.Z), Xf(-E.X, -I.Y,  I.Z), Xf( E.X, -I.Y,  I.Z), Xf( I.X, -I.Y,  E.Z), BevCol);

		// 4 Vertical Corner Bevel Chamfers
		AppendQuad(OutSection, Xf( E.X,  I.Y, -I.Z), Xf( I.X,  E.Y, -I.Z), Xf( I.X,  E.Y,  I.Z), Xf( E.X,  I.Y,  I.Z), BevCol);
		AppendQuad(OutSection, Xf( I.X, -E.Y, -I.Z), Xf( E.X, -I.Y, -I.Z), Xf( E.X, -I.Y,  I.Z), Xf( I.X, -E.Y,  I.Z), BevCol);
		AppendQuad(OutSection, Xf(-E.X, -I.Y, -I.Z), Xf(-I.X, -E.Y, -I.Z), Xf(-I.X, -E.Y,  I.Z), Xf(-E.X, -I.Y,  I.Z), BevCol);
		AppendQuad(OutSection, Xf(-I.X,  E.Y, -I.Z), Xf(-E.X,  I.Y, -I.Z), Xf(-E.X,  I.Y,  I.Z), Xf(-I.X,  E.Y,  I.Z), BevCol);
	}
}

void FIstanbulMeshUtils::AppendTaperedPrism(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
	const FVector2D& BottomHalfXY, const FVector2D& TopHalfXY, const FVector& TopOffsetXYZ,
	float HalfHeightZ, const FLinearColor& Color)
{
	auto Xf = [&Transform](const FVector& P) -> FVector
	{
		return Transform.TransformPosition(P);
	};

	const FVector B00(-BottomHalfXY.X, -BottomHalfXY.Y, -HalfHeightZ);
	const FVector B10( BottomHalfXY.X, -BottomHalfXY.Y, -HalfHeightZ);
	const FVector B11( BottomHalfXY.X,  BottomHalfXY.Y, -HalfHeightZ);
	const FVector B01(-BottomHalfXY.X,  BottomHalfXY.Y, -HalfHeightZ);

	const FVector T00 = FVector(-TopHalfXY.X, -TopHalfXY.Y, HalfHeightZ) + TopOffsetXYZ;
	const FVector T10 = FVector( TopHalfXY.X, -TopHalfXY.Y, HalfHeightZ) + TopOffsetXYZ;
	const FVector T11 = FVector( TopHalfXY.X,  TopHalfXY.Y, HalfHeightZ) + TopOffsetXYZ;
	const FVector T01 = FVector(-TopHalfXY.X,  TopHalfXY.Y, HalfHeightZ) + TopOffsetXYZ;

	// Top & Bottom
	AppendQuad(OutSection, Xf(T00), Xf(T10), Xf(T11), Xf(T01), Color * 1.06f);
	AppendQuad(OutSection, Xf(B01), Xf(B11), Xf(B10), Xf(B00), Color * 0.80f);

	// 4 Sloped Sides (+X Front, -X Back, +Y Right, -Y Left)
	AppendQuad(OutSection, Xf(B10), Xf(B11), Xf(T11), Xf(T10), Color * 0.98f);
	AppendQuad(OutSection, Xf(B01), Xf(B00), Xf(T00), Xf(T01), Color * 0.95f);
	AppendQuad(OutSection, Xf(B11), Xf(B01), Xf(T01), Xf(T11), Color);
	AppendQuad(OutSection, Xf(B00), Xf(B10), Xf(T10), Xf(T00), Color);
}

void FIstanbulMeshUtils::AppendCylinder(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
	float RadiusCm, float HalfHeightCm, int32 Sides, const FLinearColor& Color, bool bCapEnds)
{
	const int32 N = FMath::Clamp(Sides, 6, 36);
	for (int32 i = 0; i < N; ++i)
	{
		const float A0 = (2.0f * PI * i) / N;
		const float A1 = (2.0f * PI * (i + 1)) / N;
		const float X0 = FMath::Cos(A0) * RadiusCm, Y0 = FMath::Sin(A0) * RadiusCm;
		const float X1 = FMath::Cos(A1) * RadiusCm, Y1 = FMath::Sin(A1) * RadiusCm;

		const FVector B0 = Transform.TransformPosition(FVector(X0, Y0, -HalfHeightCm));
		const FVector B1 = Transform.TransformPosition(FVector(X1, Y1, -HalfHeightCm));
		const FVector T1 = Transform.TransformPosition(FVector(X1, Y1,  HalfHeightCm));
		const FVector T0 = Transform.TransformPosition(FVector(X0, Y0,  HalfHeightCm));

		AppendQuad(OutSection, B0, B1, T1, T0, Color);

		if (bCapEnds)
		{
			const FVector CenterTop = Transform.TransformPosition(FVector(0, 0,  HalfHeightCm));
			const FVector CenterBot = Transform.TransformPosition(FVector(0, 0, -HalfHeightCm));
			AppendQuad(OutSection, CenterTop, T0, T1, CenterTop, Color * 0.92f);
			AppendQuad(OutSection, CenterBot, B1, B0, CenterBot, Color * 0.85f);
		}
	}
}

void FIstanbulMeshUtils::AppendCone(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
	float BottomRadiusCm, float TopRadiusCm, float HalfHeightCm, int32 Sides, const FLinearColor& Color, bool bCapBottom)
{
	const int32 N = FMath::Clamp(Sides, 6, 36);
	for (int32 i = 0; i < N; ++i)
	{
		const float A0 = (2.0f * PI * i) / N;
		const float A1 = (2.0f * PI * (i + 1)) / N;
		const float C0 = FMath::Cos(A0), S0 = FMath::Sin(A0);
		const float C1 = FMath::Cos(A1), S1 = FMath::Sin(A1);

		const FVector B0 = Transform.TransformPosition(FVector(C0 * BottomRadiusCm, S0 * BottomRadiusCm, -HalfHeightCm));
		const FVector B1 = Transform.TransformPosition(FVector(C1 * BottomRadiusCm, S1 * BottomRadiusCm, -HalfHeightCm));
		const FVector T1 = Transform.TransformPosition(FVector(C1 * TopRadiusCm,    S1 * TopRadiusCm,     HalfHeightCm));
		const FVector T0 = Transform.TransformPosition(FVector(C0 * TopRadiusCm,    S0 * TopRadiusCm,     HalfHeightCm));

		AppendQuad(OutSection, B0, B1, T1, T0, Color);

		if (bCapBottom && BottomRadiusCm > 0.5f)
		{
			const FVector CenterBot = Transform.TransformPosition(FVector(0, 0, -HalfHeightCm));
			AppendQuad(OutSection, CenterBot, B1, B0, CenterBot, Color * 0.85f);
		}
		if (TopRadiusCm > 0.5f)
		{
			const FVector CenterTop = Transform.TransformPosition(FVector(0, 0, HalfHeightCm));
			AppendQuad(OutSection, CenterTop, T0, T1, CenterTop, Color * 1.04f);
		}
	}
}

void FIstanbulMeshUtils::AppendContouredSedanBody(
	FIstanbulMeshSectionData& OutPaintSection,
	FIstanbulMeshSectionData& OutGlassSection,
	FIstanbulMeshSectionData& OutTrimSection,
	FIstanbulMeshSectionData& OutLightsSection,
	const FLinearColor& PaintColor,
	bool bIsTaxi,
	bool bBrakeLightsBright)
{
	const FLinearColor DarkGlass(0.03f, 0.05f, 0.08f, 1.0f);
	const FLinearColor MatteTrim(0.04f, 0.04f, 0.05f, 1.0f);
	const FLinearColor ChromeCol(0.76f, 0.79f, 0.84f, 1.0f);
	const FLinearColor HeadlightGlow(1.0f, 0.96f, 0.84f, 1.0f);
	const FLinearColor TaillightGlow = bBrakeLightsBright
		? FLinearColor(1.0f, 0.08f, 0.05f, 1.0f)
		: FLinearColor(0.78f, 0.04f, 0.04f, 1.0f);
	const FLinearColor AmberSignal(1.0f, 0.58f, 0.02f, 1.0f);
	const FLinearColor PlateWhite(0.95f, 0.95f, 0.95f, 1.0f);
	const FLinearColor PlateBlue(0.04f, 0.22f, 0.78f, 1.0f);

	// 1. Sculpted Lower Chassis & Rocker Panels (Slot 0 Paint + Slot 2 Dark Undertray)
	AppendBeveledBox(OutTrimSection, FTransform(FVector(0.0f, 0.0f, 22.0f)),
		FVector(204.0f, 84.0f, 6.0f), 2.5f, MatteTrim);

	// Main Lower Beltline Body Core (between front and rear axles, with sculpted waist)
	AppendTaperedPrism(OutPaintSection, FTransform(FVector(0.0f, 0.0f, 52.0f)),
		FVector2D(206.0f, 89.0f), FVector2D(200.0f, 86.0f), FVector(0.0f, 0.0f, 0.0f),
		24.0f, PaintColor);

	// Sloped Aerodynamic Front Hood (Kaput: slopes down toward the front grille)
	AppendTaperedPrism(OutPaintSection, FTransform(FVector(142.0f, 0.0f, 75.0f)),
		FVector2D(62.0f, 84.0f), FVector2D(58.0f, 76.0f), FVector(-4.0f, 0.0f, -8.0f),
		7.0f, PaintColor * 1.03f);
	// Hood Power-Dome Crease Ribs
	AppendBeveledBox(OutPaintSection, FTransform(FRotator(-4.5f, 0.0f, 0.0f), FVector(138.0f, 0.0f, 79.0f)),
		FVector(52.0f, 38.0f, 2.5f), 1.5f, PaintColor * 1.05f);

	// Sloped Rear Trunk Decklid + Integrated Lip Spoiler
	AppendTaperedPrism(OutPaintSection, FTransform(FVector(-156.0f, 0.0f, 77.0f)),
		FVector2D(46.0f, 84.0f), FVector2D(42.0f, 78.0f), FVector(4.0f, 0.0f, -4.0f),
		6.5f, PaintColor * 1.02f);
	AppendBeveledBox(OutPaintSection, FTransform(FVector(-196.0f, 0.0f, 83.5f)),
		FVector(6.0f, 77.0f, 2.8f), 1.2f, PaintColor * 0.94f);

	// 2. Contoured Cabin Greenhouse (45-deg raked windshield, curved roof, sloped rear glass)
	// Central Cabin Core (Slot 1 Dark Glass inset slightly so pillars frame it)
	AppendTaperedPrism(OutGlassSection, FTransform(FVector(-16.0f, 0.0f, 102.0f)),
		FVector2D(104.0f, 82.0f), FVector2D(66.0f, 70.0f), FVector(-8.0f, 0.0f, 0.0f),
		25.0f, DarkGlass);

	// 45-Degree Raked Front Windshield Pane (Slot 1 Glass)
	AppendBeveledBox(OutGlassSection, FTransform(FRotator(38.0f, 0.0f, 0.0f), FVector(68.0f, 0.0f, 102.0f)),
		FVector(3.5f, 73.0f, 32.0f), 1.0f, DarkGlass);

	// Sloped Rear Fastback Windshield Pane (Slot 1 Glass)
	AppendBeveledBox(OutGlassSection, FTransform(FRotator(-36.0f, 0.0f, 0.0f), FVector(-108.0f, 0.0f, 101.0f)),
		FVector(3.5f, 72.0f, 31.0f), 1.0f, DarkGlass);

	// Contoured Roof Cap (Slot 0 Paint) sitting flush above the cabin glass
	AppendTaperedPrism(OutPaintSection, FTransform(FVector(-24.0f, 0.0f, 128.5f)),
		FVector2D(68.0f, 72.0f), FVector2D(58.0f, 65.0f), FVector(0.0f, 0.0f, 0.0f),
		3.8f, PaintColor * 1.05f);

	// Contoured A-Pillars, B-Pillars, C-Pillars & Roof Rails (Slot 0 Paint + Slot 2 Trim)
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float SY = Side * 1.0f;
		// A-Pillar (raked along front windshield)
		AppendBeveledBox(OutPaintSection, FTransform(FRotator(38.0f, 0.0f, -SY * 10.0f), FVector(67.0f, SY * 76.5f, 103.0f)),
			FVector(5.5f, 3.8f, 33.0f), 1.2f, PaintColor);
		// B-Pillar (center door divider in matte black trim)
		AppendBeveledBox(OutTrimSection, FTransform(FRotator(0.0f, 0.0f, -SY * 11.0f), FVector(-14.0f, SY * 78.5f, 102.0f)),
			FVector(6.5f, 2.5f, 24.0f), 0.8f, MatteTrim);
		// C-Pillar (sloped rear quarter pillar)
		AppendBeveledBox(OutPaintSection, FTransform(FRotator(-35.0f, 0.0f, -SY * 10.0f), FVector(-104.0f, SY * 76.0f, 102.0f)),
			FVector(8.5f, 4.2f, 31.0f), 1.5f, PaintColor);

		// Flared 3D Wheel Arches (Front & Rear Fender Flares + Dark Wheel Wells)
		for (float WheelX : { 130.0f, -130.0f })
		{
			AppendBeveledBox(OutPaintSection, FTransform(FVector(WheelX, SY * 89.5f, 48.0f)),
				FVector(42.0f, 4.5f, 18.0f), 3.0f, PaintColor * 0.98f);
			AppendBeveledBox(OutTrimSection, FTransform(FVector(WheelX, SY * 84.0f, 38.0f)),
				FVector(38.0f, 8.0f, 18.0f), 2.0f, MatteTrim);
		}

		// Sculpted Side Rocker Skirt (Marşpiyel)
		AppendBeveledBox(OutTrimSection, FTransform(FVector(0.0f, SY * 88.5f, 24.0f)),
			FVector(92.0f, 3.5f, 4.5f), 1.5f, MatteTrim);

		// Side-View Mirror on Stalk + Mirror Glass
		AppendBeveledBox(OutTrimSection, FTransform(FVector(52.0f, SY * 86.0f, 84.0f)),
			FVector(4.0f, 6.0f, 2.5f), 0.8f, MatteTrim);
		AppendBeveledBox(OutPaintSection, FTransform(FVector(48.0f, SY * 94.5f, 86.0f)),
			FVector(8.0f, 6.5f, 5.5f), 2.0f, PaintColor);
		AppendBeveledBox(OutGlassSection, FTransform(FVector(43.5f, SY * 94.5f, 86.0f)),
			FVector(1.0f, 5.2f, 4.2f), 0.2f, ChromeCol);

		// Front & Rear Door Handles
		AppendBeveledBox(OutTrimSection, FTransform(FVector(8.0f, SY * 89.2f, 70.0f)),
			FVector(9.0f, 1.8f, 2.2f), 0.6f, MatteTrim);
		AppendBeveledBox(OutTrimSection, FTransform(FVector(-62.0f, SY * 89.2f, 71.0f)),
			FVector(9.0f, 1.8f, 2.2f), 0.6f, MatteTrim);
	}

	// 3. Front & Rear Bumpers, Radiator Grille, Windshield Wipers (Slot 2 Trim)
	// Front Bumper + Lower Splitter
	AppendBeveledBox(OutTrimSection, FTransform(FVector(207.0f, 0.0f, 36.0f)),
		FVector(9.0f, 88.0f, 14.0f), 4.0f, MatteTrim);
	AppendBeveledBox(OutPaintSection, FTransform(FVector(205.0f, 0.0f, 44.0f)),
		FVector(8.0f, 89.0f, 8.0f), 3.0f, PaintColor * 0.96f);
	// Front Upper & Lower Radiator Grille + Chrome Crossbars
	AppendBeveledBox(OutTrimSection, FTransform(FVector(204.5f, 0.0f, 58.0f)),
		FVector(4.0f, 48.0f, 9.5f), 1.5f, MatteTrim);
	AppendBeveledBox(OutLightsSection, FTransform(FVector(207.5f, 0.0f, 60.0f)),
		FVector(1.5f, 45.0f, 1.4f), 0.4f, ChromeCol);
	AppendBeveledBox(OutLightsSection, FTransform(FVector(207.5f, 0.0f, 56.0f)),
		FVector(1.5f, 45.0f, 1.4f), 0.4f, ChromeCol);
	// Windshield Wipers
	AppendBeveledBox(OutTrimSection, FTransform(FRotator(0.0f, 14.0f, 0.0f), FVector(90.0f, -22.0f, 81.5f)),
		FVector(3.0f, 24.0f, 1.2f), 0.4f, MatteTrim);
	AppendBeveledBox(OutTrimSection, FTransform(FRotator(0.0f, 14.0f, 0.0f), FVector(90.0f,  24.0f, 81.5f)),
		FVector(3.0f, 24.0f, 1.2f), 0.4f, MatteTrim);

	// Rear Bumper + Diffuser + Dual Exhaust Tips
	AppendBeveledBox(OutTrimSection, FTransform(FVector(-207.0f, 0.0f, 37.0f)),
		FVector(9.0f, 88.0f, 14.0f), 4.0f, MatteTrim);
	AppendBeveledBox(OutPaintSection, FTransform(FVector(-205.0f, 0.0f, 46.0f)),
		FVector(8.0f, 89.0f, 8.0f), 3.0f, PaintColor * 0.96f);
	AppendCylinder(OutTrimSection, FTransform(FRotator(90.0f, 0.0f, 0.0f), FVector(-212.0f, -52.0f, 24.0f)),
		4.2f, 6.0f, 10, ChromeCol, true);
	AppendCylinder(OutTrimSection, FTransform(FRotator(90.0f, 0.0f, 0.0f), FVector(-212.0f,  52.0f, 24.0f)),
		4.2f, 6.0f, 10, ChromeCol, true);

	// 4. Emissive Lights, Turn Signals, License Plates & Taxi Roof Dome (Slot 3 Emissive)
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float SY = Side * 1.0f;
		// Wrap-around Projector Headlights (Warm White) + Amber Corner Blinkers
		AppendBeveledBox(OutLightsSection, FTransform(FRotator(0.0f, SY * 8.0f, 0.0f), FVector(203.0f, SY * 64.0f, 60.0f)),
			FVector(5.0f, 17.0f, 6.5f), 1.5f, HeadlightGlow);
		AppendBeveledBox(OutLightsSection, FTransform(FVector(199.0f, SY * 82.5f, 60.0f)),
			FVector(6.0f, 4.0f, 6.0f), 1.0f, AmberSignal);

		// Rear LED Taillight Clusters + Amber Rear Blinkers
		AppendBeveledBox(OutLightsSection, FTransform(FVector(-203.5f, SY * 64.0f, 64.0f)),
			FVector(5.0f, 19.0f, 7.0f), 1.5f, TaillightGlow);
		AppendBeveledBox(OutLightsSection, FTransform(FVector(-200.0f, SY * 83.0f, 64.0f)),
			FVector(5.5f, 3.8f, 6.5f), 1.0f, AmberSignal);
	}
	// Center High-Mount Stop Lamp (CHMSL) at top of rear glass
	AppendBeveledBox(OutLightsSection, FTransform(FVector(-88.0f, 0.0f, 121.0f)),
		FVector(2.5f, 22.0f, 1.8f), 0.4f, TaillightGlow);

	// Front & Rear Turkish License Plates (TR Blue Strip + White Plate)
	AppendBeveledBox(OutLightsSection, FTransform(FVector(216.2f, 0.0f, 38.0f)),
		FVector(1.2f, 26.0f, 5.8f), 0.3f, PlateWhite);
	AppendBeveledBox(OutLightsSection, FTransform(FVector(216.8f, -22.5f, 38.0f)),
		FVector(1.2f, 3.5f, 5.8f), 0.2f, PlateBlue);
	AppendBeveledBox(OutLightsSection, FTransform(FVector(-216.2f, 0.0f, 54.0f)),
		FVector(1.2f, 26.0f, 5.8f), 0.3f, PlateWhite);
	AppendBeveledBox(OutLightsSection, FTransform(FVector(-216.8f, -22.5f, 54.0f)),
		FVector(1.2f, 3.5f, 5.8f), 0.2f, PlateBlue);

	if (bIsTaxi)
	{
		// Iconic Istanbul TAKSI Roof Dome Sign (Black base + Glowing Yellow Aerodynamic Prism)
		const FLinearColor TaxiGlow(1.0f, 0.85f, 0.05f, 1.0f);
		AppendBeveledBox(OutTrimSection, FTransform(FVector(-12.0f, 0.0f, 133.5f)),
			FVector(15.0f, 34.0f, 2.2f), 0.8f, MatteTrim);
		AppendTaperedPrism(OutLightsSection, FTransform(FVector(-12.0f, 0.0f, 141.5f)),
			FVector2D(13.0f, 31.0f), FVector2D(8.5f, 25.0f), FVector(0.0f, 0.0f, 0.0f),
			6.5f, TaxiGlow);

		// Classic Checkered Black/Yellow Taxi Door Decal Strips along both sides
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			for (int32 Checker = -5; Checker <= 5; ++Checker)
			{
				const FLinearColor CheckerCol = (FMath::Abs(Checker) % 2 == 0) ? MatteTrim : TaxiGlow;
				AppendBeveledBox(OutLightsSection,
					FTransform(FVector(Checker * 18.0f, Side * 88.8f, 58.0f)),
					FVector(8.5f, 1.0f, 2.8f), 0.2f, CheckerCol);
			}
		}
	}
}

void FIstanbulMeshUtils::AppendDetailedWheelMesh(
	FIstanbulMeshSectionData& OutTireSection,
	FIstanbulMeshSectionData& OutRimSection,
	float RadiusCm,
	float HalfWidthCm,
	int32 Sides)
{
	const FLinearColor RubberCol(0.04f, 0.04f, 0.05f, 1.0f);
	const FLinearColor SidewallCol(0.07f, 0.07f, 0.08f, 1.0f);
	const FLinearColor RimSilver(0.76f, 0.79f, 0.84f, 1.0f);
	const FLinearColor RimDark(0.14f, 0.15f, 0.18f, 1.0f);

	// Wheel rotation axis is Y, so rotate Z-cylinder by Roll = 90 deg
	const FTransform WheelAxisXf(FRotator(0.0f, 0.0f, 90.0f), FVector::ZeroVector);

	// Outer rubber tire tread + beveled shoulder rings
	AppendCylinder(OutTireSection, WheelAxisXf, RadiusCm, HalfWidthCm * 0.86f, Sides, RubberCol, true);
	AppendCylinder(OutTireSection, WheelAxisXf, RadiusCm * 0.94f, HalfWidthCm, Sides, SidewallCol, true);

	// Inner dark brake disc / wheel barrel
	const float RimRadius = RadiusCm * 0.64f;
	AppendCylinder(OutRimSection, WheelAxisXf, RimRadius, HalfWidthCm * 1.03f, Sides, RimDark, true);

	// Outer alloy rim lip ring + central hubcap boss
	AppendCylinder(OutRimSection, WheelAxisXf, RimRadius * 0.92f, HalfWidthCm * 1.06f, Sides, RimSilver * 0.88f, true);
	AppendCylinder(OutRimSection, WheelAxisXf, RimRadius * 0.28f, HalfWidthCm * 1.12f, 10, RimSilver, true);

	// 5 Sculpted Alloy Spokes (extending radially in the X-Z plane so wheel rotation in Pitch is unmistakably visible!)
	const int32 NumSpokes = 5;
	for (int32 s = 0; s < NumSpokes; ++s)
	{
		const float AngleDeg = s * (360.0f / (float)NumSpokes);
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector SpokeCenter(
			FMath::Cos(Rad) * (RimRadius * 0.52f),
			0.0f,
			FMath::Sin(Rad) * (RimRadius * 0.52f));
		const FRotator SpokeRot(AngleDeg, 0.0f, 0.0f);

		AppendBeveledBox(OutRimSection, FTransform(SpokeRot, SpokeCenter),
			FVector(RimRadius * 0.44f, HalfWidthCm * 1.10f, 3.2f), 0.8f, RimSilver);
	}
}

void FIstanbulMeshUtils::AppendRecessedWindow(
	FIstanbulMeshSectionData& OutWallAndTrimSection,
	FIstanbulMeshSectionData& OutGlassSection,
	const FTransform& WindowCenterTransform,
	float WidthCm,
	float HeightCm,
	float RecessDepthCm,
	const FLinearColor& FrameColor,
	const FLinearColor& SillColor,
	bool bAddShutters,
	const FLinearColor& ShutterColor)
{
	const float HalfW = WidthCm * 0.5f;
	const float HalfH = HeightCm * 0.5f;
	const float Depth = FMath::Max(RecessDepthCm, 8.0f);
	const FLinearColor GlassTone(0.05f, 0.09f, 0.15f, 1.0f);

	// 1. Recessed Dark Reflective Glass Pane (inset by RecessDepthCm along local -Y, outward normal is +Y)
	AppendBeveledBox(OutGlassSection,
		FTransform(FVector(0.0f, -Depth * 0.35f, 0.0f)) * WindowCenterTransform,
		FVector(HalfW * 0.88f, 2.5f, HalfH * 0.88f), 0.5f, GlassTone);

	// 2. 4 Recessed Window Reveal Jambs (Top, Bottom, Left, Right frame depth boxes of 15cm)
	const float FrameThick = 6.5f;
	// Left & Right Vertical Reveal Frames
	AppendBeveledBox(OutWallAndTrimSection,
		FTransform(FVector(-HalfW, 0.0f, 0.0f)) * WindowCenterTransform,
		FVector(FrameThick, Depth * 0.5f, HalfH + FrameThick), 1.2f, FrameColor);
	AppendBeveledBox(OutWallAndTrimSection,
		FTransform(FVector( HalfW, 0.0f, 0.0f)) * WindowCenterTransform,
		FVector(FrameThick, Depth * 0.5f, HalfH + FrameThick), 1.2f, FrameColor);
	// Top Lintel & Bottom Projecting Stone Sill
	AppendBeveledBox(OutWallAndTrimSection,
		FTransform(FVector(0.0f, 3.0f, HalfH + FrameThick)) * WindowCenterTransform,
		FVector(HalfW + FrameThick * 1.8f, Depth * 0.65f, FrameThick * 1.2f), 1.5f, SillColor);
	AppendBeveledBox(OutWallAndTrimSection,
		FTransform(FVector(0.0f, 5.0f, -HalfH - FrameThick * 0.9f)) * WindowCenterTransform,
		FVector(HalfW + FrameThick * 2.0f, Depth * 0.78f, FrameThick * 1.1f), 1.5f, SillColor * 0.95f);

	// 3. Classic Sash Muntin Crossbars (Horizontal + Vertical)
	AppendBeveledBox(OutWallAndTrimSection,
		FTransform(FVector(0.0f, -Depth * 0.15f, 4.0f)) * WindowCenterTransform,
		FVector(HalfW * 0.88f, 2.2f, 2.4f), 0.5f, FrameColor);
	AppendBeveledBox(OutWallAndTrimSection,
		FTransform(FVector(0.0f, -Depth * 0.15f, 0.0f)) * WindowCenterTransform,
		FVector(2.2f, 2.2f, HalfH * 0.88f), 0.5f, FrameColor);

	// 4. Optional Exterior Louvered Wooden Shutters (Kepenk) flanking left & right
	if (bAddShutters)
	{
		const float ShutterW = HalfW * 0.46f;
		AppendBeveledBox(OutWallAndTrimSection,
			FTransform(FRotator(0.0f, -12.0f, 0.0f), FVector(-(HalfW + ShutterW + 5.0f), 4.5f, 0.0f)) * WindowCenterTransform,
			FVector(ShutterW, 2.5f, HalfH * 0.96f), 1.0f, ShutterColor);
		AppendBeveledBox(OutWallAndTrimSection,
			FTransform(FRotator(0.0f,  12.0f, 0.0f), FVector( (HalfW + ShutterW + 5.0f), 4.5f, 0.0f)) * WindowCenterTransform,
			FVector(ShutterW, 2.5f, HalfH * 0.96f), 1.0f, ShutterColor);
	}
}

void FIstanbulMeshUtils::AppendCumbaWithCorbels(
	FIstanbulMeshSectionData& OutWallSection,
	FIstanbulMeshSectionData& OutGlassSection,
	FIstanbulMeshSectionData& OutRoofSection,
	const FTransform& CumbaCenterTransform,
	float WidthCm,
	float HeightCm,
	float ProjectionDepthCm,
	const FLinearColor& CumbaBodyColor,
	const FLinearColor& TrimColor)
{
	const float HalfW = WidthCm * 0.5f;
	const float HalfH = HeightCm * 0.5f;
	const float HalfD = ProjectionDepthCm * 0.5f;
	const FLinearColor TerracottaCol(0.68f, 0.25f, 0.15f, 1.0f);

	// 1. Overhanging Cumba Main Box
	AppendBeveledBox(OutWallSection,
		FTransform(FVector(0.0f, HalfD, 0.0f)) * CumbaCenterTransform,
		FVector(HalfW, HalfD, HalfH), 4.0f, CumbaBodyColor);

	// 2. Two Angled 3D Wooden/Stone Corbel Brackets (Eli Böğründe / Payanda) underneath the Cumba
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float CorbelX = Side * (HalfW * 0.68f);
		AppendBeveledBox(OutWallSection,
			FTransform(FRotator(0.0f, 0.0f, 38.0f), FVector(CorbelX, HalfD * 0.72f, -HalfH - 28.0f)) * CumbaCenterTransform,
			FVector(8.5f, HalfD * 0.75f, 10.0f), 2.0f, TrimColor * 0.82f);
		// Carved corbel base block against wall
		AppendBeveledBox(OutWallSection,
			FTransform(FVector(CorbelX, 8.0f, -HalfH - 46.0f)) * CumbaCenterTransform,
			FVector(10.0f, 8.0f, 14.0f), 2.0f, TrimColor * 0.78f);
	}

	// 3. Recessed Front Window on Cumba Face
	AppendRecessedWindow(
		OutWallSection, OutGlassSection,
		FTransform(FVector(0.0f, ProjectionDepthCm, 6.0f)) * CumbaCenterTransform,
		WidthCm * 0.68f, HeightCm * 0.56f, 14.0f, TrimColor, TrimColor * 0.92f, false);

	// 4. Pitched Terracotta Roof Cap over the Cumba
	AppendTaperedPrism(OutRoofSection,
		FTransform(FVector(0.0f, HalfD + 4.0f, HalfH + 14.0f)) * CumbaCenterTransform,
		FVector2D(HalfW + 12.0f, HalfD + 10.0f),
		FVector2D(HalfW * 0.55f, 6.0f),
		FVector(0.0f, -HalfD * 0.4f, 0.0f),
		14.0f, TerracottaCol);
}

void FIstanbulMeshUtils::AppendStripedShopAwning(
	FIstanbulMeshSectionData& OutAwningSection,
	const FTransform& AwningTopWallTransform,
	float WidthCm,
	float ProjectionCm,
	float DropHeightCm,
	const FLinearColor& PrimaryStripeColor,
	const FLinearColor& SecondaryStripeColor)
{
	const int32 NumStripes = FMath::Clamp(FMath::RoundToInt(WidthCm / 42.0f), 5, 15);
	const float StripeW = WidthCm / (float)NumStripes;
	const float SlopeAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DropHeightCm, ProjectionCm));
	const float CanopyLen = FMath::Sqrt(ProjectionCm * ProjectionCm + DropHeightCm * DropHeightCm);

	for (int32 i = 0; i < NumStripes; ++i)
	{
		const float LocalX = -WidthCm * 0.5f + (i + 0.5f) * StripeW;
		const FLinearColor Col = (i % 2 == 0) ? PrimaryStripeColor : SecondaryStripeColor;

		// Sloped canopy stripe panel (protruding along +Y and dropping along -Z)
		const FTransform StripeXf = FTransform(
			FRotator(0.0f, 0.0f, -SlopeAngleDeg),
			FVector(LocalX, ProjectionCm * 0.5f, -DropHeightCm * 0.5f)) * AwningTopWallTransform;

		AppendBeveledBox(OutAwningSection, StripeXf,
			FVector(StripeW * 0.51f, CanopyLen * 0.5f, 2.5f), 0.6f, Col);

		// Front scalloped vertical valance flap
		const FTransform ValanceXf = FTransform(
			FRotator::ZeroRotator,
			FVector(LocalX, ProjectionCm, -DropHeightCm - 9.0f)) * AwningTopWallTransform;
		AppendBeveledBox(OutAwningSection, ValanceXf,
			FVector(StripeW * 0.49f, 2.2f, 9.0f), 0.8f, Col * 0.94f);
	}

	// Two dark metal side support arms under the awning
	const FLinearColor IronCol(0.10f, 0.10f, 0.12f, 1.0f);
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const FTransform ArmXf = FTransform(
			FRotator(0.0f, 0.0f, -SlopeAngleDeg * 0.65f),
			FVector(Side * (WidthCm * 0.46f), ProjectionCm * 0.48f, -DropHeightCm * 0.72f)) * AwningTopWallTransform;
		AppendBeveledBox(OutAwningSection, ArmXf,
			FVector(2.2f, ProjectionCm * 0.52f, 2.2f), 0.4f, IronCol);
	}
}

void FIstanbulMeshUtils::AppendSatelliteDish(
	FIstanbulMeshSectionData& OutSection,
	const FTransform& BaseTransform,
	float DishRadiusCm)
{
	const FLinearColor DishGrey(0.78f, 0.80f, 0.83f, 1.0f);
	const FLinearColor BracketDark(0.18f, 0.19f, 0.22f, 1.0f);

	// Vertical mounting mast
	AppendCylinder(OutSection, FTransform(FVector(0.0f, 0.0f, 28.0f)) * BaseTransform,
		3.2f, 28.0f, 8, BracketDark, true);

	// Tilted parabolic satellite dish reflector
	const FTransform DishXf = FTransform(FRotator(38.0f, 0.0f, 0.0f), FVector(6.0f, 0.0f, 52.0f)) * BaseTransform;
	AppendCone(OutSection, DishXf, DishRadiusCm * 0.25f, DishRadiusCm, 6.0f, 14, DishGrey, true);

	// LNB Feed Horn Arm extending out from the dish center
	AppendBeveledBox(OutSection, FTransform(FVector(0.0f, 0.0f, 18.0f)) * DishXf,
		FVector(2.0f, 2.0f, 16.0f), 0.4f, BracketDark);
	AppendBeveledBox(OutSection, FTransform(FVector(0.0f, 0.0f, 34.0f)) * DishXf,
		FVector(4.5f, 4.5f, 5.0f), 1.0f, DishGrey * 0.9f);
}

void FIstanbulMeshUtils::AppendRoadRibbonWithSidewalks(FIstanbulMeshSectionData& OutRoadSection, FIstanbulMeshSectionData& OutSidewalkSection,
	const FVector& Start, const FVector& End, float RoadWidthCm, float SidewalkWidthCm, float SidewalkHeightCm,
	const FLinearColor& RoadColor, const FLinearColor& SidewalkColor, int32 Subdivisions,
	FIstanbulMeshSectionData* OptionalMarkingsSection, bool bTwoWayYellowCenterLine)
{
	const int32 Segs = FMath::Clamp(Subdivisions, 1, 64);
	const FVector Delta = End - Start;
	const FVector Dir2D = FVector(Delta.X, Delta.Y, 0.0f).GetSafeNormal();
	const FVector Right(-Dir2D.Y, Dir2D.X, 0.0f);
	const float HalfW = RoadWidthCm * 0.5f;

	for (int32 i = 0; i < Segs; ++i)
	{
		const float T0 = (float)i / (float)Segs;
		const float T1 = (float)(i + 1) / (float)Segs;
		// Smooth hermite easing at top/bottom of steep grades so Chaos wheels never hit a sharp crease
		const float Z0 = FMath::Lerp(Start.Z, End.Z, FMath::SmoothStep(0.0f, 1.0f, T0));
		const float Z1 = FMath::Lerp(Start.Z, End.Z, FMath::SmoothStep(0.0f, 1.0f, T1));

		const FVector P0(FMath::Lerp(Start.X, End.X, T0), FMath::Lerp(Start.Y, End.Y, T0), Z0);
		const FVector P1(FMath::Lerp(Start.X, End.X, T1), FMath::Lerp(Start.Y, End.Y, T1), Z1);

		// Multi-strip cross-section for subtle crown and authentic cobblestone/asphalt tone variation
		const float Shade = 0.95f + 0.05f * ((i % 2 == 0) ? 1.0f : -1.0f);
		AppendQuad(OutRoadSection,
			P0 - Right * HalfW, P0 + Right * HalfW,
			P1 + Right * HalfW, P1 - Right * HalfW,
			RoadColor * Shade);

		// Painted Lane Markings (Center Line + White Shoulder Lines)
		if (OptionalMarkingsSection)
		{
			const FVector PaintLift(0.0f, 0.0f, 1.2f);
			const FLinearColor EdgeWhite(0.88f, 0.89f, 0.90f, 1.0f);
			const FLinearColor CenterCol = bTwoWayYellowCenterLine
				? FLinearColor(0.95f, 0.74f, 0.08f, 1.0f)
				: EdgeWhite;

			// Dashed center line on even segments (or double line on wide boulevards)
			if (i % 2 == 0)
			{
				AppendQuad(*OptionalMarkingsSection,
					P0 - Right * 8.0f + PaintLift, P0 + Right * 8.0f + PaintLift,
					P1 + Right * 8.0f + PaintLift, P1 - Right * 8.0f + PaintLift,
					CenterCol);
			}

			// Continuous white edge lines on wide roads (>= 7.5m)
			if (RoadWidthCm >= 750.0f)
			{
				const float EdgeOffset = HalfW - 32.0f;
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					const FVector E0 = P0 + Right * (Side * EdgeOffset) + PaintLift;
					const FVector E1 = P1 + Right * (Side * EdgeOffset) + PaintLift;
					AppendQuad(*OptionalMarkingsSection,
						E0 - Right * 6.0f, E0 + Right * 6.0f,
						E1 + Right * 6.0f, E1 - Right * 6.0f,
						EdgeWhite * 0.92f);
				}
			}
		}

		if (SidewalkHeightCm > 1.0f && SidewalkWidthCm > 10.0f)
		{
			const FVector Up(0.0f, 0.0f, SidewalkHeightCm);
			// Alternating red/white or grey/granite curb stone blocks along the sidewalk edge
			const FLinearColor CurbTone = (i % 2 == 0) ? SidewalkColor * 1.05f : SidewalkColor * 0.78f;

			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const FVector RInner0 = P0 + Right * (Side * HalfW);
				const FVector RInner1 = P1 + Right * (Side * HalfW);
				const FVector ROuter0 = P0 + Right * (Side * (HalfW + SidewalkWidthCm));
				const FVector ROuter1 = P1 + Right * (Side * (HalfW + SidewalkWidthCm));

				if (Side > 0)
				{
					AppendQuad(OutSidewalkSection, RInner0, RInner0 + Up, RInner1 + Up, RInner1, CurbTone);
					AppendQuad(OutSidewalkSection, RInner0 + Up, ROuter0 + Up, ROuter1 + Up, RInner1 + Up, SidewalkColor * Shade);
				}
				else
				{
					AppendQuad(OutSidewalkSection, RInner1, RInner1 + Up, RInner0 + Up, RInner0, CurbTone);
					AppendQuad(OutSidewalkSection, ROuter0 + Up, RInner0 + Up, RInner1 + Up, ROuter1 + Up, SidewalkColor * Shade);
				}
			}
		}
	}
}

void FIstanbulMeshUtils::AppendJunctionPatch(FIstanbulMeshSectionData& OutRoadSection, FIstanbulMeshSectionData& OutSidewalkSection,
	const FVector& NodeCenter, float RadiusCm, float CurbHeightCm,
	const FLinearColor& RoadColor, const FLinearColor& SidewalkColor, bool bAddCrosswalkStrips)
{
	const float R = FMath::Max(RadiusCm, 250.0f);
	// Slightly raised (+0.8cm) octagonal junction pad so overlapping ribbon seams are 100% flush and invisible
	AppendCylinder(OutRoadSection, FTransform(FRotator::ZeroRotator, NodeCenter + FVector(0.0f, 0.0f, 0.4f)),
		R, 0.4f, 16, RoadColor * 1.03f, true);

	// 4 rounded curb corner returns (3-6m radius, 4.5m default) at the 4 quadrant corners
	const float CornerDist = R + 210.0f;
	const float CornerRadius = 420.0f; // 4.2m rounded curb return radius (within 3-6m spec)
	for (int32 qx = -1; qx <= 1; qx += 2)
	{
		for (int32 qy = -1; qy <= 1; qy += 2)
		{
			const FVector CornerCenter = NodeCenter + FVector(qx * CornerDist, qy * CornerDist, CurbHeightCm * 0.5f);
			AppendCylinder(OutSidewalkSection, FTransform(FRotator::ZeroRotator, CornerCenter),
				CornerRadius, CurbHeightCm * 0.5f, 12, SidewalkColor * 0.96f, true);
		}
	}

	if (bAddCrosswalkStrips)
	{
		const FLinearColor ZebraWhite(0.92f, 0.92f, 0.90f, 1.0f);
		const int32 NumStripes = 5;
		const float StripeSpacing = 72.0f;
		const float CrossDist = R * 0.78f;

		for (int32 s = -NumStripes / 2; s <= NumStripes / 2; ++s)
		{
			const float Offset = s * StripeSpacing;
			// North & South zebra crosswalk bars
			AppendBeveledBox(OutRoadSection,
				FTransform(FRotator::ZeroRotator, NodeCenter + FVector(CrossDist, Offset, 1.2f)),
				FVector(95.0f, 18.0f, 0.6f), 0.2f, ZebraWhite);
			AppendBeveledBox(OutRoadSection,
				FTransform(FRotator::ZeroRotator, NodeCenter + FVector(-CrossDist, Offset, 1.2f)),
				FVector(95.0f, 18.0f, 0.6f), 0.2f, ZebraWhite);
		}
	}
}

void FIstanbulMeshUtils::AppendCatenaryCable(FIstanbulMeshSectionData& OutSection, const FVector& Start, const FVector& End,
	float SagCm, float ThicknessCm, int32 Segments, const FLinearColor& Color)
{
	const int32 Segs = FMath::Clamp(Segments, 4, 24);
	const FVector Up(0.0f, 0.0f, ThicknessCm);
	for (int32 i = 0; i < Segs; ++i)
	{
		const float T0 = (float)i / (float)Segs;
		const float T1 = (float)(i + 1) / (float)Segs;
		// Parabolic sag: 4 * t * (1 - t) peaks at 1.0 at midpoint t = 0.5
		const float Sag0 = 4.0f * T0 * (1.0f - T0) * SagCm;
		const float Sag1 = 4.0f * T1 * (1.0f - T1) * SagCm;

		const FVector P0 = FMath::Lerp(Start, End, T0) - FVector(0.0f, 0.0f, Sag0);
		const FVector P1 = FMath::Lerp(Start, End, T1) - FVector(0.0f, 0.0f, Sag1);

		AppendQuad(OutSection, P0, P1, P1 + Up, P0 + Up, Color);
		AppendQuad(OutSection, P1, P0, P0 + Up, P1 + Up, Color);
	}
}

void FIstanbulMeshUtils::AppendExtrudedProfileXZ(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
	const TArray<FVector2D>& ProfilePointsXZ, float ThicknessYCm, const FLinearColor& Color)
{
	if (ProfilePointsXZ.Num() < 2) return;
	const float HalfY = ThicknessYCm * 0.5f;

	for (int32 i = 0; i < ProfilePointsXZ.Num() - 1; ++i)
	{
		const FVector2D& A = ProfilePointsXZ[i];
		const FVector2D& B = ProfilePointsXZ[i + 1];

		// Extrude from Ground Z = 0 up to the profile ridge (A.Y, B.Y) on both front (-HalfY) and back (+HalfY) faces
		const FVector F0 = Transform.TransformPosition(FVector(A.X, -HalfY, 0.0f));
		const FVector F1 = Transform.TransformPosition(FVector(B.X, -HalfY, 0.0f));
		const FVector F2 = Transform.TransformPosition(FVector(B.X, -HalfY, B.Y));
		const FVector F3 = Transform.TransformPosition(FVector(A.X, -HalfY, A.Y));

		const FVector B0 = Transform.TransformPosition(FVector(B.X,  HalfY, 0.0f));
		const FVector B1 = Transform.TransformPosition(FVector(A.X,  HalfY, 0.0f));
		const FVector B2 = Transform.TransformPosition(FVector(A.X,  HalfY, A.Y));
		const FVector B3 = Transform.TransformPosition(FVector(B.X,  HalfY, B.Y));

		AppendQuad(OutSection, F0, F1, F2, F3, Color);
		AppendQuad(OutSection, B0, B1, B2, B3, Color);
		// Top rim cap
		AppendQuad(OutSection, F3, F2, B3, B2, Color * 1.08f);
	}
}

void FIstanbulMeshUtils::AppendStripedBollard(FIstanbulMeshSectionData& OutSection, const FVector& BaseLocation,
	float RadiusCm, float HeightCm, int32 Sides)
{
	// 4 alternating vertical bands: Yellow -> Black -> Yellow -> Black
	const FLinearColor BandYellow = FLinearColor::FromSRGBColor(FColor(0xF5, 0xC4, 0x00));
	const FLinearColor BandBlack(0.05f, 0.05f, 0.06f, 1.0f);
	const int32 NumBands = 4;
	const float BandH = HeightCm / (float)NumBands;

	for (int32 b = 0; b < NumBands; ++b)
	{
		const float ZCenter = (b + 0.5f) * BandH;
		const FLinearColor Col = (b % 2 == 0) ? BandYellow : BandBlack;
		AppendCylinder(
			OutSection,
			FTransform(FRotator::ZeroRotator, BaseLocation + FVector(0.0f, 0.0f, ZCenter)),
			RadiusCm, BandH * 0.5f, Sides, Col, (b == NumBands - 1));
	}
}

void FIstanbulMeshUtils::AppendStoneWallWithVariation(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
	float LengthCm, float HeightCm, float ThicknessCm, FRandomStream& Rng)
{
	const int32 CoursesZ = FMath::Clamp(FMath::RoundToInt(HeightCm / 65.0f), 2, 8);
	const int32 BlocksX  = FMath::Clamp(FMath::RoundToInt(LengthCm / 220.0f), 4, 28);
	const float CourseH  = HeightCm / (float)CoursesZ;
	const float BlockW   = LengthCm / (float)BlocksX;
	const float HalfY    = ThicknessCm * 0.5f;

	for (int32 iz = 0; iz < CoursesZ; ++iz)
	{
		const bool bBrickCourse = (iz == 1 || iz == CoursesZ - 1);
		for (int32 ix = 0; ix < BlocksX; ++ix)
		{
			const float X0 = -LengthCm * 0.5f + ix * BlockW;
			const float Z0 = -HeightCm * 0.5f + iz * CourseH;
			const float Noise = Rng.FRandRange(0.84f, 1.14f);
			const FLinearColor BaseTone = bBrickCourse
				? FLinearColor(0.56f, 0.25f, 0.19f) * Noise  // Khorasan brick course
				: FLinearColor(0.54f, 0.50f, 0.44f) * Noise; // Warm Istanbul limestone block

			const FTransform BlockXf = FTransform(FRotator::ZeroRotator, FVector(X0 + BlockW * 0.5f, 0.0f, Z0 + CourseH * 0.5f)) * Transform;
			AppendBeveledBox(OutSection, BlockXf, FVector(BlockW * 0.49f, HalfY, CourseH * 0.48f), 2.5f, BaseTone);
		}
	}
}

void FIstanbulMeshUtils::AppendBackstreetLaundryOrLantern(FIstanbulMeshSectionData& OutSection, const FVector& Start,
	const FVector& End, float SagCm, bool bHangingLantern, FRandomStream& Rng)
{
	AppendCatenaryCable(OutSection, Start, End, SagCm, 2.2f, 8, FLinearColor(0.04f, 0.04f, 0.05f));

	if (bHangingLantern)
	{
		const FVector Mid = (Start + End) * 0.5f - FVector(0.0f, 0.0f, SagCm + 18.0f);
		AppendBeveledBox(OutSection, FTransform(FRotator::ZeroRotator, Mid),
			FVector(14.0f, 14.0f, 20.0f), 4.0f, FLinearColor(1.0f, 0.82f, 0.35f));
	}
	else
	{
		static const FLinearColor ClothPalette[5] = {
			FLinearColor(0.88f, 0.22f, 0.18f),
			FLinearColor(0.92f, 0.92f, 0.90f),
			FLinearColor(0.18f, 0.46f, 0.82f),
			FLinearColor(0.94f, 0.72f, 0.14f),
			FLinearColor(0.24f, 0.68f, 0.42f)
		};
		for (int32 c = 1; c <= 5; ++c)
		{
			const float T = (float)c / 6.0f;
			const float Sag = 4.0f * T * (1.0f - T) * SagCm;
			const FVector Pos = FMath::Lerp(Start, End, T) - FVector(0.0f, 0.0f, Sag + 32.0f);
			const FLinearColor Col = ClothPalette[Rng.RandRange(0, 4)];
			AppendBeveledBox(OutSection, FTransform(FRotator(0.0f, 90.0f, 0.0f), Pos),
				FVector(24.0f, 2.5f, 30.0f), 1.0f, Col);
		}
	}
}

void FIstanbulMeshUtils::AppendCafeTableAndCatCluster(FIstanbulMeshSectionData& OutSection, const FVector& BaseLocation,
	float YawDeg, FRandomStream& Rng)
{
	const FTransform BaseXf(FRotator(0.0f, YawDeg, 0.0f), BaseLocation);
	const FLinearColor WoodCol(0.46f, 0.28f, 0.14f);
	const FLinearColor ClothCol(0.85f, 0.16f, 0.14f);

	// Small round tea-garden table (Çay masası) + 2 low stools (Tabure)
	AppendCylinder(OutSection, FTransform(FVector(0, 0, 36.0f)) * BaseXf, 6.0f, 36.0f, 8, WoodCol, false);
	AppendCylinder(OutSection, FTransform(FVector(0, 0, 72.0f)) * BaseXf, 42.0f, 3.0f, 10, ClothCol, true);
	AppendBeveledBox(OutSection, FTransform(FVector(-58.0f, 0.0f, 22.0f)) * BaseXf, FVector(16.0f, 16.0f, 22.0f), 2.0f, WoodCol);
	AppendBeveledBox(OutSection, FTransform(FVector( 58.0f, 0.0f, 22.0f)) * BaseXf, FVector(16.0f, 16.0f, 22.0f), 2.0f, WoodCol);

	// Iconic Istanbul Street Cat silhouette (Sokak Kedisi: body + head + upright tail)
	const FLinearColor CatCol = (Rng.FRand() < 0.5f)
		? FLinearColor(0.85f, 0.48f, 0.14f) // Sarman (Orange tabby)
		: FLinearColor(0.18f, 0.18f, 0.20f); // Tuxedo / Smoke cat
	const FTransform CatXf = FTransform(FRotator(0.0f, 35.0f, 0.0f), FVector(28.0f, 48.0f, 0.0f)) * BaseXf;
	AppendBeveledBox(OutSection, FTransform(FVector(0.0f, 0.0f, 12.0f)) * CatXf,  FVector(16.0f, 7.0f, 9.0f), 2.5f, CatCol);
	AppendBeveledBox(OutSection, FTransform(FVector(15.0f, 0.0f, 21.0f)) * CatXf, FVector(7.0f,  6.5f, 6.5f), 2.0f, CatCol);
	AppendBeveledBox(OutSection, FTransform(FVector(-15.0f, 0.0f, 22.0f)) * CatXf, FVector(2.5f, 2.0f, 11.0f), 0.5f, CatCol);
}
