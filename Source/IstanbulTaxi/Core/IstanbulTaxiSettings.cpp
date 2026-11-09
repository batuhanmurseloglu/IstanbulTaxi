#include "Core/IstanbulTaxiSettings.h"
#include "Vehicle/TaxiSurfaceResponseDataAsset.h"
#include "Taximeter/TaximeterTariffDataAsset.h"
#include "Missions/TaxiRidePolicyDataAsset.h"
#include "Environment/TimeOfDayPresetDataAsset.h"
#include "Audio/DistrictAmbienceDataAsset.h"
#include "Engine/DataTable.h"

UIstanbulTaxiSettings::UIstanbulTaxiSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("IstanbulTaxi");
}
