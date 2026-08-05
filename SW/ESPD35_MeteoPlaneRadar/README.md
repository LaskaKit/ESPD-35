# ESPD35 PlaneRadar + Meteoradar

**Živý radar letadel (ADS-B) a srážkový meteoradar ČHMÚ na dotykovém displeji.**

Zařízení běží na desce **LaskaKit ESPD-3.5"** a v jednom přístroji spojuje **sledování letadel** v okolí a **animovanou srážkovou situaci** nad Českou republikou. Stačí deska a USB-C kabel - nic se nepájí ani nedrátuje, polohu si zjistí samo podle IP adresy.

**Programovat nemusíte.** Hotový firmware nahrajete z prohlížeče - viz [Nahrání firmwaru](#nahrání-firmwaru-bez-programování).

| Komponenta | Popis |
| --- | --- |
| **Deska** | [LaskaKit ESPD-3.5" ESP32-S3 TFT ILI9488 CAP Touch **Rev. 3.2**](https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/?variantId=12161) |
| **MCU** | ESP32-S3, 16 MB flash + PSRAM |
| **Displej** | 3.5" TFT ILI9488, 480x320, SPI |
| **Dotyk** | kapacitní FT5436 (I2C) |
| **Krabička** | [Krabička pro ESPD-3.5"](https://www.laskakit.cz/laskakit--krabicka-pro-espd-35/?variantId=12574) |

> ### Inspirace
> Projekt vychází z [petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar) (autor Petr, původně pro [chiptron.cz](https://chiptron.cz)), portovaného a rozšířeného pro obdélníkový displej ESPD-3.5. Ten sám staví na projektech [MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar) (radar letadel a zdroj adsb.fi) a [mylms/ESP-MeteoRadar](https://github.com/mylms/ESP-MeteoRadar) (srážkový meteoradar ČHMÚ).

---

## Co to umí

Tři obrazovky v cyklu: **Letadla -> Meteoradar -> Nastavení**.

### Radar letadel

Data z [adsb.fi](https://adsb.fi/) - veřejná ADS-B síť, zdarma a bez klíče. Vlastní přijímač nepotřebujete.

- Displej rozdělený **3/4 mapa + 1/4 detail**. Střed mapy je vaše poloha (zaměřovací kříž).
- Letadla jako **siluety natočené podle skutečného kurzu**; stroje bez hlášeného kurzu jako kroužek.
- **Barva podle nadmořské výšky** (letová hladina), legenda vlevo. Vlevo nahoře počet letadel, vlevo dole rozsah.
- **Detail letadla** v pravém panelu: volací znak, ICAO adresa, typ, vzdálenost, výška, rychlost, kurz, stoupání/klesání a hodiny. Ukazuje automaticky nejbližší letadlo, klepnutím zafixujete jiné.
- **Rozsahy** 10 / 25 / 50 / 100 km. Pod srážkami i pod letadly se kreslí obrys ČR a města.
- **Značky S/V/J/Z** po obvodu radaru, otáčejí se spolu s mapou.

### Meteoradar ČHMÚ

Srážkový kompozit z [ČHMÚ OpenData](https://opendata.chmi.cz/), nový snímek každých 5 minut.

- **Animace 6 snímků** za posledních 25 minut, 2 snímky/s, mezi cykly krátká pauza. Nahoře uprostřed indikátor s časem snímku.
- Barevná škála intenzity srážek v legendě, pod srážkami obrys ČR a města.
- **Rozsahy** 25 / 50 / 100 / 200 km, pamatují se odděleně od rozsahu letadel.

> Meteoradar ČHMÚ pokrývá **ČR a blízké okolí**. S polohou nastavenou jinam zůstane meteo obrazovka prázdná - data pro tu oblast neexistují. Radar letadel funguje kdekoliv.

### Nastavení

Jas, orientace mapy, jednotky (letecké ft/kt ↔ metrické m/km-h), stav sítě a uložená poloha, verze firmwaru. Tlačítka **WiFi + poloha (AP)**, **Firmware update (OTA)** a **Tovární reset**.

**Pamatuje si stav.** Poslední obrazovka, rozsahy, jas, jednotky, orientace i poloha přežijí restart - deska naskočí tam, kde jste skončili.

---

## Ovládání

Deska nemá žádné tlačítko, ovládá se celá dotykem.

| Gesto | Funkce |
| --- | --- |
| **Klepnutí na tečky dole uprostřed** | skok přímo na danou obrazovku |
| **Dlouhý stisk (> 0,5 s) v levé polovině** | předchozí obrazovka |
| **Dlouhý stisk v pravé polovině** | následující obrazovka |
| Přejetí prstem vlevo/vpravo | změna rozsahu (Letadla, Meteoradar) |
| Krátké klepnutí na letadlo | zafixování letadla v detailu |
| Klepnutí do prázdné mapy nebo do panelu | zpět na automaticky nejbližší letadlo |

### Orientace mapy ("Nahoře")

V Nastavení zvolíte, **který světový směr je nahoře** na radaru letadel - tedy směr, kterým se díváte z okna. Nastavíte `V` a letadla na displeji jsou ve stejném směru jako ta za sklem. Osm poloh po 45°, vedle tlačítek je kompasový náhled.

Otáčí se projekce, ne displej, takže se spolu s letadly správně otočí i obrys států, města a ikony. **Meteoradar se záměrně neotáčí** - srážková mapa se čte severem nahoru.

### Tovární reset

Smaže WiFi údaje i nastavení a desku restartuje do konfiguračního portálu.

---

## Nahrání firmwaru (bez programování)

Potřebujete jen prohlížeč **Chrome, Edge nebo Operu** (Firefox a Safari to neumí) a USB-C kabel, který přenáší data - hodně kabelů umí jen nabíjení a s nimi se deska v počítači vůbec neobjeví.

### 1. Stáhněte firmware

Z [**Releases**](../../releases) si stáhněte soubor **`ESPD35_MeteoPlaneRadar_vX.USB.merged.bin`** - ten s `USB.merged`. Je v něm celý obraz paměti včetně jejího rozdělení, takže funguje i na úplně nové desce.

### 2. Nahrajte ho z prohlížeče

Otevřete **[esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz)** a projděte čtyři kroky na stránce:

1. **Vyberte čip** - `ESP32-S3`.
2. **Přetáhněte stažený `USB.merged.bin`** do vyznačené plochy.
3. **Připojte desku** USB-C kabelem a klepněte na *Připojit desku*. Vyberte port v dialogu prohlížeče. Nástroj si ověří, že na desce opravdu je ESP32-S3.
4. **Nahrát firmware**. Při prvním nahrání zapněte volbu *Smazat celou flash* - vyčistí i případná stará data z předchozího firmwaru.

Po dokončení dejte desce reset (nebo odpojit a připojit napájení).

> Nic se nikam neodesílá, celý nástroj běží ve vašem prohlížeči.

### 3. Připojení k WiFi

Po prvním zapnutí (nebo po továrním resetu) si deska vytvoří **vlastní otevřenou WiFi síť** a na displeji ukáže QR kód.

1. **Namiřte na QR kód fotoaparát telefonu** - nabídne připojení k síti. Ručně: *Nastavení -> WiFi -> `ESPD35-MeteoPlaneRadar`*, síť je bez hesla.
2. Telefon oznámí, že **síť nemá přístup k internetu**. To je v pořádku, zůstaňte připojení. Na Androidu na chvíli vypněte mobilní data, jinak telefon síť sám opustí a přepne se zpět.
3. Konfigurační stránka se většinou otevře sama. Když ne, zadejte do prohlížeče **`http://192.168.4.1`**.
4. Klepněte na **Configure WiFi**, vyberte svoji domácí síť a zadejte heslo. Na stejné stránce jsou i políčka pro ruční zadání polohy (`lat` / `lon`) - nechte je být, pokud vám sedí poloha zjištěná podle IP. Jinak je můžete přepsat.
5. Uložte. Deska se připojí a naskočí radar.

Konfiguraci můžete kdykoli vyvolat znovu: **Nastavení -> WiFi + poloha (AP)**.

### 4. Aktualizace přes WiFi (OTA)

Novou verzi (od verze 0.3.0) už nahrajete bezdrátově, bez kabelu.

1. Z [Releases](../../releases) si stáhněte **`ESPD35_MeteoPlaneRadar_vX.OTA.bin`** - pozor, **ten bez `USB.merged`**.
2. V zařízení jděte do **Nastavení -> Firmware update (OTA)**.
3. Deska vytvoří stejnou WiFi síť jako při prvním nastavení a ukáže QR kód. **Připojte se k ní podle kroků 1 a 2 výše.**
4. V prohlížeči otevřete **`http://192.168.4.1/update`**, vyberte stažený soubor a nahrajte ho.
5. Průběh vidíte na displeji i v prohlížeči. Deska se sama restartuje do nové verze - ověřte si v Nastavení, že se změnila.

Když se aktualizace nepovede, zůstane v desce původní verze. Režim OTA opustíte klepnutím na displej, po 5 minutách nečinnosti skončí sám.

> ### ⚠️ Přechod z verze 0.2 a nižší
> Verze 0.3.0 mění rozdělení paměti (dvě aplikační oblasti, aby bylo kam nahrát bezdrátovou aktualizaci). Poprvé je proto nutné nahrát `*.USB.merged.bin` přes USB podle kroků 1 a 2 - bezdrátová aktualizace by neměla kam zapsat. Stačí to jednou.
>
> Jakou verzi máte, zjistíte v **Nastavení** pod nadpisem. Když tam žádná není, máte verzi starší než 0.3.0.

---

## Kompilace ze zdrojáků

Tahle část je jen pro ty, kdo si chtějí projekt upravit. Pokud jste nahráli hotový firmware, přeskočte ji.

### 1. Knihovny

V Arduino IDE (**Nástroje -> Spravovat knihovny**) nainstalujte:

| Knihovna | Autor | K čemu |
| --- | --- | --- |
| **GFX Library for Arduino** | moononournation | kreslení (POZOR: ne Adafruit GFX) |
| **PNGdec** | bitbank2 | dekódování snímků ČHMÚ |
| **ArduinoJson** (v7) | bblanchon | parsování dat z adsb.fi |
| **WiFiManager** | tzapu | konfigurační WiFi portál |
| **ElegantOTA** | ayushsharma82 | aktualizace přes WiFi |

`Preferences`, `Wire`, `HTTPClient` a `WebServer` jsou součástí ESP32 core. Knihovny **QRCode** (ricmoo) a **FT6236** pro dotyk jsou přibalené v projektu.

> ElegantOTA se používá ve **výchozím (synchronním) režimu** nad `WebServer` z core - nic se v knihovně needituje a `ESPAsyncWebServer` ani `AsyncTCP` nejsou potřeba.

### 2. Nastavení Arduino IDE

Vyžaduje **ESP32 core 3.x**. V nabídce **Nástroje**:

| Položka | Hodnota |
| --- | --- |
| Deska | ESP32S3 Dev Module |
| **PSRAM** | **OPI PSRAM** |
| Flash Size | 16MB (128Mb) |
| **Partition Scheme** | **Custom** |
| USB CDC On Boot | Disable |
| Upload Speed | 921600 |

**PSRAM** je nutná - bez ní se nemá kam alokovat obrazový buffer a displej zůstane černý.

**Partition Scheme = Custom** použije přiložený `src/partitions.csv` se dvěma aplikačními oblastmi (2x 6 MB), aby bylo kam nahrát novou verzi přes OTA. Po překladu zkontrolujte v logu, že se hlásí `of 6291456 bytes`.

Piny jsou předvyplněné pro ESPD-3.5 Rev 3.2 a měnit se nemusí. Ostatní konfigurace (časová zóna, výchozí poloha, rozsahy, intervaly stahování, limity, ladicí přepínače) je pohromadě v **`src/Config.h`**. Verze firmwaru je v **`src/Version.h`**, historie změn v **[CHANGELOG.md](CHANGELOG.md)**.

### 3. Překlad a nahrání

Všechny soubory musí být v **jedné složce `ESPD35_MeteoPlaneRadar`** - Arduino IDE vyžaduje, aby se složka jmenovala stejně jako hlavní `.ino`.

Pro vydání se hodí oba soubory: `Sketch -> Export Compiled Binary` vyrobí aplikaci pro OTA, sloučený obraz pro web flasher pak musí vzniknout se **stejnou partition tabulkou**.

### Řešení problémů

- **Displej zůstává černý** -> zkontrolujte **PSRAM: OPI PSRAM** v Nástrojích. Zdaleka nejčastější příčina.
- **Deska se neobjeví v dialogu portu** -> skoro vždy kabel bez datových vodičů. Zkuste jiný.
- **Dotyk nereaguje nebo má přehozené souřadnice** -> ověřte `I2C_SDA` / `I2C_SCL` v `Config.h`, případně přehoďte `TOUCH_ROTATION` mezi `3` a `1`.
- **Výběr letadla se ruší "sám"** -> zapněte `TOUCH_DEBUG 1` v `Config.h` a sledujte sériovou linku (115200 Bd). Řádky `TOUCH: zahozeno N vadnych cteni` znamenají problém na I2C sběrnici (rušení, dlouhé vodiče), ne v datech z adsb.fi.
- **OTA hlásí, že není kam zapsat** -> není zvolený **Partition Scheme = Custom**, takže se nepoužila tabulka se dvěma aplikačními oblastmi.
- **Meteo hlásí "snimek moc siroky"** -> verze PNGdec s malým řádkovým bufferem.
- **Prázdná mapa u meteoradaru** -> když zrovna neprší, kompozit je skoro černý. To je normální.
- **Radar letadel je prázdný** -> zkuste větší rozsah; v noci nebo mimo letové koridory nemusí být nic.

---

## Zdroje dat

| Data | Zdroj | Poznámka |
| --- | --- | --- |
| Letadla | [adsb.fi](https://adsb.fi/) | zdarma, bez klíče, **jen osobní nekomerční použití** |
| Srážky | [ČHMÚ OpenData](https://opendata.chmi.cz/) | srážkový kompozit MAX_Z, nový snímek každých ~5 min |
| Poloha | [ip-api.com](https://ip-api.com/) | automatická detekce podle IP |

> ⚠️ **ČHMÚ i adsb.fi vyžadují uvedení zdroje.** Bezplatné API adsb.fi je určené pro osobní použití.

---

## Licence a poděkování

Kód je pod licencí **MIT** - volně použitelný a upravitelný.

Projekt vychází z [petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar) (autor Petr, původně pro chiptron.cz). Port a rozšíření pro ESPD-3.5 (ILI9488 480x320, dotyk FT5436) pro [laskakit.cz](https://www.laskakit.cz).

Vloženou QRCode knihovnu napsal ricmoo (MIT). Data ČHMÚ a adsb.fi podléhají podmínkám poskytovatelů.
