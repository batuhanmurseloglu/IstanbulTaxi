#include "Core/IstanbulMeshUtils.h"
#include "IstanbulTaxi.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

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
		FMath::Max(HalfExtentsCm.X, 1.0f),
		FMath::Max(HalfExtentsCm.Y, 1.0f),
		FMath::Max(HalfExtentsCm.Z, 1.0f));
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

void FIstanbulMeshUtils::AppendRoadRibbonWithSidewalks(FIstanbulMeshSectionData& OutRoadSection, FIstanbulMeshSectionData& OutSidewalkSection,
	const FVector& Start, const FVector& End, float RoadWidthCm, float SidewalkWidthCm, float SidewalkHeightCm,
	const FLinearColor& RoadColor, const FLinearColor& SidewalkColor, int32 Subdivisions)
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

		// Slight subtle vertex color variation per quad for natural stone/asphalt texture
		const float Shade = 0.96f + 0.04f * ((i % 2 == 0) ? 1.0f : -1.0f);
		AppendQuad(OutRoadSection,
			P0 - Right * HalfW, P0 + Right * HalfW,
			P1 + Right * HalfW, P1 - Right * HalfW,
			RoadColor * Shade);

		if (SidewalkHeightCm > 1.0f && SidewalkWidthCm > 10.0f)
		{
			const FVector Up(0.0f, 0.0f, SidewalkHeightCm);
			// Left & Right Sidewalk Top + Curb Vertical Riser
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const FVector RInner0 = P0 + Right * (Side * HalfW);
				const FVector RInner1 = P1 + Right * (Side * HalfW);
				const FVector ROuter0 = P0 + Right * (Side * (HalfW + SidewalkWidthCm));
				const FVector ROuter1 = P1 + Right * (Side * (HalfW + SidewalkWidthCm));

				if (Side > 0)
				{
					AppendQuad(OutSidewalkSection, RInner0, RInner0 + Up, RInner1 + Up, RInner1, SidewalkColor * 0.82f);
					AppendQuad(OutSidewalkSection, RInner0 + Up, ROuter0 + Up, ROuter1 + Up, RInner1 + Up, SidewalkColor);
				}
				else
				{
					AppendQuad(OutSidewalkSection, RInner1, RInner1 + Up, RInner0 + Up, RInner0, SidewalkColor * 0.82f);
					AppendQuad(OutSidewalkSection, ROuter0 + Up, RInner0 + Up, RInner1 + Up, ROuter1 + Up, SidewalkColor);
				}
			}
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
