#include "IstanbulTaxi.h"
#include "Modules/ModuleManager.h"
#include "Internationalization/StringTableRegistry.h"

DEFINE_LOG_CATEGORY(LogIstanbulTaxi);

class FIstanbulTaxiModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// Code-facing String Tables (Turkish source; English via Localization Dashboard).
		// Paths are relative to the project Content folder.
		LOCTABLE_FROMFILE_GAME("ST_Taximeter", "ST_Taximeter", "Localization/StringTables/ST_Taximeter.csv");
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FIstanbulTaxiModule, IstanbulTaxi, "IstanbulTaxi");
