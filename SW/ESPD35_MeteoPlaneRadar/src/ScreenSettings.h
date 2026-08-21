// =============================================================================
//  ESPD35_MeteoPlaneRadar - obrazovka Nastaveni (rozhrani).
//
//  Dvousloupcove rozvrzeni pro 480x320:
//    vlevo  - jas (posuvnik), orientace mapy (- / hodnota / +), jednotky, poloha
//    vpravo - stav site, tlacitko WiFi + poloha, tlacitko Firmware update
//
//  Tlacitka sem prisla z praveho panelu obrazovky letadel, kde se tisnila
//  v pruhu sirokem 116 px. Panel je diky tomu cely pro detail letadla.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenSettings_Enter();
bool ScreenSettings_Tick();     // vraci true = prekreslit
void ScreenSettings_Draw();

// Kratky dotyk. Vraci true, kdyz je potreba prekreslit.
bool ScreenSettings_HandleTap(int x, int y);

// Lezi bod na nekterem ovladacim prvku?
//
// Pouziva to hlavni smycka: dlouhy stisk normalne prepina obrazovku, ale kdyz
// padne na tlacitko, provede se radeji akce tlacitka. Bez toho by pomalejsi
// stisk (nad 0,5 s) na "Firmware update" misto toho preskocil na jinou
// obrazovku - to je presne ta zaludnost, kterou uzivatel nepozna a nada.
// Volne misto pro prepinani dlouhym stiskem zustava nahore (titulek) a dole.
bool ScreenSettings_HitsControl(int x, int y);

// Zadost o zapomenuti ulozene WiFi site (obsluha v .ino - vyvesi pristupovy
// bod a nakresli QR kod).
//
// Od 0.5.0 uz neni co "spoustet": blokujici portal zanikl spolu s WiFiManagerem
// a aktualizace firmwaru se dela z prohlizece na /update. Z puvodnich dvou
// tlacitek v pravem sloupci tak zbylo jedno a misto druheho je tam adresa
// webove konfigurace.
bool ScreenSettings_WantsWifiReset();
void ScreenSettings_ClearWifiReset();

// Zadost o tovarni reset (smazani WiFi + nastaveni, pak restart).
//
// ESPD-3.5 nema zadne tlacitko, takze se reset nedá vyvolat drzenim pri startu
// jako u desek s BOOT. Tlacitko je proto primo na teto obrazovce - a protoze
// dotykem se da trefit i omylem, vyzaduje DVE klepnuti: prvni ho "natahne",
// druhe (do RESET_CONFIRM_MS) reset provede.
bool ScreenSettings_WantsReset();
void ScreenSettings_ClearReset();
