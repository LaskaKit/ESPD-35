// =============================================================================
//  ESPD35_MeteoPlaneRadar - jedno misto pro "stahni tuhle URL pres TLS".
//
//  Kazdy koncovy bod v projektu potrebuje tytez ctyri veci: kontrolu, ze je
//  dost INTERNI pameti na TLS handshake, insecure TLS klienta (certifikaty
//  nepripiname), telo nactene celé pred zpracovanim a hlavicku "Date" predanou
//  hodinam cestou kolem. Opakovat to ve tech modulech je zpusob, jak se
//  rozejdou.
//
//  ADSB.cpp si vlastni cteni tela nechava - potrebuje ho do PSRAM bufferu,
//  ktery se recykluje mezi stazenimi. Pouziva odtud aspon Net_HeapOk()
//  a Net_NoteDate().
// =============================================================================
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>

// Stahne URL a da cele telo do `out`.
//   tag - kratky nazev pro seriovy log ("ADSB", "CHMU", ...)
// Vraci false pri chybejicim pripojeni, nedostatku pameti, jinem nez 200
// nebo prazdnem tele; duvod se jednou vypise, takze volajici muze proste
// zkusit pozdeji znovu.
bool Net_GetString(const char* url, String& out, const char* tag);

// Stahne URL do bufferu dodaneho volajicim - pro binarni data (PNG snimky),
// kde by String pamet zdvojnasobil. Pri uspechu je v *outLen pocet bajtu.
//
// Neuplny prenos je CHYBA, ne castecny uspech: uriznuty PNG se dekoduje na
// smeti, takze se to musi poznat tady a ne az v dekoderu.
bool Net_GetBinary(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                   const char* tag);

// --- Relace s otevrenym spojenim -------------------------------------------
//
// TLS handshake potrebuje ~45 kB interni RAM. Sest snimku meteoradaru za sebou
// znamena sedm handshaku (index + 6 PNG) a sedmkrat tuhle spicku - a az na
// pozadi pobezi web server, nemusi ta pamet byt k dispozici.
// Mezi Begin a End se pouziva jedno spojeni. Vzdy parovat: otevrena relace
// drzi svuj buffer, dokud se nezavre.
void Net_SessionBegin();
void Net_SessionEnd();

// --- Pomucky pro moduly s vlastnim ctenim tela (ADSB.cpp) ------------------

// Je dost volne INTERNI pameti na TLS handshake? Pri nedostatku vypise duvod
// a vrati false - volajici pak poll preskoci a na displeji zustanou
// predchozi data.
bool Net_HeapOk(const char* tag);

// Nazev hlavicky, kterou je potreba nechat posbirat pres http.collectHeaders(),
// aby ji slo po pozadavku precist.
extern const char* NET_DATE_HEADER[1];

// Precte hlavicku "Date" z dokonceneho pozadavku a preda ji hodinam.
void Net_NoteDate(HTTPClient& http);

// Vola se behem dlouhych prenosu, aby watchdog zustal nakrmeny a dotyk se
// dal snimat i uprostred stahovani (viz netPoll v .ino).
void Net_SetPollFn(void (*fn)());
