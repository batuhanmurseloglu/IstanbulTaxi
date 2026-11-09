#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Taximeter/TaximeterComponent.h"
#include "TaximeterDisplayWidget.generated.h"

class UTextBlock;

/**
 * C++ base (and standalone fallback) for the 3D interior dashboard taximeter widget.
 * If used directly without a Blueprint child, WidgetTree builds a clean digital display automatically.
 * If subclassed in Blueprint (WBP_TaximeterDisplay), OnDisplayUpdated also fires for custom visuals.
 */
UCLASS()
class ISTANBULTAXI_API UTaximeterDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Taximeter")
	void BindToTaximeter(UTaximeterComponent* InMeter);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

	/** FareText "71,94", StateText "BOŞ/DOLU/BEKLEMEDE", bWaitBilling lights the clock icon. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Taximeter")
	void OnDisplayUpdated(const FText& FareText, const FText& StateText, ETaximeterState State, bool bWaitBilling);

	UPROPERTY(BlueprintReadOnly, Category = "Taximeter")
	TObjectPtr<UTaximeterComponent> Meter;

private:
	UFUNCTION() void HandleFare(int32 Kurus);
	UFUNCTION() void HandleState(ETaximeterState NewState, FGameplayTag Tag);
	void Refresh();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> FallbackStateLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FallbackFareLabel;
};
