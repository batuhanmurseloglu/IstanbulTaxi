#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/**
 * Accumulator buffer for a single UProceduralMeshComponent section.
 * Allows merging multiple beveled boxes, cylinders, road ribbons, and extruded polygons
 * into a single draw call with vertex colors and smooth normals.
 */
struct ISTANBULTAXI_API FIstanbulMeshSectionData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	void Reset()
	{
		Vertices.Reset();
		Triangles.Reset();
		Normals.Reset();
		UVs.Reset();
		VertexColors.Reset();
		Tangents.Reset();
	}

	void CommitToComponent(UProceduralMeshComponent* PMC, int32 SectionIndex, bool bCreateCollision, UMaterialInterface* Material = nullptr) const;
};

/**
 * Reusable procedural geometry & safe Engine asset loading helpers for IstanbulTaxi.
 */
class ISTANBULTAXI_API FIstanbulMeshUtils
{
public:
	/** Safe LoadObject wrapper for UStaticMesh with UE_LOG warning and automatic Cube fallback. */
	static UStaticMesh* LoadBasicShapeSafe(const TCHAR* PreferredPath);

	/** Safe material loader that prefers a vertex-color capable engine material and falls back to BasicShapeMaterial. */
	static UMaterialInterface* LoadVertexColorMaterialSafe();

	/** Appends a 12-edge beveled 3D box (mid-poly look, rounded edges) to OutSection. */
	static void AppendBeveledBox(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		const FVector& HalfExtentsCm, float BevelCm, const FLinearColor& Color);

	/** Appends a closed cylinder (axis along local Z, or rotated via Transform) to OutSection. */
	static void AppendCylinder(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		float RadiusCm, float HalfHeightCm, int32 Sides, const FLinearColor& Color, bool bCapEnds = true);

	/** Appends a smooth, continuous road ribbon + raised left/right concrete sidewalks (15-25cm curb) with exact slope normals. */
	static void AppendRoadRibbonWithSidewalks(FIstanbulMeshSectionData& OutRoadSection, FIstanbulMeshSectionData& OutSidewalkSection,
		const FVector& Start, const FVector& End, float RoadWidthCm, float SidewalkWidthCm, float SidewalkHeightCm,
		const FLinearColor& RoadColor, const FLinearColor& SidewalkColor, int32 Subdivisions = 8);

	/** Appends a sagging parabolic catenary cable between Start and End. */
	static void AppendCatenaryCable(FIstanbulMeshSectionData& OutSection, const FVector& Start, const FVector& End,
		float SagCm, float ThicknessCm, int32 Segments, const FLinearColor& Color);

	/** Appends an extruded 2D silhouette polygon (defined in local X-Z coordinates) with thickness along local Y. */
	static void AppendExtrudedProfileXZ(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		const TArray<FVector2D>& ProfilePointsXZ, float ThicknessYCm, const FLinearColor& Color);

	/** Appends a quad with automatic normal, UVs, and vertex color. */
	static void AppendQuad(FIstanbulMeshSectionData& OutSection, const FVector& V0, const FVector& V1,
		const FVector& V2, const FVector& V3, const FLinearColor& Color);
};
