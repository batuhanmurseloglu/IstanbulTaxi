#include "Core/IstanbulTaxiHUD.h"
#include "Vehicle/TaxiPawn.h"
#include "Vehicle/TaxiSurfaceComponent.h"
#include "Missions/PassengerInteractionComponent.h"
#include "Taximeter/TaximeterComponent.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Environment/TimeOfDaySubsystem.h"
#include "Environment/WeatherSubsystem.h"
#include "Audio/AudioAmbienceSubsystem.h"
#include "Audio/DistrictAmbienceDataAsset.h"
#include "Audio/PrayerTimeCalculator.h"
#include "Traffic/IstanbulTrafficManager.h"
#include "Core/TestSceneBuilder.h"
#include "AI/IstanbulPedestrian.h"
#include "Environment/IstanbulFerry.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AIstanbulTaxiHUD::AIstanbulTaxiHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AIstanbulTaxiHUD::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* W = GetWorld())
	{
		if (UPassengerMissionSubsystem* Missions = W->GetSubsystem<UPassengerMissionSubsystem>())
		{
			Missions->OnRideCompleted.AddDynamic(this, &AIstanbulTaxiHUD::HandleRideCompleted);
			Missions->OnPassengerRemark.AddDynamic(this, &AIstanbulTaxiHUD::HandlePassengerRemark);
		}
		if (UAudioAmbienceSubsystem* Audio = W->GetSubsystem<UAudioAmbienceSubsystem>())
		{
			Audio->OnEzanTriggered.AddDynamic(this, &AIstanbulTaxiHUD::HandleEzanTriggered);
		}
	}

	PushToast(TEXT("İstanbul Taksi Simülatörü — Sahil Bulvarı & Sultanahmet Ara Sokakları Hazır! [E] Yolcu Al | [T] Taksimetre"),
		FLinearColor(0.98f, 0.82f, 0.15f), 9.0f);
}

void AIstanbulTaxiHUD::PushToast(const FString& Message, const FLinearColor& Color, float Duration)
{
	FToastEntry E;
	E.Text = Message;
	E.Color = Color;
	E.ExpireTime = GetWorld() ? GetWorld()->GetTimeSeconds() + Duration : Duration;
	Toasts.Add(E);
	if (Toasts.Num() > 5)
	{
		Toasts.RemoveAt(0);
	}
}

void AIstanbulTaxiHUD::HandleRideCompleted(const FTaxiRideResult& Result)
{
	const FString Msg = FString::Printf(
		TEXT("YOLCULUK BİTTİ: Ödenen %s TL + Bahşiş %s TL | Yıldız: %d/5 (%.2f km)"),
		*UTaximeterComponent::FormatKurus(Result.PaidFareKurus).ToString(),
		*UTaximeterComponent::FormatKurus(Result.TipKurus).ToString(),
		Result.Rating,
		Result.Receipt.DistanceKm);
	PushToast(Msg, FLinearColor(0.25f, 0.95f, 0.45f), 9.0f);
}

void AIstanbulTaxiHUD::HandlePassengerRemark(const FTaxiRideRequest&, FGameplayTag RemarkTag)
{
	PushToast(FString::Printf(TEXT("Yolcu: \"Abi taksimetreyi açmadın!\" (%s)"), *RemarkTag.ToString()),
		FLinearColor(1.0f, 0.35f, 0.25f), 7.0f);
}

void AIstanbulTaxiHUD::HandleTollCharged(FGameplayTag Crossing, int32 FeeKurus, bool bAddedToFare)
{
	const FString Msg = FString::Printf(TEXT("HGS GEÇİŞ (%s): %s TL %s"),
		*Crossing.ToString(),
		*UTaximeterComponent::FormatKurus(FeeKurus).ToString(),
		bAddedToFare ? TEXT("[Taksimetreye Eklendi]") : TEXT("[Boş Geçiş]"));
	PushToast(Msg, FLinearColor(0.25f, 0.82f, 1.0f), 7.0f);
}

void AIstanbulTaxiHUD::HandleEzanTriggered(FGameplayTag PrayerTag, float PrayerHour)
{
	PushToast(FString::Printf(TEXT("Ezan Vakti: %s (%s) — Sultanahmet & Sahil Camileri"),
		*PrayerTag.ToString(), *UPrayerTimeCalculator::FormatPrayerHour(PrayerHour).ToString()),
		FLinearColor(0.95f, 0.85f, 0.45f), 8.0f);
}

void AIstanbulTaxiHUD::HandleTrafficHorn(AActor*, FVector, FString ReasonText)
{
	PushToast(FString::Printf(TEXT("TRAFİK KORNA: %s"), *ReasonText), FLinearColor(1.0f, 0.65f, 0.15f), 3.8f);
}

void AIstanbulTaxiHUD::DrawBezelBox(float X, float Y, float W, float H, const FLinearColor& Fill, const FLinearColor& Border, float BorderThickness)
{
	DrawRect(Fill, X, Y, W, H);
	DrawLine(X, Y, X + W, Y, Border, BorderThickness);
	DrawLine(X, Y + H, X + W, Y + H, Border, BorderThickness);
	DrawLine(X, Y, X, Y + H, Border, BorderThickness);
	DrawLine(X + W, Y, X + W, Y + H, Border, BorderThickness);
}

void AIstanbulTaxiHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GetWorld()) return;

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	ATaxiPawn* Taxi = Cast<ATaxiPawn>(GetOwningPawn());
	UTaximeterComponent* Meter = Taxi ? Taxi->GetTaximeter() : nullptr;
	UTaxiSurfaceComponent* SurfComp = Taxi ? Taxi->GetSurfaceComponent() : nullptr;

	if (Meter && !bBoundToPawnDelegates)
	{
		Meter->OnTollCharged.AddDynamic(this, &AIstanbulTaxiHUD::HandleTollCharged);
		bBoundToPawnDelegates = true;
	}
	if (!bBoundToTrafficDelegates)
	{
		if (AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AIstanbulTrafficManager::StaticClass())))
		{
			TM->OnAnyVehicleHonked.AddDynamic(this, &AIstanbulTaxiHUD::HandleTrafficHorn);
			bBoundToTrafficDelegates = true;
		}
	}

	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;

	// =========================================================================
	// 1. TOP-LEFT: COMPACT ISTANBUL AMBIENCE, CLOCK & SURFACE TELEMETRY BAR
	// =========================================================================
	DrawBezelBox(20.0f, 18.0f, 480.0f, 126.0f, FLinearColor(0.012f, 0.016f, 0.024f, 0.86f), FLinearColor(0.95f, 0.70f, 0.04f, 0.85f), 1.5f);
	DrawRect(FLinearColor(0.95f, 0.70f, 0.04f, 0.95f), 20.0f, 18.0f, 6.0f, 126.0f);

	FString ClockStr = TEXT("08:30"), PhaseStr = TEXT("MorningRush");
	float SunAlt = 28.0f;
	if (const UTimeOfDaySubsystem* TimeSub = GetWorld()->GetSubsystem<UTimeOfDaySubsystem>())
	{
		ClockStr = TimeSub->GetFormattedClockText().ToString();
		PhaseStr = TimeSub->GetCurrentPhaseTag().ToString().Replace(TEXT("Time."), TEXT(""));
		SunAlt   = TimeSub->GetSunAltitudeDegrees();
	}

	FString DistrictStr = TEXT("Eminönü / Sahil Bulvarı");
	float Gulls = 0.0f, LPFHz = 20000.0f;
	FString PrayerLine;
	if (const UAudioAmbienceSubsystem* AudioSub = GetWorld()->GetSubsystem<UAudioAmbienceSubsystem>())
	{
		if (const UDistrictAmbienceDataAsset* Prof = AudioSub->GetActiveDistrictProfile())
		{
			DistrictStr = Prof->DisplayName.IsEmpty() ? Prof->DistrictTag.ToString() : Prof->DisplayName.ToString();
		}
		Gulls = AudioSub->GetCurrentSeagullIntensity();
		LPFHz = AudioSub->GetCurrentExteriorLPFHz();
		const FDiyanetPrayerSchedule& S = AudioSub->GetTodayPrayerSchedule();
		PrayerLine = FString::Printf(TEXT("Ezan: İmsak %s | Öğle %s | İkindi %s | Akşam %s | Yatsı %s"),
			*UPrayerTimeCalculator::FormatPrayerHour(S.Sabah).ToString(),
			*UPrayerTimeCalculator::FormatPrayerHour(S.Ogle).ToString(),
			*UPrayerTimeCalculator::FormatPrayerHour(S.Ikindi).ToString(),
			*UPrayerTimeCalculator::FormatPrayerHour(S.Aksam).ToString(),
			*UPrayerTimeCalculator::FormatPrayerHour(S.Yatsi).ToString());
	}

	const FString SurfName = SurfComp ? SurfComp->GetDominantSurfaceTag().ToString().Replace(TEXT("Surface."), TEXT("")) : TEXT("Asphalt");
	const float VibPct = SurfComp ? SurfComp->GetVibrationIntensity() * 100.0f : 0.0f;

	DrawText(FString::Printf(TEXT("İSTANBUL  %s   |   %s (Güneş %.0f°)   |   %s"), *ClockStr, *PhaseStr, SunAlt, *DistrictStr),
		FLinearColor(1.0f, 0.90f, 0.45f), 36.0f, 26.0f, Font, 0.96f);
	DrawText(FString::Printf(TEXT("Zemin: %s (Sarsıntı %.0f%%)   |   Martı: %.0f%%   |   Cam LPF: %.0f Hz"),
		*SurfName, VibPct, Gulls * 100.0f, LPFHz),
		FLinearColor(0.76f, 0.88f, 0.98f), 36.0f, 54.0f, Font, 0.92f);

	FString WeatherStr = TEXT("Açık / Güneşli");
	if (const UWeatherSubsystem* WeatherSub = GetWorld()->GetSubsystem<UWeatherSubsystem>())
	{
		WeatherStr = WeatherSub->GetWeatherDisplayName();
	}
	DrawText(FString::Printf(TEXT("Hava: %s [B: Değiştir]   |   Kamera: %s [C: Değiştir]"),
		*WeatherStr, (Taxi && Taxi->IsInteriorCamera()) ? TEXT("İç / Kokpit") : (Taxi && Taxi->GetCameraViewModeIndex() == 1) ? TEXT("Kaput/Tampon") : TEXT("Dış/Takip")),
		FLinearColor(0.85f, 0.92f, 0.65f), 36.0f, 82.0f, Font, 0.86f);
	DrawText(PrayerLine, FLinearColor(0.68f, 0.74f, 0.82f), 36.0f, 108.0f, Font, 0.82f);

	// =========================================================================
	// 2. BOTTOM-RIGHT: AUTHENTIC TURKISH DIGITAL TAXIMETER UNIT & INSTRUMENT CLUSTER
	// =========================================================================
	const float UnitW = 430.0f;
	const float UnitH = 228.0f;
	const float UnitX = ScreenW - UnitW - 24.0f;
	const float UnitY = ScreenH - UnitH - 24.0f;

	// Outer automotive plastic housing bezel
	DrawBezelBox(UnitX, UnitY, UnitW, UnitH, FLinearColor(0.03f, 0.032f, 0.038f, 0.95f), FLinearColor(0.22f, 0.24f, 0.28f, 1.0f), 2.5f);
	// Top yellow taxi header bar
	DrawRect(FLinearColor(0.95f, 0.68f, 0.02f, 1.0f), UnitX + 2.0f, UnitY + 2.0f, UnitW - 4.0f, 26.0f);

	const UTaximeterTariffDataAsset* Tariff = Meter ? Meter->GetActiveTariff() : nullptr;
	const FString SegTitle = Tariff ? Tariff->SegmentName.ToString().ToUpper() : TEXT("SARI TAKSİ");
	DrawText(FString::Printf(TEXT("İBB UKOME DİJİTAL TAKSİMETRE  —  %s"), *SegTitle),
		FLinearColor(0.02f, 0.02f, 0.02f, 1.0f), UnitX + 14.0f, UnitY + 5.0f, Font, 0.92f);

	// Recessed LCD Fare Screen
	const float LcdX = UnitX + 14.0f;
	const float LcdY = UnitY + 36.0f;
	const float LcdW = UnitW - 28.0f;
	const float LcdH = 104.0f;
	DrawBezelBox(LcdX, LcdY, LcdW, LcdH, FLinearColor(0.008f, 0.012f, 0.010f, 0.98f), FLinearColor(0.12f, 0.16f, 0.14f, 1.0f), 1.5f);

	// Status LED Pills (BOŞ / DOLU / BEKLEME)
	const ETaximeterState S = Meter ? Meter->GetState() : ETaximeterState::Free;
	auto DrawLedPill = [&](float PX, const TCHAR* Label, bool bActive, const FLinearColor& ActiveCol)
	{
		const FLinearColor Fill = bActive ? ActiveCol : FLinearColor(0.06f, 0.07f, 0.08f, 1.0f);
		const FLinearColor TextCol = bActive ? FLinearColor(0.02f, 0.02f, 0.02f, 1.0f) : FLinearColor(0.32f, 0.35f, 0.38f, 1.0f);
		DrawBezelBox(PX, LcdY + 8.0f, 88.0f, 22.0f, Fill, ActiveCol * (bActive ? 1.0f : 0.25f), 1.0f);
		DrawText(Label, TextCol, PX + 12.0f, LcdY + 9.0f, Font, 0.85f);
	};
	DrawLedPill(LcdX + 10.0f,  TEXT("  BOŞ"),     S == ETaximeterState::Free,   FLinearColor(0.18f, 0.92f, 0.38f));
	DrawLedPill(LcdX + 106.0f, TEXT("  DOLU"),    S == ETaximeterState::Hired,  FLinearColor(1.00f, 0.24f, 0.18f));
	DrawLedPill(LcdX + 202.0f, TEXT("BEKLEME"),   S == ETaximeterState::Paused || (S == ETaximeterState::Hired && Meter && Meter->IsBillingWaitTime()), FLinearColor(1.00f, 0.72f, 0.08f));

	// Tariff & Minimum Fare badge on right of LCD
	if (Tariff)
	{
		DrawText(FString::Printf(TEXT("İndi-Bindi: %s"), *UTaximeterComponent::FormatKurus(Tariff->MinimumFareKurus).ToString()),
			FLinearColor(0.55f, 0.68f, 0.60f), LcdX + 300.0f, LcdY + 10.0f, Font, 0.78f);
	}

	// Giant 7-Segment-Style Amber/Red Fare Readout
	const FString FareStr = Meter ? UTaximeterComponent::FormatKurus(Meter->GetDisplayedFareKurus()).ToString() : TEXT("0,00");
	const FLinearColor DigitColor = (S == ETaximeterState::Hired)
		? FLinearColor(1.0f, 0.26f, 0.14f)  // Classic bright red/amber LED digits when hired
		: (S == ETaximeterState::Paused) ? FLinearColor(1.0f, 0.76f, 0.12f) : FLinearColor(0.22f, 0.92f, 0.45f);

	DrawText(FString::Printf(TEXT("%s TL"), *FareStr), DigitColor, LcdX + 14.0f, LcdY + 38.0f, Font, 1.85f);

	// Distance & Rate sub-readout inside LCD
	const float DistKm = Meter ? Meter->GetHireDistanceKm() : 0.0f;
	DrawText(FString::Printf(TEXT("MESAFE: %05.2f km"), DistKm),
		FLinearColor(0.95f, 0.78f, 0.25f), LcdX + 245.0f, LcdY + 46.0f, Font, 0.95f);
	if (Tariff)
	{
		DrawText(FString::Printf(TEXT("Açılış %s | Km %s"),
			*UTaximeterComponent::FormatKurus(Tariff->OpeningFareKurus).ToString(),
			*UTaximeterComponent::FormatKurus(Tariff->PerKmKurus).ToString()),
			FLinearColor(0.52f, 0.58f, 0.55f), LcdX + 245.0f, LcdY + 72.0f, Font, 0.80f);
	}

	// ---- Lower Instrument Cluster: Speed, Gear, RPM Bar ----
	const float InstY = LcdY + LcdH + 10.0f;
	const float SpeedKmh = Taxi ? FMath::Abs(Taxi->GetSpeedKmh()) : 0.0f;
	const float RPM = Taxi ? Taxi->GetEngineRPM() : 850.0f;
	const int32 Gear = Taxi ? Taxi->GetCurrentGear() : 0;
	const FString GearStr = (Gear < 0) ? TEXT("R") : (Gear == 0) ? TEXT("N") : FString::FromInt(Gear);

	// Gear Box
	DrawBezelBox(LcdX, InstY, 46.0f, 46.0f, FLinearColor(0.08f, 0.09f, 0.11f, 1.0f), FLinearColor(0.95f, 0.68f, 0.02f, 0.9f), 1.5f);
	DrawText(GearStr, FLinearColor(0.98f, 0.82f, 0.15f), LcdX + 15.0f, InstY + 8.0f, Font, 1.45f);

	// Speed Readout
	DrawText(FString::Printf(TEXT("%3.0f km/s"), SpeedKmh), FLinearColor::White, LcdX + 58.0f, InstY + 2.0f, Font, 1.25f);
	DrawText(FString::Printf(TEXT("%.0f d/dk [%s]"), RPM, (Taxi && Taxi->IsInteriorCamera()) ? TEXT("İÇ") : TEXT("DIŞ")),
		FLinearColor(0.68f, 0.74f, 0.80f), LcdX + 60.0f, InstY + 28.0f, Font, 0.82f);

	// Segmented RPM Bar (20 LED segments on the right side of the cluster)
	const float RpmBarX = LcdX + 188.0f;
	const float RpmBarY = InstY + 8.0f;
	const int32 NumSegs = 18;
	const float RpmRatio = FMath::Clamp((RPM - 700.0f) / 3900.0f, 0.0f, 1.0f);
	const int32 LitSegs = FMath::RoundToInt(RpmRatio * NumSegs);

	for (int32 i = 0; i < NumSegs; ++i)
	{
		const float SegAlpha = (float)i / (float)NumSegs;
		const FLinearColor SegCol = (SegAlpha > 0.82f) ? FLinearColor(1.0f, 0.18f, 0.15f)
			: (SegAlpha > 0.60f) ? FLinearColor(1.0f, 0.72f, 0.08f) : FLinearColor(0.18f, 0.90f, 0.42f);
		const FLinearColor DrawCol = (i < LitSegs) ? SegCol : FLinearColor(0.10f, 0.11f, 0.13f, 1.0f);
		DrawRect(DrawCol, RpmBarX + i * 11.5f, RpmBarY, 9.0f, 20.0f);
	}
	DrawText(TEXT("DEVİR (RPM) x1000"), FLinearColor(0.55f, 0.60f, 0.66f), RpmBarX, RpmBarY + 24.0f, Font, 0.76f);

	// =========================================================================
	// 3. TOP-RIGHT: PASSENGER MISSION TRACKER, PATIENCE/COMFORT & DIALOGUE UI
	// =========================================================================
	if (const UPassengerMissionSubsystem* Missions = GetWorld()->GetSubsystem<UPassengerMissionSubsystem>())
	{
		const ETaxiRidePhase Phase = Missions->GetPhase();
		const float MisW = 430.0f;
		const float MisX = ScreenW - MisW - 24.0f;
		DrawBezelBox(MisX, 18.0f, MisW, 82.0f, FLinearColor(0.012f, 0.016f, 0.024f, 0.86f), FLinearColor(0.25f, 0.78f, 1.0f, 0.85f));

		if (Phase == ETaxiRidePhase::None)
		{
			DrawText(TEXT("YOLCU ÇAĞRISI BEKLENİYOR"), FLinearColor(0.78f, 0.84f, 0.92f), MisX + 14.0f, 28.0f, Font, 0.98f);
			DrawText(TEXT("[E] Tuşu: Yeni Yolcu Çağrısı (Yokuş / Sahil / Köprü)"), FLinearColor(0.98f, 0.82f, 0.20f), MisX + 14.0f, 56.0f, Font, 0.88f);
		}
		else
		{
			const FTaxiRideRequest& R = Missions->GetActiveRide();
			const FVector ObjLoc = Missions->GetCurrentObjectiveLocation();
			const float DistM = Taxi ? FVector::Dist2D(Taxi->GetActorLocation(), ObjLoc) / 100.0f : 0.0f;

			const FString PhaseTitle = (Phase == ETaxiRidePhase::ToPickup)
				? FString::Printf(TEXT("YOLCUYA GİDİLİYOR: %s"), *R.PickupName.ToString())
				: FString::Printf(TEXT("VARIŞ NOKTASI: %s"), *R.DropoffName.ToString());
			const FLinearColor ObjColor = (Phase == ETaxiRidePhase::ToPickup)
				? FLinearColor(0.25f, 0.95f, 0.45f) : FLinearColor(1.0f, 0.75f, 0.15f);

			DrawText(PhaseTitle, ObjColor, MisX + 14.0f, 28.0f, Font, 0.94f);
			DrawText(FString::Printf(TEXT("Mesafe: %.0f m  (Alan içinde 2 sn durun)"), DistM),
				FLinearColor::White, MisX + 14.0f, 56.0f, Font, 0.90f);

			const FVector ScreenPos = Project(ObjLoc + FVector(0, 0, 220.0f));
			if (ScreenPos.Z > 0.0f)
			{
				const float SX = FMath::Clamp(ScreenPos.X, 40.0f, ScreenW - 40.0f);
				const float SY = FMath::Clamp(ScreenPos.Y, 40.0f, ScreenH - 40.0f);
				DrawBezelBox(SX - 52.0f, SY - 28.0f, 104.0f, 24.0f, FLinearColor(0.01f, 0.02f, 0.03f, 0.88f), ObjColor, 1.5f);
				DrawText(FString::Printf(TEXT("%s %.0fm"), Phase == ETaxiRidePhase::ToPickup ? TEXT("YOLCU") : TEXT("HEDEF"), DistM),
					ObjColor, SX - 44.0f, SY - 26.0f, Font, 0.85f);
			}
		}

		// Deep Passenger-Driver Interaction Panel (when passenger is onboard)
		const UPassengerInteractionComponent* PassComp = Taxi ? Taxi->GetPassengerInteraction() : nullptr;
		if (PassComp && PassComp->IsPassengerOnboard())
		{
			const float PanelX = MisX;
			float CurY = 108.0f;

			// 1) Archetype, Sabır (Patience) & Konfor (Comfort) Progress Bars
			DrawBezelBox(PanelX, CurY, MisW, 92.0f, FLinearColor(0.014f, 0.018f, 0.028f, 0.90f), FLinearColor(0.95f, 0.72f, 0.08f, 0.85f), 1.5f);
			DrawText(FString::Printf(TEXT("YOLCU PROFİLİ: %s"), *PassComp->GetArchetypeDisplayName()),
				FLinearColor(1.0f, 0.86f, 0.28f), PanelX + 12.0f, CurY + 8.0f, Font, 0.92f);
			DrawText(PassComp->GetArchetypeTraitSummary(),
				FLinearColor(0.72f, 0.80f, 0.88f), PanelX + 12.0f, CurY + 30.0f, Font, 0.78f);

			auto DrawMeterBar = [&](float BX, float BY, const TCHAR* Label, float Pct)
			{
				const float Ratio = FMath::Clamp(Pct / 100.0f, 0.0f, 1.0f);
				const FLinearColor BarCol = (Ratio > 0.65f) ? FLinearColor(0.18f, 0.92f, 0.42f)
					: (Ratio > 0.35f) ? FLinearColor(1.0f, 0.75f, 0.12f) : FLinearColor(1.0f, 0.22f, 0.18f);
				DrawText(FString::Printf(TEXT("%s: %.0f%%"), Label, Pct), FLinearColor::White, BX, BY, Font, 0.82f);
				DrawBezelBox(BX, BY + 19.0f, 188.0f, 11.0f, FLinearColor(0.06f, 0.07f, 0.09f, 1.0f), FLinearColor(0.28f, 0.32f, 0.38f, 1.0f), 1.0f);
				DrawRect(BarCol, BX + 1.0f, BY + 20.0f, 186.0f * Ratio, 9.0f);
			};

			DrawMeterBar(PanelX + 12.0f,  CurY + 52.0f, TEXT("SABIR"),  PassComp->GetPatienceScore());
			DrawMeterBar(PanelX + 224.0f, CurY + 52.0f, TEXT("KONFOR"), PassComp->GetComfortScore());
			CurY += 100.0f;

			// 2) Turkish Passenger Speech Bubble
			if (PassComp->GetSpeechTimeRemaining() > 0.0f && !PassComp->GetCurrentSpeechBubble().IsEmpty())
			{
				DrawBezelBox(PanelX, CurY, MisW, 56.0f, FLinearColor(0.04f, 0.06f, 0.10f, 0.94f), FLinearColor(0.35f, 0.88f, 1.0f, 0.95f), 1.5f);
				DrawText(TEXT("💬 YOLCU:"), FLinearColor(0.35f, 0.88f, 1.0f), PanelX + 12.0f, CurY + 6.0f, Font, 0.82f);
				DrawText(FString::Printf(TEXT("\"%s\""), *PassComp->GetCurrentSpeechBubble()),
					FLinearColor(0.98f, 0.96f, 0.88f), PanelX + 12.0f, CurY + 27.0f, Font, 0.84f);
				CurY += 64.0f;
			}

			// 3) Active Live Request Prompt ([1]/[2]/[3] Dialogue Choice OR [V] Window Request)
			const EPassengerLiveRequestType ReqType = PassComp->GetActiveRequestType();
			if (ReqType == EPassengerLiveRequestType::DialogueChoice)
			{
				const TArray<FPassengerDialogueOption>& Opts = PassComp->GetDialogueOptions();
				const float BoxH = 34.0f + Opts.Num() * 24.0f;
				DrawBezelBox(PanelX, CurY, MisW, BoxH, FLinearColor(0.08f, 0.05f, 0.01f, 0.94f), FLinearColor(1.0f, 0.78f, 0.12f, 1.0f), 2.0f);
				DrawText(FString::Printf(TEXT("YANIT SEÇİN ([1] / [2] / [3])  —  Kalan Süre: %.0f sn"), PassComp->GetRequestTimeRemaining()),
					FLinearColor(1.0f, 0.84f, 0.18f), PanelX + 12.0f, CurY + 7.0f, Font, 0.85f);
				for (int32 i = 0; i < Opts.Num(); ++i)
				{
					DrawText(Opts[i].OptionText, FLinearColor(0.94f, 0.96f, 1.0f), PanelX + 14.0f, CurY + 31.0f + i * 22.0f, Font, 0.82f);
				}
			}
			else if (ReqType == EPassengerLiveRequestType::CloseWindowDraft || ReqType == EPassengerLiveRequestType::OpenWindowFresh)
			{
				DrawBezelBox(PanelX, CurY, MisW, 54.0f, FLinearColor(0.10f, 0.03f, 0.02f, 0.94f), FLinearColor(1.0f, 0.52f, 0.14f, 1.0f), 2.0f);
				DrawText(FString::Printf(TEXT("YOLCU İSTEĞİ (%.0f sn):"), PassComp->GetRequestTimeRemaining()),
					FLinearColor(1.0f, 0.65f, 0.20f), PanelX + 12.0f, CurY + 6.0f, Font, 0.85f);
				const FString ActStr = (ReqType == EPassengerLiveRequestType::CloseWindowDraft)
					? TEXT("[V] Tuşuna Basarak Camı KAPATIN (Bahşiş & Konfor Bonusu)")
					: TEXT("[V] Tuşuna Basarak Camı AÇIN (Bahşiş & Konfor Bonusu)");
				DrawText(ActStr, FLinearColor(0.28f, 0.98f, 0.52f), PanelX + 12.0f, CurY + 28.0f, Font, 0.84f);
			}
		}
	}

	// =========================================================================
	// 4. 3D AI TRAFFIC HORN & DOUBLE-PARKED HAZARD CALLOUTS IN VIEWPORT
	// =========================================================================
	if (AIstanbulTrafficManager* TM = Cast<AIstanbulTrafficManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AIstanbulTrafficManager::StaticClass())))
	{
		for (const ATrafficVehicle* V : TM->GetSpawnedVehicles())
		{
			if (!V || !Taxi) continue;
			const float DistM = FVector::Dist(V->GetActorLocation(), Taxi->GetActorLocation()) / 100.0f;
			if (DistM > 75.0f) continue;

			if (V->IsHonkingNow())
			{
				const FVector SP = Project(V->GetActorLocation() + FVector(0, 0, 210.0f));
				if (SP.Z > 0.0f)
				{
					DrawBezelBox(SP.X - 48.0f, SP.Y - 22.0f, 96.0f, 22.0f, FLinearColor(0.15f, 0.04f, 0.02f, 0.9f), FLinearColor(1.0f, 0.65f, 0.1f), 1.5f);
					DrawText(TEXT("📢 KORNA!"), FLinearColor(1.0f, 0.80f, 0.15f), SP.X - 38.0f, SP.Y - 20.0f, Font, 0.85f);
				}
			}
		}
	}

	// =========================================================================
	// 5. 2D MINIMAP / RADAR & DISTRICT COMPASS + COMPACT CONTROLS BAR
	// =========================================================================
	DrawMinimapAndCompass(Taxi, Font, ScreenW, ScreenH);

	const float HelpX = 278.0f;
	const float HelpY = ScreenH - 96.0f;
	DrawBezelBox(HelpX, HelpY, 595.0f, 72.0f, FLinearColor(0.012f, 0.016f, 0.024f, 0.82f), FLinearColor(0.35f, 0.40f, 0.46f, 0.7f));
	DrawText(TEXT("[WASD] Sürüş  |  [Space] El Freni (Yokuş Kalkış)  |  [H] Korna  |  [C] Kamera  |  [R] Düzelt"),
		FLinearColor(0.92f, 0.94f, 0.98f), HelpX + 12.0f, HelpY + 8.0f, Font, 0.85f);
	DrawText(TEXT("[T] Taksimetre Aç/Kapat  |  [P] Beklet  |  [Y] Tarife  |  [1/2/3] Yolcu Diyalog Seçimi"),
		FLinearColor(0.98f, 0.82f, 0.22f), HelpX + 12.0f, HelpY + 28.0f, Font, 0.85f);
	DrawText(TEXT("[E] Yolcu Görevi  |  [V] Cam Aç/Kapat  |  [N] Ezan Oku  |  [M] Saati +1.5s İlerlet"),
		FLinearColor(0.75f, 0.88f, 0.98f), HelpX + 12.0f, HelpY + 48.0f, Font, 0.85f);

	// =========================================================================
	// 6. TOAST NOTIFICATIONS
	// =========================================================================
	const float Now = GetWorld()->GetTimeSeconds();
	float ToastY = 152.0f;
	for (int32 i = Toasts.Num() - 1; i >= 0; --i)
	{
		if (Now > Toasts[i].ExpireTime)
		{
			Toasts.RemoveAt(i);
			continue;
		}
		DrawBezelBox(20.0f, ToastY, 620.0f, 30.0f, FLinearColor(0.02f, 0.03f, 0.05f, 0.90f), Toasts[i].Color);
		DrawText(Toasts[i].Text, Toasts[i].Color, 32.0f, ToastY + 5.0f, Font, 0.90f);
		ToastY += 35.0f;
	}
}

void AIstanbulTaxiHUD::DrawMinimapAndCompass(ATaxiPawn* Taxi, UFont* Font, float ScreenW, float ScreenH)
{
	UWorld* W = GetWorld();
	if (!W) return;

	const FVector PlayerLoc = Taxi ? Taxi->GetActorLocation() : FVector(5200.0f, -420.0f, 35.0f);
	const float PlayerYawDeg = Taxi ? Taxi->GetActorRotation().Yaw : -38.0f;
	const float Now = W->GetTimeSeconds();

	// 1) Top-Center District Compass Strip showing heading + direction to Seafront & Landmarks
	const float CompW = 360.0f;
	const float CompX = (ScreenW - CompW) * 0.5f;
	const float CompY = 18.0f;
	DrawBezelBox(CompX, CompY, CompW, 34.0f, FLinearColor(0.012f, 0.016f, 0.024f, 0.86f), FLinearColor(0.30f, 0.72f, 0.95f, 0.85f), 1.5f);

	const float YawNorm = FRotator::ClampAxis(PlayerYawDeg);
	static const TCHAR* CompassDirs[8] = { TEXT("D (Boğaz)"), TEXT("GD"), TEXT("G (Sultanahmet)"), TEXT("GB"), TEXT("B (Yarımada)"), TEXT("KB"), TEXT("K (Deniz / Galata)"), TEXT("KD") };
	const int32 DirIdx = FMath::RoundToInt(YawNorm / 45.0f) % 8;
	const FString NearestArea =
		(PlayerLoc.Y < -35000.0f) ? TEXT("Karaköy / Galata") :
		(PlayerLoc.Y < -2000.0f)  ? TEXT("Galata Köprüsü") :
		(PlayerLoc.X > 65000.0f)  ? TEXT("Beşiktaş / Boğaz Köprüsü") :
		(PlayerLoc.Y > 3000.0f)   ? TEXT("Sultanahmet / Cihangir") :
		TEXT("Eminönü Sahil Kordonu");
	DrawText(FString::Printf(TEXT("PUSULA: %s (%.0f°)  |  %s"), CompassDirs[DirIdx], YawNorm, *NearestArea),
		FLinearColor(0.86f, 0.94f, 1.0f), CompX + 14.0f, CompY + 8.0f, Font, 0.86f);

	// 2) Bottom-Left 2D Minimap / Radar Panel (1.2 km coverage radius -> 2.4 km width around player)
	const float MapW = 244.0f;
	const float MapH = 244.0f;
	const float MapX = 20.0f;
	const float MapY = ScreenH - MapH - 24.0f;
	const float BodyY = MapY + 24.0f;
	const float BodyH = MapH - 24.0f;
	const float CenterX = MapX + MapW * 0.5f;
	const float CenterY = BodyY + BodyH * 0.5f;

	// North-up top-down radar covering +/- 55,000 cm (1.1 km total box width)
	const float RadarHalfRangeCm = 55000.0f;
	const float ScalePxPerCm = (MapW * 0.5f) / RadarHalfRangeCm;

	auto WorldToMap = [&](const FVector2D& WorldXY) -> FVector2D
	{
		// +X is East (Right on map), +Y is Inland South/Up on screen (-ScreenY in UE UI canvas? Wait: in this map Y<0 is sea toward Golden Horn/Bosphorus!)
		// Map +X right, -Y UP or DOWN: let's put Golden Horn / Bosphorus sea (-Y) in the UPPER/NORTH half or LOWER half consistently:
		// Map +Y (World +Y) downward -> World -Y (Sea, Galata Bridge, Kız Kulesi) is UP on the radar so facing sea looks toward top!
		const float DX = (WorldXY.X - PlayerLoc.X) * ScalePxPerCm;
		const float DY = (WorldXY.Y - PlayerLoc.Y) * ScalePxPerCm;
		return FVector2D(CenterX + DX, CenterY + DY);
	};

	DrawBezelBox(MapX, MapY, MapW, MapH, FLinearColor(0.05f, 0.06f, 0.07f, 0.92f), FLinearColor(0.95f, 0.70f, 0.04f, 0.90f), 1.8f);
	DrawRect(FLinearColor(0.95f, 0.70f, 0.04f, 0.95f), MapX + 1.0f, MapY + 1.0f, MapW - 2.0f, 22.0f);
	DrawText(TEXT("İSTANBUL HARİTA & DENİZ RADARI"), FLinearColor(0.02f, 0.02f, 0.02f), MapX + 8.0f, MapY + 4.0f, Font, 0.78f);

	// Draw Blue Bosphorus & Golden Horn Water Zones clipped inside radar bounds
	auto DrawClippedWorldRect = [&](float MinWX, float MaxWX, float MinWY, float MaxWY, const FLinearColor& Col)
	{
		const FVector2D P0 = WorldToMap(FVector2D(MinWX, MinWY));
		const FVector2D P1 = WorldToMap(FVector2D(MaxWX, MaxWY));
		const float RX0 = FMath::Clamp(FMath::Min(P0.X, P1.X), MapX + 2.0f, MapX + MapW - 2.0f);
		const float RY0 = FMath::Clamp(FMath::Min(P0.Y, P1.Y), BodyY + 2.0f, MapY + MapH - 2.0f);
		const float RX1 = FMath::Clamp(FMath::Max(P0.X, P1.X), MapX + 2.0f, MapX + MapW - 2.0f);
		const float RY1 = FMath::Clamp(FMath::Max(P0.Y, P1.Y), BodyY + 2.0f, MapY + MapH - 2.0f);
		if (RX1 > RX0 && RY1 > RY0)
		{
			DrawRect(Col, RX0, RY0, RX1 - RX0, RY1 - RY0);
		}
	};

	// Golden Horn sea between Eminönü shoreline (Y=-1240) and Karaköy shore (Y=-40800) + Eastern Bosphorus channel
	const FLinearColor SeaRadarCol(0.03f, 0.30f, 0.52f, 0.88f);
	DrawClippedWorldRect(-150000.0f, 250000.0f, -40800.0f, -1240.0f, SeaRadarCol);
	DrawClippedWorldRect(38000.0f, 250000.0f, -150000.0f, -1240.0f, SeaRadarCol);

	// Draw Procedural Road Segments clipped to radar box
	if (ATestSceneBuilder* SceneBuilder = Cast<ATestSceneBuilder>(UGameplayStatics::GetActorOfClass(W, ATestSceneBuilder::StaticClass())))
	{
		for (const FIstanbulRoadSegmentSpec& Seg : SceneBuilder->GetRoadSegments())
		{
			const FVector2D A = WorldToMap(FVector2D(Seg.Start.X, Seg.Start.Y));
			const FVector2D B = WorldToMap(FVector2D(Seg.End.X, Seg.End.Y));
			if ((A.X < MapX && B.X < MapX) || (A.X > MapX + MapW && B.X > MapX + MapW) ||
				(A.Y < BodyY && B.Y < BodyY) || (A.Y > MapY + MapH && B.Y > MapY + MapH))
			{
				continue;
			}
			const FVector2D CA(FMath::Clamp(A.X, MapX + 3.0f, MapX + MapW - 3.0f), FMath::Clamp(A.Y, BodyY + 3.0f, MapY + MapH - 3.0f));
			const FVector2D CB(FMath::Clamp(B.X, MapX + 3.0f, MapX + MapW - 3.0f), FMath::Clamp(B.Y, BodyY + 3.0f, MapY + MapH - 3.0f));

			const bool bIsBridge = Seg.SegmentId.ToString().Contains(TEXT("Bridge"));
			const bool bIsCobble = (Seg.SurfaceType == TEXT("Surface_Cobblestone"));
			const FLinearColor RoadCol = bIsBridge ? FLinearColor(0.35f, 0.90f, 1.0f, 0.95f)
				: bIsCobble ? FLinearColor(0.78f, 0.62f, 0.36f, 0.90f) : FLinearColor(0.82f, 0.84f, 0.88f, 0.90f);
			DrawLine(CA.X, CA.Y, CB.X, CB.Y, RoadCol, FMath::Clamp(Seg.WidthM * 0.16f, 1.4f, 3.2f));
		}
	}

	// Draw Moving Bosphorus Ferries (Cyan blips)
	for (TActorIterator<AIstanbulFerry> It(W); It; ++It)
	{
		const FVector2D FP = WorldToMap(FVector2D(It->GetActorLocation().X, It->GetActorLocation().Y));
		if (FP.X >= MapX + 4.0f && FP.X <= MapX + MapW - 4.0f && FP.Y >= BodyY + 4.0f && FP.Y <= MapY + MapH - 4.0f)
		{
			DrawRect(FLinearColor(0.15f, 0.95f, 0.98f, 1.0f), FP.X - 3.0f, FP.Y - 3.0f, 6.0f, 6.0f);
		}
	}

	// Draw Waving Hailing Passengers (Pulsing Yellow Dots)
	const float Pulse = 0.65f + 0.35f * FMath::Sin(Now * 5.0f);
	for (TActorIterator<AIstanbulPedestrian> It(W); It; ++It)
	{
		if (It->GetRole() == EPedestrianRole::HailingPassenger && !It->IsBoarded())
		{
			const FVector2D PP = WorldToMap(FVector2D(It->GetActorLocation().X, It->GetActorLocation().Y));
			if (PP.X >= MapX + 4.0f && PP.X <= MapX + MapW - 4.0f && PP.Y >= BodyY + 4.0f && PP.Y <= MapY + MapH - 4.0f)
			{
				DrawRect(FLinearColor(1.0f, 0.88f, 0.08f, Pulse), PP.X - 3.0f, PP.Y - 3.0f, 6.0f, 6.0f);
			}
		}
	}

	// Draw Landmark Labels when inside or near radar viewport
	struct FLandmarkPin { const TCHAR* Name; FVector2D WorldXY; FLinearColor Col; };
	static const FLandmarkPin Landmarks[5] = {
		{ TEXT("GALATA KÖP."), FVector2D( 8800.0f, -18000.0f), FLinearColor(0.55f, 0.95f, 1.0f) },
		{ TEXT("KIZ KULESİ"),  FVector2D(24000.0f, -14500.0f), FLinearColor(1.00f, 0.85f, 0.35f) },
		{ TEXT("GALATA KUL."), FVector2D(22000.0f, -50900.0f), FLinearColor(0.95f, 0.80f, 0.55f) },
		{ TEXT("SULTANAHMET"), FVector2D(15200.0f,   4200.0f), FLinearColor(0.95f, 0.88f, 0.70f) },
		{ TEXT("BOĞAZ KÖP."),  FVector2D(108000.0f,  4000.0f), FLinearColor(1.00f, 0.45f, 0.35f) }
	};
	for (const FLandmarkPin& LM : Landmarks)
	{
		const FVector2D LP = WorldToMap(LM.WorldXY);
		if (LP.X >= MapX + 4.0f && LP.X <= MapX + MapW - 58.0f && LP.Y >= BodyY + 4.0f && LP.Y <= MapY + MapH - 14.0f)
		{
			DrawText(LM.Name, LM.Col, LP.X, LP.Y, Font, 0.65f);
		}
	}

	// Draw Player Taxi Position + Forward Heading Arrow at Center
	const float Rad = FMath::DegreesToRadians(PlayerYawDeg);
	const FVector2D Fwd2D(FMath::Cos(Rad), FMath::Sin(Rad));
	const FVector2D Side2D(-Fwd2D.Y, Fwd2D.X);
	const FVector2D Tip = FVector2D(CenterX, CenterY) + Fwd2D * 11.0f;
	const FVector2D TailL = FVector2D(CenterX, CenterY) - Fwd2D * 6.0f + Side2D * 6.0f;
	const FVector2D TailR = FVector2D(CenterX, CenterY) - Fwd2D * 6.0f - Side2D * 6.0f;
	DrawLine(TailL.X, TailL.Y, Tip.X, Tip.Y, FLinearColor(1.0f, 0.86f, 0.08f), 2.5f);
	DrawLine(TailR.X, TailR.Y, Tip.X, Tip.Y, FLinearColor(1.0f, 0.86f, 0.08f), 2.5f);
	DrawRect(FLinearColor(1.0f, 0.25f, 0.15f), CenterX - 2.5f, CenterY - 2.5f, 5.0f, 5.0f);
}
