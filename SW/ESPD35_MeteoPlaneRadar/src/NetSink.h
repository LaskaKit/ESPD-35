// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Dva zpusoby cteni tela HTTP, oba pres HTTPClient::writeToStream().
//
//  Proc to existuje
//  ----------------
//  Telo se driv cetlo rucne z http.getStreamPtr(). Ten ukazatel je SYROVY
//  socket: NEODSTRANUJE Transfer-Encoding: chunked. Kdyz adsb.fi presla za
//  proxy, ktera odpovida chunked, zustaly hexadecimalni velikosti bloku primo
//  v tele - to pak zacinalo "2f8a\r\n{"ac":[", ArduinoJson precetl uvodni hex
//  jako cislo, vratil Ok a pole "ac" v dokumentu vubec nebylo. Chybu nenahlasil
//  nikdo, protoze z pohledu parseru se nic nepokazilo.
//
//  writeToStream() chunked dekoduje spravne, jen potrebuje, kam bajty ulozit -1
//  a getString() by znamenal cele telo na hromade, ze ktere si bere pamet TLS.
//  Obe tridy, ktere praci odvedou, jsou schovane v NetSink.cpp; ven je nikdo
//  nepotrebuje. Nesou navic dve pojistky, ktere writeToStream() nema: strop
//  kapacity, ktery se ohlasi misto tiseho uriznuti, a casovy rozpocet, protoze
//  writeToStreamDataBlock() se toci na delay(1) bez vlastniho timeoutu.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

class HTTPClient;

// Nacte uz odeslany GET do buf. Vraci pocet bajtu, nebo -1 pri jakemkoli
// selhani (kazde se vypise s danym tagem). Telo zustane ukoncene nulou.
// Pro odpovedi, ktere je potreba mit cele: JSON, PNG.
long Net_ReadBody(HTTPClient& http, uint8_t* buf, size_t cap, const char* tag,
                  void (*poll)() = nullptr);

// Nejdelsi retezec, ktery smi scanner hledat. Cokoli kratsiho dorazi cele
// aspon v jednom okne.
#define NET_SCAN_MAX_TOKEN 64

// Projde uz odeslany GET scannerem, aniz by se telo kamkoli ukladalo - pro
// odpovedi, ktere se prohledavaji, ne uchovavaji. Vypis adresare CHMU je presne
// ten pripad: nema rozumnou horni mez, takze jakykoli buffer vyhrazeny pro nej
// je strop, na ktery se jednou narazi. cb dostane nulou ukoncene okno, ktere
// nese konec toho predchoziho, takze retezec kratsi nez NET_SCAN_MAX_TOKEN
// dorazi vzdy cely - a protoze se okna prekryvaji, muze dorazit dvakrat.
// Scanner to musi snest. Vraci pocet bajtu, nebo -1 pri selhani.
typedef void (*NetScanFn)(const char* window, void* user);
long Net_ScanBody(HTTPClient& http, NetScanFn cb, void* user, const char* tag,
                  void (*poll)() = nullptr);
