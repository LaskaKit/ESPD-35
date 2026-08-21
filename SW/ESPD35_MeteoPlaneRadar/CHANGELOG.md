# Changelog

Změny v projektu **ESPD35_MeteoPlaneRadar**.
Verze je na jediném místě: `Version.h` (`FW_VERSION`).

---

## [0.4.0]

Konfigurace se přesunula do prohlížeče, přibyly hodiny a předpověď.

> **Aktualizace z 0.3.0:** rozdělení paměti se nemění, jde přes OTA.
> Poslední zobrazená obrazovka se převede automaticky (nový klíč `scr2`).

### Přidáno

- **Obrazovka hodin** — čas, datum, počasí, vítr, východ a západ slunce.
  Ukazatel sekund běží po elipse; čtyři styly (vypnuto / tečky / čára /
  kometa), barvy se nastavují na webu. Klepnutí přepne den/noc, když je
  automatika vypnutá.
- **Obrazovka předpovědi** — 6 hodin, 3 dny, ovzduší (AQI, PM2.5, pyl).
- **Webová konfigurace** na `http://espd35meteoradar.local/`, v záložkách.
  Poloha (i vyhledáním města), jas, obrazovky, jednotky, orientace mapy,
  filtry letadel, WiFi, záloha a obnova, aktualizace firmwaru.
  Stránka je jeden soubor bez externích závislostí — funguje i z přístupového
  bodu, kde deska nemá internet.
- **Stav zdrojů dat** na webu: čas, adresa, signál, doba běhu, volná paměť
  a jednořádkové hlášení ADS‑B, meteoradaru a předpovědi. Plus přepínání
  obrazovek a rozsahu na dálku.
- **Automatický noční jas** podle východu a západu slunce, s posunem ±120 min.
- **Evropská mapa** (30 894 bodů hranic, 1 100 měst) místo obrysu ČR.
- **Pátý rozsah meteoradaru: celá ČR** — pevný výřez státu bez ohledu na polohu.
- **Trasa letu** (adsbdb.com) v detailu letadla: odkud, kam, registrace.
- **Nouzový squawk** (7500/7600/7700) — červený pruh v detailu. Letadlo v nouzi
  se zobrazí i mimo nastavené výškové pásmo.
- **Pět obrazovek, čtyři vypínatelné**, plus automatické střídání.
  Vypnutá obrazovka nemá tečku a přeskočí ji dlouhý stisk i střídání.
- **Filtry letadel**: výškové pásmo, jen se znakem volání, hlídaný volací znak.
- **Heslo správce** — chrání aktualizaci firmwaru, obnovu ze zálohy a reset.

### Změněno

- **Pryč ElegantOTA.** Je pod AGPL‑3.0 a u zařízení s webovou stránkou se
  licence uplatní. Nahradila ho třída `Update` z ESP32 core, kterou ElegantOTA
  stejně jen obalovala — o jednu závislost míň.
- **Pryč WiFiManager.** Blokoval smyčku (kvůli tomu se musel uspávat watchdog)
  a jeho portál je anglický. Náhradou je vlastní přístupový bod a stránka
  projektu. **Přístupový bod nemá časový limit.**
- **Žádná změna nastavení nevyžaduje restart.** Restart zůstal jako tlačítko
  ve Správě a u obnovy ze zálohy.
- **`Net.*`** — jedno místo pro HTTPS. Kontroluje volnou interní paměť, protože
  TLS handshake potřebuje ~45 kB; bez toho selže jako nic neříkající `HTTP -1`.
- **Dotyk se neztrácí během stahování.** Snímání běží i uvnitř přenosu,
  provedení až v `loop()`.
- **Obrazovka Nastavení**: místo dvou tlačítek je tam „Znovu nastavit WiFi“
  a adresa webu.
- **`Watchdog_Suspend()` / `Resume()` zrušeny** — nic už smyčku neblokuje.

### Opraveno

- **Jednotky u předpovědi a hodnoty ovzduší se nekreslily.** Hodnota a jednotka
  si o místo říkaly zvlášť a druhý nárok Layout vždy odmítl, protože obdélníky
  se překrývaly. Skupina teď zabírá jeden obdélník.
- **Tlačítko „Uložit nastavení“ bylo jen v záložce Správa** — schovalo se
  s ní, takže změny z ostatních záložek nešlo uložit. Teď je pod všemi.
- **Východ a západ slunce nešly přečíst** (tmavá šedá, velikost 1) → světlá
  šedá, velikost 2.
- **Ikony počasí splývaly s pozadím**, nejvíc u deště a bouřky. Přibyla světlejší
  šedá pro mraky.
- **Běh sekund nebyl v krabičce vidět** — vedl po samém okraji. Teď je to
  elipsa s odsazením 16 px.
- **Ohon komety byl příliš krátký** (5 s) → 20 s, plynulé hasnutí.
- **Detail letadla zůstával na stroji mimo mapu** → vrátí se na nejbližší,
  jakmile je vybrané letadlo dál než zobrazený rozsah.
- **Detail letadla byl celý drobný** → hodnota velikosti 2, popisek a jednotka
  malé. Když se to nevejde, hodnota se zmenší, místo aby přetekla.
- **Modrý obdélník u předpovědi na dny** (sloupeček srážek bez měřítka) zrušen.
- **Druh pylu byl na místě jednotky** → je v popisku (`Pyl travy`), číslo velké.
  Jednotka se u pylu neuvádí: bez tabulky prahů stejně nic neřekne.
- **PM2.5 mělo jednotku `ug`** — nedokončenou. Správně `ug/m3`.
- **Srážky u předpovědi na hodiny se nekreslily.** Řádek zabírá 20 px, ale
  oddělovací čára pod hodinovým pásem stála 16 px pod ním, takže nárok
  kolidoval a Layout ho odmítl. Svislé rozestupy jsou přepočítané a hlídá
  je test, který konstanty čte přímo ze zdrojáku.
- **Automatické střídání obrazovek se nerozjelo.** Dvě příčiny:
  z obrazovky Nastavení se záměrně nikdy neodcházelo, takže deska nechaná
  na Nastavení se sama nevrátila k datům; a pauza po doteku byla pevných
  10 minut bez ohledu na interval — po jediném doteku se u desetisekundového
  střídání deset minut nedělo nic. Nově se z Nastavení odchází taky (uživatele
  chrání pauza) a pauza je desetinásobek intervalu, nejméně 30 s a nejvýše
  10 minut. Nově uložená hodnota navíc platí okamžitě, nečeká se na doběhnutí
  staré pauzy. V záložce Stav je vidět, jestli je střídání vypnuté, nebo jen
  pozastavené a na jak dlouho.
- **Teploty měly barvu podle počasí, ne podle teploty** — 18 °C mohlo být
  červené a 22 °C modré. Barva teď znamená teplotu (modrá → červená),
  stav počasí nese ikona nad číslem. Platí i pro denní maxima a minima.
- **Posuvník jasu neříkal, kterou úroveň nastavuje** → `Jas (den, automaticky)`.
- **Vítr byl schovaný pod teplotou** → pod hodinami, velikost 2.
- **Tečky obrazovek byly moc velké** (poloměr 6 → 4).
- **Podpis „laskakit.cz“ zabíral místo v panelu letadel** → odstraněn.
- **Nešlo přeložit na Windows.** `Strings.h` kolidoval se systémovým
  `<strings.h>` (Windows nerozlišuje velikost písmen) → `Lang.h`.
- **Nešlo přeložit `WebConfig.cpp`.** Pole `MR` kolidovalo s makrem z
  `xtensa/config/specreg.h` → popisné názvy. Krátké názvy velkými písmeny
  v tomhle projektu nepoužívat.

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
