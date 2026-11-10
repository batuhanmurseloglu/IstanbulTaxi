#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

class UMaterialInstanceDynamic;

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

	/** Creates a lit PBR dynamic material instance with custom BaseColor, Metallic, Roughness, and Specular. */
	static UMaterialInstanceDynamic* CreatePBRMaterial(UObject* Outer, const FLinearColor& BaseColor,
		float Metallic = 0.0f, float Roughness = 0.5f, float Specular = 0.5f);

	/** Appends a 12-edge beveled 3D box (mid-poly look, rounded edges) to OutSection. */
	static void AppendBeveledBox(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		const FVector& HalfExtentsCm, float BevelCm, const FLinearColor& Color);

	/** Appends a tapered/sloped 3D frustum/prism between a bottom XY rectangle and an offset top XY rectangle (for sloped hoods, windshields, roofs, and cumbas). */
	static void AppendTaperedPrism(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		const FVector2D& BottomHalfXY, const FVector2D& TopHalfXY, const FVector& TopOffsetXYZ,
		float HalfHeightZ, const FLinearColor& Color);

	/** Appends a closed cylinder (axis along local Z, or rotated via Transform) to OutSection. */
	static void AppendCylinder(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		float RadiusCm, float HalfHeightCm, int32 Sides, const FLinearColor& Color, bool bCapEnds = true);

	/** Appends a cone / tapered cylinder (BottomRadius to TopRadius along local Z) to OutSection. */
	static void AppendCone(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		float BottomRadiusCm, float TopRadiusCm, float HalfHeightCm, int32 Sides, const FLinearColor& Color, bool bCapBottom = true);

	/**
	 * Appends a contoured multi-slot 3D sedan/hatchback/van/bus vehicle body across 4 material slots:
	 * - OutPaintSection (Slot 0: Glossy automotive body panels, sloped hood, roof, doors, fenders)
	 * - OutGlassSection (Slot 1: 45-deg raked front windshield, sloped rear glass, recessed side windows)
	 * - OutTrimSection  (Slot 2: Matte black bumpers, radiator grille, wheel arch liners, wipers, side mirror stalks)
	 * - OutLightsSection(Slot 3: Emissive projector headlights, red brake light bars, amber blinkers, plates, roof sign)
	 */
	static void AppendContouredSedanBody(
		FIstanbulMeshSectionData& OutPaintSection,
		FIstanbulMeshSectionData& OutGlassSection,
		FIstanbulMeshSectionData& OutTrimSection,
		FIstanbulMeshSectionData& OutLightsSection,
		const FLinearColor& PaintColor,
		bool bIsTaxi,
		bool bBrakeLightsBright = false);

	/**
	 * Appends a detailed 3D wheel with beveled rubber tire tread (OutTireSection) and a 5-spoke sculpted metallic rim (OutRimSection)
	 * centered at the origin (axis along Y) so rotating the wheel component visibly spins the 5 spokes.
	 */
	static void AppendDetailedWheelMesh(
		FIstanbulMeshSectionData& OutTireSection,
		FIstanbulMeshSectionData& OutRimSection,
		float RadiusCm = 35.0f,
		float HalfWidthCm = 13.0f,
		int32 Sides = 18);

	/**
	 * Appends a recessed 3D architectural window (inset 15cm into facade) with stone sill/lintel, dark reflective glass pane,
	 * muntin bars, and optional exterior shutters.
	 */
	static void AppendRecessedWindow(
		FIstanbulMeshSectionData& OutWallAndTrimSection,
		FIstanbulMeshSectionData& OutGlassSection,
		const FTransform& WindowCenterTransform,
		float WidthCm,
		float HeightCm,
		float RecessDepthCm,
		const FLinearColor& FrameColor,
		const FLinearColor& SillColor,
		bool bAddShutters = false,
		const FLinearColor& ShutterColor = FLinearColor(0.18f, 0.34f, 0.24f));

	/**
	 * Appends an authentic overhanging Istanbul Cumba (bay window) supported by angled 3D corbel brackets,
	 * recessed front/side windows, and a pitched terracotta roof cap.
	 */
	static void AppendCumbaWithCorbels(
		FIstanbulMeshSectionData& OutWallSection,
		FIstanbulMeshSectionData& OutGlassSection,
		FIstanbulMeshSectionData& OutRoofSection,
		const FTransform& CumbaCenterTransform,
		float WidthCm,
		float HeightCm,
		float ProjectionDepthCm,
		const FLinearColor& CumbaBodyColor,
		const FLinearColor& TrimColor);

	/**
	 * Appends a 3D striped fabric shop awning (Tente) with alternating colored stripes, front valance, and metal support arms.
	 */
	static void AppendStripedShopAwning(
		FIstanbulMeshSectionData& OutAwningSection,
		const FTransform& AwningTopWallTransform,
		float WidthCm,
		float ProjectionCm,
		float DropHeightCm,
		const FLinearColor& PrimaryStripeColor,
		const FLinearColor& SecondaryStripeColor);

	/**
	 * Appends a rooftop parabolic satellite dish (Çanak Anten) with mounting bracket and LNB arm.
	 */
	static void AppendSatelliteDish(
		FIstanbulMeshSectionData& OutSection,
		const FTransform& BaseTransform,
		float DishRadiusCm = 42.0f);

	/** Appends a smooth, continuous road ribbon + raised left/right concrete sidewalks (15-25cm curb) with exact slope normals. */
	static void AppendRoadRibbonWithSidewalks(FIstanbulMeshSectionData& OutRoadSection, FIstanbulMeshSectionData& OutSidewalkSection,
		const FVector& Start, const FVector& End, float RoadWidthCm, float SidewalkWidthCm, float SidewalkHeightCm,
		const FLinearColor& RoadColor, const FLinearColor& SidewalkColor, int32 Subdivisions = 8,
		FIstanbulMeshSectionData* OptionalMarkingsSection = nullptr, bool bTwoWayYellowCenterLine = false);

	/**
	 * Appends a junction patch (T, cross, or Y intersection) at a shared node with rounded curb corner returns (3-6m radius),
	 * matched sidewalk elevation, and white zebra crosswalk strips.
	 */
	static void AppendJunctionPatch(FIstanbulMeshSectionData& OutRoadSection, FIstanbulMeshSectionData& OutSidewalkSection,
		const FVector& NodeCenter, float RadiusCm, float CurbHeightCm,
		const FLinearColor& RoadColor, const FLinearColor& SidewalkColor, bool bAddCrosswalkStrips = true);

	/** Appends a sagging parabolic catenary cable between Start and End. */
	static void AppendCatenaryCable(FIstanbulMeshSectionData& OutSection, const FVector& Start, const FVector& End,
		float SagCm, float ThicknessCm, int32 Segments, const FLinearColor& Color);

	/** Appends an extruded 2D silhouette polygon (defined in local X-Z coordinates) with thickness along local Y. */
	static void AppendExtrudedProfileXZ(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		const TArray<FVector2D>& ProfilePointsXZ, float ThicknessYCm, const FLinearColor& Color);

	/** Appends a yellow-black alternating-band striped traffic bollard (Duba) using vertex colors. */
	static void AppendStripedBollard(FIstanbulMeshSectionData& OutSection, const FVector& BaseLocation,
		float RadiusCm = 12.0f, float HeightCm = 88.0f, int32 Sides = 10);

	/** Appends a coursed historic stone retaining wall with per-block warm beige/grey vertex-color variation. */
	static void AppendStoneWallWithVariation(FIstanbulMeshSectionData& OutSection, const FTransform& Transform,
		float LengthCm, float HeightCm, float ThicknessCm, FRandomStream& Rng);

	/** Appends a sagging backstreet cable with either hanging laundry clothes or a warm central street lantern. */
	static void AppendBackstreetLaundryOrLantern(FIstanbulMeshSectionData& OutSection, const FVector& Start,
		const FVector& End, float SagCm, bool bHangingLantern, FRandomStream& Rng);

	/** Appends a low-cost café table + 2 chairs + optional street cat silhouette cluster on a sidewalk/square. */
	static void AppendCafeTableAndCatCluster(FIstanbulMeshSectionData& OutSection, const FVector& BaseLocation,
		float YawDeg, FRandomStream& Rng);

	/** Appends a quad with automatic normal, UVs, and vertex color. */
	static void AppendQuad(FIstanbulMeshSectionData& OutSection, const FVector& V0, const FVector& V1,
		const FVector& V2, const FVector& V3, const FLinearColor& Color);
};
