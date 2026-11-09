#include "Taximeter/TaximeterComponent.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Missions/PassengerMissionSubsystem.h"
#include "Core/IstanbulTaxiSettings.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "IstanbulTaxi.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

UTaximeterComponent::UTaximeterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UTaximeterComponent::BeginPlay()
{
	Super::BeginPlay();

	// Build the 4 official 2026 UKOME tariff presets in memory so CycleTariffPreset() and fallback always work
	auto MakeTariff = [this](const FText& Name, int32 Open, int32 Km, int32 WaitHr, int32 MinFare)
	{
		UTaximeterTariffDataAsset* T = NewObject<UTaximeterTariffDataAsset>(this);
		T->SegmentName = Name;
		T->OpeningFareKurus = Open;
		T->PerKmKurus = Km;
		T->HourlyWaitKurus = WaitHr;
		T->MinimumFareKurus = MinFare;
		BuiltInTariffs.Add(T);
		return T;
	};
	MakeTariff(FText::FromString(TEXT("Sarı Taksi")),    7194,  4792,  59890, 23000);
	MakeTariff(FText::FromString(TEXT("Turkuaz Taksi")), 8273,  5510,  68872, 26500);
	MakeTariff(FText::FromString(TEXT("8+1 Taksi")),     9352,  6229,  77856, 30000);
	MakeTariff(FText::FromString(TEXT("Siyah Taksi")),  12230,  8146, 101812, 40000);

	ActiveTariff = Tariff;
	if (!ActiveTariff)
	{
		ActiveTariff = UIstanbulTaxiSettings::Get()->DefaultTariff.LoadSynchronous();
	}
	if (!ActiveTariff && BuiltInTariffs.Num() > 0)
	{
		ActiveTariff = BuiltInTariffs[0];
		UE_LOG(LogIstanbulTaxi, Log, TEXT("Taximeter: using built-in UKOME Sarı Taksi preset."));
	}

	if (UPassengerMissionSubsystem* Missions = GetWorld()->GetSubsystem<UPassengerMissionSubsystem>())
	{
		Missions->RegisterTaximeter(this);
	}
	SetState(ETaximeterState::Free);
}

void UTaximeterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
	{
		if (UPassengerMissionSubsystem* Missions = World->GetSubsystem<UPassengerMissionSubsystem>())
		{
			Missions->UnregisterTaximeter(this);
		}
	}
	Super::EndPlay(Reason);
}

void UTaximeterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (State != ETaximeterState::Hired || !GetOwner())
	{
		return;
	}

	DurationSeconds += DeltaTime;

	const FVector Vel = GetOwner()->GetVelocity();
	const float SpeedCms = FVector(Vel.X, Vel.Y, 0.0f).Size(); // horizontal; bridges' slope is negligible
	const float SpeedKmh = SpeedCms * 0.036f;
	const float StepKm = (SpeedKmh < StandstillSpeedKmh) ? 0.0f : SpeedCms * DeltaTime / 100000.0f;
	DistanceKm += StepKm;

	// Real taksimetre logic: below threshold bill time, above it bill distance (never both).
	bBillingWait = SpeedKmh < ActiveTariff->GetWaitThresholdKmh();
	if (bBillingWait)
	{
		WaitingSeconds += DeltaTime;
		WaitingChargeKurus += ActiveTariff->HourlyWaitKurus * (DeltaTime / 3600.0);
	}
	else
	{
		DistanceChargeKurus += ActiveTariff->PerKmKurus * (double)StepKm;
	}

	BroadcastFareIfChanged();
}

bool UTaximeterComponent::StartHire()
{
	if (State != ETaximeterState::Free) return false;
	ResetAccumulators();
	SetState(ETaximeterState::Hired);
	BroadcastFareIfChanged();
	return true;
}

bool UTaximeterComponent::TogglePause()
{
	if (State == ETaximeterState::Hired)  { SetState(ETaximeterState::Paused); return true; }
	if (State == ETaximeterState::Paused) { SetState(ETaximeterState::Hired);  return true; }
	return false;
}

FTaxiFareReceipt UTaximeterComponent::EndHire()
{
	FTaxiFareReceipt R;
	if (State == ETaximeterState::Free)
	{
		return R; // bValid = false
	}

	R.bValid = true;
	R.OpeningKurus = ActiveTariff->OpeningFareKurus;
	R.DistanceKurus = FMath::RoundToInt(DistanceChargeKurus);
	R.WaitingKurus = FMath::RoundToInt(WaitingChargeKurus);
	R.TollsKurus = TollsKurus;
	const int32 MeterPart = R.OpeningKurus + R.DistanceKurus + R.WaitingKurus;
	R.MinimumTopUpKurus = FMath::Max(0, ActiveTariff->MinimumFareKurus - MeterPart);
	R.TotalKurus = MeterPart + R.MinimumTopUpKurus + R.TollsKurus;
	R.DistanceKm = DistanceKm;
	R.WaitingSeconds = WaitingSeconds;
	R.DurationSeconds = DurationSeconds;
	R.TollCrossings = TollCrossings;

	SetState(ETaximeterState::Free);
	OnHireEnded.Broadcast(R);
	UE_LOG(LogIstanbulTaxi, Log, TEXT("Taximeter receipt: %s TL (%.2f km, %.0f s wait, tolls %s)"),
		*FormatKurus(R.TotalKurus).ToString(), R.DistanceKm, R.WaitingSeconds, *FormatKurus(R.TollsKurus).ToString());
	return R;
}

void UTaximeterComponent::ToggleMeter()
{
	if (State == ETaximeterState::Free) StartHire();
	else EndHire();
}

void UTaximeterComponent::CycleTariffPreset()
{
	if (BuiltInTariffs.Num() == 0) return;
	BuiltInTariffIndex = (BuiltInTariffIndex + 1) % BuiltInTariffs.Num();
	ActiveTariff = BuiltInTariffs[BuiltInTariffIndex];
	LastBroadcastFare = -1;
	BroadcastFareIfChanged();
	UE_LOG(LogIstanbulTaxi, Log, TEXT("Switched UKOME Tariff -> %s (Open: %s TL, Km: %s TL, Min: %s TL)"),
		*ActiveTariff->SegmentName.ToString(),
		*FormatKurus(ActiveTariff->OpeningFareKurus).ToString(),
		*FormatKurus(ActiveTariff->PerKmKurus).ToString(),
		*FormatKurus(ActiveTariff->MinimumFareKurus).ToString());
}

void UTaximeterComponent::ChargeToll(FGameplayTag Crossing, int32 FeeKurus)
{
	const bool bAddToFare = IsRunning();
	if (bAddToFare)
	{
		TollsKurus += FeeKurus;
		TollCrossings.Add(Crossing);
		BroadcastFareIfChanged();
	}
	OnTollCharged.Broadcast(Crossing, FeeKurus, bAddToFare);
}

FGameplayTag UTaximeterComponent::GetStateTag() const
{
	switch (State)
	{
	case ETaximeterState::Hired:  return IstanbulTaxiTags::Taximeter_Hired;
	case ETaximeterState::Paused: return IstanbulTaxiTags::Taximeter_Paused;
	default:                      return IstanbulTaxiTags::Taximeter_Free;
	}
}

int32 UTaximeterComponent::GetCurrentFareKurus() const
{
	if (State == ETaximeterState::Free || !ActiveTariff) return 0;
	return ActiveTariff->OpeningFareKurus + FMath::RoundToInt(DistanceChargeKurus + WaitingChargeKurus) + TollsKurus;
}

int32 UTaximeterComponent::GetDisplayedFareKurus() const
{
	const int32 Raw = GetCurrentFareKurus();
	const int32 Step = ActiveTariff ? ActiveTariff->DisplayStepKurus : 0;
	return Step > 0 ? (Raw / Step) * Step : Raw;
}

FText UTaximeterComponent::FormatKurus(int32 Kurus)
{
	FNumberFormattingOptions Opts;
	Opts.MinimumFractionalDigits = 2;
	Opts.MaximumFractionalDigits = 2;
	Opts.UseGrouping = true;
	const FCulturePtr Tr = FInternationalization::Get().GetCulture(TEXT("tr"));
	return FText::AsNumber(Kurus / 100.0, &Opts, Tr);
}

void UTaximeterComponent::SetState(ETaximeterState NewState)
{
	State = NewState;
	OnStateChanged.Broadcast(State, GetStateTag());
	BroadcastFareIfChanged();
}

void UTaximeterComponent::ResetAccumulators()
{
	DistanceChargeKurus = WaitingChargeKurus = 0.0;
	TollsKurus = 0;
	DistanceKm = WaitingSeconds = DurationSeconds = 0.0f;
	bBillingWait = false;
	TollCrossings.Reset();
}

void UTaximeterComponent::BroadcastFareIfChanged()
{
	const int32 Shown = GetDisplayedFareKurus();
	if (Shown != LastBroadcastFare)
	{
		LastBroadcastFare = Shown;
		OnFareUpdated.Broadcast(Shown);
	}
}
