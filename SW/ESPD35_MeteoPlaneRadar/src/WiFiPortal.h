// =============================================================================
//  ESPD35_MeteoPlaneRadar - WiFi: pripojit se, nebo drzet pristupovy bod,
//  dokud nam nekdo sit nezada.
//
//  WiFiManager je od verze 0.5.0 pryc. Duvody dva:
//
//    1) BLOKOVAL CELOU SMYCKU. Po dobu portalu se muselo uspavat watchdog
//       (Watchdog_Suspend), displej zamrzl a nic se nestahovalo. Deska byla
//       nekolik minut bez dohledu - presne v okamziku, kdy s ni uzivatel
//       pracuje.
//    2) Jeho portal je anglicky a prelozit se neda.
//
//  Nahradou je obycejny pristupovy bod plus vlastni webova stranka projektu
//  (WebConfig). Portal tak mluvi cesky jako zbytek zarizeni a hlavni smycka
//  bezi po celou dobu dal.
//
//  Pristupovy bod NEMA TIMEOUT. Bez site si zarizeni stejne nema co stahnout,
//  takze neni kam se vracet - puvodni tri minuty jen shodily uzivatele na
//  prazdny radar, coz nikomu nepomohlo.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // AP_SSID / AP_PASSWORD

#define PORTAL_IP "192.168.4.1"

// Pripoji se ulozenymi udaji, nebo vyvesi pristupovy bod. Blokuje pouze po
// dobu prvniho pokusu o pripojeni; vsechno dalsi resi WiFi_Loop().
// Web server bezi, at to dopadne jakkoli.
void WiFi_Begin();

// Udrzba spojeni a prevzeti udaju z portalu. Volat z loop().
void WiFi_Loop();

bool   WiFi_IsConnected();
bool   WiFi_IsAP();
String WiFi_SSID();
String WiFi_IP();

// Zapomenout sit a vratit se k pristupovemu bodu.
void WiFi_Reset();

// Prekreslit obrazovku pristupoveho bodu (SSID, QR kod, adresa).
void WiFi_DrawApScreen();
