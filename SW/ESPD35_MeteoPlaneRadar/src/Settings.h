// =============================================================================
//  ESPD35_MeteoPlaneRadar - ulozeni nastaveni do NVS (rozhrani).
//  Vychozi poloha je DEFAULT_LAT / DEFAULT_LON v Config.h.
//
//  Poznamka k 0.4.0: z sesti hodnot se to rozrostlo na dvacet a webova
//  konfigurace (0.5.0) je bude potrebovat vsechny najednou. Misto rozsypani
//  dvaceti klicu Preferences po celem kodu je vsechno tady a da se jednim
//  volanim serializovat do JSON - stranka, zaloha i import pak ctou tentyz
//  zdroj. Jednotlive klice v NVS zustavaji samostatne (levne castecne zapisy).
//
//  KLICE V NVS SE NEPREJMENOVAVAJI. Deska po aktualizaci z 0.3.0 musi najit
//  svou polohu, jas i rozsahy tam, kde je nechala. Klic "bl" se nove chape
//  jako DENNI uroven jasu - stavajici hodnota tak zustava v platnosti.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"   // DEFAULT_LAT / DEFAULT_LON, BRIGHT_*, ALT_*

void   Settings_Begin();

// --- Poloha -----------------------------------------------------------------
double Settings_Lat();
double Settings_Lon();
bool   Settings_HasLocation();
void   Settings_SetLocation(double lat, double lon);

// --- Jas --------------------------------------------------------------------
//
// Nove dve urovne. Settings_Backlight() vraci tu, ktera prave plati, takze
// VSICHNI dosavadni volajici funguji beze zmeny a o nocnim rezimu vedet
// nemusi. Settings_SetBacklight() nastavuje AKTIVNI uroven - kdo si prijasnuje
// v noci, nastavuje nocni, coz je presne to, co tim myslel.
uint8_t Settings_Backlight();
void    Settings_SetBacklight(uint8_t pct);

uint8_t Settings_BrightDay();
uint8_t Settings_BrightNight();
void    Settings_SetBrightDay(uint8_t pct);
void    Settings_SetBrightNight(uint8_t pct);

// Automatika podle slunce. Casy vychodu a zapadu prijdou v teze odpovedi jako
// predpoved, takze prepinani nestoji zadny pozadavek navic (viz NightMode).
bool    Settings_NightAuto();
void    Settings_SetNightAuto(bool on);
int8_t  Settings_NightOffsetMin();          // +/- minut kolem vychodu a zapadu
void    Settings_SetNightOffsetMin(int8_t m);

// Ktera uroven prave plati. Nastavuje NightMode, cte vsechno, co kresli.
// Neuklada se do NVS - po restartu se dopocita znovu.
bool    Settings_IsNight();
void    Settings_SetNight(bool night);

// --- Jednotky ---------------------------------------------------------------
// false = letecke (ft/kt), true = metricke (m/kmh).
bool    Settings_MetricUnits();
void    Settings_SetMetricUnits(bool metric);

// --- Filtry letadel ---------------------------------------------------------
//
// Vychozi hodnoty nefiltruji nic. Vyskove pasmo je ve STOPACH bez ohledu na
// zvolene jednotky - je to hodnota z ADS-B, ne udaj k zobrazeni.
uint16_t Settings_AltMinFt();
uint16_t Settings_AltMaxFt();
void     Settings_SetAltRangeFt(uint16_t lo, uint16_t hi);

// Skryt letadla, ktera nehlasi volaci znak (typicky vojenske a soukrome).
bool     Settings_OnlyWithCallsign();
void     Settings_SetOnlyWithCallsign(bool on);

// Zvyraznit letadlo s nouzovym kodem odpovidace (7500 / 7600 / 7700).
bool     Settings_SquawkAlert();
void     Settings_SetSquawkAlert(bool on);

// Hlidany volaci znak - zvyrazni se, jakmile se objevi v datech. "" = nic.
// Porovnava se na ZACATEK znaku, takze "CSA" chyti vsechny lety CSA.
const char* Settings_WatchCallsign();
void        Settings_SetWatchCallsign(const char* s);

// --- Obrazovky --------------------------------------------------------------
//
// idx je SCREEN_CLOCK_I .. SCREEN_MIX_I. Nastaveni je vzdy zapnute -
// bez nej by se nedalo dostat zpet k webu, kdyby nekdo vypnul vsechno ostatni.
bool    Settings_ScreenEnabled(uint8_t idx);
void    Settings_SetScreenEnabled(uint8_t idx, bool on);
uint8_t Settings_EnabledCount();            // pocet zapnutych datovych obrazovek

// --- Meteoradar -------------------------------------------------------------
// Legenda dBZ / mm/h v levem hornim rohu. Kdo stupnici zna, vypnutim uvidi
// vic uzemi - zmizi i misto, ktere si legenda rezervovala pro popisky mest.
bool    Settings_MeteoLegend();
void    Settings_SetMeteoLegend(bool on);

// --- Energie ----------------------------------------------------------------
// Prirazka k burzovni cene v Kc/MWh (distribuce, regulovane slozky, marze) a
// DPH v procentech. Obe 0 = displej ukazuje cistou burzovni cenu.
uint16_t Settings_PriceFee();
void     Settings_SetPriceFee(uint16_t czkPerMwh);
uint8_t  Settings_PriceVat();
void     Settings_SetPriceVat(uint8_t pct);

// Zeme pro obrazovku vyroby: dvoupismenny kod energy-charts ("cz", "sk",
// "at"...) nebo "eu" pro celou Evropu.
const char* Settings_MixCountry();
void        Settings_SetMixCountry(const char* code);

// Automaticke stridani obrazovek v SEKUNDACH. 0 = vypnuto.
uint16_t Settings_AutoRotateSec();
void     Settings_SetAutoRotateSec(uint16_t s);

// --- Vzhled hodin -----------------------------------------------------------
uint8_t  Settings_SecondsStyle();           // SEC_STYLE_*
void     Settings_SetSecondsStyle(uint8_t s);
uint16_t Settings_ClockColor();             // RGB565
void     Settings_SetClockColor(uint16_t c);
uint16_t Settings_SecondsColor();
void     Settings_SetSecondsColor(uint16_t c);

// --- Stav UI (odlozeny zapis) -----------------------------------------------
//
// Zapisuje se ODLOZENE (viz Settings_Tick). Puvodni verze zapisovala do flash
// pri KAZDE zmene rozsahu, takze rychle prejizdeni prstem primo opotrebovavalo
// NVS. Nove se zmeny akumuluji a zapisou az po chvili klidu.
uint8_t Settings_PlaneRange();
void    Settings_SetPlaneRange(uint8_t idx);
uint8_t Settings_MeteoRange();
void    Settings_SetMeteoRange(uint8_t idx);
uint8_t Settings_Screen();
void    Settings_SetScreen(uint8_t idx);

// Ktery svetovy smer (azimut ve stupnich, 0..359) je NAHORE na radaru letadel.
// 0 = sever nahoru, 90 = divam se na vychod. Zadava se smer pohledu, ne
// "o kolik mapu otocit" - to je jina velicina a plete se (viz CHANGELOG).
uint16_t Settings_TopBearing();
void     Settings_SetTopBearing(uint16_t deg);

// --- Prihlasovaci udaje k WiFi ----------------------------------------------
//
// Firmware si je vlastni sam, misto aby je nechal na WiFiManageru: pristupovy
// bod musi zustat nahore, dokud uzivatel sit opravdu nezada, a portal musi
// mluvit cesky - obojí vyzaduje, aby udaje patrily nam (viz WiFiPortal.h).
// Az WIFI_SLOTS siti (0.8.0). Slot 0 drzi puvodni klice, takze aktualizovana
// deska svou sit nezapomene.
const char* Settings_WifiSsid(uint8_t slot = 0);
const char* Settings_WifiPass(uint8_t slot = 0);
bool        Settings_HasWifi();                 // aspon jeden slot vyplneny
// Z portalu: zapamatovat si sit (zname SSID = nove heslo, jinak na prvni misto).
void        Settings_SetWifi(const char* ssid, const char* pass);
// Z webu: zapsat jeden slot presne. Prazdne ssid slot vymaze; pass == nullptr
// ponecha ulozene heslo (stranka ho nikdy nevidi).
void        Settings_SetWifiSlot(uint8_t slot, const char* ssid, const char* pass);
void        Settings_ClearWifi();

// --- Heslo spravce ----------------------------------------------------------
//
// Chrani aktualizaci firmwaru, tovarni reset a import konfigurace.
// Ulozeno v citelne podobe, a to si zaslouzi vysvetleni: stranka /update se
// overuje pres HTTP Basic, kde knihovne musite predat skutecne heslo
// k porovnani - digest tam pouzit nejde. Hashovat kopii v NVS, kdyz vedle ni
// lezi tataz hodnota v otevrene podobe, by bylo divadlo. Ulozi se tedy jednou,
// poctive, a nikdy neopusti desku: neni v konfiguracnim JSON, neni v zaloze
// a stranka se dozvi jen to, JESTLI je nejake nastavene.
//
// Je to ochrana pred domacnosti, ne pred utocnikem na siti. Kdo precte flash,
// precte i heslo.
bool        Settings_HasAdminPassword();
void        Settings_SetAdminPassword(const char* plain);   // "" = ochrana pryc
bool        Settings_CheckAdminPassword(const char* plain);
const char* Settings_AdminPassword();                       // pro HTTP Basic

// --- Serializace ------------------------------------------------------------
// Vsechno krome hesla a udaju k WiFi. Pouzije web, zaloha i import.
void Settings_ToJson(JsonObject out);
// Aplikuje, co v JSON je; chybejici klice si nechavaji soucasnou hodnotu.
// Vraci true, kdyz se aspon jedna hodnota zmenila.
bool Settings_FromJson(JsonObjectConst in);

// Volat jednou za loop(). Zapise cekajici zmeny UI stavu do NVS po kratke
// dobe klidu, aby jeden swipe neznamenal jeden zapis do flash.
void Settings_Tick();

void Settings_ClearAll();

// =============================================================================
//  Funkce prevzate z petus/MeteoPlaneRadar 0.8.0 - 0.8.5
// =============================================================================

// --- Meteoradar: CHMU, nebo CHMU masked --------------------------------------
uint8_t Settings_RadarSource();            // RADAR_SRC_CHMU / RADAR_SRC_CHMU_MASKED
void    Settings_SetRadarSource(uint8_t src);

// --- Pismo na displeji (CLOCK_FONT_*) ----------------------------------------
uint8_t Settings_ClockFont();
void    Settings_SetClockFont(uint8_t f);

// Jazyk displeje i webu: LANG_CS / LANG_SK (Config.h).
uint8_t Settings_Lang();
void    Settings_SetLang(uint8_t l);

// --- Barvy vysek letadel -----------------------------------------------------
// ALT_BANDS_N hranic ve STOPACH (vzestupne) a o jednu barvu vic: pasmo 0 je pod
// prvni hranici, posledni nad posledni.
uint16_t Settings_AltBandFt(uint8_t i);              // i = 0..ALT_BANDS_N-1
bool     Settings_SetAltBandsFt(const uint16_t* b);  // ALT_BANDS_N hodnot; mimo poradi = false
uint16_t Settings_AltColor(uint8_t band);            // band = 0..ALT_BANDS_N
void     Settings_SetAltColor(uint8_t band, uint16_t c);

// --- Hlidani bourky ----------------------------------------------------------
// Se StormOnly je meteoradar ve stridani jen tehdy, kdyz je do StormKm odraz
// aspon StormDbz.
bool     Settings_StormOnly();
void     Settings_SetStormOnly(bool on);
uint8_t  Settings_StormDbz();
void     Settings_SetStormDbz(uint8_t dbz);
uint16_t Settings_StormKm();
void     Settings_SetStormKm(uint16_t km);

// --- Letadlo poblic a vyhled z okna -----------------------------------------
// S PlaneAlert se obrazovka Letadla sama nestrida, ale na PlaneAlertSec sekund
// naskoci, kdyz se letadlo priblizi na PlaneAlertKm - nebo proleti vyhledem
// z okna, ktery ma prednost i pred blizsim letadlem.
bool     Settings_PlaneAlert();
void     Settings_SetPlaneAlert(bool on);
uint16_t Settings_PlaneAlertKm();
void     Settings_SetPlaneAlertKm(uint16_t km);
uint16_t Settings_PlaneAlertSec();
void     Settings_SetPlaneAlertSec(uint16_t s);
bool     Settings_WindowOn();
void     Settings_SetWindowOn(bool on);
uint16_t Settings_WindowAz();              // azimut stredu okna = Settings_TopBearing()
uint16_t Settings_WindowWidth();           // cela sirka vyhledu ve stupnich
void     Settings_SetWindowWidth(uint16_t deg);
uint16_t Settings_WindowKm();
void     Settings_SetWindowKm(uint16_t km);

// --- Home Assistant ----------------------------------------------------------
bool     Settings_HaEnabled();
void     Settings_SetHaEnabled(bool on);
