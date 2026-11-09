#include "Environment/TimeOfDayPresetDataAsset.h"
#include "Core/IstanbulTaxiGameplayTags.h"

UTimeOfDayPresetDataAsset::UTimeOfDayPresetDataAsset()
{
	auto Add = [this](FGameplayTag Tag, float Hour, float SunLux, FLinearColor SunCol, float Sky, float Fog, FLinearColor FogCol, float Exp, float Street, float Win, float Bridge)
	{
		FTimeOfDayPhaseSample S;
		S.PhaseTag = Tag;
		S.KeyHour = Hour;
		S.SunIntensityLux = SunLux;
		S.SunColor = SunCol;
		S.SkyLightIntensity = Sky;
		S.FogDensity = Fog;
		S.FogInscatteringColor = FogCol;
		S.ExposureCompensation = Exp;
		S.StreetLightEmissive = Street;
		S.WindowEmissive = Win;
		S.BridgeLedIntensity = Bridge;
		Phases.Add(S);
	};

	// 8 canonical Istanbul time phases (sorted by KeyHour ascending).
	// Balanced in Lux (0.5 .. 12.0 Lux) with locked EV100 = 1.0 exposure so scenes never wash out to white.
	Add(IstanbulTaxiTags::Time_LateNight,   2.5f,   0.35f, FLinearColor(0.20f, 0.28f, 0.52f), 0.35f, 0.018f, FLinearColor(0.05f, 0.07f, 0.14f),  0.1f, 1.0f, 0.25f, 0.35f);
	Add(IstanbulTaxiTags::Time_Dawn,        5.5f,   2.80f, FLinearColor(0.68f, 0.74f, 0.96f), 0.65f, 0.026f, FLinearColor(0.35f, 0.45f, 0.68f),  0.0f, 0.6f, 0.35f, 0.20f);
	Add(IstanbulTaxiTags::Time_MorningRush, 8.25f,  7.50f, FLinearColor(1.00f, 0.88f, 0.74f), 0.95f, 0.020f, FLinearColor(0.68f, 0.64f, 0.58f), -0.1f, 0.0f, 0.10f, 0.00f);
	Add(IstanbulTaxiTags::Time_Midday,     12.75f, 10.50f, FLinearColor(1.00f, 0.97f, 0.92f), 1.10f, 0.010f, FLinearColor(0.62f, 0.74f, 0.90f), -0.2f, 0.0f, 0.00f, 0.00f);
	Add(IstanbulTaxiTags::Time_Ikindi,     16.0f,   8.50f, FLinearColor(1.00f, 0.90f, 0.76f), 1.00f, 0.014f, FLinearColor(0.72f, 0.68f, 0.62f), -0.1f, 0.0f, 0.05f, 0.00f);
	Add(IstanbulTaxiTags::Time_GoldenHour, 18.5f,   4.80f, FLinearColor(1.00f, 0.62f, 0.32f), 0.80f, 0.022f, FLinearColor(0.85f, 0.50f, 0.30f),  0.0f, 0.3f, 0.35f, 0.40f);
	Add(IstanbulTaxiTags::Time_Dusk,       20.0f,   1.40f, FLinearColor(0.42f, 0.52f, 0.86f), 0.50f, 0.018f, FLinearColor(0.18f, 0.24f, 0.42f),  0.1f, 1.0f, 0.85f, 1.00f);
	Add(IstanbulTaxiTags::Time_Night,      22.5f,   0.50f, FLinearColor(0.24f, 0.32f, 0.56f), 0.40f, 0.015f, FLinearColor(0.08f, 0.10f, 0.18f),  0.1f, 1.0f, 0.75f, 1.00f);
}

FTimeOfDayPhaseSample UTimeOfDayPresetDataAsset::EvaluateAtHour(float Hour, FGameplayTag& OutActivePhaseTag) const
{
	if (Phases.Num() == 0)
	{
		OutActivePhaseTag = IstanbulTaxiTags::Time_Midday;
		return FTimeOfDayPhaseSample();
	}

	const float H = FMath::Fmod(FMath::Max(Hour, 0.0f), 24.0f);
	const int32 N = Phases.Num();

	// Find bracketing samples [IdxA, IdxB] around H (wrapping across midnight)
	int32 IdxB = 0;
	for (; IdxB < N; ++IdxB)
	{
		if (Phases[IdxB].KeyHour > H) break;
	}
	const int32 IdxA = (IdxB - 1 + N) % N;
	IdxB = IdxB % N;

	const FTimeOfDayPhaseSample& A = Phases[IdxA];
	const FTimeOfDayPhaseSample& B = Phases[IdxB];

	float Span = B.KeyHour - A.KeyHour;
	if (Span <= 0.0f) Span += 24.0f;
	float Elapsed = H - A.KeyHour;
	if (Elapsed < 0.0f) Elapsed += 24.0f;

	const float RawAlpha = Span > KINDA_SMALL_NUMBER ? FMath::Clamp(Elapsed / Span, 0.0f, 1.0f) : 0.0f;
	const float Alpha = FMath::SmoothStep(0.0f, 1.0f, RawAlpha);

	OutActivePhaseTag = (RawAlpha < 0.5f) ? A.PhaseTag : B.PhaseTag;

	FTimeOfDayPhaseSample Out;
	Out.PhaseTag = OutActivePhaseTag;
	Out.KeyHour = H;
	Out.SunIntensityLux = FMath::Lerp(A.SunIntensityLux, B.SunIntensityLux, Alpha);
	Out.SunColor = FMath::Lerp(A.SunColor, B.SunColor, Alpha);
	Out.SkyLightIntensity = FMath::Lerp(A.SkyLightIntensity, B.SkyLightIntensity, Alpha);
	Out.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, Alpha);
	Out.FogInscatteringColor = FMath::Lerp(A.FogInscatteringColor, B.FogInscatteringColor, Alpha);
	Out.ExposureCompensation = FMath::Lerp(A.ExposureCompensation, B.ExposureCompensation, Alpha);
	Out.StreetLightEmissive = FMath::Lerp(A.StreetLightEmissive, B.StreetLightEmissive, Alpha);
	Out.WindowEmissive = FMath::Lerp(A.WindowEmissive, B.WindowEmissive, Alpha);
	Out.BridgeLedIntensity = FMath::Lerp(A.BridgeLedIntensity, B.BridgeLedIntensity, Alpha);
	return Out;
}
