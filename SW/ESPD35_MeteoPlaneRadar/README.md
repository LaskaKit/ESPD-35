# ESPD35 PlaneRadar + Meteoradar

Radar letadel (**adsb.fi**) a srážkový **meteoradar ČHMÚ** s animací na desce **LaskaKit ESPD‑3.5"** (ESP32‑S3, TFT ILI9488 480×320 s kapacitním dotykem).

Dvě obrazovky na jednom zařízení — letecký provoz nad tebou a přicházející srážky včetně jejich pohybu. Bez pájení, stačí deska, USB kabel a WiFi.

> Připraveno pro **[laskakit.cz](https://www.laskakit.cz)**. Projekt vychází z [petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar) (původně pro **chiptron.cz**), portováno a rozšířeno pro obdélníkový displej ESPD‑3.5.

---

## Co to umí

Dvě obrazovky, mezi kterými se přepíná **dlouhým stiskem prstu** (kdekoli na displeji).

### ✈️ Radar letadel

Data z [adsb.fi](https://adsb.fi/) — veřejná ADS‑B síť, zdarma a bez klíče. Vlastní přijímač nepotřebuješ.

- Displej rozdělený **3/4 mapa + 1/4 detail**. Střed mapy = tvoje poloha (zaměřovací kříž).
- Letadla se kreslí jako **siluety natočené podle skutečného kurzu**; stroje bez hlášeného kurzu jako kroužek.
- **Barva podle nadmořské výšky** (letová hladina) — legenda vlevo. Vlevo nahoře počet letadel, vlevo dole rozsah.
- V pravém panelu **detail nejbližšího letadla** k tvé poloze (volací znak, typ, vzdálenost, výška, rychlost, kurz, stoupání/klesání). Krátkým klepnutím na letadlo ho v detailu zafixuješ (bílý kroužek), klepnutím do prázdna zpět na automaticky nejbližší.
- Tlačítka v panelu: **přepnutí jednotek** (letecké ft/kt ↔ metrické m/km‑h) a **WiFi + poloha** (spustí konfigurační portál).
- **Rozsahy:** 10 / 25 / 50 / 100 km.
- **Stahování z adsb.fi podle rozsahu** (≈5 s do 25 km, 10 s do 50 km, 15 s na 100 km). Data se čtou jako celé tělo a parsují až kompletní, takže neúplné/uříznuté stažení už radar nevymaže — při chybě zůstane poslední platný snímek a po chybě se interval zdvojnásobí, aby API nebylo zbytečně zatěžováno.

### 🌧️ Meteoradar ČHMÚ s animací

Srážkový kompozit (maximální odrazivost MAX_Z) z [ČHMÚ OpenData](https://opendata.chmi.cz/) — nový snímek každých 5 minut.

- **Animace 6 snímků** za posledních 25 minut (−25 / −20 / −15 / −10 / −5 / nyní), 2 snímky/s, mezi cykly krátká pauza. Nahoře uprostřed **indikátor snímku** (tečky + čas).
- Pod srážkami se kreslí **obrys ČR a města**, takže je jasné, kde přesně prší.
- Legenda vlevo nahoře: **barevná škála intenzity** (dBZ / mm/h, převod Marshall‑Palmer).
- Nové snímky se stahují jen v pauze (když běží poslední snímek), aby se animace nepřerušovala.
- **Rozsahy:** 25 / 50 / 100 / 200 km.

Obě obrazovky používají **správnou geografickou projekci** — meteoradar Web Mercator (jako ČHMÚ), letadla plochou azimutální projekci se stejným měřítkem v obou osách, takže polohy sedí s obrysem a městy nezávisle na poloze i rozsahu.

---

## Hardware

| Komponenta | Popis |
| --- | --- |
| **Deska** | [LaskaKit ESPD‑3.5" ESP32‑S3 TFT ILI9488 CAP Touch **Rev. 3.2**](https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/?variantId=12161) |
| **MCU** | ESP32‑S3 (PSRAM + flash) |
| **Displej** | 3.5" TFT **ILI9488**, 480×320, SPI |
| **Dotyk** | kapacitní **FT5436** (I2C, knihovna FT6236) |
| **Krabička** | [Krabička pro ESPD‑3.5"](https://www.laskakit.cz/laskakit--krabicka-pro-espd-35/?variantId=12574) |
| **Repozitář desky** | [github.com/LaskaKit/ESPD‑35](https://github.com/LaskaKit/ESPD-35) |

Stačí deska a USB‑C kabel. Polohu není třeba zadávat — zjistí se automaticky podle IP. Pokud nesedí, je možné jí upravit ručně ve WiFi amanageru - klikni na tlačítko WiFi + Poloha (AP) a deska si vytvoří vlastní WiFi Access Point, ke kterému se připojíš a pozici upravíš.

---

## Závislosti (knihovny)

V Arduino IDE (**Nástroje → Spravovat knihovny**) nainstaluj:

| Knihovna | Autor | K čemu |
| --- | --- | --- |
| **GFX Library for Arduino** | moononournation | kreslení + off‑screen canvas (POZOR: ne Adafruit GFX) |
| **PNGdec** | Larry Bank (bitbank2) | dekódování snímků ČHMÚ |
| **ArduinoJson** (v7) | Benoit Blanchon | parsování dat z adsb.fi a ip‑api |
| **WiFiManager** | tzapu | konfigurační WiFi portál |
| **ElegantOTA** | ayushsharma82 | aktualizace firmwaru přes WiFi |
| **QRCode** | Richard Moore (ricmoo) | QR kód v portálu (přibaleno v projektu) |

> ElegantOTA se používá ve **výchozím (synchronním) režimu** nad `WebServer`
> z ESP32 core — nic se v knihovně needituje a `ESPAsyncWebServer` ani
> `AsyncTCP` nejsou potřeba.

**Dotyk** používá knihovnu **FT6236** (DustinWatts, s upraveným CHIPID/VENDID pro FT5436), kterou LaskaKit přikládá k desce.

---

## Nastavení Arduino IDE

**Nástroje →**

| Položka | Hodnota |
| --- | --- |
| Deska | ESP32S3 Dev Module |
| **PSRAM** | **OPI PSRAM** ← nutné (bez toho zůstane displej černý) |
| Flash Size | 16MB (128Mb) |
| **Partition Scheme** | **Custom** ← použije se přiložený `src/partitions.csv` |
| USB CDC On Boot | Disable |
| Upload Speed | 921600 |

Partition **Custom** je pro OTA nutná — přiložená tabulka má dvě aplikační
oblasti (2× 6 MB), aby bylo kam nahrát novou verzi. Po překladu zkontroluj
v logu, že se hlásí `of 6291456 bytes`.

Nastavení jako časová zóna, výchozí poloha, rozsahy nebo limity najdeš
pohromadě v **`src/Config.h`**. Verze firmwaru je v **`src/Version.h`**,
historie změn v **[CHANGELOG.md](CHANGELOG.md)**.

---

## Piny (Config.h)

Piny jsou v `Config.h` a jsou **předvyplněné pro ESPD‑3.5 Rev 3.2**:

```c
// Displej ILI9488 (SPI)
#define TFT_SCK   12
#define TFT_MOSI  11
#define TFT_MISO  13
#define TFT_CS    48
#define TFT_DC    47
#define TFT_RST   -1
#define TFT_BL    45
// Dotyk FT5436 (I2C)
#define I2C_SDA   42
#define I2C_SCL    2
```

> Máš‑li jinou revizi desky, zkontroluj piny podle pinoutu a ukázek v `SW/` repozitáře LaskaKit.

---

## Instalace

1. Všechny soubory dej do **jedné složky `ESPD35_MeteoPlaneRadar`** (Arduino vyžaduje, aby se složka jmenovala stejně jako hlavní `.ino`).
2. Zkontroluj `Config.h`, přelož a nahraj `ESPD35_MeteoPlaneRadar.ino`.

### První spuštění

1. Zařízení vytvoří otevřenou WiFi síť **`ESPD35-MeteoPlaneRadar`**.
2. Na displeji se ukáže **QR kód** — naskenuj telefonem, připoj se a vyber domácí WiFi (případně zadej ručně polohu lat/lon).
3. Poloha se jinak zjistí automaticky podle IP.

---

## Ovládání

**Dotyk (kapacitní):**

Obrazovky jsou tři: **Letadla → Meteoradar → Nastavení** (dokola).

| Gesto | Funkce |
| --- | --- |
| **Klepnutí na tečky dole uprostřed** | skok přímo na danou obrazovku |
| **Dlouhý stisk (> 0,5 s) v levé polovině** | předchozí obrazovka |
| **Dlouhý stisk v pravé polovině** | následující obrazovka |
| Přejetí prstem vlevo/vpravo | změna rozsahu aktivní obrazovky |
| Krátké klepnutí na letadlo | zafixování letadla v detailu (bílý kroužek) |
| Klepnutí do prázdné mapy / do panelu | zpět na automaticky nejbližší letadlo |

> **Deska nemá žádné tlačítko** — ESPD‑3.5 je celá ovládaná dotykem.

Jas, jednotky, orientace mapy, WiFi + poloha, aktualizace firmwaru a tovární
reset jsou na obrazovce **Nastavení**. Na ní dlouhý stisk *na tlačítku* provede
tlačítko, ne přepnutí obrazovky — přepíná se ve volném pruhu nahoře a dole,
nebo tečkami.

### Tovární reset

Červené tlačítko **Tovarni reset** dole na obrazovce Nastavení. Protože se
dotykem dá trefit i omylem, vyžaduje **dvě klepnutí**: první ho natáhne (změní
se na `Opravdu? Klepni`), druhé do 6 s reset provede. Klepnutí kamkoli jinam
ho zruší; když nic neuděláš, sám zhasne.

Smaže uložené WiFi údaje i nastavení (poloha, jas, jednotky, orientace, rozsahy)
a desku restartuje — pak se zase přihlásí konfiguračním portálem.

> Kdyby dotyk vůbec nefungoval, reset touhle cestou nejde. Záchranou je nahrání
> `*.merged.bin` přes USB, které přepíše i NVS.

### Orientace mapy („Nahoře")

V Nastavení se dá zvolit, **který světový směr je nahoře** na radaru letadel —
tedy směr, kterým se díváš z okna. Nastavíš `V` a letadla na displeji jsou ve
stejném směru jako ta za sklem. Osm poloh po 45°, vedle tlačítek je kompasový
náhled a po obvodu radaru značky S/V/J/Z.

Meteoradar se záměrně **neotáčí** — srážková mapa se čte severem nahoru.

---

## Aktualizace firmwaru přes WiFi (OTA)

Od verze 0.3.0 jde nový firmware nahrát bezdrátově.

1. V zařízení jdi do **Nastavení** a klepni na **Firmware update (OTA)**.
2. Zařízení vytvoří WiFi síť `ESPD35-MeteoPlaneRadar` (bez hesla) a ukáže QR kód.
3. V prohlížeči otevři **`http://192.168.4.1/update`** a nahraj soubor
   `ESPD35_MeteoPlaneRadar.ino.bin` — **ten bez `merged`**.
4. Průběh je vidět na displeji i v prohlížeči; deska se sama restartuje.

Telefon nahlásí, že síť nemá internet — to nevadí, soubor už máš stažený.
Když se aktualizace nepovede, zůstane v desce původní verze.

> ### ⚠️ Přechod z verze 0.2 a nižší
> Verze 0.3.0 mění rozdělení paměti. Poprvé je proto nutné nahrát
> `*.merged.bin` **přes USB** — bezdrátová aktualizace by neměla kam zapsat.
> Stačí to jednou.

---

## Sériový výstup (115200 Bd)

Vypisují se jen základní informace:

```
=== ESPD35_MeteoPlaneRadar v0.3.0 ===
WiFi ok, IP 192.168.1.42
GeoIP: 50.0755, 14.4378 - Prague
Letadla: 11 (8421 bajtu)
Meteoradar: 6 ramcu
```

Podrobnější ladicí výpisy (gesta dotyku, důvod zrušení výběru letadla, počet
zahozených vadných čtení dotyku) se zapínají v `src/Config.h` přepínačem
`TOUCH_DEBUG`. Pro běžný provoz ho nech na `0`.

---

## Řešení problémů

- **Displej zůstává černý** → zkontroluj **PSRAM: OPI PSRAM** v Nástrojích. To je nejčastější příčina.
- **Dotyk nereaguje / souřadnice přehozené** → ověř `I2C_SDA`/`I2C_SCL` v `Config.h`; případně přehoď `TOUCH_ROTATION` mezi 3 a 1.
- **Displej občas „sám" zruší výběr letadla** → zapni `TOUCH_DEBUG 1` a sleduj
  řádky `TOUCH: zahozeno N vadnych cteni`. Když jich je hodně, je problém na
  I2C sběrnici (rušení, dlouhé vodiče), ne v datech z adsb.fi.
- **OTA hlásí, že není kam zapsat** → v Nástrojích není zvolený
  **Partition Scheme = Custom**, takže se nepoužila přiložená tabulka se dvěma
  aplikačními oblastmi.
- **Meteo hlásí „snimek moc siroky"** → verze PNGdec s malým řádkovým bufferem; snímek ČHMÚ (680 px) se ale běžně vejde, jde spíš o jiný produkt.
- **Prázdná mapa u meteo** → když zrovna neprší, kompozit je skoro černý. To je normální.
- **Radar letadel je prázdný** → zkus větší rozsah; v noci nebo mimo koridory nemusí být nic.

---

## Zdroje dat

| Data | Zdroj | Poznámka |
| --- | --- | --- |
| Letadla | [adsb.fi](https://adsb.fi/) | zdarma, bez klíče, **jen osobní nekomerční použití** |
| Srážky | [ČHMÚ OpenData](https://opendata.chmi.cz/) | srážkový kompozit MAX_Z, nový snímek každých ~5 min |
| Poloha | [ip‑api.com](https://ip-api.com/) | automatická detekce podle IP |

> ⚠️ ČHMÚ i adsb.fi vyžadují uvedení zdroje. Bezplatné API adsb.fi je určené pro osobní použití; pro komerční nasazení si zajisti odpovídající přístup k datům.

---

## Licence a poděkování

Kód je pod licencí **MIT** — volně použitelný a upravitelný.

Projekt vychází z **[petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar)** (autor Petr, původně pro **chiptron.cz**), který sám staví na projektech MatixYo/ESP32‑Plane‑Radar, mylms/ESP‑MeteoRadar a Selbyl/ESP32‑S3‑Touch‑LCD‑2.1_Plane‑Radar. Port a rozšíření (ILI9488 480×320, dotyk FT5436, animace meteoradaru) pro **laskakit.cz**.

Vloženou QRCode knihovnu napsal ricmoo (MIT). Data ČHMÚ a adsb.fi podléhají podmínkám poskytovatelů.
