# ESPD35 MeteoPlaneRadar

Hodiny, radar letadel, srážkový meteoradar a předpověď počasí na desce
**LaskaKit ESPD‑3.5"**. Bez pájení — stačí deska, USB‑C kabel a WiFi.

> Vychází z [petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar)
> (autor Petr, původně pro chiptron.cz). Portováno a rozšířeno pro
> [laskakit.cz](https://www.laskakit.cz).

---

## Co to umí

Pět obrazovek. Přepínají se dlouhým stiskem nebo klepnutím na tečky dole.
Čtyři z nich se dají vypnout.

| Obrazovka | Co ukazuje |
| --- | --- |
| 🕐 **Hodiny** | Čas, datum, aktuální počasí, vítr, východ a západ slunce. Sekundy běží po elipse kolem displeje. |
| ✈️ **Letadla** | Radar z [adsb.fi](https://adsb.fi) — silueta natočená podle kurzu, barva podle letové hladiny. V panelu detail nejbližšího letadla včetně trasy letu. Rozsahy 10 / 25 / 50 / 100 km. |
| 🌧️ **Meteoradar** | Srážky z [ČHMÚ](https://opendata.chmi.cz), animace 6 snímků za posledních 25 minut. Rozsahy 25 / 50 / 100 / 200 km a celá ČR. |
| 🌤️ **Předpověď** | 6 hodin, 3 dny, ovzduší (AQI, PM2.5, pyl). Z [Open‑Meteo](https://open-meteo.com). |
| ⚙️ **Nastavení** | Jas, orientace mapy, jednotky, adresa webu, tovární reset. |

Všechno ostatní se nastavuje **v prohlížeči** — poloha, obrazovky, filtry
letadel, WiFi i aktualizace firmwaru.

Polohu není třeba zadávat, zjistí se podle IP adresy.

---

## Hardware

| | |
| --- | --- |
| Deska | [LaskaKit ESPD‑3.5" ESP32‑S3 TFT ILI9488 CAP Touch Rev. 3.2](https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/?variantId=12161) |
| Displej | 3.5" TFT ILI9488, 480×320, SPI |
| Dotyk | kapacitní FT5436 (I2C) |
| Krabička | [Krabička pro ESPD‑3.5"](https://www.laskakit.cz/laskakit--krabicka-pro-espd-35/?variantId=12574) |

Deska nemá žádné tlačítko — ovládá se celá dotykem.

---

# Pro uživatele: nahrání bez programování

Nepotřebuješ Arduino IDE ani nic instalovat. Stačí prohlížeč.

**Potřebuješ:** Chrome nebo Edge (Firefox a Safari to neumí) a USB‑C kabel,
který vede i data — ne jen nabíjecí.

1. Otevři **webový flasher** (odkaz na stránce projektu).
2. Připoj desku do počítače kabelem ke konektoru označenému **„USB"**.
3. Klikni na **Connect** a v seznamu vyber port desky.
4. Klikni na **Install** a počkej. Deska se sama restartuje.

### První spuštění

1. Deska vytvoří otevřenou WiFi síť **`ESPD35-MeteoPlaneRadar`**.
2. Na displeji se ukáže **QR kód** — naskenuj ho telefonem a připoj se.
3. V prohlížeči otevři **`http://192.168.4.1/`**, vyber svou WiFi a zadej heslo.

Deska čeká, dokud síť nezadáš — žádný časový limit. Telefon nahlásí, že síť
nemá internet; to nevadí.

Potom najdeš nastavení na **`http://espd35meteoradar.local/`** (adresa je
i na obrazovce Nastavení).

### Ovládání dotykem

| Gesto | Co udělá |
| --- | --- |
| Dlouhý stisk vlevo / vpravo | předchozí / následující obrazovka |
| Klepnutí na tečky dole | skok na danou obrazovku |
| Přejetí prstem vlevo / vpravo | změna rozsahu |
| Klepnutí na letadlo | zafixuje ho v detailu |
| Klepnutí do prázdné mapy | zpět na nejbližší letadlo |

### Nastavení v prohlížeči

Šest záložek:

- **Stav** — čas, adresa, signál, doba běhu, volná paměť, stav zdrojů dat.
  Odsud jde přepnout obrazovku i rozsah na dálku.
- **Poloha a displej** — poloha (ručně nebo vyhledáním města), jas ve dne
  a v noci, automatické přepínání podle slunce.
- **Obrazovky** — které se zobrazují, automatické střídání, styl sekund.
- **Letadla** — jednotky, orientace mapy, výškové pásmo, filtry, hlídaný znak.
- **WiFi** — vyhledání sítí a připojení.
- **Správa** — heslo, záloha nastavení, aktualizace firmwaru, tovární reset.

Změny platí **po klepnutí na Uložit nastavení**. Restart není potřeba.

### Orientace mapy

V Nastavení se volí, **který světový směr je nahoře** na radaru letadel — tedy
směr, kterým se díváš z okna. Nastavíš `V` a letadla na displeji jsou ve stejném
směru jako ta za sklem. Meteoradar se záměrně neotáčí, srážky se čtou severem
nahoru.

### Aktualizace firmwaru

Ve **Správě** vyber soubor `ESPD35_MeteoPlaneRadar.ino.bin` (ten **bez**
„merged") a klikni na Nahrát. Průběh je vidět v prohlížeči i na displeji.
Když se to nepovede, zůstane v desce původní verze.

### Když se něco pokazí

| Problém | Řešení |
| --- | --- |
| Displej je černý | Nahrávalo se se špatným nastavením PSRAM. Nahraj znovu přes webový flasher. |
| Prázdný meteoradar | Když neprší, kompozit je skoro černý. To je normální. |
| Prázdný radar letadel | Zkus větší rozsah. V noci nebo mimo koridory nemusí být nic. |
| Deska se nepřipojí k WiFi | Síť sama zapomene a vrátí se k QR kódu. Zkus zadat heslo znovu. |
| Nejde `espd35meteoradar.local` | Použij IP adresu z obrazovky Nastavení. |
| Zapomenuté heslo správce | Tovární reset na obrazovce Nastavení (dvě klepnutí na červené tlačítko). |

---

# Pro vývojáře: překlad ze zdrojáků

### Knihovny

V Arduino IDE (**Nástroje → Spravovat knihovny**):

| Knihovna | Autor |
| --- | --- |
| GFX Library for Arduino | moononournation (**ne** Adafruit GFX) |
| PNGdec | bitbank2 |
| ArduinoJson (v7) | bblanchon |
| FT6236 | DustinWatts — přikládá LaskaKit k desce |

QRCode (ricmoo) je přibalený v projektu. Zbytek je součást ESP32 core:
`WebServer`, `DNSServer`, `ESPmDNS`, `Update`, `Preferences`, `HTTPClient`.

> WiFiManager ani ElegantOTA se od 0.4.0 **nepoužívají**. Portál i nahrávání
> firmwaru obsluhuje vlastní web server. ElegantOTA je pod AGPL‑3.0, což
> u zařízení obsluhujícího webovou stránku není zadarmo.

### Nastavení Arduino IDE

**Nástroje →**

| Položka | Hodnota |
| --- | --- |
| Deska | ESP32S3 Dev Module |
| **PSRAM** | **OPI PSRAM** ← bez toho zůstane displej černý |
| Flash Size | 16MB (128Mb) |
| **Partition Scheme** | **Custom** ← použije přiložený `partitions.csv` |
| USB CDC On Boot | Disable |
| Upload Speed | 921600 |

Partition **Custom** je nutná pro OTA — tabulka má dvě aplikační oblasti
po 6 MB. Po překladu zkontroluj v logu `of 6291456 bytes`.

Alternativně `arduino-cli compile --profile default` — verze core i knihoven
jsou připnuté v `sketch.yaml`.

### Piny (Config.h)

Předvyplněné pro ESPD‑3.5 Rev 3.2:

```c
#define TFT_SCK   12      // displej ILI9488 (SPI)
#define TFT_MOSI  11
#define TFT_MISO  13
#define TFT_CS    48
#define TFT_DC    47
#define TFT_RST   -1
#define TFT_BL    45
#define I2C_SDA   42      // dotyk FT5436
#define I2C_SCL    2
```

### Struktura

| Soubor | K čemu |
| --- | --- |
| `Config.h` | **jediný soubor, který obvykle měníš** — piny, časová zóna, rozsahy, intervaly, limity |
| `Version.h` | verze firmwaru |
| `Settings.*` | nastavení v NVS + serializace do JSON |
| `Net.*` | HTTPS GET na jednom místě, kontrola paměti |
| `Status.*` | stav zdrojů dat pro webovou stránku |
| `Layout.*` | pevné pásy obrazovek + kontrola překryvů |
| `Lang.*` | texty (ASCII na displej, UTF‑8 na web) |
| `ADSB.*`, `Route.*` | letadla a trasy letů |
| `CHMU.*` | srážkový kompozit |
| `Forecast.*` | předpověď, slunce, ovzduší — jedním požadavkem |
| `EuBorder.*`, `EuMapData.h` | hranice a města |
| `Screen*.cpp` | jednotlivé obrazovky |
| `WebConfig.*`, `WebPage.h` | web server a stránka |
| `WiFiPortal.*` | připojení a přístupový bod |

### Na co si dát pozor

- **Krátké názvy velkými písmeny.** `xtensa/config/specreg.h` obsazuje `MR`,
  `BR`, `PS`, `SAR`, `DDR`, `EPC`, `MISC`, `M0`–`M3` a další. Používej popisné
  názvy.
- **Názvy hlaviček** nesmí kolidovat se systémovými ani při ignorování velikosti
  písmen — proto `Lang.h`, ne `Strings.h`.
- **Z obsluhy HTTP požadavku se nikdy nekreslí.** Požadavky se řadí do fronty
  a provádí je `loop()`. Jediná výjimka je průběh OTA.
- **Font je 7bitové ASCII.** Na displej patří text bez diakritiky — `Lang.h`
  drží obě verze.
- **Klíče v NVS se nepřejmenovávají**, aby desky po aktualizaci nepřišly
  o nastavení. Když se mění význam, zakládá se nový klíč.
- **Layout: jedna skupina = jeden nárok.** Hodnota a jednotka se nesmí
  narokovat zvlášť, obdélníky by se překryly a druhý nárok Layout odmítne.

### Sériový výstup (115200 Bd)

Přes konektor označený **„USB"** (nativní USB ESP32‑S3), ne přes ten druhý.

```
=== ESPD35_MeteoPlaneRadar v0.4.0 ===
Duvod restartu: zapnuti napajeni
WiFi ok, IP 192.168.1.42
Web: http://espd35meteoradar.local/
Letadla: 11 (8421 bajtu)
Meteoradar: 6 ramcu
Predpoved: 6 h / 3 d
```

Podrobnější výpisy: `TOUCH_DEBUG` a `LAYOUT_DEBUG` v `Config.h`.

---

## Zdroje dat

| Data | Zdroj | Poznámka |
| --- | --- | --- |
| Letadla, registrace a typ | [adsb.fi](https://adsb.fi/) | zdarma, bez klíče |
| Trasy letů | [adsb.lol](https://adsb.lol/) | zdarma, bez klíče; ověřuje trasu proti poloze letadla |
| Srážky | [ČHMÚ OpenData](https://opendata.chmi.cz/) | kompozit MAX_Z, nový snímek ~5 min |
| Počasí | [Open‑Meteo](https://open-meteo.com/) | zdarma, bez klíče |
| Poloha | [ip‑api.com](https://ip-api.com/) | detekce podle IP |
| Mapa | Natural Earth, GeoNames | volné dílo / CC BY 4.0 |

> ⚠️ Zdroje vyžadují uvedení. Bezplatná API jsou pro **osobní nekomerční
> použití**; pro komerční nasazení si zajisti odpovídající přístup k datům.

---

## Licence

Kód je pod **MIT**. Vložená knihovna QRCode (ricmoo) rovněž MIT.
Data ČHMÚ, adsb.fi, adsb.lol a Open‑Meteo podléhají podmínkám poskytovatelů.

Historie změn: [CHANGELOG.md](CHANGELOG.md).
