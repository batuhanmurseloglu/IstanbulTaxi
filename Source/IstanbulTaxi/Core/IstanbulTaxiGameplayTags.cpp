#include "Core/IstanbulTaxiGameplayTags.h"

namespace IstanbulTaxiTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Default,         "Surface.Default",         "Unknown / unmapped surface");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Asphalt,         "Surface.Asphalt",         "Fresh asphalt");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_PatchedAsphalt,  "Surface.PatchedAsphalt",  "Patched / worn asphalt");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Cobblestone,     "Surface.Cobblestone",     "Arnavut kaldırımı");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_TramRail,        "Surface.TramRail",        "Raised tram rails (T1, Nostalgic)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Pothole,         "Surface.Pothole",         "Çukur");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Wet_Asphalt,     "Surface.Wet.Asphalt",     "Puddled asphalt");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Wet_Cobblestone, "Surface.Wet.Cobblestone", "Wet cobblestone");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Surface_Wet_TramRail,    "Surface.Wet.TramRail",    "Wet tram rail");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_Dawn,        "Time.Dawn",        "Sabah ezanı / dawn");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_MorningRush, "Time.MorningRush", "Morning rush");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_Midday,      "Time.Midday",      "Midday");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_Ikindi,      "Time.Ikindi",      "İkindi");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_GoldenHour,  "Time.GoldenHour",  "Golden hour");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_Dusk,        "Time.Dusk",        "Akşam / dusk");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_Night,       "Time.Night",       "Night");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Time_LateNight,   "Time.LateNight",   "Late night");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Prayer_Sabah,  "Prayer.Sabah",  "İmsak / Sabah ezanı (Fajr)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Prayer_Gunes,  "Prayer.Gunes",  "Güneş doğuşu (Sunrise)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Prayer_Ogle,   "Prayer.Ogle",   "Öğle ezanı (Dhuhr)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Prayer_Ikindi, "Prayer.Ikindi", "İkindi ezanı (Asr)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Prayer_Aksam,  "Prayer.Aksam",  "Akşam ezanı (Maghrib)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Prayer_Yatsi,  "Prayer.Yatsi",  "Yatsı ezanı (Isha)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Clear,    "Weather.Clear",    "Açık");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Overcast, "Weather.Overcast", "Kapalı");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Lodos,    "Weather.Lodos",    "Warm SW wind");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Poyraz,   "Weather.Poyraz",   "Cold NE wind");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Rain,     "Weather.Rain",     "Yağmur");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Storm,    "Weather.Storm",    "Fırtına");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Fog,      "Weather.Fog",      "Sis");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weather_Snow,     "Weather.Snow",     "Kar");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Taximeter_Free,   "Taximeter.Free",   "Boş");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Taximeter_Hired,  "Taximeter.Hired",  "Dolu");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Taximeter_Paused, "Taximeter.Paused", "Beklemede");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crossing_Bridge_15Temmuz,         "Crossing.Bridge.15Temmuz",         "15 Temmuz Şehitler Köprüsü");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crossing_Bridge_FSM,              "Crossing.Bridge.FSM",              "Fatih Sultan Mehmet Köprüsü");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crossing_Bridge_YavuzSultanSelim, "Crossing.Bridge.YavuzSultanSelim", "Yavuz Sultan Selim Köprüsü");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crossing_Tunnel_Avrasya,          "Crossing.Tunnel.Avrasya",          "Avrasya Tüneli");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ride_Note_MeterNotStarted, "Ride.Note.MeterNotStarted", "Driver didn't start the taximeter");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ride_Note_LongRoute,       "Ride.Note.LongRoute",       "Route noticeably longer than optimal");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ride_Note_Disputed,        "Ride.Note.Disputed",        "Passenger refused full fare");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ride_Note_Tipped,          "Ride.Note.Tipped",          "Üstü kalsın");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Vehicle_SurfaceImpact, "Event.Vehicle.SurfaceImpact", "Wheel hit pothole/rail edge");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Vehicle_Reset,         "Event.Vehicle.Reset",         "Vehicle was reset/flipped upright");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(District_HistoricPeninsula, "District.HistoricPeninsula", "Tarihi Yarımada");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(District_Beyoglu,           "District.Beyoglu",           "Beyoğlu");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(District_Besiktas,          "District.Besiktas",          "Beşiktaş");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(District_Bridges,           "District.Bridges",           "Köprüler");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(District_AsianSide,         "District.AsianSide",         "Anadolu Yakası");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(District_Airports,          "District.Airports",          "Havalimanları");
}
