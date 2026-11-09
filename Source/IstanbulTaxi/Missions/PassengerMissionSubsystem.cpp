#include "Missions/PassengerMissionSubsystem.h"
#include "Missions/PassengerInteractionComponent.h"
#include "Missions/TaxiRidePolicyDataAsset.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Core/IstanbulTaxiSettings.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "IstanbulTaxi.h"

void UPassengerMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Policy = UIstanbulTaxiSettings::Get()->DefaultRidePolicy.LoadSynchronous();
	if (!Policy)
	{
		Policy = GetDefault<UTaxiRidePolicyDataAsset>();
	}
}

void UPassengerMissionSubsystem::RegisterTaximeter(UTaximeterComponent* Meter)
{
	if (!Meter || Taximeter == Meter) return;
	UnregisterTaximeter(Taximeter);
	Taximeter = Meter;
	Taximeter->OnStateChanged.AddDynamic(this, &UPassengerMissionSubsystem::HandleMeterStateChanged);
}

void UPassengerMissionSubsystem::UnregisterTaximeter(UTaximeterComponent* Meter)
{
	if (Meter && Taximeter == Meter)
	{
		Taximeter->OnStateChanged.RemoveDynamic(this, &UPassengerMissionSubsystem::HandleMeterStateChanged);
		Taximeter = nullptr;
		if (Phase == ETaxiRidePhase::ToPickup || Phase == ETaxiRidePhase::Onboard)
		{
			CancelRide();
		}
	}
}

bool UPassengerMissionSubsystem::StartRide(FTaxiRideRequest Request)
{
	if (Phase == ETaxiRidePhase::ToPickup || Phase == ETaxiRidePhase::Onboard || !Taximeter)
	{
		return false;
	}
	if (!Request.RideId.IsValid()) Request.RideId = FGuid::NewGuid();
	if (Request.OptimalDistanceKm <= 0.0f)
	{
		Request.OptimalDistanceKm = FVector::Dist2D(Request.PickupLocation, Request.DropoffLocation) / 100000.0f
			* Policy->StraightLineToRoadFactor;
	}

	ActiveRide = Request;
	StopTimer = 0.0f;
	OnboardTime = 0.0f;
	bMeterStartedThisRide = false;
	bMeterComplaintRaised = false;
	SetPhase(ETaxiRidePhase::ToPickup);
	return true;
}

bool UPassengerMissionSubsystem::StartDebugRide(FVector PickupOffset, FVector DropoffOffset)
{
	if (!Taximeter || !Taximeter->GetOwner()) return false;
	static int32 RouteCycle = 0;
	const int32 RouteIdx = RouteCycle % 3;
	++RouteCycle;

	FTaxiRideRequest R;
	if (RouteIdx == 0)
	{
		R.PickupName = FText::FromString(TEXT("Eminönü İETT Durağı (Sahil)"));
		R.DropoffName = FText::FromString(TEXT("Cihangir / Galata Tepesi (Yokuş Üstü)"));
		R.PickupLocation = FVector(3000.0f, 800.0f, 5.0f);
		R.DropoffLocation = FVector(11800.0f, 7600.0f, 1180.0f);
	}
	else if (RouteIdx == 1)
	{
		R.PickupName = FText::FromString(TEXT("Cihangir Tepesi Camii Meydanı"));
		R.DropoffName = FText::FromString(TEXT("15 Temmuz Köprüsü Girişi (Beşiktaş)"));
		R.PickupLocation = FVector(11800.0f, 7600.0f, 1180.0f);
		R.DropoffLocation = FVector(21500.0f, 600.0f, 5.0f);
	}
	else
	{
		R.PickupName = FText::FromString(TEXT("Kabataş Sahil Durağı"));
		R.DropoffName = FText::FromString(TEXT("Anadolu Yakası (Köprü Geçişli)"));
		R.PickupLocation = FVector(15500.0f, 800.0f, 5.0f);
		R.DropoffLocation = FVector(28500.0f, 500.0f, 5.0f);
	}
	return StartRide(R);
}

void UPassengerMissionSubsystem::CancelRide()
{
	if (Phase == ETaxiRidePhase::ToPickup || Phase == ETaxiRidePhase::Onboard)
	{
		SetPhase(ETaxiRidePhase::Cancelled);
		Phase = ETaxiRidePhase::None;
	}
}

FVector UPassengerMissionSubsystem::GetCurrentObjectiveLocation() const
{
	return Phase == ETaxiRidePhase::ToPickup ? ActiveRide.PickupLocation
		: Phase == ETaxiRidePhase::Onboard ? ActiveRide.DropoffLocation
		: FVector::ZeroVector;
}

void UPassengerMissionSubsystem::Tick(float DeltaTime)
{
	if (!Taximeter || (Phase != ETaxiRidePhase::ToPickup && Phase != ETaxiRidePhase::Onboard))
	{
		return;
	}

	if (Phase == ETaxiRidePhase::Onboard)
	{
		OnboardTime += DeltaTime;
		if (!bMeterStartedThisRide && !bMeterComplaintRaised && OnboardTime > Policy->MeterGraceSeconds)
		{
			bMeterComplaintRaised = true;
			OnPassengerRemark.Broadcast(ActiveRide, IstanbulTaxiTags::Ride_Note_MeterNotStarted);
		}
	}

	const FVector Target = GetCurrentObjectiveLocation();
	StopTimer = IsTaxiStoppedNear(Target) ? StopTimer + DeltaTime : 0.0f;
	if (StopTimer < Policy->BoardingSeconds)
	{
		return;
	}
	StopTimer = 0.0f;

	if (Phase == ETaxiRidePhase::ToPickup)
	{
		// Meter already running when they get in counts as started.
		bMeterStartedThisRide = Taximeter->IsRunning();
		SetPhase(ETaxiRidePhase::Onboard);
	}
	else
	{
		FinishRide();
	}
}

bool UPassengerMissionSubsystem::IsTaxiStoppedNear(const FVector& Location) const
{
	const AActor* Taxi = Taximeter->GetOwner();
	if (!Taxi) return false;
	const float SpeedKmh = Taxi->GetVelocity().Size() * 0.036f;
	return SpeedKmh <= Policy->StopSpeedKmh
		&& FVector::Dist2D(Taxi->GetActorLocation(), Location) <= Policy->StopRadius;
}

void UPassengerMissionSubsystem::HandleMeterStateChanged(ETaximeterState NewState, FGameplayTag)
{
	if (Phase == ETaxiRidePhase::Onboard && NewState == ETaximeterState::Hired)
	{
		bMeterStartedThisRide = true;
	}
}

void UPassengerMissionSubsystem::FinishRide()
{
	const FTaxiFareReceipt Receipt = Taximeter->IsRunning() ? Taximeter->EndHire() : FTaxiFareReceipt();
	const FTaxiRideResult Result = SettleFare(Receipt);

	SetPhase(ETaxiRidePhase::Completed);
	OnRideCompleted.Broadcast(Result);
	UE_LOG(LogIstanbulTaxi, Log, TEXT("Ride done: paid %s + tip %s TL, %d★, route ratio %.2f"),
		*UTaximeterComponent::FormatKurus(Result.PaidFareKurus).ToString(),
		*UTaximeterComponent::FormatKurus(Result.TipKurus).ToString(), Result.Rating, Result.RouteRatio);
	Phase = ETaxiRidePhase::None;
}

FTaxiRideResult UPassengerMissionSubsystem::SettleFare(const FTaxiFareReceipt& Receipt) const
{
	FTaxiRideResult Res;
	Res.RideId = ActiveRide.RideId;
	Res.Receipt = Receipt;
	Res.OptimalDistanceKm = ActiveRide.OptimalDistanceKm;

	const UTaximeterTariffDataAsset* Tariff = Taximeter->GetActiveTariff();
	Res.ExpectedFareKurus = Tariff->EstimateFareKurus(Res.OptimalDistanceKm) + Receipt.TollsKurus;
	int32 Rating = 5;

	if (!Receipt.bValid || !bMeterStartedThisRide)
	{
		// Meter off (or started late): passenger pays what they believe is fair and is annoyed.
		Res.Notes.AddTag(IstanbulTaxiTags::Ride_Note_MeterNotStarted);
		Rating -= 1;
	}

	if (Receipt.bValid)
	{
		Res.RouteRatio = Receipt.DistanceKm / FMath::Max(Res.OptimalDistanceKm, 0.1f);
		Res.PaidFareKurus = Receipt.TotalKurus;

		if (Res.RouteRatio > Policy->DisputeRouteRatio)
		{
			Res.Notes.AddTag(IstanbulTaxiTags::Ride_Note_Disputed);
			Res.PaidFareKurus = FMath::Min(Receipt.TotalKurus, FMath::RoundToInt(Res.ExpectedFareKurus * Policy->DisputePayCap));
			Rating -= 2;
		}
		else if (Res.RouteRatio > Policy->SuspiciousRouteRatio)
		{
			Res.Notes.AddTag(IstanbulTaxiTags::Ride_Note_LongRoute);
			Rating -= 1;
		}
	}
	else
	{
		Res.PaidFareKurus = Res.ExpectedFareKurus;
	}

	Res.Rating = FMath::Clamp(Rating, 1, 5);

	// "Üstü kalsın" tip: round up to the next note.
	const float TipChance = Res.Rating >= 5 ? Policy->TipChanceAtFiveStars : Res.Rating == 4 ? Policy->TipChanceAtFourStars : 0.0f;
	if (Policy->TipRoundUpKurus > 0 && FMath::FRand() < TipChance)
	{
		const int32 Step = Policy->TipRoundUpKurus;
		const int32 Rounded = ((Res.PaidFareKurus + Step - 1) / Step) * Step;
		Res.TipKurus = (Rounded == Res.PaidFareKurus) ? Step : Rounded - Res.PaidFareKurus;
		Res.Notes.AddTag(IstanbulTaxiTags::Ride_Note_Tipped);
	}

	// Apply Passenger Personality, Comfort & Patience evaluation from UPassengerInteractionComponent
	if (Taximeter && Taximeter->GetOwner())
	{
		if (const UPassengerInteractionComponent* Interaction = Taximeter->GetOwner()->FindComponentByClass<UPassengerInteractionComponent>())
		{
			Interaction->EvaluateRidePerformance(Res.PaidFareKurus, Res.Rating, Res.TipKurus);
			if (Res.TipKurus > 0)
			{
				Res.Notes.AddTag(IstanbulTaxiTags::Ride_Note_Tipped);
			}
		}
	}
	return Res;
}

void UPassengerMissionSubsystem::SetPhase(ETaxiRidePhase NewPhase)
{
	Phase = NewPhase;
	OnRidePhaseChanged.Broadcast(ActiveRide, Phase);
}
