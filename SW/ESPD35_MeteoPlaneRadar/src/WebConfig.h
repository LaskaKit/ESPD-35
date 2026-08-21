// =============================================================================
//  ESPD35_MeteoPlaneRadar - konfiguracni web server (rozhrani).
//
//  Jeden HTTP server ve dvou rolich, proto jsou spolu:
//
//    PRISTUPOVY BOD (jeste nejsou ulozene udaje k WiFi). Deska vyvesi otevrenou
//    sit a captive portal. Drzi ho, DOKUD nekdo sit opravdu nezada - zadny
//    timeout, protoze bez WiFi nema deska co delat. Stara verze to po trech
//    minutach vzdala a vratila radar bez dat, coz nikomu nepomohlo.
//
//    PRIPOJENO. Tentyz server bezi trvale na domaci siti a obsluhuje uplne
//    nastaveni na http://espd35meteoradar.local/ (nebo na IP adrese). Taky bez
//    casovace: stranka, kterou musite jit nejdriv zapnout na zarizeni, je
//    stranka, kterou prestanete pouzivat.
//
//  Aktualizace firmwaru je nove tady, na /update - samostatny AP jen kvuli
//  nahravani firmwaru a jeho obrazovka tim padem zanikly (soubor OTA.cpp).
//
//  LICENCNI POZNAMKA: nahravani obsluhuje trida Update z ESP32 core, ne
//  ElegantOTA. Ta je pod AGPL-3.0 (s placenou Pro variantou) a u zarizeni,
//  ktere obsluhuje webovou stranku po siti, je to presne ten pripad, kdy se
//  licence uplatni. Update.h je soucast core, ktery uz stejne pouzivame,
//  takze nahrada nepridava zadnou zavislost - jen ubira jednu.
// =============================================================================
#pragma once
#include <Arduino.h>

// Spusti (nebo prepne) server pro aktualni roli WiFi.
void WebConfig_Begin(bool apMode);

// Obslouzit server a DNS captive portalu. Volat z loop().
void WebConfig_Loop();

// Probiha nahravani firmwaru? Hlavni smycka pak prestane kreslit i stahovat,
// aby zapis do flash dostal sbernici pro sebe.
bool WebConfig_UpdateBusy();

// Z portalu prisly udaje k siti a chce se pokus o pripojeni.
bool WebConfig_WantsWifiConnect();
void WebConfig_ClearWifiConnect();

// Uzivatel si vyzadal restart, nebo probehla obnova ze zalohy.
//
// Bezna zmena nastaveni restart NEVYZADUJE - viz poznamka u handlePostConfig().
bool WebConfig_WantsRestart();

// Zmenila se poloha. Obrazovky si podle toho maji zahodit, co drzi pro tu
// starou. Vraci a zaroven maze.
bool WebConfig_TakeLocationChanged();

// Uzivatel si vyzadal tovarni reset ze stranky.
bool WebConfig_WantsFactoryReset();

// --- Dalkove ovladani -------------------------------------------------------
//
// Stranka umi ovladat displej, aniz by se nekdo dotkl skla. Pozadavky se tu
// ZARADI DO FRONTY a provede je loop() - ze stejneho duvodu jako u dotyku:
// prepnout obrazovku nebo prekreslit zevnitr obsluhy HTTP pozadavku znamena
// vlezt do kresliciho kodu uprostred snimku.
//
// Kazde volani vrati cekajici pozadavek a smaze ho.
int WebConfig_TakeScreen();       // index obrazovky, nebo -1 kdyz nic neceka
int WebConfig_TakeScreenStep();   // -1 / +1, nebo 0
int WebConfig_TakeRangeStep();    // -1 / +1, nebo 0

// Stav automatickeho stridani obrazovek pro stavovou stranku: interval
// v sekundach a kolik sekund jeste trva pauza po doteku. Bez toho neslo
// poznat, jestli je stridani vypnute, nebo jen pozastavene.
void WebConfig_SetRotateInfo(uint16_t secs, uint32_t pauseLeftSec);

// Kresli obrazovku prubehu aktualizace. Definovano v WebConfig.cpp, vola se
// z obsluhy nahravani.
void WebConfig_DrawOtaProgress(int pct);
