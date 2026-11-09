#include "Core/IstanbulTaxiHUD.h"
#include "Vehicle/TaxiPawn.h"
#include "Vehicle/TaxiSurfaceComponent.h"
#include "Taximeter/TaximeterComponent.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Environment/TimeOfDaySubsystem.h"
#include "Audio/AudioAmbienceSubsystem.h"
#include "Audio/DistrictAmbienceDataAsset.h"
#include "Audio/PrayerTimeCalculator.h"
#include "Traffic/IstanbulTrafficManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
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
	DrawText(PrayerLine, FLinearColor(0.68f, 0.74f, 0.82f), 36.0f, 82.0f, Font, 0.86f);
	DrawText(TEXT("İpucu: X=+35m'den sağa dönüp Arnavut kaldırımlı Sultanahmet yokuşuna çıkabilirsiniz!"),
		FLinearColor(0.55f, 0.88f, 0.62f), 36.0f, 110.0f, Font, 0.84f);

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
	// 3. TOP-RIGHT: PASSENGER MISSION TRACKER & 3D WAYPOINT
	// =========================================================================
	if (const UPassengerMissionSubsystem* Missions = GetWorld()->GetSubsystem<UPassengerMissionSubsystem>())
	{
		const ETaxiRidePhase Phase = Missions->GetPhase();
		const float MisW = 390.0f;
		const float MisX = ScreenW - MisW - 24.0f;
		DrawBezelBox(MisX, 18.0f, MisW, 82.0f, FLinearColor(0.012f, 0.016f, 0.024f, 0.86f), FLinearColor(0.25f, 0.78f, 1.0f, 0.85f));

		if (Phase == ETaxiRidePhase::None)
		{
			DrawText(TEXT("YOLCU ÇAĞRISI BEKLENİYOR"), FLinearColor(0.78f, 0.84f, 0.92f), MisX + 14.0f, 28.0f, Font, 0.98f);
			DrawText(TEXT("[E] Tuşu: Eminönü Durağı -> Köprü Girişi Çağrısı"), FLinearColor(0.98f, 0.82f, 0.20f), MisX + 14.0f, 56.0f, Font, 0.88f);
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

			DrawText(PhaseTitle, ObjColor, MisX + 14.0f, 28.0f, Font, 0.96f);
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
	// 5. BOTTOM-LEFT: COMPACT CONTROLS BAR
	// =========================================================================
	const float HelpY = ScreenH - 96.0f;
	DrawBezelBox(20.0f, HelpY, 545.0f, 72.0f, FLinearColor(0.012f, 0.016f, 0.024f, 0.82f), FLinearColor(0.35f, 0.40f, 0.46f, 0.7f));
	DrawText(TEXT("[WASD] Sürüş  |  [Space] El Freni  |  [H] Korna  |  [C] İç/Dış Kamera  |  [R] Düzelt"),
		FLinearColor(0.92f, 0.94f, 0.98f), 32.0f, HelpY + 8.0f, Font, 0.86f);
	DrawText(TEXT("[T] Taksimetre Aç/Kapat  |  [P] Beklet  |  [Y] Tarife (Sarı/Turkuaz/8+1/Siyah)"),
		FLinearColor(0.98f, 0.82f, 0.22f), 32.0f, HelpY + 28.0f, Font, 0.86f);
	DrawText(TEXT("[E] Yolcu Görevi  |  [V] Cam Aç/Kapat  |  [N] Ezan Oku  |  [M] Saati +1.5s İlerlet"),
		FLinearColor(0.75f, 0.88f, 0.98f), 32.0f, HelpY + 48.0f, Font, 0.86f);

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
