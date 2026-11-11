# Changelog

Změny v projektu **ESPD35_MeteoPlaneRadar**.
Verze je na jediném místě: `Version.h` (`FW_VERSION`).

---

## [0.5.0] — 2026-11-05

Převzato z [petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar)
0.6.4–0.8.5. Níže jsou nejdřív funkce z řady 0.8, pod nimi změny z 0.6.4–0.7.0.

### Přidáno (z 0.8.0–0.8.5)

- **Vyhlazená písma na všech obrazovkách.** Inter, Poppins, Inter tenké nebo
  původní pixelové — volí se ve webovém nastavení (Poloha a displej → Písmo).
  Hodiny mají skutečné obrysy písma 98 px vysoké, ostatní text jde přes
  `AACanvas` (odvozený `Arduino_Canvas`), takže se rozložení obrazovek nemění:
  písmena jsou stejně vysoká jako u pixelového písma a text nikdy není širší.
  Písma mají diakritiku, displej tedy píše „Předpověď“, „Út“, „listopadu“.
  Data písem (`ClockFonts.h`) zabírají asi 680 kB flash.
- **Ikony letadel podle typu** (`PlaneIcon.*`) podle schématu ADS-B Radar —
  vrtulník má rotor, kluzák dlouhá křídla, A380 je větší než Cessna. Typ
  z kódu typu z adsb.fi, jinak z kategorie vysílače. Vysvětlivky ikon jsou
  na webu na záložce Letadla.
- **Vlastní barvy výšek letadel.** Čtyři hranice v tisících stop a pět barev
  na záložce Letadla; výchozí hodnoty jsou přesně dosavadní legenda této
  desky, legenda na displeji se přizpůsobí.
- **Letadlo poblíž.** Volitelně se obrazovka Letadla sama nestřídá, ale když se
  letadlo přiblíží na zadanou vzdálenost (výchozí 5 km), deska na ni přepne,
  otevře jeho detail a po zadané době (15 s) se vrátí. Každé letadlo jednou za
  15 minut, dotyk ukázku ukončí (`PlaneWatch.*`).
- **Výhled z okna.** Směr, šířka a dohled; letadlo ve výhledu má přednost před
  bližším, které z okna vidět není. Výseč je zakreslená v radaru letadel.
- **Meteoradar jen při bouřce.** Volitelně je meteoradar ve střídání jen tehdy,
  když je do zadané vzdálenosti odraz zadané síly (výchozí 20 dBZ do 50 km).
  Kontroluje se nejnovější snímek ČHMÚ každých 5 minut (`StormWatch.*`).
- **ČHMÚ masked** jako druhý zdroj meteoradaru — plnou barvou jen srážky, které
  dopadají na zem. Přepíná se na webu bez restartu.
- **Tři WiFi sítě.** Jdou zapsat i mimo dosah; při startu se zkusí nejsilnější
  dosažitelná, po minutě výpadku i ostatní uložené.
- **Home Assistant** — `http://espd35meteoradar.local/api/ha` vrací JSON
  pro integraci RESTful; na nové záložce je hotový blok `configuration.yaml`.
  Ve výchozím stavu vypnuto. **Nevyzkoušeno se skutečným Home Assistantem.**
- Stavová stránka ukazuje bouřku, letadla poblíž a skutečně zobrazenou
  obrazovku (i když na ni přepnulo střídání nebo upozornění).

### Změněno (z 0.8.x)

- **Šetrnější k přetíženému adsb.fi** (0.8.4): každá další chyba v řadě
  zdvojnásobí pauzu (10 s … 5 min) pro všechny, kdo se ptají; na 429 a 5xx se
  v témže pokusu znovu neptá a `Retry-After` se respektuje.
- **Stará letadla nevypadají jako živá** (0.8.4): po minutě bez dat zešednou
  a nahoře je napsané, jak jsou stará; po deseti minutách zmizí. Ze starých
  dat se nespustí ani upozornění.
- **Výroba se načte i při velkém zpoždění zdroje** (0.8.3): když v posledních
  4 hodinách nic není, zkusí se 12 a 36 hodin.
- Při chybě HTTP se do sériového výpisu napíše i adresa a začátek odpovědi.

### Přidáno (jen ESPD-3.5)

- **Slovenčina.** Na webu v Poloha a displej → Jazyk; přepíná displej i webové
  nastavení, změna se na displeji projeví hned po uložení. Displej má
  slovenské texty s diakritikou i bez ní (pro pixelové písmo), dny a měsíce
  („Pi 6. novembra“), pyl i kvalitu ovzduší. Webová stránka je dál psaná česky
  a slovensky se překládá sama ze slovníku v `WebPage.h`; texty, které ve
  slovníku nejsou, zůstanou česky. Vyhledávání města vrací názvy ve zvoleném
  jazyce. Stavové zprávy zdrojů dat (záložka Stav) zůstávají česky.
- Největší řezy vyhlazeného písma nemají slovenská písmena ä, ľ, ĺ, ô, ŕ;
  místo vynechání se nakreslí bez diakritiky.
- Texty psané přímo v kódu jdou přes makro `LT()` (čeština / slovenčina,
  s diakritikou i bez), které nahradilo `CZT()`.

### Změněno (jen ESPD-3.5)

- **Webové nastavení je přehlednější:**
  - každé pole má vlastní popisek s jednotkou (dřív „Odraz aspoň (dBZ) / do
    vzdálenosti (km)“ nad dvěma poli bez rozlišení);
  - nastavení vypnuté funkce (posun proti slunci, bouřka, výhled z okna,
    letadlo poblíž, Home Assistant) se ukážou až po jejím zapnutí;
  - „Meteoradar jen při bouřce“ je v kartě Meteoradar, „Letadlo poblíž“ na
    záložce Letadla vedle Výhledu z okna; karta „Chytřejší střídání“ zmizela;
  - Uložit nastavení není na záložkách Stav, WiFi a Správa, které mají
    vlastní tlačítka;
  - u barev výšek jsou skutečné rozsahy („3–10 tis. ft“), mění se při psaní;
    čtyři hranice jsou v jedné řadě i na telefonu;
  - nápovědy mají jednu větu, zbytek je pod „Více“.
- **Směr okna = orientace radaru.** „Nahoře na radaru je“ a „Okno míří na“
  byla dvě nastavení téhož a mohla si odporovat. Výhled z okna teď bere směr
  z orientace radaru (i když se mění tlačítky na displeji); klíč `windowAz`
  se už neukládá a v záloze se ignoruje.
- **Heslo správce:** pole „Heslo správce“ slouží jen k povolení akcí; nové
  heslo má vlastní kartu a koncový bod `/api/password`, který chce současné
  heslo. Dřív se nové heslo posílalo s každým Uložit z téhož pole — překlep
  v hesle pro reset se tak uložil jako nové heslo a heslo šlo změnit bez
  znalosti starého.
- **Odstraněn `OTA.cpp`** (a `OTA.h`, `OTA_IDLE_MS`). Soubor se od 0.4.0
  nevolal, ale pořád vtahoval knihovnu ElegantOTA, takže bez ní překlad
  končil chybou.

### Opraveno (jen ESPD-3.5)

- Obrazovka průběhu aktualizace firmwaru byla jen česky.

- **Legenda výšek letadel byla vždy ve stopách**, i s metrickými jednotkami.
  Teď se řídí nastavením: „FL x1000ft“ s hranicemi v tisících stop, nebo
  „výška km“ s hranicemi v kilometrech (0.9 / 3 / 6.1 / 9.1). Displej se po
  přepnutí jednotek na webu hned překreslí. Na webu se podle téhož
  zaškrtávátka přepíná i nastavení barev (hranice v km, popisky „3–6,1 km“)
  a filtr výšek (m / ft); hodnoty v polích se při přepnutí přepočítají.
  Deska dál ukládá stopy; hodnota, na kterou uživatel nesáhl, se uloží
  přesně původní, takže Uložit v metrických jednotkách hranice neposouvá.
- **Názvy letadel kolidovaly s ikonami**, hlavně u vybraného letadla:
  popisek šel vždy 16 px pod střed, ale velká letadla (A380, B747, nákladní)
  mají ikonu do 22 px a kroužek vybraného letadla poloměr 17 px. Odstup
  se teď počítá ze skutečné velikosti ikony (`PlaneIcon_Radius`), popisek
  se nesmí překrývat s ikonou žádného letadla a když pod ikonou není místo,
  zkusí se nad ní, vpravo a vlevo. Vybrané letadlo dostane místo pro popisek
  první a kreslí se navrch.
- **Radar letadel:** při upozornění na letadlo poblíž a při ztrátě signálu
  zasahoval řádek „v okně 4,0 km“ / „signál ztracen“ do popisku „Typ“. Teď je
  místo ICAO kódu pod volačkou. Popisek letadla u spodního okraje mapy se
  kreslí nad ikonu, ne mimo displej.
- **Webové nastavení:** nápovědy pod poli měly stejnou barvu a velikost jako
  popisky (CSS selektor `.row .hint` neseděl na `<div class="row hint">`).

- **Po restartu watchdogem deska zůstala na QR kódu s výběrem WiFi**, ačkoli
  uloženou síť měla. Při pádu deska routeru neohlásí odpojení a router ji pak
  chvíli odmítá; jeden 12s pokus nestačil. Teď se rádio před připojením uvede
  do výchozího stavu a uložené sítě se zkoušejí ve třech kolech (další kola
  čekají 20 s). Když ani pak nic, ukáže se portál jako dřív — a pokud se
  k němu do **30 minut** nikdo nepřipojí, deska se restartuje a zkusí uložené
  sítě znovu (`WIFI_PORTAL_RESTART_MS` v `Config.h`, 0 = vypnuto). Bez uložené
  sítě se nerestartuje.
- **Překlad s ESP32 core 3.3:** lokální konstanta `HALF_PI` v `UI.cpp`
  kolidovala se stejnojmenným makrem z `Arduino.h`.

### Opraveno (z 0.8.x)

- **Legenda meteoradaru měla špatné hodnoty u čtyř nižších barev** — podle
  škály ČHMÚ je modrá 12 dBZ, zelená 24, žlutá 36 a oranžová 44.
- **Názvy měst na meteoradaru jsou čitelné i nad srážkami** (0.8.1): nad
  světlým pozadím se popisek nakreslí černě se světlým okrajem.
- Čas snímku ČHMÚ se převádí podle data snímku, ne podle aktuálních hodin.

### Přidáno

- **Cena elektřiny.** Nová obrazovka se spotovou cenou z denního trhu OTE
  (přes spotovaelektrina.cz, zdarma a bez registrace). Vlevo je den jako
  prstenec — půlnoc nahoře, 96 čtvrthodin od zelené po červenou, ručička na
  probíhající čtvrthodině. Vpravo velké číslo s cenou právě teď v Kč/kWh,
  pod ním nejlevnější a nejdražší blok dne. Klepnutí na prstenec vypíše
  vybranou čtvrthodinu, klepnutí doprostřed nebo na pravý panel (i swipe)
  přepne dnes/zítra. Zítřek vychází kolem 14:00.
- **Výroba elektřiny.** Druhá nová obrazovka: z čeho se právě vyrábí (ENTSO-E
  přes energy-charts.info). Prstenec podílů, uprostřed podíl OZE, vpravo
  legenda seřazená podle podílu, spotřeba, vývoz/dovoz a čas vzorku — data
  chodí se zpožděním 1–2 hodiny. Zemi lze zvolit, včetně celé Evropy.
- **Záložka Energie ve webovém nastavení.** Přirážka k burzovní ceně
  v Kč/MWh (s přepočtem na Kč/kWh hned pod polem), DPH a země pro výrobu.
  Dokud je přirážka i DPH nula, displej ukazuje čistou burzu a napíše to.
- Obě energetické obrazovky jsou po aktualizaci **vypnuté**; zapínají se na
  záložce Obrazovky. Dokud jsou vypnuté, deska si o data neřekne. Po přesunu
  polohy mimo ČR (cena) nebo mimo Evropu (výroba) se jednou samy vypnou.
- **Legendu meteoradaru jde skrýt** (záložka Obrazovky → Meteoradar).
  Vypnutím zmizí i místo, které si legenda rezervovala pro popisky měst.
- Na stavové stránce přibyl stav ceny, výroby a **časové pásmo**.

### Změněno

- **Předpověď se čte po řádcích**: „nyní“, dvě tříhodinová okna popsaná časem
  (např. `14-17h`), pod nimi dnešek a další čtyři dny s datem. Okno sečte
  srážky, vezme nejsilnější vítr a nejvýraznější ikonu. Vítr se ukazuje až
  od 12 km/h, stejně jako srážky jen když nějaké jsou.
- **Čas se řídí polohou.** Posun proti UTC chodí spolu s předpovědí z Open-Meteo
  (`TimeZone.*`); v Česku zůstává zkompilované pravidlo s letním časem.
  Denní minima a maxima se nově počítají přes místní dny, ne přes UTC.
- **Na displeji je `°C` místo `degC`.** Vestavěný font znak má na 0xF8.
- `Net_GetString()` čte tělo přes sink (chunked se dekóduje) do bufferu v PSRAM
  se stropem `NET_MAX_TEXT` a zvládá i `http://`. Poloha podle IP (ip-api.com)
  jde nově touto cestou místo vlastního `HTTPClient::getString()`.
- Sedm obrazovek místo pěti. Poslední obrazovka se ukládá pod klíčem `scr3`,
  starší `scr2` se jednorázově převede (Nastavení bylo 4, nově 6).
- Prstence se vyplňují po pixelech (`UI_FillRing`), bez černých teček mezi
  radiálními čarami.

### Opraveno

- **Po neúspěchu se výpis ČHMÚ stahoval prakticky pořád dokola.** Nový pokus
  je až za minutu (`METEO_RETRY_MS`), po úspěchu za pět (`METEO_PERIOD_MS`).
- **Meteoradar při obnově nezčerná** — zůstane poslední snímek s malou žlutou
  poznámkou. Když stažení selže, snímky se nezahodí a overlay napíše, že jde
  o starší data.
- **Na velkých rozsazích u hranic se poloha posouvala.** Výřez ze snímku ČHMÚ
  se ořezával na okraj, takže přestal být vystředěný na uživatele, ale křížek
  se kreslil doprostřed. Výřez se už neořezává, chybějící část je černá.
- **Snímek meteoradaru se kontroluje na PNG signaturu** — chybová stránka
  s HTTP 200 se už nedostane do dekodéru.
- Radar letadel přijme i pole `aircraft` (servery odvozené z ADSBexchange)
  a squawk poslaný jako číslo, takže nouzový kód nepropadne.

---

## [0.4.2]

### Opraveno

- **Trasa letu se nezobrazovala.** `deserializeJson()` četl přímo
  z `http.getStream()`, což je surový socket: když adsb.lol odpoví chunked,
  jdou hexadecimální hlavičky bloků rovnou do parseru a ten vrátí `Ok` nad
  prázdným dokumentem.
- **Radar letadel mohl zůstat prázdný** ze stejné příčiny — hlavičky bloků
  zůstávaly v těle a pole `ac` v dokumentu chybělo. Těla se nově čtou přes
  `writeToStream()`, které chunked dekóduje.
- **Snímek meteoradaru se mohl vykreslit poškozený**, protože stahování PNG
  četlo socket stejným ručním způsobem.
- **Zařízení se mohlo restartovat během TLS handshake.** Ten má vlastní limit
  120 s, tedy šestinásobek watchdogu; nově 8 s.
- **Stažení trasy mohlo restartovat zařízení**, protože se během něj
  neresetoval watchdog. Přibylo `Route_SetPollFn()`.
- **Letadlo na zemi se počítalo jako v nulové výšce.** `alt_baro` u nich nese
  text `ground`, ze kterého `atof()` udělal 0.0 a ohlásil úspěch; nově
  `strtof()` s koncovým ukazatelem.

### Změněno

- Index ČHMÚ se prohledává za běhu v klouzavém okně, takže na jeho velikosti
  nezáleží. Odpadl i desetisekundový timeout, na který dobíhalo každé stažení.
- Sériový výpis říká velikost indexu ČHMÚ a u nečekané odpovědi z adsb.fi
  její `msg` a začátek těla.

---

## [0.4.1]

### Opraveno

- **V detailu letadla se ukazovala trasa, která neodpovídala skutečnosti** —
  třeba Atény → Istanbul u letadla nad Prahou. Sešly se tři příčiny:
  letadlo bez volací značky poslalo do dotazu svou ICAO adresu (`a31234` se
  normalizuje na `A31234`, což je platné číslo letu Aegean Airlines 1234),
  zdroj tras neověřoval polohu, a u letů s mezipřistáním se ukazoval první
  odlet a poslední přílet, i když letadlo letělo prostřední úsek.
- **Místo typu letounu se u některých letadel ukazovalo `adsb_icao`.** To je
  typ zprávy, ne typ draku; jako záloha se používat neměl. Když adsb.fi drak
  nezná, řádek se teď prostě nevykreslí.
- **Hlavička `User-Agent` se neposílala vůbec.** `HTTPClient::addHeader()` ji
  mlčky zahazuje, takže odcházelo výchozí `ESP32HTTPClient`.
- **Trasa se dokreslila až s dalším stažením letadel**, o několik sekund
  později, než dorazila. Nově se objeví hned.
- **Keš tras si odpověď pamatovala až do restartu.** Volací značky se recyklují
  a výsledek platí k poloze, se kterou se ptalo, takže po mezipřistání
  ukazoval pořád první úsek. Záznam teď platí 20 minut (1 minutu, když
  trasa nebyla nalezena).

### Změněno

- **Trasy nově z adsb.lol** místo adsbdb.com. Spolu s volací značkou se posílá
  i poloha letadla a server vrátí, jestli k ní trasa vůbec sedí — co neprojde,
  se nezobrazí. U letů s mezipřistáním se vybere ten úsek, který letadlo právě
  letí. Letadlo, které volací značku nevysílá, se na trasu neptá vůbec.
- **Registrace a typ letounu se berou z adsb.fi** (pole `r` a `t`) ze stejné
  odpovědi, která se stahuje kvůli polohám. Odpadl tím dotaz na druhé API
  a registrace se ukáže i u letadla bez trasy.

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
- **Trasa letu** v detailu letadla: odkud, kam, registrace.
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
