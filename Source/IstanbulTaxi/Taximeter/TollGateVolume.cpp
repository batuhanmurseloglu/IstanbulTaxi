#include "Taximeter/TollGateVolume.h"
#include "Taximeter/TaximeterComponent.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "Core/TaxiGameClock.h"
#include "IstanbulTaxi.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"

ATollGateVolume::ATollGateVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(150.0f, 1200.0f, 300.0f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Trigger;

	TowardAsiaArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("TowardAsiaArrow"));
	TowardAsiaArrow->SetupAttachment(Trigger);
	TowardAsiaArrow->ArrowSize = 5.0f;
	TowardAsiaArrow->ArrowColor = FColor::Yellow;

	FallbackToll.DisplayName = FText::FromString(TEXT("15 Temmuz Şehitler Köprüsü"));
	FallbackToll.CrossingTag = IstanbulTaxiTags::Crossing_Bridge_15Temmuz;
	FallbackToll.FeeKurus = 5900;
	FallbackToll.ChargedDirection = ETaxiTollDirection::EuropeToAsia;
}

void ATollGateVolume::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATollGateVolume::OnOverlap);
}

void ATollGateVolume::OnOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	UTaximeterComponent* Meter = OtherActor ? OtherActor->FindComponentByClass<UTaximeterComponent>() : nullptr;
	const FTaxiTollRow* Row = Toll.IsNull() ? &FallbackToll : Toll.GetRow<FTaxiTollRow>(TEXT("TollGate"));
	if (!Row) Row = &FallbackToll;
	if (!Meter) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (const float* Last = LastChargeTime.Find(OtherActor); Last && Now - *Last < RechargeCooldown) return;

	if (Row->ChargedDirection != ETaxiTollDirection::Both)
	{
		const bool bGoingToAsia = FVector::DotProduct(OtherActor->GetVelocity(), GetActorForwardVector()) > 0.0f;
		const bool bCharged = (Row->ChargedDirection == ETaxiTollDirection::EuropeToAsia) == bGoingToAsia;
		if (!bCharged) return;
	}

	LastChargeTime.Add(OtherActor, Now);
	const int32 Fee = Row->GetFeeAtHour(FTaxiGameClock::GetHourOfDay(GetWorld()));
	Meter->ChargeToll(Row->CrossingTag, Fee);
	UE_LOG(LogIstanbulTaxi, Log, TEXT("HGS: %s charged %s TL"), *Row->DisplayName.ToString(), *UTaximeterComponent::FormatKurus(Fee).ToString());
}
