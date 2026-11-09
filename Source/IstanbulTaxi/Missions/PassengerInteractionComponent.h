#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Missions/PassengerMissionSubsystem.h"
#include "PassengerInteractionComponent.generated.h"

class ATaxiPawn;

UENUM(BlueprintType)
enum class EPassengerArchetypeType : uint8
{
	RushedCommuter UMETA(DisplayName = "Aceleci Beyaz Yakalı"),
	PickyAuntie    UMETA(DisplayName = "Titiz / Huysuz Teyze"),
	ForeignTourist UMETA(DisplayName = "Yabancı Turist"),
	ChattyLocal    UMETA(DisplayName = "Muhabbetçi Esnaf / Öğrenci")
};

UENUM(BlueprintType)
enum class EPassengerLiveRequestType : uint8
{
	None,
	DialogueChoice,   // Answered with [1], [2], [3] on a 60% compact keyboard
	CloseWindowDraft, // "Cereyan yaptı!" -> Press [V] within 15s to close window
	OpenWindowFresh   // "İçerisi havasız kaldı!" -> Press [V] within 15s to open window
};

USTRUCT(BlueprintType)
struct ISTANBULTAXI_API FPassengerDialogueOption
{
	GENERATED_BODY()

	/** Shown on HUD, e.g. "[1] Sahilden basıp gidelim abla, manzaralı olur." */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FString OptionText;

	/** Passenger's reply when this option is chosen. */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FString PassengerReply;

	/** Delta applied to ComfortScore (0..100). */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	float ComfortDelta = 5.0f;

	/** Delta applied to PatienceScore (0..100). */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	float PatienceDelta = 8.0f;
};

/**
 * Deep Passenger-Driver Interaction Component (attached to ATaxiPawn).
 * Manages:
 *  - 4 distinct Istanbul passenger archetypes (Aceleci Beyaz Yakalı, Titiz Teyze, Yabancı Turist, Muhabbetçi Esnaf)
 *  - Live [1]/[2]/[3] route & small-talk dialogue choices (60% compact keyboard friendly, no F-keys)
 *  -Timed 15-second Window / Draft requests ([V] Cam Aç/Kapat)
 *  - Real-time Patience (Sabır 0..100) and Comfort (Konfor 0..100) meters reacting to G-forces,
 *    cobblestone speeding, potholes, collisions, and traffic speed
 *  - Dynamic Tip (Bahşiş) & Star Rating (1..5) modifiers consumed by UPassengerMissionSubsystem
 */
UCLASS(ClassGroup = (IstanbulTaxi), meta = (BlueprintSpawnableComponent))
class ISTANBULTAXI_API UPassengerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPassengerInteractionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Player Input Actions (60% Compact Keyboard: [1], [2], [3]) ----
	UFUNCTION(BlueprintCallable, Category = "Passenger")
	void SelectDialogueChoice(int32 ChoiceIndexOneBased);

	/** Called by ATaxiPawn when a collision or hard pothole jolt happens. */
	void NotifyHardJoltOrCollision(float Severity01, bool bIsCollision);

	// ---- Queries for HUD & Mission Settlement ----
	UFUNCTION(BlueprintPure, Category = "Passenger") bool IsPassengerOnboard() const { return bPassengerOnboard; }
	UFUNCTION(BlueprintPure, Category = "Passenger") EPassengerArchetypeType GetArchetype() const { return CurrentArchetype; }
	UFUNCTION(BlueprintPure, Category = "Passenger") FString GetArchetypeDisplayName() const;
	UFUNCTION(BlueprintPure, Category = "Passenger") FString GetArchetypeTraitSummary() const;
	UFUNCTION(BlueprintPure, Category = "Passenger") float GetComfortScore() const { return ComfortScore; }
	UFUNCTION(BlueprintPure, Category = "Passenger") float GetPatienceScore() const { return PatienceScore; }
	UFUNCTION(BlueprintPure, Category = "Passenger") FString GetCurrentSpeechBubble() const { return ActiveSpeechText; }
	UFUNCTION(BlueprintPure, Category = "Passenger") float GetSpeechTimeRemaining() const { return SpeechBubbleTimer; }

	UFUNCTION(BlueprintPure, Category = "Passenger") EPassengerLiveRequestType GetActiveRequestType() const { return ActiveRequestType; }
	UFUNCTION(BlueprintPure, Category = "Passenger") FString GetActiveRequestPrompt() const { return ActiveRequestPrompt; }
	UFUNCTION(BlueprintPure, Category = "Passenger") float GetRequestTimeRemaining() const { return RequestTimer; }
	UFUNCTION(BlueprintPure, Category = "Passenger") const TArray<FPassengerDialogueOption>& GetDialogueOptions() const { return CurrentOptions; }

	/** Calculates final star adjustment (-3..+1) and extra personality tip (kuruş) at drop-off. */
	void EvaluateRidePerformance(int32 BaseFareKurus, int32& InOutRating, int32& InOutTipKurus) const;

private:
	UFUNCTION()
	void HandleRidePhaseChanged(const FTaxiRideRequest& Ride, ETaxiRidePhase NewPhase);

	void StartArchetypeSession(EPassengerArchetypeType InArchetype);
	void TriggerOpeningRouteQuestion();
	void TriggerMidRideSmallTalkOrWindowRequest();
	void TriggerWindowRequest(bool bWantClosed);
	void ResolveWindowRequestSuccess();
	void FailActiveRequest();
	void SayLine(const FString& Line, float Duration = 6.5f);

	EPassengerArchetypeType CurrentArchetype = EPassengerArchetypeType::RushedCommuter;
	int32 NextArchetypeCycle = 0;
	bool bPassengerOnboard = false;

	float ComfortScore = 92.0f;
	float PatienceScore = 90.0f;
	int32 FulfilledRequestsCount = 0;
	int32 FailedRequestsCount = 0;
	float ScenicBosphorusBonusTime = 0.0f;

	// Live speech bubble
	FString ActiveSpeechText;
	float SpeechBubbleTimer = 0.0f;

	// Active interactive request ([1]/[2]/[3] or [V] within 15s)
	EPassengerLiveRequestType ActiveRequestType = EPassengerLiveRequestType::None;
	FString ActiveRequestPrompt;
	float RequestTimer = 0.0f;
	TArray<FPassengerDialogueOption> CurrentOptions;

	// Telemetry timers
	float LastSpeedKmh = 0.0f;
	float ComplaintCooldown = 0.0f;
	float MidRideEventTimer = 14.0f;
	bool bMidRideEventFired = false;
	bool bSecondEventFired = false;
};
