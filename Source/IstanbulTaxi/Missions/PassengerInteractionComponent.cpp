#include "Missions/PassengerInteractionComponent.h"
#include "Vehicle/TaxiPawn.h"
#include "Vehicle/TaxiSurfaceComponent.h"
#include "Taximeter/TaximeterComponent.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Audio/AudioAmbienceSubsystem.h"
#include "Core/IstanbulTaxiGameplayTags.h"
#include "IstanbulTaxi.h"
#include "Engine/World.h"

UPassengerInteractionComponent::UPassengerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPassengerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* W = GetWorld())
	{
		if (UPassengerMissionSubsystem* Missions = W->GetSubsystem<UPassengerMissionSubsystem>())
		{
			Missions->OnRidePhaseChanged.AddDynamic(this, &UPassengerInteractionComponent::HandleRidePhaseChanged);
		}
	}
}

FString UPassengerInteractionComponent::GetArchetypeDisplayName() const
{
	switch (CurrentArchetype)
	{
	case EPassengerArchetypeType::RushedCommuter: return TEXT("Aceleci Beyaz Yakalı (Levent/Maslak)");
	case EPassengerArchetypeType::PickyAuntie:    return TEXT("Titiz / Huysuz Teyze (Nişantaşı)");
	case EPassengerArchetypeType::ForeignTourist: return TEXT("Yabancı Turist (Sultanahmet/Boğaz)");
	case EPassengerArchetypeType::ChattyLocal:    return TEXT("Muhabbetçi Esnaf (Eminönü Çarşı)");
	default:                                      return TEXT("Yolcu");
	}
}

FString UPassengerInteractionComponent::GetArchetypeTraitSummary() const
{
	switch (CurrentArchetype)
	{
	case EPassengerArchetypeType::RushedCommuter: return TEXT("Hız sever, dur-kalk trafikten bunalır");
	case EPassengerArchetypeType::PickyAuntie:    return TEXT("Sarsıntı, hız ve açık camdan şikayet eder");
	case EPassengerArchetypeType::ForeignTourist: return TEXT("Boğaz manzarası ve dürüst tarifeye yüksek döviz bahşişi verir");
	case EPassengerArchetypeType::ChattyLocal:    return TEXT("Sohbet ve esnaf muhabbetini sever");
	default:                                      return TEXT("");
	}
}

void UPassengerInteractionComponent::SayLine(const FString& Line, float Duration)
{
	ActiveSpeechText = Line;
	SpeechBubbleTimer = Duration;
	UE_LOG(LogIstanbulTaxi, Log, TEXT("[Yolcu - %s]: %s"), *GetArchetypeDisplayName(), *Line);
}

void UPassengerInteractionComponent::HandleRidePhaseChanged(const FTaxiRideRequest&, ETaxiRidePhase NewPhase)
{
	if (NewPhase == ETaxiRidePhase::ToPickup)
	{
		// Cycle through all 4 archetypes deterministically so the player experiences every profile!
		const EPassengerArchetypeType Chosen = static_cast<EPassengerArchetypeType>(NextArchetypeCycle % 4);
		++NextArchetypeCycle;
		CurrentArchetype = Chosen;
		bPassengerOnboard = false;
		ActiveRequestType = EPassengerLiveRequestType::None;
		SayLine(FString::Printf(TEXT("Yeni Çağrı: %s kaldırımda bekliyor."), *GetArchetypeDisplayName()), 5.0f);
	}
	else if (NewPhase == ETaxiRidePhase::Onboard)
	{
		StartArchetypeSession(CurrentArchetype);
	}
	else
	{
		bPassengerOnboard = false;
		ActiveRequestType = EPassengerLiveRequestType::None;
		CurrentOptions.Reset();
	}
}

void UPassengerInteractionComponent::StartArchetypeSession(EPassengerArchetypeType InArchetype)
{
	CurrentArchetype = InArchetype;
	bPassengerOnboard = true;
	ComfortScore = 92.0f;
	PatienceScore = 88.0f;
	FulfilledRequestsCount = 0;
	FailedRequestsCount = 0;
	ScenicBosphorusBonusTime = 0.0f;
	ComplaintCooldown = 3.0f;
	MidRideEventTimer = 15.0f;
	bMidRideEventFired = false;
	bSecondEventFired = false;

	TriggerOpeningRouteQuestion();
}

void UPassengerInteractionComponent::TriggerOpeningRouteQuestion()
{
	CurrentOptions.Reset();
	ActiveRequestType = EPassengerLiveRequestType::DialogueChoice;
	RequestTimer = 18.0f;

	auto AddOpt = [this](const FString& Text, const FString& Reply, float Comf, float Pat)
	{
		FPassengerDialogueOption O;
		O.OptionText = Text;
		O.PassengerReply = Reply;
		O.ComfortDelta = Comf;
		O.PatienceDelta = Pat;
		CurrentOptions.Add(O);
	};

	switch (CurrentArchetype)
	{
	case EPassengerArchetypeType::RushedCommuter:
		ActiveRequestPrompt = TEXT("Yolcu Sorusu ([1]/[2]/[3] ile yanıtla): \"Usta selam, köprü girişindeki toplantıya çok geç kaldım! Sahilden mi basacağız, ara sokaktan mı?\"");
		SayLine(TEXT("Usta biraz basabilir miyiz, toplantıya geç kaldım! Nereden gidiyoruz?"), 9.0f);
		AddOpt(TEXT("[1] Sahil bulvarından basıp geçelim, en hızlısı orası."),
			TEXT("Süpersin usta, ayağına sağlık! Yetişirsem bahşişin benden."), +5.0f, +12.0f);
		AddOpt(TEXT("[2] Sultanahmet ara sokaklarından kestirme deneyelim."),
			TEXT("Aman usta yokuşta dolmuşların arkasına takılmayalım da!"), 0.0f, +4.0f);
		AddOpt(TEXT("[3] Kısmetse yetişiriz be abi, İstanbul trafiği bu belli olmaz."),
			TEXT("Yapma usta ya, müdür beni bekliyor gözünü seveyim hızlan!"), -4.0f, -10.0f);
		break;

	case EPassengerArchetypeType::PickyAuntie:
		ActiveRequestPrompt = TEXT("Yolcu Sorusu ([1]/[2]/[3] ile yanıtla): \"Evladım merhaba, tansiyonum var benim. Sakın o taşlı dik yokuşlardan hızlı gitme, nereden gideceksin?\"");
		SayLine(TEXT("Oğlum yavaş ve sarsmadan sür lütfen, yüreğim ağzıma gelmesin!"), 9.0f);
		AddOpt(TEXT("[1] Merak etme teyzeciğim, sahilden pamuk gibi sarsmadan götürürüm."),
			TEXT("Hah şöyle aferin evladım, su gibi git su gibi gel."), +12.0f, +10.0f);
		AddOpt(TEXT("[2] Teyzem yukarı mahalleden geçeceğiz ama çok yavaş çıkacağım söz."),
			TEXT("İyi bari çukurlara dikkat et, belim ağrıyor zaten."), +4.0f, +5.0f);
		AddOpt(TEXT("[3] Teyze taksideyiz ambulans değil, sıkı tutun!"),
			TEXT("Aaa ne biçim konuşuyorsun sen! Şikayet ederim seni UKOME'ye!"), -14.0f, -16.0f);
		break;

	case EPassengerArchetypeType::ForeignTourist:
		ActiveRequestPrompt = TEXT("Yolcu Sorusu ([1]/[2]/[3] ile yanıtla): \"Merhaba! First time in Istanbul! Can we see the Bosphorus and the historic mosque on the way?\"");
		SayLine(TEXT("Merhaba! Boğaz manzarasını ve tarihi camiyi görebilir miyiz?"), 9.0f);
		AddOpt(TEXT("[1] Welcome! Hem Boğaz sahilini hem Sultanahmet tepesini göstereceğim size."),
			TEXT("Amazing! Thank you so much, Istanbul is gorgeous!"), +10.0f, +12.0f);
		AddOpt(TEXT("[2] Direkt sahil yolundan köprüye gidiyoruz, sağda cami görünecek."),
			TEXT("Great, I love watching the seagulls by the water!"), +6.0f, +8.0f);
		AddOpt(TEXT("[3] No English برادر, sadece taksimetre ne yazarsa o!"),
			TEXT("Oh... okay, just keep the taximeter honest please."), -6.0f, -8.0f);
		break;

	case EPassengerArchetypeType::ChattyLocal:
		ActiveRequestPrompt = TEXT("Yolcu Sorusu ([1]/[2]/[3] ile yanıtla): \"Selamünaleyküm kaptan, kolay gelsin! Nasıl gidiyor bugün işler, İstanbul trafiği açıldı mı?\"");
		SayLine(TEXT("Selamünaleyküm usta! İşler nasıl, bugün çorba kaynıyor mu?"), 9.0f);
		AddOpt(TEXT("[1] Aleykümselam abi, şükür koşturuyoruz. Sen nasılsın, çarşı ne alemde?"),
			TEXT("Vallahi bizde de aynı be kaptan, esnaflık zor ama muhabbetin yeter!"), +10.0f, +12.0f);
		AddOpt(TEXT("[2] Sağ ol abi, köprü trafiği başlamadan seni yetiştirelim bakalım."),
			TEXT("Eyvallah kaptan, sen işini bilirsin."), +6.0f, +6.0f);
		AddOpt(TEXT("[3] Abi hiç sorma, sabahtan beri direksiyon sallamaktan kafam şişti."),
			TEXT("Haklısın be usta, İstanbul'da şoförlük peygamber sabrı ister."), +4.0f, +4.0f);
		break;
	}
}

void UPassengerInteractionComponent::TriggerMidRideSmallTalkOrWindowRequest()
{
	const ATaxiPawn* Taxi = Cast<ATaxiPawn>(GetOwner());
	if (!Taxi) return;

	const bool bWindowCurrentlyOpen = (Taxi->GetWindowOpenRatio() > 0.3f);

	// Picky Auntie dislikes open windows ("Cereyan yaptı!"); Rushed Commuter / Tourist / Local often ask for fresh air or window toggle
	if (CurrentArchetype == EPassengerArchetypeType::PickyAuntie)
	{
		if (bWindowCurrentlyOpen)
		{
			TriggerWindowRequest(true); // asks to CLOSE window
		}
		else
		{
			// If already closed, she asks to open briefly then close, or initiates small talk
			TriggerWindowRequest(false);
		}
		return;
	}

	if (!bSecondEventFired)
	{
		// Trigger a 15-second Window / Draft request so the player can test [V] interaction!
		TriggerWindowRequest(bWindowCurrentlyOpen);
	}
}

void UPassengerInteractionComponent::TriggerWindowRequest(bool bWantClosed)
{
	ActiveRequestType = bWantClosed ? EPassengerLiveRequestType::CloseWindowDraft : EPassengerLiveRequestType::OpenWindowFresh;
	RequestTimer = 15.0f;
	CurrentOptions.Reset();

	if (bWantClosed)
	{
		ActiveRequestPrompt = TEXT("YOLCU İSTEĞİ (15 sn içinde [V] tuşuna basarak CAMI KAPAT): \"Oğlum camı kapat cereyan yaptı, boynum tutulacak!\"");
		SayLine(TEXT("Oğlum şu camı kapat lütfen, cereyan yaptı! [V]"), 8.0f);
	}
	else
	{
		ActiveRequestPrompt = TEXT("YOLCU İSTEĞİ (15 sn içinde [V] tuşuna basarak CAMI AÇ): \"Usta içerisi biraz basık oldu, camı açar mısın Boğaz havası gelsin?\"");
		SayLine(TEXT("Usta camı biraz açabilir misin, temiz Boğaz havası alalım! [V]"), 8.0f);
	}
}

void UPassengerInteractionComponent::SelectDialogueChoice(int32 ChoiceIndexOneBased)
{
	if (!bPassengerOnboard || ActiveRequestType != EPassengerLiveRequestType::DialogueChoice)
	{
		return;
	}
	const int32 Idx = ChoiceIndexOneBased - 1;
	if (!CurrentOptions.IsValidIndex(Idx)) return;

	const FPassengerDialogueOption Chosen = CurrentOptions[Idx];
	ComfortScore  = FMath::Clamp(ComfortScore + Chosen.ComfortDelta, 0.0f, 100.0f);
	PatienceScore = FMath::Clamp(PatienceScore + Chosen.PatienceDelta, 0.0f, 100.0f);
	if (Chosen.PatienceDelta >= 0.0f) ++FulfilledRequestsCount;
	else ++FailedRequestsCount;

	SayLine(Chosen.PassengerReply, 7.0f);
	ActiveRequestType = EPassengerLiveRequestType::None;
	ActiveRequestPrompt.Empty();
	CurrentOptions.Reset();
}

void UPassengerInteractionComponent::ResolveWindowRequestSuccess()
{
	++FulfilledRequestsCount;
	ComfortScore  = FMath::Clamp(ComfortScore + 10.0f, 0.0f, 100.0f);
	PatienceScore = FMath::Clamp(PatienceScore + 12.0f, 0.0f, 100.0f);
	SayLine(TEXT("Çok teşekkürler usta, ellerine sağlık! (+Sabır / +Bahşiş)"), 6.0f);
	ActiveRequestType = EPassengerLiveRequestType::None;
	ActiveRequestPrompt.Empty();
}

void UPassengerInteractionComponent::FailActiveRequest()
{
	++FailedRequestsCount;
	PatienceScore = FMath::Clamp(PatienceScore - 14.0f, 0.0f, 100.0f);
	ComfortScore  = FMath::Clamp(ComfortScore - 8.0f, 0.0f, 100.0f);
	SayLine(TEXT("Neyse usta duymadın herhalde... (-Sabır)"), 5.5f);
	ActiveRequestType = EPassengerLiveRequestType::None;
	ActiveRequestPrompt.Empty();
	CurrentOptions.Reset();
}

void UPassengerInteractionComponent::NotifyHardJoltOrCollision(float Severity01, bool bIsCollision)
{
	if (!bPassengerOnboard) return;

	const float PenaltyScale = (CurrentArchetype == EPassengerArchetypeType::PickyAuntie) ? 1.55f : 1.0f;
	ComfortScore  = FMath::Clamp(ComfortScore  - Severity01 * 18.0f * PenaltyScale, 0.0f, 100.0f);
	PatienceScore = FMath::Clamp(PatienceScore - Severity01 * 12.0f * PenaltyScale, 0.0f, 100.0f);

	if (ComplaintCooldown <= 0.0f)
	{
		ComplaintCooldown = 4.0f;
		if (bIsCollision)
		{
			SayLine((CurrentArchetype == EPassengerArchetypeType::PickyAuntie)
				? TEXT("Ay! Yavaş ol evladım nereye çarpıyorsun, yüreğim ağzıma geldi!")
				: TEXT("Aman usta dikkat et, vurduk arabaya!"), 5.5f);
		}
		else
		{
			SayLine(TEXT("Oof! Çukura çok sert girdik usta, böbrek taşları döküldü!"), 5.0f);
		}
	}
}

void UPassengerInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (SpeechBubbleTimer > 0.0f)
	{
		SpeechBubbleTimer = FMath::Max(0.0f, SpeechBubbleTimer - DeltaTime);
	}
	if (ComplaintCooldown > 0.0f)
	{
		ComplaintCooldown = FMath::Max(0.0f, ComplaintCooldown - DeltaTime);
	}

	if (!bPassengerOnboard) return;

	const ATaxiPawn* Taxi = Cast<ATaxiPawn>(GetOwner());
	if (!Taxi) return;

	const float SpeedKmh = FMath::Abs(Taxi->GetSpeedKmh());
	const float AccelKmhPerSec = (SpeedKmh - LastSpeedKmh) / FMath::Max(DeltaTime, 0.001f);
	LastSpeedKmh = SpeedKmh;

	// 1. Check active window/dialogue request timers
	if (ActiveRequestType != EPassengerLiveRequestType::None)
	{
		if (ActiveRequestType == EPassengerLiveRequestType::CloseWindowDraft && Taxi->GetWindowOpenRatio() < 0.1f)
		{
			ResolveWindowRequestSuccess();
		}
		else if (ActiveRequestType == EPassengerLiveRequestType::OpenWindowFresh && Taxi->GetWindowOpenRatio() > 0.85f)
		{
			ResolveWindowRequestSuccess();
		}
		else
		{
			RequestTimer -= DeltaTime;
			if (RequestTimer <= 0.0f)
			{
				FailActiveRequest();
			}
		}
	}
	else if (!bMidRideEventFired)
	{
		MidRideEventTimer -= DeltaTime;
		if (MidRideEventTimer <= 0.0f)
		{
			bMidRideEventFired = true;
			TriggerMidRideSmallTalkOrWindowRequest();
		}
	}

	// 2. Harsh braking detection
	if (AccelKmhPerSec < -48.0f)
	{
		ComfortScore = FMath::Clamp(ComfortScore - 6.0f * DeltaTime, 0.0f, 100.0f);
		if (ComplaintCooldown <= 0.0f)
		{
			ComplaintCooldown = 5.0f;
			SayLine(TEXT("Hop usta! Freni kökledin, öne yapıştık!"), 4.5f);
		}
	}

	// 3. Cobblestone speeding & Archetype-specific comfort/patience reactions
	const UTaxiSurfaceComponent* Surf = Taxi->GetSurfaceComponent();
	const bool bOnCobblestone = Surf && (Surf->GetDominantSurfaceTag() == IstanbulTaxiTags::Surface_Cobblestone);

	if (bOnCobblestone && SpeedKmh > 38.0f)
	{
		const float Mult = (CurrentArchetype == EPassengerArchetypeType::PickyAuntie) ? 5.5f : 2.2f;
		ComfortScore = FMath::Clamp(ComfortScore - Mult * DeltaTime, 0.0f, 100.0f);
		if (CurrentArchetype == EPassengerArchetypeType::PickyAuntie && ComplaintCooldown <= 0.0f)
		{
			ComplaintCooldown = 5.5f;
			SayLine(TEXT("Oğlum Arnavut kaldırımında bu kadar hızlı gidilir mi! Yavaş git yüreğim ağzıma geldi!"), 5.5f);
		}
	}

	switch (CurrentArchetype)
	{
	case EPassengerArchetypeType::RushedCommuter:
		if (SpeedKmh > 52.0f)
		{
			PatienceScore = FMath::Clamp(PatienceScore + 2.8f * DeltaTime, 0.0f, 100.0f);
		}
		else if (SpeedKmh < 10.0f)
		{
			PatienceScore = FMath::Clamp(PatienceScore - 2.4f * DeltaTime, 0.0f, 100.0f);
			if (ComplaintCooldown <= 0.0f && PatienceScore < 65.0f)
			{
				ComplaintCooldown = 8.0f;
				SayLine(TEXT("Usta dur-kalk trafiğe sıkıştık ya, başka şeritten sıyrılamaz mıyız?"), 5.0f);
			}
		}
		break;

	case EPassengerArchetypeType::PickyAuntie:
		if (SpeedKmh > 78.0f)
		{
			ComfortScore  = FMath::Clamp(ComfortScore  - 4.5f * DeltaTime, 0.0f, 100.0f);
			PatienceScore = FMath::Clamp(PatienceScore - 3.5f * DeltaTime, 0.0f, 100.0f);
			if (ComplaintCooldown <= 0.0f)
			{
				ComplaintCooldown = 6.5f;
				SayLine(TEXT("Evladım yarış pisti mi burası! Yavaş git tansiyonum çıktı!"), 5.5f);
			}
		}
		else if (SpeedKmh > 12.0f && SpeedKmh <= 52.0f && !bOnCobblestone)
		{
			ComfortScore = FMath::Clamp(ComfortScore + 1.5f * DeltaTime, 0.0f, 100.0f);
		}
		break;

	case EPassengerArchetypeType::ForeignTourist:
		if (UWorld* W = GetWorld())
		{
			if (const UAudioAmbienceSubsystem* Audio = W->GetSubsystem<UAudioAmbienceSubsystem>())
			{
				if (Audio->GetCurrentSeagullIntensity() > 0.35f || Taxi->GetActorLocation().Z > 250.0f)
				{
					ScenicBosphorusBonusTime += DeltaTime;
					ComfortScore = FMath::Clamp(ComfortScore + 1.8f * DeltaTime, 0.0f, 100.0f);
				}
			}
		}
		break;

	case EPassengerArchetypeType::ChattyLocal:
		ComfortScore = FMath::Clamp(ComfortScore + 0.8f * DeltaTime, 0.0f, 100.0f);
		break;
	}
}

void UPassengerInteractionComponent::EvaluateRidePerformance(int32 BaseFareKurus, int32& InOutRating, int32& InOutTipKurus) const
{
	const float Combined = ComfortScore * 0.5f + PatienceScore * 0.5f;

	if (Combined >= 82.0f && FailedRequestsCount == 0)
	{
		InOutRating = FMath::Clamp(InOutRating, 5, 5);
	}
	else if (Combined >= 65.0f)
	{
		InOutRating = FMath::Min(InOutRating, 4);
	}
	else if (Combined >= 45.0f)
	{
		InOutRating = FMath::Min(InOutRating, 3);
	}
	else
	{
		InOutRating = FMath::Min(InOutRating, 2);
	}

	// Personality & fulfilled request tip bonus (in kuruş)
	int32 ExtraTipKurus = FulfilledRequestsCount * 2500; // +25,00 TL per fulfilled request
	if (CurrentArchetype == EPassengerArchetypeType::ForeignTourist && ScenicBosphorusBonusTime > 4.0f)
	{
		ExtraTipKurus += 6000; // +60,00 TL FX/scenic Bosphorus tourist tip!
	}
	else if (CurrentArchetype == EPassengerArchetypeType::RushedCommuter && PatienceScore >= 80.0f)
	{
		ExtraTipKurus += 4500; // +45,00 TL fast-commute bonus
	}
	else if (CurrentArchetype == EPassengerArchetypeType::PickyAuntie && ComfortScore >= 85.0f)
	{
		ExtraTipKurus += 3500; // +35,00 TL "Al evladım çorba parası"
	}

	if (InOutRating >= 4 && Combined >= 65.0f)
	{
		InOutTipKurus += ExtraTipKurus;
	}
	else if (Combined < 50.0f)
	{
		InOutTipKurus = 0;
	}
}
