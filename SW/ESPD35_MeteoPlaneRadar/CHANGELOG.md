# Changelog

Všechny podstatné změny v projektu **ESPD35_MeteoPlaneRadar**.
Formát vychází z [Keep a Changelog](https://keepachangelog.com/cs/1.1.0/),
verzování je [semantické](https://semver.org/lang/cs/).

Verze je na jediném místě: `src/Version.h` (`FW_VERSION`). Zobrazuje se na
obrazovce Nastavení, na OTA obrazovce a v sériovém výpisu při startu.
Laditelné konstanty jsou pohromadě v `src/Config.h`.

---

## [0.3.0]

Přenesení vychytávek z [petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar)
v0.4 – 0.5.2 na hardware ESPD‑3.5, plus vlastní vylepšení.

> ### ⚠️ Upozornění k aktualizaci
> Verze 0.3.0 mění **rozdělení paměti** (dvě aplikační oblasti, aby bylo kam
> nahrát bezdrátovou aktualizaci). Z verze 0.2 a nižší se na ni proto **nedá
> přejít přes OTA** — je nutné jednou nahrát `*.merged.bin` přes USB
> a v Arduino IDE zvolit **Partition Scheme = Custom**. Od 0.3.0 dál už
> aktualizace probíhá bezdrátově.

### Opraveno

- **Detail letadla se zavíral sám od sebe.** Příčinou nebyla data z adsb.fi, ale
  **vadná čtení z dotykového řadiče**. Knihovna `FT6236.cpp` čte z I2C bez
  kontroly návratových hodnot — když přenos selže, `Wire.read()` vrací `-1`,
  do bufferu se uloží `0xFF` a `getPoint()` pak vrátí bod s příznakem
  neplatnosti `z = 0`, který se po rotaci mapuje na **levý dolní roh (0, 320)**.
  Původní `Touch_Read()` příznak `z` ignoroval a hlásil to jako skutečné
  klepnutí mimo letadlo — tedy jako pokyn zrušit výběr.
  Nově se každý vzorek ověřuje: příznak platnosti, souřadnice uvnitř displeje,
  hodnota 4095 (otisk prázdné sběrnice) a nesmyslný skok během jednoho doteku
  (`TOUCH_MAX_JUMP_PX`). Počet zahozených čtení se při `TOUCH_DEBUG` vypisuje
  jednou za sekundu.
  Knihovna `FT6236.cpp` se záměrně nemění — LaskaKit ji distribuuje jako
  převzatou. Veškerá ochrana je v `Touch_FT6236.cpp`.
- **Poloviční provoz na I2C.** Místo dvou nezávislých transakcí za smyčku
  (`touched()` + `getPoint()`) se čte jen jednou. Kromě poloviční zátěže tím
  mizí i závod mezi oběma čteními.
- **Detail už neuskakuje při výpadku dat.** adsb.fi občas letadlo v jednom
  stažení vynechá a v dalším ho zase pošle. Nově se tolerují dvě po sobě jdoucí
  chybějící stažení (`DETAIL_GRACE_POLLS`), během nichž panel drží poslední
  známé hodnoty s poznámkou „signal ztracen".
- **Watchdog hlídá stav přihlášení.** Když `esp_task_wdt_add()` selhal, `Feed()`
  dál volal `reset()` na nepřihlášeném tasku a vypadalo to, že watchdog běží;
  `Resume()` mohl task přidat podruhé.
- **Opotřebení flash.** Změna rozsahu zapisovala do NVS okamžitě, takže rychlé
  přejíždění prstem přímo opotřebovávalo paměť. Nově se změny akumulují a
  zapíšou až po 2 s klidu (`Settings_Tick`).

### Přidáno

- **Obrazovka Nastavení** (třetí obrazovka): posuvník jasu, orientace mapy
  s kompasovým náhledem, přepínání jednotek, stav sítě, tlačítko WiFi + poloha
  a tlačítko Firmware update. Tlačítka sem přišla z pravého panelu obrazovky
  letadel, kde se tísnila v pruhu širokém 116 px.
- **Tlačítko továrního resetu** na obrazovce Nastavení. ESPD‑3.5 nemá žádné
  uživatelské tlačítko, takže reset nešlo vyvolat držením při startu jako
  u desek s BOOT. Tlačítko je menší, červené, vycentrované pod ostatními a
  odsazené, aby na něj nešla trefit ruka mířící na „Firmware update".
  Vyžaduje **dvě klepnutí** (první natáhne, druhé do `RESET_CONFIRM_MS`
  potvrdí); klepnutí kamkoli jinam ho zruší.
- **Klepatelné tečky přepínání obrazovek** (dole uprostřed). Dlouhý stisk je
  rychlý, ale není vidět — tečky ukazují, kde uživatel je, a zároveň slouží
  jako tlačítko pro skok na konkrétní obrazovku. Zóna je na všech obrazovkách
  volná a na radaru se pod ni nekreslí letadla.
- **Směrové přepínání obrazovek** dlouhým stiskem: levá polovina = předchozí,
  pravá = následující, dokola.
- **Dlouhý stisk na tlačítku v Nastavení provede tlačítko, ne přepnutí.** Bez
  toho by pomalejší stisk (nad 0,5 s) na „Firmware update" místo toho skočil na
  jinou obrazovku.
- **Aktualizace firmwaru přes WiFi (OTA, ElegantOTA).** Deska vytvoří AP, ukáže
  QR kód a firmware se nahraje z prohlížeče na `192.168.4.1/update`.
  Vyžaduje `src/partitions.csv` (dva app sloty po 6 MB).
  **Na rozdíl od zdrojového projektu se kreslí skutečný progress bar** — ten
  musel průběh vynechat a zhasínat podsvícení, protože jeho RGB panel si obraz
  průběžně čte z PSRAM a zápis do flash mu data odřezával. ILI9488 na SPI si
  obraz drží sám a data mu posílá tentýž task, který zapisuje flash, takže se
  nikdy nepřekrývají.
- **Orientace mapy** — v Nastavení řádek `Nahore` s tlačítky `−` / `+`, krok
  45°. Nastavuje se **směr, kterým se díváte z okna**, ne „o kolik mapu otočit".
  Otáčí se projekce, ne displej, takže se správně otočí i obrys, města a ikony
  letadel a dotykové souřadnice zůstávají platné. Vedle ovládání je kompasový
  náhled, po obvodu radaru značky S/V/J/Z. **Meteoradar se záměrně neotáčí** —
  srážková mapa se čte severem nahoru.
- **Zapamatování stavu UI** — poslední rozsah (zvlášť pro letadla a meteoradar)
  i naposledy zobrazená obrazovka se ukládají do NVS a obnoví se po restartu.
- **Zobrazení verze firmwaru** na obrazovce Nastavení, na OTA obrazovce a
  v sériovém výpisu. Nová sdílená hlavička `src/Version.h`.
- **Hodiny (NTP)** v pravém panelu obrazovky letadel — je tam na ně místo
  po přesunu tlačítek.
- **ICAO hex adresa** v detailu letadla — je to identita, podle které se
  letadlo drží mezi staženími.
- `sketch.yaml` (připnuté verze core i knihoven) a tento `CHANGELOG.md`.

### Změněno

- **Sjednocení nastavení do `src/Config.h`** — časová zóna, výchozí poloha,
  rozsahy obou obrazovek, intervaly stahování, název AP, `ADSB_MAX`,
  `WDT_TIMEOUT_S`, `DETAIL_GRACE_POLLS`, `MAP_ROT_STEP_DEG`, `OTA_IDLE_MS`
  a ladicí přepínač `TOUCH_DEBUG`. Rozsahy byly dřív zadrátované přímo
  v `ScreenPlanes.cpp` a `ScreenWeather.cpp`, `ADSB_MAX` v `ADSB.h`,
  výchozí poloha v `Settings.h`.
- **Při přetečení `ADSB_MAX` se drží nejbližší letadla, ne první v pořadí.**
  Dřív se při naplnění pole prostě skončilo, takže o tom, která letadla
  uvidíte, rozhodovalo pořadí v odpovědi serveru — u velkého rozsahu nad hustým
  provozem tak mohlo vypadnout letadlo přímo nad vámi.
- **Letadla na zemi se zahazují už při parsování**, aby nikdy nezabrala místo
  stroji ve vzduchu (u letiště by jinak dokázala zaplnit celý `ADSB_MAX`).
- **Ochrana proti nesmyslným souřadnicím** v ADS-B datech i v `Settings_SetLocation()`
  (mimo ±90 / ±180, nuly, NaN).
- **Diagnostické výpisy** u GeoIP (HTTP kód, chyba JSON, poloha mimo rozsah)
  a u připojení k WiFi.
- **Odstraněna veškerá obsluha tlačítka.** Původní kód počítal s tlačítkem
  na `BUTTON_PIN` (krátký stisk = rozsah, dlouhý = obrazovka, držení při startu
  = tovární reset), jenže ESPD‑3.5 žádné uživatelské tlačítko nemá. Ovládání je
  celé dotykové.
- Název konfiguračního AP zkrácen na `ESPD35-MeteoPlaneRadar` a čte se
  z `Config.h` i pro text na displeji, aby se nemusel hlídat na dvou místech.

---

## [0.2] a starší

Viz historie repozitáře. Radar letadel (adsb.fi) + animovaný srážkový
meteoradar ČHMÚ na displeji 480×320, port projektu petus/MeteoPlaneRadar.
