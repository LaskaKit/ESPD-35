# ESPD35 PlaneRadar + Meteoradar

Radar letadel (**adsb.fi**) a srážkový **meteoradar ČHMÚ** s animací na desce **LaskaKit ESPD‑3.5"** (ESP32‑S3, TFT ILI9488 480×320 s kapacitním dotykem).

Dvě obrazovky na jednom zařízení — letecký provoz nad tebou a přicházející srážky včetně jejich pohybu. Bez pájení, stačí deska, USB kabel a WiFi.

> Připraveno pro **[laskakit.cz](https://www.laskakit.cz)**. Projekt vychází z [petus/Meteo‑PlaneRadar](https://github.com/petus/Meteo-PlaneRadar) (původně pro **chiptron.cz**), portováno a rozšířeno pro obdélníkový displej ESPD‑3.5.

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

Stačí deska a USB‑C kabel. Polohu není třeba zadávat — zjistí se automaticky podle IP.

---

## Závislosti (knihovny)

V Arduino IDE (**Nástroje → Spravovat knihovny**) nainstaluj:

| Knihovna | Autor | K čemu |
| --- | --- | --- |
| **GFX Library for Arduino** | moononournation | kreslení + off‑screen canvas (POZOR: ne Adafruit GFX) |
| **PNGdec** | Larry Bank (bitbank2) | dekódování snímků ČHMÚ |
| **ArduinoJson** (v7) | Benoit Blanchon | parsování dat z adsb.fi a ip‑api |
| **WiFiManager** | tzapu | konfigurační WiFi portál |
| **QRCode** | Richard Moore (ricmoo) | QR kód v portálu |

**Dotyk** používá knihovnu **FT6236** (DustinWatts, s upraveným CHIPID/VENDID pro FT5436), kterou LaskaKit přikládá k desce.

---

## Nastavení Arduino IDE

**Nástroje →**

| Položka | Hodnota |
| --- | --- |
| Deska | ESP32S3 Dev Module |
| **PSRAM** | **OPI PSRAM** ← nutné (bez toho zůstane displej černý) |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP / 9.9MB FATFS) — nebo cokoli s APP ≥ 3 MB |
| USB CDC On Boot | Disable |
| Upload Speed | 921600 |

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
// Ovladaci tlacitko
#define BUTTON_PIN 0     // BOOT
```

> Máš‑li jinou revizi desky, zkontroluj piny podle pinoutu a ukázek v `SW/` repozitáře LaskaKit.

---

## Instalace

1. Všechny soubory dej do **jedné složky `ESPD35_MeteoPlaneRadar`** (Arduino vyžaduje, aby se složka jmenovala stejně jako hlavní `.ino`).
2. Zkontroluj `Config.h`, přelož a nahraj `ESPD35_MeteoPlaneRadar.ino`.

### První spuštění

1. Zařízení vytvoří otevřenou WiFi síť **`ESPD35_MeteoPlaneRadar-Setup`**.
2. Na displeji se ukáže **QR kód** — naskenuj telefonem, připoj se a vyber domácí WiFi (případně zadej ručně polohu lat/lon).
3. Poloha se jinak zjistí automaticky podle IP.

---

## Ovládání

**Dotyk (kapacitní):**

| Gesto | Funkce |
| --- | --- |
| **Dlouhý stisk (> 0,5 s) kdekoli** | přepnutí Letadla ↔ Meteoradar |
| Přejetí prstem vlevo/vpravo | změna rozsahu aktivní obrazovky |
| Krátké klepnutí na letadlo | detail letadla (bílý kroužek) |
| Klepnutí do prázdné mapy | zpět na automaticky nejbližší letadlo |
| Tlačítko „Jednotky" v panelu | přepnutí letecké ↔ metrické |
| Tlačítko „WiFi + poloha" v panelu | spustí konfigurační portál |

---

## Sériový výstup (115200 Bd)

Vypisují se jen základní informace:

```
=== ESPD35 PlaneRadar ===
WiFi: MojeSit  IP: 192.168.1.42
Letadla: 11
Meteoradar: 6 ramcu
```

---

## Řešení problémů

- **Displej zůstává černý** → zkontroluj **PSRAM: OPI PSRAM** v Nástrojích. To je nejčastější příčina.
- **Dotyk nereaguje / souřadnice přehozené** → ověř `I2C_SDA`/`I2C_SCL` v `Config.h`; případně přehoď `TOUCH_ROTATION` mezi 3 a 1.
- **Meteo hlásí „snimek moc siroky"** → verze PNGdec s malým řádkovým bufferem; snímek ČHMÚ (680 px) se ale běžně vejde, jde spíš o jiný produkt.
- **Prázdná mapa u meteo** → když zrovna neprší, kompozit je skoro černý. To je normální.
- **Radar letadel je prázdný** → zkus větší rozsah; v noci nebo mimo koridory nemusí být nic.

---

## Zdroje dat — při použití uveďte

| Data | Zdroj | Poznámka |
| --- | --- | --- |
| Letadla | [adsb.fi](https://adsb.fi/) | zdarma, bez klíče, **jen osobní nekomerční použití** |
| Srážky | [ČHMÚ OpenData](https://opendata.chmi.cz/) | srážkový kompozit MAX_Z, nový snímek každých ~5 min |
| Poloha | [ip‑api.com](https://ip-api.com/) | automatická detekce podle IP |

> ⚠️ ČHMÚ i adsb.fi vyžadují uvedení zdroje. Bezplatné API adsb.fi je určené pro osobní použití; pro komerční nasazení si zajisti odpovídající přístup k datům.

---

## Licence a poděkování

Kód je pod licencí **MIT** — volně použitelný a upravitelný.

Projekt vychází z **[petus/Meteo‑PlaneRadar](https://github.com/petus/Meteo-PlaneRadar)** (autor Petr, původně pro **chiptron.cz**), který sám staví na projektech MatixYo/ESP32‑Plane‑Radar, mylms/ESP‑MeteoRadar a Selbyl/ESP32‑S3‑Touch‑LCD‑2.1_Plane‑Radar. Port a rozšíření (ILI9488 480×320, dotyk FT5436, animace meteoradaru) pro **laskakit.cz**.

Vloženou QRCode knihovnu napsal ricmoo (MIT). Data ČHMÚ a adsb.fi podléhají podmínkám poskytovatelů.
