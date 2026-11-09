#include "Taximeter/TaximeterDisplayWidget.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Internationalization/StringTableRegistry.h"

TSharedRef<SWidget> UTaximeterDisplayWidget::RebuildWidget()
{
	// If no Blueprint child populated the WidgetTree, build a crisp digital taximeter layout in C++
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MeterBg"));
		Bg->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f));
		Bg->SetPadding(FMargin(12.0f, 6.0f));
		WidgetTree->RootWidget = Bg;

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MeterVBox"));
		Bg->SetContent(VBox);

		FallbackStateLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StateLabel"));
		FSlateFontInfo StateFont = FallbackStateLabel->GetFont();
		StateFont.Size = 16;
		FallbackStateLabel->SetFont(StateFont);
		FallbackStateLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.95f, 0.4f)));
		FallbackStateLabel->SetText(FText::FromString(TEXT("BOŞ | Sarı Taksi")));
		VBox->AddChildToVerticalBox(FallbackStateLabel);

		FallbackFareLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FareLabel"));
		FSlateFontInfo FareFont = FallbackFareLabel->GetFont();
		FareFont.Size = 36;
		FallbackFareLabel->SetFont(FareFont);
		FallbackFareLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.78f, 0.1f)));
		FallbackFareLabel->SetText(FText::FromString(TEXT("0,00 TL")));
		VBox->AddChildToVerticalBox(FallbackFareLabel);
	}

	return Super::RebuildWidget();
}

void UTaximeterDisplayWidget::BindToTaximeter(UTaximeterComponent* InMeter)
{
	if (Meter)
	{
		Meter->OnFareUpdated.RemoveDynamic(this, &UTaximeterDisplayWidget::HandleFare);
		Meter->OnStateChanged.RemoveDynamic(this, &UTaximeterDisplayWidget::HandleState);
	}
	Meter = InMeter;
	if (Meter)
	{
		Meter->OnFareUpdated.AddDynamic(this, &UTaximeterDisplayWidget::HandleFare);
		Meter->OnStateChanged.AddDynamic(this, &UTaximeterDisplayWidget::HandleState);
	}
	Refresh();
}

void UTaximeterDisplayWidget::NativeDestruct()
{
	BindToTaximeter(nullptr);
	Super::NativeDestruct();
}

void UTaximeterDisplayWidget::HandleFare(int32) { Refresh(); }
void UTaximeterDisplayWidget::HandleState(ETaximeterState, FGameplayTag) { Refresh(); }

void UTaximeterDisplayWidget::Refresh()
{
	if (!Meter) return;
	const ETaximeterState S = Meter->GetState();
	const TCHAR* Key = S == ETaximeterState::Hired ? TEXT("State_Hired") : S == ETaximeterState::Paused ? TEXT("State_Paused") : TEXT("State_Free");
	FText StateText = FText::FromStringTable(TEXT("ST_Taximeter"), Key);
	if (StateText.IsEmpty() || StateText.ToString().Contains(TEXT("MISSING")))
	{
		StateText = FText::FromString(S == ETaximeterState::Hired ? TEXT("DOLU") : S == ETaximeterState::Paused ? TEXT("BEKLEMEDE") : TEXT("BOŞ"));
	}

	const FText FareText = UTaximeterComponent::FormatKurus(Meter->GetDisplayedFareKurus());

	if (FallbackStateLabel)
	{
		const FString Seg = Meter->GetActiveTariff() ? Meter->GetActiveTariff()->SegmentName.ToString() : TEXT("Sarı Taksi");
		const FString WaitBadge = (S == ETaximeterState::Hired && Meter->IsBillingWaitTime()) ? TEXT(" [BEKLEME]") : TEXT("");
		FallbackStateLabel->SetText(FText::FromString(FString::Printf(TEXT("%s | %s%s"), *StateText.ToString(), *Seg, *WaitBadge)));
		const FLinearColor StateCol = S == ETaximeterState::Hired
			? FLinearColor(1.0f, 0.28f, 0.2f)
			: S == ETaximeterState::Paused ? FLinearColor(1.0f, 0.75f, 0.2f) : FLinearColor(0.2f, 0.95f, 0.4f);
		FallbackStateLabel->SetColorAndOpacity(FSlateColor(StateCol));
	}
	if (FallbackFareLabel)
	{
		FallbackFareLabel->SetText(FText::FromString(FString::Printf(TEXT("%s TL"), *FareText.ToString())));
	}

	OnDisplayUpdated(FareText, StateText, S, Meter->IsBillingWaitTime());
}
