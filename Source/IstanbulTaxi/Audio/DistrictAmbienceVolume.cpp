#include "Audio/DistrictAmbienceVolume.h"
#include "Audio/AudioAmbienceSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Pawn.h"

ADistrictAmbienceVolume::ADistrictAmbienceVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(25000.0f, 25000.0f, 5000.0f)); // 500m x 500m default district block
	Bounds->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Bounds;
}

void ADistrictAmbienceVolume::BeginPlay()
{
	Super::BeginPlay();
	Bounds->OnComponentBeginOverlap.AddDynamic(this, &ADistrictAmbienceVolume::OnBeginOverlap);
	Bounds->OnComponentEndOverlap.AddDynamic(this, &ADistrictAmbienceVolume::OnEndOverlap);
}

void ADistrictAmbienceVolume::OnBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	const APawn* P = Cast<APawn>(OtherActor);
	if (P && P->IsPlayerControlled() && AmbienceProfile)
	{
		if (UAudioAmbienceSubsystem* Sub = GetWorld()->GetSubsystem<UAudioAmbienceSubsystem>())
		{
			Sub->PushAmbienceVolume(this);
		}
	}
}

void ADistrictAmbienceVolume::OnEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	const APawn* P = Cast<APawn>(OtherActor);
	if (P && P->IsPlayerControlled())
	{
		if (UAudioAmbienceSubsystem* Sub = GetWorld()->GetSubsystem<UAudioAmbienceSubsystem>())
		{
			Sub->PopAmbienceVolume(this);
		}
	}
}

// ---------------- AWaterfrontSplineActor ----------------

AWaterfrontSplineActor::AWaterfrontSplineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ShorelineSpline = CreateDefaultSubobject<USplineComponent>(TEXT("ShorelineSpline"));
	RootComponent = ShorelineSpline;
}

void AWaterfrontSplineActor::BeginPlay()
{
	Super::BeginPlay();
	if (UAudioAmbienceSubsystem* Sub = GetWorld()->GetSubsystem<UAudioAmbienceSubsystem>())
	{
		Sub->RegisterWaterfrontSpline(this);
	}
}

void AWaterfrontSplineActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld())
	{
		if (UAudioAmbienceSubsystem* Sub = W->GetSubsystem<UAudioAmbienceSubsystem>())
		{
			Sub->UnregisterWaterfrontSpline(this);
		}
	}
	Super::EndPlay(Reason);
}

float AWaterfrontSplineActor::GetDistanceToLocationCm(const FVector& WorldLocation) const
{
	if (!ShorelineSpline) return BIG_NUMBER;
	const FVector Closest = ShorelineSpline->FindLocationClosestToWorldLocation(WorldLocation, ESplineCoordinateSpace::World);
	return FVector::Dist2D(WorldLocation, Closest);
}
