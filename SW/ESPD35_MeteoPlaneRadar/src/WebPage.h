// =============================================================================
//  ESPD35_MeteoPlaneRadar - konfiguracni stranka (jeden staticky retezec).
//
//  Cely web je jeden soubor bez externich zavislosti: zadne CDN, zadne fonty,
//  zadny framework. Duvod je prosty - stranka se casto otevira z pristupoveho
//  bodu, kde deska ZADNY internet nema. Cokoli stazeneho zvenci by se v tu
//  chvili nenacetlo a rozbilo by presne to nastaveni, ktere ma uzivatel
//  zprovoznit.
//
//  PROGMEM: na ESP32 je flash namapovana do adresniho prostoru, takze se
//  retezec da predat primo do server.send() bez kopirovani do RAM.
//
//  Jazyk: cestina, nebo slovencina (Settings_Lang). Stranka je napsana cesky
//  a slovensky si ji preklada sama: slovnik SK nize prevadi texty stranky
//  (applyLang) i texty skladane v JavaScriptu (t()). Co ve slovniku neni,
//  zustane cesky - stranka tim nikdy neprijde o text.
// =============================================================================
#pragma once
#include <Arduino.h>

static const char WEB_PAGE_HTML[] PROGMEM = R"HTMLPAGE(<!doctype html>
<html lang="cs">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESPD35 MeteoPlaneRadar</title>
<style>
:root{--bg:#12151b;--card:#1b2029;--line:#2b323f;--fg:#e8ecf3;--dim:#98a2b3;
      --acc:#4aa8ff;--ok:#3ddc84;--warn:#ffc44d;--bad:#ff6b6b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
     font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{padding:18px 16px 10px;border-bottom:1px solid var(--line)}
h1{margin:0;font-size:20px}
header .sub{color:var(--dim);font-size:13px;margin-top:3px}
main{max-width:760px;margin:0 auto;padding:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;
      padding:14px 16px;margin:0 0 14px}
.card h2{margin:0 0 12px;font-size:15px;letter-spacing:.04em;
         text-transform:uppercase;color:var(--dim)}
.row{display:flex;align-items:center;gap:10px;margin:9px 0;flex-wrap:wrap}
.row label{flex:1 1 190px;min-width:150px}
/* Napovedy jsou <div class="row hint"> a <p class="hint">. Puvodni selektor
   ".row .hint" (potomek) nesedel na ani jedno, takze napovedy mely stejnou
   barvu i velikost jako popisky poli a stranka splyvala. */
.hint{color:var(--dim);font-size:12.5px}
.row.hint{flex-basis:100%;margin:-4px 0 4px;display:block}
/* Napoveda = jedna veta, zbytek pod "Vice". */
.hint details{display:inline}
.hint details[open]{display:block;margin-top:3px}
.hint summary{display:inline;cursor:pointer;color:var(--acc);list-style:none;margin-left:4px}
.hint summary::-webkit-details-marker{display:none}
.abgrid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px;flex:1 1 100%}
.abgrid input[type=number]{width:100%;min-width:0;flex:none}
input[type=file]{max-width:100%}
#iconLegend .row{flex-wrap:nowrap}
#iconLegend .row span{flex:1 1 0;min-width:0}
input,select,button{font:inherit;color:var(--fg);background:#232a35;
      border:1px solid var(--line);border-radius:8px;padding:7px 10px}
input[type=range]{padding:0;background:none;border:none;flex:1 1 160px}
input[type=checkbox]{width:18px;height:18px;accent-color:var(--acc)}
input[type=text],input[type=number],input[type=password],select{flex:0 1 170px;min-width:120px}
button{background:#2b3442;cursor:pointer}
button:hover{border-color:var(--acc)}
button.primary{background:var(--acc);border-color:var(--acc);color:#06121f;font-weight:600}
button.danger{background:#3a2226;border-color:#5c3238;color:var(--bad)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
.kv{display:flex;justify-content:space-between;gap:10px;padding:4px 0;
    border-bottom:1px dashed var(--line);font-size:14px}
.kv:last-child{border:none}
.kv span:first-child{color:var(--dim)}
.kv span:last-child{text-align:right;word-break:break-word}
#toast{position:fixed;left:50%;transform:translateX(-50%);bottom:18px;
  background:#232a35;border:1px solid var(--line);border-radius:10px;
  padding:9px 16px;opacity:0;transition:opacity .25s;pointer-events:none;z-index:9}
#toast.on{opacity:1}
#nets div{padding:7px 9px;border:1px solid var(--line);border-radius:8px;
  margin:5px 0;cursor:pointer;display:flex;justify-content:space-between;gap:8px}
#nets div:hover{border-color:var(--acc)}
#bar{height:22px;background:#232a35;border:1px solid var(--line);
  border-radius:8px;overflow:hidden;margin-top:8px;display:none}
#bar i{display:block;height:100%;width:0;background:var(--ok)}
.muted{color:var(--dim);font-size:13px}
/* Zalozky. Stranka mela devet karet pod sebou a hledat v ni cokoli znamenalo
   scrollovat pres vsechno ostatni. */
nav{display:flex;gap:4px;overflow-x:auto;padding:10px 16px 0;
    border-bottom:1px solid var(--line);position:sticky;top:0;
    background:var(--bg);z-index:5;-webkit-overflow-scrolling:touch}
nav button{border:1px solid transparent;border-bottom:none;background:none;
  color:var(--dim);border-radius:9px 9px 0 0;padding:8px 14px;white-space:nowrap}
nav button:hover{color:var(--fg)}
nav button[aria-selected=true]{background:var(--card);border-color:var(--line);
  color:var(--fg);font-weight:600}
section[hidden]{display:none}
</style>
</head>
<body>
<header>
  <h1>ESPD35 MeteoPlaneRadar</h1>
  <div class="sub" id="hdr">načítám…</div>
</header>
<nav id="tabs"></nav>
<main>
<p class="muted" id="savenote" style="margin:0 0 12px">Změny se na displeji projeví až po klepnutí na <b>Uložit nastavení</b> dole.</p>

<section id="t-stav">
<div class="card">
  <h2>Stav</h2>
  <div id="stat"></div>
  <div class="row" style="margin-top:12px">
    <button onclick="scr(-1)">◀ obrazovka</button>
    <button onclick="scr(1)">obrazovka ▶</button>
    <button onclick="rng(-1)">− rozsah</button>
    <button onclick="rng(1)">+ rozsah</button>
  </div>
</div>
</section>

<section id="t-misto" hidden>
<div class="card">
  <h2>Poloha</h2>
  <div class="row">
    <label for="city">Najít město</label>
    <input type="text" id="city" placeholder="např. Brno">
    <button onclick="geo()">Hledat</button>
  </div>
  <div id="geores"></div>
  <div class="row"><label for="lat">Zeměpisná šířka</label>
    <input type="number" id="lat" step="0.0001" min="-90" max="90"></div>
  <div class="row"><label for="lon">Zeměpisná délka</label>
    <input type="number" id="lon" step="0.0001" min="-180" max="180"></div>
  <div class="row hint">Po uložení si deska znovu stáhne předpověď i mapu, restart není potřeba.</div>
</div>

<div class="card">
  <h2>Displej</h2>
  <div class="row"><label for="lg">Jazyk (displej i web)</label>
    <select id="lg" onchange="setLang(this.value)">
      <option value="0">Čeština</option>
      <option value="1">Slovenčina</option>
    </select></div>
  <div class="row"><label>Jas ve dne <b id="bdv"></b>%</label>
    <input type="range" id="bd" min="10" max="100" oninput="bdv.textContent=this.value"></div>
  <div class="row"><label>Jas v noci <b id="bnv"></b>%</label>
    <input type="range" id="bn" min="10" max="100" oninput="bnv.textContent=this.value"></div>
  <div class="row"><label for="na">Přepínat den a noc podle slunce</label>
    <input type="checkbox" id="na"></div>
  <div class="dep" data-dep="na">
    <div class="row"><label for="no">Posun proti slunci (min)</label>
      <input type="number" id="no" min="-120" max="120" step="5"></div>
    <div class="row hint">Kladná hodnota začne noc dřív a ukončí ji později.
      <details><summary>Více</summary>Hodí se pro pokoj, který se stmívá dřív než obloha. Časy východu a západu přicházejí spolu s předpovědí, takže automatika nestojí žádný požadavek navíc.</details></div>
  </div>
  <div class="row"><label for="cf">Písmo (všechny obrazovky)</label>
    <select id="cf">
      <option value="1">Inter</option>
      <option value="2">Poppins</option>
      <option value="3">Inter tenké</option>
      <option value="0">původní pixelové</option>
    </select></div>
  <div class="row hint">Vyhlazená písma mají diakritiku.
    <details><summary>Více</summary>Displej pak píše „Předpověď“ a „Út 5. listopadu“. Rozložení obrazovek se nemění — písmena jsou stejně vysoká jako u pixelového písma a text nikdy není širší.</details></div>
</div>
</section>

<section id="t-obrazovky" hidden>
<div class="card">
  <h2>Obrazovky</h2>
  <div class="grid" id="scrs"></div>
  <div class="row" style="margin-top:10px"><label for="rot">Střídat automaticky po (s)</label>
    <input type="number" id="rot" min="0" max="3600" step="10"></div>
  <div class="row hint">0 = vypnuto.
    <details><summary>Více</summary>Swipe nebo dlouhý stisk střídání dočasně pozastaví (desetinásobek intervalu, nejméně 30 s a nejvýše 10 minut) — kolik zbývá, je vidět v záložce Stav. Nastavení vypnout nejde. Když vypnete právě zobrazenou obrazovku, deska přepne na první zapnutou.</details></div>
  <div class="row"><label for="sec">Běh sekund na hodinách (elipsa)</label>
    <select id="sec">
      <option value="0">vypnutý</option>
      <option value="1">tečky po obvodu</option>
      <option value="2">čára</option>
      <option value="3">kometa</option>
    </select></div>
</div>

<div class="card">
  <h2>Meteoradar</h2>
  <div class="row"><label for="rs">Zdroj</label>
    <select id="rs">
      <option value="0">ČHMÚ</option>
      <option value="2">ČHMÚ masked</option>
    </select></div>
  <div class="row hint">ČHMÚ masked ukazuje plnou barvou jen srážky, které opravdu dopadají na zem.
    <details><summary>Více</summary>ČHMÚ snímek porovná s měřením ve výšce 2 km; srážky, které se cestou vypaří, a kovadliny vzdálených bouřek jsou bledší. Na otázku „bude u nás pršet?“ odpovídá přesněji, pro sledování bouřek je lepší plný ČHMÚ.</details></div>
  <div class="row"><label for="ml">Zobrazit legendu (dBZ / mm/h)</label>
    <input type="checkbox" id="ml"></div>
  <div class="row hint">Stupnice zabírá levý horní roh mapy; bez ní je vidět víc území.</div>
  <div class="row"><label for="so">Meteoradar jen při bouřce</label>
    <input type="checkbox" id="so"></div>
  <div class="dep" data-dep="so">
    <div class="row"><label for="sd">Odraz aspoň (dBZ)</label>
      <input type="number" id="sd" min="4" max="60" step="1"></div>
    <div class="row"><label for="sk">Do vzdálenosti (km)</label>
      <input type="number" id="sk" min="5" max="250" step="5"></div>
    <div class="row hint">Meteoradar se do střídání přidá, jen když je v okolí bouřka.
      <details><summary>Více</summary>Každých 5 minut se zkontroluje nejnovější snímek ČHMÚ; po dvou klidných snímcích meteoradar zase vypadne. Funguje i s vypnutou obrazovkou Meteoradar. Jen v pokrytí ČHMÚ.</details></div>
  </div>
</div>
</section>

<section id="t-energie" hidden>
<div class="card">
  <h2>Cena elektřiny</h2>
  <div class="row"><label for="pf">Přirážka k burzovní ceně (Kč/MWh)</label>
    <input type="number" id="pf" min="0" max="20000" step="10" oninput="feeHint()"></div>
  <div class="row hint" id="pfk"></div>
  <div class="row"><label for="pv">DPH (%, 0 = nepřipočítávat)</label>
    <input type="number" id="pv" min="0" max="100" step="1"></div>
  <div class="row hint">S nulami ukazuje displej čistou burzovní cenu a napíše k ní „burza“.
    <details><summary>Více</summary>Distribuci, regulované složky a marži obchodníka si opište z vyúčtování; ceník je obvykle uvádí v Kč/kWh, takže je vynásobte tisícem. Deska si nic nedomýšlí: připočte přesně to, co sem zadáte.</details></div>
  <div class="row hint">Čtvrthodinové ceny OTE (přes spotovaelektrina.cz), jen pro Česko. Zítřek vychází kolem 14:00.</div>
</div>

<div class="card">
  <h2>Výroba elektřiny</h2>
  <div class="row"><label for="mc">Země</label>
    <select id="mc">
      <option value="eu">Celá Evropa</option>
      <option value="cz">Česko</option>
      <option value="sk">Slovensko</option>
      <option value="at">Rakousko</option>
      <option value="de">Německo</option>
      <option value="pl">Polsko</option>
      <option value="hu">Maďarsko</option>
      <option value="si">Slovinsko</option>
      <option value="hr">Chorvatsko</option>
      <option value="be">Belgie</option>
      <option value="bg">Bulharsko</option>
      <option value="ch">Švýcarsko</option>
      <option value="dk">Dánsko</option>
      <option value="ee">Estonsko</option>
      <option value="es">Španělsko</option>
      <option value="fi">Finsko</option>
      <option value="fr">Francie</option>
      <option value="gr">Řecko</option>
      <option value="ie">Irsko</option>
      <option value="it">Itálie</option>
      <option value="lt">Litva</option>
      <option value="lu">Lucembursko</option>
      <option value="lv">Lotyšsko</option>
      <option value="nl">Nizozemsko</option>
      <option value="no">Norsko</option>
      <option value="pt">Portugalsko</option>
      <option value="ro">Rumunsko</option>
      <option value="rs">Srbsko</option>
      <option value="se">Švédsko</option>
      <option value="ua">Ukrajina</option>
      <option value="uk">Velká Británie</option>
    </select></div>
  <div class="row hint">Z čeho se právě vyrábí, podíl obnovitelných zdrojů, spotřeba a přeshraniční toky.
    <details><summary>Více</summary>Data ENTSO-E přes rozhraní energy-charts vycházejí se zpožděním jedné až dvou hodin — čas vzorku je proto napsaný na obrazovce. Oba zdroje jsou zdarma, bez klíče a bez registrace. Obrazovky se zapínají na záložce Obrazovky; dokud jsou vypnuté, deska si o data vůbec neřekne. Mimo Česko (cena) a mimo Evropu (výroba) se po změně polohy samy vypnou.</details></div>
</div>
</section>

<section id="t-letadla" hidden>
<div class="card">
  <h2>Letadla</h2>
  <div class="row"><label for="mu">Metrické jednotky (m, km/h)</label>
    <input type="checkbox" id="mu" onchange="unitsChanged()"></div>
  <div class="row"><label for="tb">Okno míří na (nahoře na radaru)</label>
    <select id="tb"></select></div>
  <div class="row hint">Radar se otočí tak, aby letadla na displeji mířila stejně jako ta za oknem. Stejný směr používá výhled z okna.</div>
  <div class="row"><label for="alo">Výška od (ft)</label>
    <input type="number" id="alo" min="0" max="60000" step="500"></div>
  <div class="row"><label for="ahi">Výška do (ft)</label>
    <input type="number" id="ahi" min="0" max="60000" step="500"></div>
  <div class="row"><label for="oc">Jen letadla s volacím znakem</label>
    <input type="checkbox" id="oc"></div>
  <div class="row"><label for="sa">Hlásit nouzový squawk (7500/7600/7700)</label>
    <input type="checkbox" id="sa"></div>
  <div class="row hint">Letadlo v nouzi se zobrazí i mimo nastavené výšky.</div>
  <div class="row"><label for="wc">Hlídaný volací znak</label>
    <input type="text" id="wc" maxlength="8" placeholder="např. CSA"></div>
  <div class="row hint">Porovnává se začátek znaku, takže „CSA“ chytí všechny lety CSA.</div>
</div>

<div class="card">
  <h2>Výhled z okna</h2>
  <div class="row"><label for="wo">Zakreslit a upřednostnit výhled z okna</label>
    <input type="checkbox" id="wo"></div>
  <div class="dep" data-dep="wo">
    <div class="row"><label for="ww">Šířka výhledu (°)</label>
      <input type="number" id="ww" min="10" max="300" step="5"></div>
    <div class="row"><label for="wk">Dohled (km)</label>
      <input type="number" id="wk" min="1" max="100" step="1"></div>
    <div class="row hint">Směr okna je nahoře v kartě Letadla.
      <details><summary>Více</summary>Letadlo ve výhledu má při upozornění přednost před bližším letadlem, které z okna vidět není — letadlo 3 km za domem je blíž než to 9 km před oknem, ale uvidíte jen to druhé. Výseč se zakreslí do radaru letadel.</details></div>
  </div>
</div>

<div class="card">
  <h2>Letadlo poblíž</h2>
  <div class="row"><label for="pa">Ukázat Letadla, jen když je letadlo poblíž</label>
    <input type="checkbox" id="pa"></div>
  <div class="dep" data-dep="pa">
    <div class="row"><label for="pk">Vzdálenost (km)</label>
      <input type="number" id="pk" min="1" max="50" step="1"></div>
    <div class="row"><label for="ps">Ukázat na (s)</label>
      <input type="number" id="ps" min="5" max="300" step="5"></div>
    <div class="row hint">Obrazovka Letadla se sama nestřídá; ukáže se, až se letadlo přiblíží nebo proletí výhledem z okna.
      <details><summary>Více</summary>Ukáže se i s otevřeným detailem a pak se pokračuje tam, kde se skončilo. Každé letadlo jednou za 15 minut. Dotyk ukázku ukončí.</details></div>
  </div>
</div>

<div class="card">
  <h2>Barvy podle výšky</h2>
  <div id="altc"></div>
  <div class="row hint" id="altchint"></div>
</div>

<div class="card">
  <h2>Ikony letadel</h2>
  <p class="hint">Tvar podle typu letadla, velikost podle hmotnosti, barva podle výšky.</p>
  <div id="iconLegend" style="display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:6px 14px;margin-top:10px"></div>
  <p class="hint">Typ se pozná z kódu typu (A20N, B738, EC35…), u neznámého z kategorie, kterou vysílá transpondér. Rozdělení do ikon vychází ze schématu <a style="color:var(--acc)" href="https://adsb-radar.com/help/icons.html">ADS-B Radar</a>.</p>
</div>
</section>

<section id="t-ha" hidden>
<div class="card">
  <h2>Home Assistant</h2>
  <div class="row"><label for="ha">Zpřístupnit data pro Home Assistant</label>
    <input type="checkbox" id="ha"></div>
  <div class="dep" data-dep="ha">
    <div class="row hint">⚠ Nevyzkoušeno se skutečným Home Assistantem.
      <details><summary>Více</summary>Formát odpovídá dokumentaci integrace RESTful. Pokud něco nesedí, dejte vědět.</details></div>
    <div class="row"><span>Adresa:</span> <a id="haLink" style="color:var(--acc)" target="_blank"><code id="haUrl"></code></a></div>
    <div class="row hint">Do configuration.yaml vložte tohle a Home Assistant restartujte:</div>
    <pre id="haYaml" style="background:#0e1218;border:1px solid #2a3340;border-radius:6px;padding:10px;overflow-x:auto;font-size:12px;white-space:pre"></pre>
    <div class="row"><button onclick="copyHa()">Zkopírovat</button></div>
    <div class="row hint">Data jsou jen ke čtení a bez hesla.
      <details><summary>Více</summary>Home Assistant si je sám stahuje integrací RESTful — bez MQTT, brokeru i doplňku. Neříkají nic, co by nebylo vidět na displeji. Dokud je funkce zapnutá, deska hlídá bouřku (jeden snímek ČHMÚ za 5 minut) a letadla poblíž (malý dotaz jednou za 30 s).</details></div>
  </div>
</div>
</section>

<section id="t-sit" hidden>
<div class="card">
  <h2>WiFi</h2>
  <div id="wifinow" class="muted"></div>
  <div class="row" style="margin-top:8px">
    <button onclick="scan()">Vyhledat sítě</button></div>
  <div id="nets"></div>
  <div class="row"><label for="ss">Název sítě</label><input type="text" id="ss"></div>
  <div class="row"><label for="pw">Heslo</label><input type="password" id="pw"></div>
  <div class="row"><button class="primary" onclick="wifi()">Připojit</button></div>
  <div class="row hint">Deska opustí přístupový bod a stránka se odpojí; pak ji najdete na adrese nahoře.</div>
</div>

<div class="card">
  <h2>Uložené sítě</h2>
  <div id="wslots"></div>
  <div class="row"><button class="primary" onclick="saveNets()">Uložit sítě</button></div>
  <div class="row hint">Až tři sítě, zapsat jdou i mimo dosah.
    <details><summary>Více</summary>Síť v kanceláři se tak nastaví ještě doma. Při startu se zkusí nejsilnější dosažitelná, po výpadku spojení deska minutu zkouší svou síť a pak projde ostatní. Prázdné heslo nechá uložené beze změny; prázdný název síť smaže.</details></div>
</div>
</section>

<section id="t-sprava" hidden>
<div class="card">
  <h2>Firmware</h2>
  <div class="row"><label for="fw">Soubor .ino.bin (bez „merged“)</label>
    <input type="file" id="fw" accept=".bin"></div>
  <div class="row"><button class="primary" onclick="flash()">Nahrát</button></div>
  <div id="bar"><i></i></div>
  <div class="row hint">Průběh je vidět i na displeji. Když se aktualizace nepovede, zůstane v desce původní verze.</div>
</div>

<div class="card">
  <h2>Správa</h2>
  <div class="row"><label for="ap">Heslo správce</label>
    <input type="password" id="ap" placeholder="heslo není nastavené"></div>
  <div class="row hint">Zadejte ho před obnovou ze zálohy, restartem, továrním resetem a změnou hesla. Aktualizace firmwaru se na něj zeptá sama (uživatel admin).</div>
  <div class="row">
    <button onclick="location.href='/api/export'">Stáhnout zálohu</button>
    <input type="file" id="imp" accept=".json" style="flex:0 1 200px">
    <button onclick="imprt()">Obnovit ze zálohy</button>
  </div>
  <div class="row" style="margin-top:10px">
    <button onclick="reboot()">Restartovat</button>
    <button class="danger" onclick="factory()">Tovární reset</button>
  </div>
  <div class="row hint">Běžná změna nastavení restart nevyžaduje.</div>
</div>

<div class="card">
  <h2>Změnit heslo správce</h2>
  <div class="row"><label for="np">Nové heslo (prázdné = bez ochrany)</label>
    <input type="password" id="np" maxlength="32"></div>
  <div class="row"><button onclick="chpw()">Změnit heslo</button></div>
  <div class="row hint">Je-li heslo nastavené, vyplňte nahoře i současné. Je to ochrana před domácností, ne před útočníkem na síti.</div>
</div>
</section>

<!-- Ulozit patri POD sekce, ne do nektere z nich. Prvni verze ho mela uvnitr
     zalozky Sprava, takze se skryl spolu s ni - a nastaveni ze zbylych
     zalozek neslo ulozit, aniz by uzivatel vedel, ze ma prejit jinam.
     Na zalozkach Stav, WiFi a Sprava neni co ukladat (maji vlastni tlacitka),
     tam se skryva - showTab(). -->
<div id="savebar" style="position:sticky;bottom:0;padding:10px 0;background:var(--bg)">
  <button class="primary" style="width:100%" onclick="save()">Uložit nastavení</button>
</div>

<p class="muted" style="text-align:center">
  Data: <a style="color:var(--acc)" href="https://adsb.fi">adsb.fi</a> ·
  <a style="color:var(--acc)" href="https://opendata.chmi.cz">ČHMÚ</a> ·
  <a style="color:var(--acc)" href="https://open-meteo.com">Open-Meteo</a> ·
  <a style="color:var(--acc)" href="https://adsb.lol">adsb.lol</a> ·
  <a style="color:var(--acc)" href="https://spotovaelektrina.cz">spotovaelektrina.cz</a> ·
  <a style="color:var(--acc)" href="https://energy-charts.info">Energy-Charts.info</a> (Fraunhofer ISE, CC BY 4.0) ·
  ikony letadel podle <a style="color:var(--acc)" href="https://adsb-radar.com/help/icons.html">ADS-B Radar</a> ·
  písma Inter a Poppins (SIL OFL 1.1)<br>
  Jen pro osobní nekomerční použití.
</p>
</main>
<div id="toast"></div>

<script>
const $=id=>document.getElementById(id);

// --- Jazyk -------------------------------------------------------------------
// Klic je cesky text (mezery sloucene), hodnota slovensky. {0}, {1} jsou
// mista pro cisla a nazvy, ktere doplni t().
const SK={"načítám…":"načítavam…","Změny se na displeji projeví až po klepnutí na":"Zmeny sa na displeji prejavia až po kliknutí na","Uložit nastavení":"Uložiť nastavenia","dole.":"dole.","◀ obrazovka":"◀ obrazovka","obrazovka ▶":"obrazovka ▶","− rozsah":"− rozsah","+ rozsah":"+ rozsah","Najít město":"Nájsť mesto","např. Brno":"napr. Žilina","Hledat":"Hľadať","Zeměpisná šířka":"Zemepisná šírka","Zeměpisná délka":"Zemepisná dĺžka","Jas ve dne":"Jas cez deň","Jas v noci":"Jas v noci","Posun proti slunci (min)":"Posun voči slnku (min)","Písmo (všechny obrazovky)":"Písmo (všetky obrazovky)","Inter tenké":"Inter tenké","původní pixelové":"pôvodné pixelové","Jazyk (displej i web)":"Jazyk (displej aj web)","Obrazovky":"Obrazovky","Střídat automaticky po (s)":"Striedať automaticky po (s)","Běh sekund na hodinách (elipsa)":"Beh sekúnd na hodinách (elipsa)","vypnutý":"vypnutý","tečky po obvodu":"bodky po obvode","čára":"čiara","kometa":"kométa","Meteoradar jen při bouřce":"Meteoradar len pri búrke","Zdroj":"Zdroj","Zobrazit legendu (dBZ / mm/h)":"Zobraziť legendu (dBZ / mm/h)","Cena elektřiny":"Cena elektriny","Přirážka k burzovní ceně (Kč/MWh)":"Prirážka k burzovej cene (Kč/MWh)","DPH (%, 0 = nepřipočítávat)":"DPH (%, 0 = nepripočítavať)","Výroba elektřiny":"Výroba elektriny","Země":"Krajina","Celá Evropa":"Celá Európa","Česko":"Česko","Slovensko":"Slovensko","Rakousko":"Rakúsko","Německo":"Nemecko","Polsko":"Poľsko","Maďarsko":"Maďarsko","Slovinsko":"Slovinsko","Chorvatsko":"Chorvátsko","Belgie":"Belgicko","Bulharsko":"Bulharsko","Švýcarsko":"Švajčiarsko","Dánsko":"Dánsko","Estonsko":"Estónsko","Španělsko":"Španielsko","Finsko":"Fínsko","Francie":"Francúzsko","Řecko":"Grécko","Irsko":"Írsko","Itálie":"Taliansko","Litva":"Litva","Lucembursko":"Luxembursko","Lotyšsko":"Lotyšsko","Nizozemsko":"Holandsko","Norsko":"Nórsko","Portugalsko":"Portugalsko","Rumunsko":"Rumunsko","Srbsko":"Srbsko","Švédsko":"Švédsko","Ukrajina":"Ukrajina","Velká Británie":"Veľká Británia","Letadla":"Lietadlá","Metrické jednotky (m, km/h)":"Metrické jednotky (m, km/h)","Jen letadla s volacím znakem":"Len lietadlá s volacím znakom","Hlásit nouzový squawk (7500/7600/7700)":"Hlásiť núdzový squawk (7500/7600/7700)","Hlídaný volací znak":"Sledovaný volací znak","např. CSA":"napr. TVS","Výhled z okna":"Výhľad z okna","Letadlo ve výhledu má při upozornění přednost před bližším letadlem, které z okna vidět není — letadlo 3 km za domem je blíž než to 9 km před oknem, ale uvidíte jen to druhé. Výseč se zakreslí do radaru letadel.":"Lietadlo vo výhľade má pri upozornení prednosť pred bližším lietadlom, ktoré z okna nevidno — lietadlo 3 km za domom je bližšie ako to 9 km pred oknom, ale uvidíte len to druhé. Výsek sa zakreslí do radaru lietadiel.","Barvy podle výšky":"Farby podľa výšky","Ikony letadel":"Ikony lietadiel","Zpřístupnit data pro Home Assistant":"Sprístupniť dáta pre Home Assistant","Adresa:":"Adresa:","Zkopírovat":"Skopírovať","Vyhledat sítě":"Vyhľadať siete","Název sítě":"Názov siete","Heslo":"Heslo","Připojit":"Pripojiť","Uložené sítě":"Uložené siete","Uložit sítě":"Uložiť siete","Firmware":"Firmvér","Soubor .ino.bin (bez „merged“)":"Súbor .ino.bin (bez „merged“)","Nahrát":"Nahrať","Průběh je vidět i na displeji. Když se aktualizace nepovede, zůstane v desce původní verze.":"Priebeh je vidieť aj na displeji. Keď sa aktualizácia nepodarí, zostane v doske pôvodná verzia.","Správa":"Správa","Stáhnout zálohu":"Stiahnuť zálohu","Obnovit ze zálohy":"Obnoviť zo zálohy","Restartovat":"Reštartovať","Tovární reset":"Továrenský reset","Data:":"Dáta:","(Fraunhofer ISE, CC BY 4.0) · ikony letadel podle":"(Fraunhofer ISE, CC BY 4.0) · ikony lietadiel podľa","· písma Inter a Poppins (SIL OFL 1.1)":"· písma Inter a Poppins (SIL OFL 1.1)","Jen pro osobní nekomerční použití.":"Len na osobné nekomerčné použitie.","Poloha a displej":"Poloha a displej","Energie":"Energia","Hodiny":"Hodiny","Předpověď":"Predpoveď","Uložená síť: {0}":"Uložená sieť: {0}","Žádná síť není uložená.":"Žiadna sieť nie je uložená.","Hranice (tisíce ft)":"Hranice (tisíce ft)","úzkotrupé — A320, B737, B757":"úzkotrupé — A320, B737, B757","širokotrupé dvoumotorové — A330, A350, B767, B777, B787":"širokotrupé dvojmotorové — A330, A350, B767, B777, B787","čtyřmotorové — A340, A380, B747":"štvormotorové — A340, A380, B747","regionální — CRJ, Embraer E-Jet, Fokker 100":"regionálne — CRJ, Embraer E-Jet, Fokker 100","business jet — Citation, Learjet, Falcon, Gulfstream":"business jet — Citation, Learjet, Falcon, Gulfstream","turbovrtulové — ATR, Dash 8, King Air, L 410":"turbovrtuľové — ATR, Dash 8, King Air, L 410","malá letadla — Cessna, Piper, Zlin, ultralighty":"malé lietadlá — Cessna, Piper, Zlín, ultraľahké","vojenská transportní — C-130, C-160, A400M":"vojenské transportné — C-130, C-160, A400M","těžká nákladní — An-124, C-17, C-5, Il-76":"ťažké nákladné — An-124, C-17, C-5, Il-76","stíhačky a cvičné proudové":"stíhačky a cvičné prúdové","kluzáky a motorové kluzáky":"vetrone a motorové vetrone","konvertoplány — V-22, AW609":"konvertoplány — V-22, AW609","vrtulníky":"vrtuľníky","balony a vzducholodě":"balóny a vzducholode","drony":"drony","nezjištěný typ":"nezistený typ","neznámý směr letu":"neznámy smer letu","Zkopírováno":"Skopírované","Sítě uloženy":"Siete uložené","Chyba: {0}":"Chyba: {0}","Uloženo":"Uložené","hledám…":"hľadám…","nic nenalezeno":"nič sa nenašlo","nedostupné: {0}":"nedostupné: {0}","Poloha vyplněna, nezapomeňte uložit":"Poloha vyplnená, nezabudnite uložiť","hledám sítě…":"hľadám siete…","(skrytá)":"(skrytá)"," · otevřená":" · otvorená","žádná síť":"žiadna sieť","Připojuji — sledujte displej":"Pripájam — sledujte displej","Restartovat desku?":"Reštartovať dosku?","Restartuji…":"Reštartujem…","Smazat VŠECHNO nastavení včetně WiFi a restartovat?":"Zmazať VŠETKY nastavenia vrátane WiFi a reštartovať?","Mažu…":"Mažem…","Vyberte soubor":"Vyberte súbor","Obnoveno, restartuji…":"Obnovené, reštartujem…","Nahrát {0} ({1} kB)?":"Nahrať {0} ({1} kB)?","Hotovo, deska se restartuje":"Hotovo, doska sa reštartuje","Aktualizace selhala":"Aktualizácia zlyhala","Spojení přerušeno":"Spojenie prerušené","Nelze načíst nastavení: {0}":"Nedajú sa načítať nastavenia: {0}","název sítě":"názov siete","heslo uložené":"heslo uložené","heslo":"heslo","deska neodpovídá":"doska neodpovedá","verze {0}":"verzia {0}","přístupový bod":"prístupový bod","síť {0}":"sieť {0}","Čas v desce":"Čas v doske","Signál":"Signál","Běží":"Beží","Volná paměť":"Voľná pamäť","{0} kB interní · {1} kB PSRAM":"{0} kB interná · {1} kB PSRAM","Časové pásmo":"Časové pásmo","Bouřka":"Búrka","Letadla poblíž":"Lietadlá nablízku","Režim":"Režim","noční":"nočný","denní":"denný","Střídání obrazovek":"Striedanie obrazoviek","vypnuto":"vypnuté","po {0} s — pozastaveno ještě {1} s":"po {0} s — pozastavené ešte {1} s","po {0} s":"po {0} s","Odpovídá {0} Kč/kWh bez DPH.":"Zodpovedá {0} Kč/kWh bez DPH.","MPR Teplota":"MPR Teplota","MPR Vitr":"MPR Vietor","MPR Srazky":"MPR Zrazky","MPR Bourka max dBZ":"MPR Burka max dBZ","MPR Bourka vzdalenost":"MPR Burka vzdialenost","MPR Letadla v okoli":"MPR Lietadla v okoli","MPR Nejblizsi letadlo":"MPR Najblizsie lietadlo","MPR Cena elektriny":"MPR Cena elektriny","MPR Bourka":"MPR Burka","MPR Letadlo nablizku":"MPR Lietadlo nablizku","Více":"Viac","Po uložení si deska znovu stáhne předpověď i mapu, restart není potřeba.":"Po uložení si doska znova stiahne predpoveď aj mapu, reštart nie je potrebný.","Přepínat den a noc podle slunce":"Prepínať deň a noc podľa slnka","Kladná hodnota začne noc dřív a ukončí ji později.":"Kladná hodnota začne noc skôr a ukončí ju neskôr.","Hodí se pro pokoj, který se stmívá dřív než obloha. Časy východu a západu přicházejí spolu s předpovědí, takže automatika nestojí žádný požadavek navíc.":"Hodí sa pre izbu, v ktorej sa stmieva skôr ako vonku. Časy východu a západu prichádzajú spolu s predpoveďou, takže automatika nestojí žiadnu požiadavku navyše.","Vyhlazená písma mají diakritiku.":"Vyhladené písma majú diakritiku.","Displej pak píše „Předpověď“ a „Út 5. listopadu“. Rozložení obrazovek se nemění — písmena jsou stejně vysoká jako u pixelového písma a text nikdy není širší.":"Displej potom píše „Predpoveď“ a „Št 5. novembra“. Rozloženie obrazoviek sa nemení — písmená sú rovnako vysoké ako pri pixelovom písme a text nikdy nie je širší.","0 = vypnuto.":"0 = vypnuté.","Swipe nebo dlouhý stisk střídání dočasně pozastaví (desetinásobek intervalu, nejméně 30 s a nejvýše 10 minut) — kolik zbývá, je vidět v záložce Stav. Nastavení vypnout nejde. Když vypnete právě zobrazenou obrazovku, deska přepne na první zapnutou.":"Swipe alebo dlhé stlačenie striedanie dočasne pozastaví (desaťnásobok intervalu, najmenej 30 s a najviac 10 minút) — koľko zostáva, je vidieť na karte Stav. Nastavenia vypnúť nejdú. Keď vypnete práve zobrazenú obrazovku, doska prepne na prvú zapnutú.","ČHMÚ masked ukazuje plnou barvou jen srážky, které opravdu dopadají na zem.":"ČHMÚ masked ukazuje plnou farbou len zrážky, ktoré naozaj dopadajú na zem.","ČHMÚ snímek porovná s měřením ve výšce 2 km; srážky, které se cestou vypaří, a kovadliny vzdálených bouřek jsou bledší. Na otázku „bude u nás pršet?“ odpovídá přesněji, pro sledování bouřek je lepší plný ČHMÚ.":"ČHMÚ snímku porovná s meraním vo výške 2 km; zrážky, ktoré sa cestou vyparia, a kovadliny vzdialených búrok sú bledšie. Na otázku „bude u nás pršať?“ odpovedá presnejšie, na sledovanie búrok je lepší plný ČHMÚ.","Stupnice zabírá levý horní roh mapy; bez ní je vidět víc území.":"Stupnica zaberá ľavý horný roh mapy; bez nej vidno viac územia.","Odraz aspoň (dBZ)":"Odraz aspoň (dBZ)","Do vzdálenosti (km)":"Do vzdialenosti (km)","Meteoradar se do střídání přidá, jen když je v okolí bouřka.":"Meteoradar sa do striedania pridá, len keď je v okolí búrka.","Každých 5 minut se zkontroluje nejnovější snímek ČHMÚ; po dvou klidných snímcích meteoradar zase vypadne. Funguje i s vypnutou obrazovkou Meteoradar. Jen v pokrytí ČHMÚ.":"Každých 5 minút sa skontroluje najnovšia snímka ČHMÚ; po dvoch pokojných snímkach meteoradar zase vypadne. Funguje aj s vypnutou obrazovkou Meteoradar. Len v pokrytí ČHMÚ (zachytí aj západné Slovensko).","S nulami ukazuje displej čistou burzovní cenu a napíše k ní „burza“.":"S nulami ukazuje displej čistú burzovú cenu a napíše k nej „burza“.","Distribuci, regulované složky a marži obchodníka si opište z vyúčtování; ceník je obvykle uvádí v Kč/kWh, takže je vynásobte tisícem. Deska si nic nedomýšlí: připočte přesně to, co sem zadáte.":"Distribúciu, regulované zložky a maržu obchodníka si odpíšte z vyúčtovania; cenník ich zvyčajne uvádza v Kč/kWh, takže ich vynásobte tisícom. Doska si nič nedomýšľa: pripočíta presne to, čo sem zadáte.","Čtvrthodinové ceny OTE (přes spotovaelektrina.cz), jen pro Česko. Zítřek vychází kolem 14:00.":"Štvrťhodinové ceny českého OTE (cez spotovaelektrina.cz), len pre Česko, nie pre Slovensko. Zajtrajšok vychádza okolo 14:00.","Z čeho se právě vyrábí, podíl obnovitelných zdrojů, spotřeba a přeshraniční toky.":"Z čoho sa práve vyrába, podiel obnoviteľných zdrojov, spotreba a cezhraničné toky.","Data ENTSO-E přes rozhraní energy-charts vycházejí se zpožděním jedné až dvou hodin — čas vzorku je proto napsaný na obrazovce. Oba zdroje jsou zdarma, bez klíče a bez registrace. Obrazovky se zapínají na záložce Obrazovky; dokud jsou vypnuté, deska si o data vůbec neřekne. Mimo Česko (cena) a mimo Evropu (výroba) se po změně polohy samy vypnou.":"Dáta ENTSO-E cez rozhranie energy-charts vychádzajú s oneskorením jednej až dvoch hodín — čas vzorky je preto napísaný na obrazovke. Oba zdroje sú zadarmo, bez kľúča a bez registrácie. Obrazovky sa zapínajú na karte Obrazovky; kým sú vypnuté, doska si o dáta vôbec nepýta. Mimo Česka (cena) a mimo Európy (výroba) sa po zmene polohy samy vypnú.","Okno míří na (nahoře na radaru)":"Okno smeruje na (hore na radare)","Radar se otočí tak, aby letadla na displeji mířila stejně jako ta za oknem. Stejný směr používá výhled z okna.":"Radar sa otočí tak, aby lietadlá na displeji mierili rovnako ako tie za oknom. Rovnaký smer používa výhľad z okna.","Letadlo v nouzi se zobrazí i mimo nastavené výšky.":"Lietadlo v núdzi sa zobrazí aj mimo nastavených výšok.","Porovnává se začátek znaku, takže „CSA“ chytí všechny lety CSA.":"Porovnáva sa začiatok znaku, takže „TVS“ zachytí všetky lety Smartwings.","Zakreslit a upřednostnit výhled z okna":"Zakresliť a uprednostniť výhľad z okna","Šířka výhledu (°)":"Šírka výhľadu (°)","Dohled (km)":"Dohľad (km)","Směr okna je nahoře v kartě Letadla.":"Smer okna je hore v karte Lietadlá.","Letadlo poblíž":"Lietadlo nablízku","Ukázat Letadla, jen když je letadlo poblíž":"Ukázať Lietadlá, len keď je lietadlo nablízku","Vzdálenost (km)":"Vzdialenosť (km)","Ukázat na (s)":"Ukázať na (s)","Obrazovka Letadla se sama nestřídá; ukáže se, až se letadlo přiblíží nebo proletí výhledem z okna.":"Obrazovka Lietadlá sa sama nestrieda; ukáže sa, keď sa lietadlo priblíži alebo preletí výhľadom z okna.","Ukáže se i s otevřeným detailem a pak se pokračuje tam, kde se skončilo. Každé letadlo jednou za 15 minut. Dotyk ukázku ukončí.":"Ukáže sa aj s otvoreným detailom a potom sa pokračuje tam, kde sa skončilo. Každé lietadlo raz za 15 minút. Dotyk ukážku ukončí.","Hranice v tisících stop (FL100 = 10 000 ft), vzestupně. Letadlo bez údaje o výšce je šedé.":"Hranice v tisícoch stôp (FL100 = 10 000 ft), vzostupne. Lietadlo bez údaja o výške je sivé.","pod {0} tis. ft":"pod {0} tis. ft","nad {0} tis. ft":"nad {0} tis. ft","{0}–{1} tis. ft":"{0}–{1} tis. ft","Tvar podle typu letadla, velikost podle hmotnosti, barva podle výšky.":"Tvar podľa typu lietadla, veľkosť podľa hmotnosti, farba podľa výšky.","Typ se pozná z kódu typu (A20N, B738, EC35…), u neznámého z kategorie, kterou vysílá transpondér. Rozdělení do ikon vychází ze schématu":"Typ sa spozná z kódu typu (A20N, B738, EC35…), pri neznámom z kategórie, ktorú vysiela transpondér. Rozdelenie do ikon vychádza zo schémy","⚠ Nevyzkoušeno se skutečným Home Assistantem.":"⚠ Nevyskúšané so skutočným Home Assistantom.","Formát odpovídá dokumentaci integrace RESTful. Pokud něco nesedí, dejte vědět.":"Formát zodpovedá dokumentácii integrácie RESTful. Ak niečo nesedí, dajte vedieť.","Do configuration.yaml vložte tohle a Home Assistant restartujte:":"Do configuration.yaml vložte toto a Home Assistant reštartujte:","Data jsou jen ke čtení a bez hesla.":"Dáta sú len na čítanie a bez hesla.","Home Assistant si je sám stahuje integrací RESTful — bez MQTT, brokeru i doplňku. Neříkají nic, co by nebylo vidět na displeji. Dokud je funkce zapnutá, deska hlídá bouřku (jeden snímek ČHMÚ za 5 minut) a letadla poblíž (malý dotaz jednou za 30 s).":"Home Assistant si ich sám sťahuje integráciou RESTful — bez MQTT, brokera aj doplnku. Nehovoria nič, čo by nebolo vidieť na displeji. Kým je funkcia zapnutá, doska sleduje búrku (jedna snímka ČHMÚ za 5 minút) a lietadlá nablízku (malý dopyt raz za 30 s).","Deska opustí přístupový bod a stránka se odpojí; pak ji najdete na adrese nahoře.":"Doska opustí prístupový bod a stránka sa odpojí; potom ju nájdete na adrese hore.","Až tři sítě, zapsat jdou i mimo dosah.":"Až tri siete, zapísať sa dajú aj mimo dosahu.","Síť v kanceláři se tak nastaví ještě doma. Při startu se zkusí nejsilnější dosažitelná, po výpadku spojení deska minutu zkouší svou síť a pak projde ostatní. Prázdné heslo nechá uložené beze změny; prázdný název síť smaže.":"Sieť v kancelárii sa tak nastaví ešte doma. Pri štarte sa skúsi najsilnejšia dostupná, po výpadku spojenia doska minútu skúša svoju sieť a potom prejde ostatné. Prázdne heslo nechá uložené bez zmeny; prázdny názov sieť zmaže.","Heslo správce":"Heslo správcu","heslo není nastavené":"heslo nie je nastavené","současné heslo":"súčasné heslo","Zadejte ho před obnovou ze zálohy, restartem, továrním resetem a změnou hesla. Aktualizace firmwaru se na něj zeptá sama (uživatel admin).":"Zadajte ho pred obnovou zo zálohy, reštartom, továrenským resetom a zmenou hesla. Aktualizácia firmvéru sa naň opýta sama (používateľ admin).","Běžná změna nastavení restart nevyžaduje.":"Bežná zmena nastavení reštart nevyžaduje.","Změnit heslo správce":"Zmeniť heslo správcu","Nové heslo (prázdné = bez ochrany)":"Nové heslo (prázdne = bez ochrany)","Změnit heslo":"Zmeniť heslo","Je-li heslo nastavené, vyplňte nahoře i současné. Je to ochrana před domácností, ne před útočníkem na síti.":"Ak je heslo nastavené, vyplňte hore aj súčasné. Je to ochrana pred domácnosťou, nie pred útočníkom na sieti.","Vypnout ochranu heslem?":"Vypnúť ochranu heslom?","Heslo změněno":"Heslo zmenené","Ochrana heslem vypnuta":"Ochrana heslom vypnutá","Špatné heslo správce (záložka Správa)":"Nesprávne heslo správcu (karta Správa)","Výška od ({0})":"Výška od ({0})","Výška do ({0})":"Výška do ({0})","Hranice (km)":"Hranice (km)","Hranice v kilometrech, vzestupně. Letadlo bez údaje o výšce je šedé.":"Hranice v kilometroch, vzostupne. Lietadlo bez údaja o výške je sivé.","pod {0} km":"pod {0} km","nad {0} km":"nad {0} km","{0}–{1} km":"{0}–{1} km"};
let L='cs';
const norm=x=>x.replace(/\s+/g,' ').trim();
function t(cs,...a){const r=(L==='sk'&&SK[cs])||cs;return r.replace(/\{(\d)\}/g,(_,i)=>a[i]);}
// Prelozi texty a zastupne texty poli na strance. Puvodni cesky text si uzel
// pamatuje, takze prepnuti zpet na cestinu funguje i bez nacteni stranky.
function applyLang(){
  document.documentElement.lang=L;
  const w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT,{acceptNode:n=>
    n.parentNode.closest('script,style,pre,code')?NodeFilter.FILTER_REJECT:NodeFilter.FILTER_ACCEPT});
  for(let n;(n=w.nextNode());){
    if(n._cs===undefined)n._cs=n.nodeValue;
    const k=norm(n._cs);if(!k)continue;
    const v=L==='sk'&&SK[k];
    n.nodeValue=v?n._cs.replace(n._cs.trim(),v):n._cs;
  }
  document.querySelectorAll('[placeholder]').forEach(e=>{
    if(e._cs===undefined)e._cs=e.placeholder;
    e.placeholder=(L==='sk'&&SK[e._cs])||e._cs;});
}
function setLang(v){L=(+v===1)?'sk':'cs';$('lg').value=(L==='sk')?'1':'0';
  applyLang();altUnits();fillAltFilter(altFilterFt('alo'),altFilterFt('ahi'));iconLegend();haInfo();feeHint();}

// --- Zalozky ---------------------------------------------------------------
// Zvolena zalozka se pamatuje v adrese (#letadla), takze po ulozeni nebo po
// obnoveni stranky zustanete tam, kde jste byli.
const TABS=[['stav','Stav'],['misto','Poloha a displej'],['obrazovky','Obrazovky'],
            ['letadla','Letadla'],['energie','Energie'],['sit','WiFi'],['ha','Home Assistant'],['sprava','Správa']];
function showTab(id){
  if(!TABS.some(t=>t[0]===id)) id=TABS[0][0];
  TABS.forEach(([k])=>{
    const sec=$('t-'+k); if(sec) sec.hidden=(k!==id);
    const b=$('tab-'+k); if(b) b.setAttribute('aria-selected', k===id);
  });
  // Ulozit je na VSECH zalozkach. Skryvat ho tam, kde zrovna "neni co
  // ulozit", znamena, ze ho uzivatel jednou nenajde a bude si myslet, ze se
  // nastaveni ukladaji sama.
  // Stav, WiFi a Sprava maji vlastni tlacitka a nic, co by Ulozit ulozilo.
  const noSave=['stav','sit','sprava'].includes(id);
  $('savebar').hidden=noSave; $('savenote').hidden=noSave;
  history.replaceState(null,'','#'+id);
  window.scrollTo(0,0);
}
function buildTabs(){
  $('tabs').innerHTML=TABS.map(([k,label])=>
    `<button id="tab-${k}" role="tab" aria-selected="false" onclick="showTab('${k}')">${label}</button>`).join('');
  showTab(location.hash.replace('#',''));
}
let CFG={};
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('on');
  clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('on'),2600);}
async function api(p,o){const r=await fetch(p,o);
  if(!r.ok){let e='';try{e=(await r.json()).error||''}catch(_){}
    if(e==='heslo')e=t('Špatné heslo správce (záložka Správa)');
    throw new Error(e||('HTTP '+r.status));}
  // Vsechny koncove body vraceji JSON; prazdna odpoved se bere jako {}.
  return r.json().catch(()=>({}));}
function pw(){return $('ap').value;}

const DIRS=[['S',0],['SV',45],['V',90],['JV',135],['J',180],['JZ',225],['Z',270],['SZ',315]];

async function loadCfg(){
  CFG=await api('/api/config');
  L=(CFG.lang==1)?'sk':'cs';
  $('lat').value=(+CFG.lat).toFixed(4);
  $('lon').value=(+CFG.lon).toFixed(4);
  $('bd').value=CFG.brightDay; $('bdv').textContent=CFG.brightDay;
  $('bn').value=CFG.brightNight; $('bnv').textContent=CFG.brightNight;
  $('na').checked=CFG.nightAuto; $('no').value=CFG.nightOffset;
  $('rot').value=CFG.autoRotateSec; $('sec').value=CFG.secondsStyle;
  $('mu').checked=CFG.metric;
  UM=!!CFG.metric; fillAltFilter(CFG.altMinFt,CFG.altMaxFt);
  $('oc').checked=CFG.onlyWithCallsign; $('sa').checked=CFG.squawkAlert;
  $('wc').value=CFG.watchCallsign||'';
  $('tb').innerHTML=DIRS.map(d=>`<option value="${d[1]}">${d[0]} (${d[1]}°)</option>`).join('');
  $('tb').value=CFG.topBearing;
  $('ml').checked=CFG.meteoLegend!==false;
  $('pf').value=CFG.priceFee||0; $('pv').value=CFG.priceVat||0; feeHint();
  $('mc').value=CFG.mixCountry||'cz';
  const names=['Hodiny','Letadla','Meteoradar','Předpověď','Cena elektřiny','Výroba elektřiny'].map(x=>t(x));
  $('scrs').innerHTML=names.map((n,i)=>
    `<label class="row" style="margin:0"><input type="checkbox" id="sc${i}"
      ${(CFG.screenMask>>i)&1?'checked':''}> ${n}</label>`).join('');
  $('wifinow').textContent=CFG.wifiSsid?t('Uložená síť: {0}',CFG.wifiSsid):t('Žádná síť není uložená.');
  $('ap')._cs=CFG.hasPassword?'současné heslo':'heslo není nastavené';
  // --- 0.8.x ---
  $('cf').value=CFG.clockFont??1; $('rs').value=CFG.radarSrc==2?2:0;
  $('so').checked=!!CFG.stormOnly; $('sd').value=CFG.stormDbz; $('sk').value=CFG.stormKm;
  $('pa').checked=!!CFG.planeAlert; $('pk').value=CFG.planeAlertKm; $('ps').value=CFG.planeAlertSec;
  $('wo').checked=!!CFG.windowOn; $('ww').value=CFG.windowWidth; $('wk').value=CFG.windowKm;
  $('ha').checked=!!CFG.haEnabled; haInfo();
  altUi(); iconLegend(); deps();
  setLang(CFG.lang);
}

// --- Barvy vysek: RGB565 <-> #rrggbb ---------------------------------------
const to565=h=>{const v=parseInt(h.slice(1),16);return((v>>19&31)<<11)|((v>>10&63)<<5)|(v>>3&31);};
const from565=c=>{const r=(c>>11&31)*255/31,g=(c>>5&63)*255/63,b=(c&31)*255/31;
  return '#'+[r,g,b].map(x=>Math.round(x).toString(16).padStart(2,'0')).join('');};
// Ctyri hranice v jedne mrizce (na telefonu se driv zalamovaly kazda jinam)
// a u kazde barvy rozsah vysek, ktery opravdu znamena - pocita se z poli
// hranic, takze se meni, jak uzivatel pise.
// --- Jednotky vysek ------------------------------------------------------------
// Deska uklada vysky ve stopach. Na webu se ale zadavaji v jednotkach, ktere
// jsou zrovna zvolene (Metricke jednotky): filtr vysek v metrech nebo stopach,
// hranice barev v kilometrech nebo tisicich stop. Prepnuti zaskrtavatka
// prepocita, co je v polich napsane, a hodnoty se ulozi zpet ve stopach.
const FT=0.3048;
let UM=false;                                  // jednotky, ve kterych jsou pole ted
const r1=x=>Math.round(x*10)/10;
const bandOut=ft=>UM?r1(ft*FT/1000):r1(ft/1000);          // km / tis. ft
const bandIn=v=>Math.round(UM?v*1000/FT:v*1000);          // -> ft
const filtOut=ft=>UM?Math.round(ft*FT/50)*50:ft;
// Pole si pamatuje puvodni hodnotu ve stopach (data-ft). Dokud ji uzivatel
// nezmeni, ulozi se zpet presne ta - jinak by kazde Ulozit v metrickych
// jednotkach hranice nenapadne posunulo (3000 ft -> 0,9 km -> 2953 ft).
const keepFt=(e,ft,shown)=>{e.value=shown;e.dataset.ft=ft;e.dataset.shown=shown;};
const readFt=(e,conv)=>(e.dataset.ft!==undefined&&String(e.value)===e.dataset.shown)?+e.dataset.ft:conv(+e.value);
function fillAltFilter(lo,hi){
  keepFt($('alo'),lo,filtOut(lo)); keepFt($('ahi'),hi,filtOut(hi));
  for(const [id,cs] of [['alo','Výška od ({0})'],['ahi','Výška do ({0})']]){
    const e=$(id);e.step=UM?50:500;e.max=UM?18000:60000;
    document.querySelector(`label[for=${id}]`).textContent=t(cs,UM?'m':'ft');}
}
const altFilterFt=id=>readFt($(id),v=>Math.round(UM?v/FT:v));
const bandFt=i=>readFt($('ab'+i),bandIn);
function unitsChanged(){
  const lo=altFilterFt('alo'),hi=altFilterFt('ahi');
  const b=[0,1,2,3].map(bandFt);
  UM=$('mu').checked;
  fillAltFilter(lo,hi);
  b.forEach((ft,i)=>keepFt($('ab'+i),ft,bandOut(ft)));
  altUnits();
}
function altUnits(){
  for(let i=0;i<4;i++){const e=$('ab'+i);e.step=UM?0.1:0.5;e.min=UM?0.1:0.5;e.max=UM?18:60;}
  const l=document.querySelector('#altc label.abh');
  if(l)l.textContent=UM?t('Hranice (km)'):t('Hranice (tisíce ft)');
  $('altchint').textContent=UM?t('Hranice v kilometrech, vzestupně. Letadlo bez údaje o výšce je šedé.')
    :t('Hranice v tisících stop (FL100 = 10 000 ft), vzestupně. Letadlo bez údaje o výšce je šedé.');
  altLabels();
}
function altUi(){
  const b=CFG.altBandsFt||[3000,10000,20000,30000],c=CFG.altColors||[63488,64512,65504,2016,735];
  let h='<div class="row"><label class="abh"></label><div class="abgrid">'+
    b.map((v,i)=>`<input type="number" id="ab${i}" value="${bandOut(v)}" data-ft="${v}" data-shown="${bandOut(v)}" oninput="altLabels()" aria-label="${i+1}.">`).join('')+'</div></div>';
  h+=c.map((v,i)=>`<div class="row"><label for="ac${i}"></label><input type="color" id="ac${i}" value="${from565(v)}"></div>`).join('');
  $('altc').innerHTML=h;
  altUnits();
}
function altLabels(){
  const n=4,v=i=>{const e=$('ab'+i);return e?String(+e.value).replace('.',','):'?';};
  const u=UM?['pod {0} km','nad {0} km','{0}–{1} km']:['pod {0} tis. ft','nad {0} tis. ft','{0}–{1} tis. ft'];
  for(let i=0;i<=n;i++){const l=document.querySelector(`label[for=ac${i}]`);if(!l)continue;
    l.textContent=i==0?t(u[0],v(0)):i==n?t(u[1],v(n-1)):t(u[2],v(i-1),v(i));}
}

// Pole, ktera patri k vypnute funkci, nejsou videt (<div class="dep" data-dep="id">).
function deps(){document.querySelectorAll('.dep').forEach(d=>{d.hidden=!$(d.dataset.dep).checked;});}
document.querySelectorAll('.dep').forEach(d=>$(d.dataset.dep).addEventListener('change',deps));

// --- Ikony letadel: tytez polygony, ktere kresli PlaneIcon.cpp ------------------
const ICN={icNarrow:"úzkotrupé — A320, B737, B757",icWide:"širokotrupé dvoumotorové — A330, A350, B767, B777, B787",
  icQuad:"čtyřmotorové — A340, A380, B747",icRegional:"regionální — CRJ, Embraer E-Jet, Fokker 100",
  icBizjet:"business jet — Citation, Learjet, Falcon, Gulfstream",icTwinprop:"turbovrtulové — ATR, Dash 8, King Air, L 410",
  icLight:"malá letadla — Cessna, Piper, Zlin, ultralighty",icMiltrans:"vojenská transportní — C-130, C-160, A400M",
  icFreighter:"těžká nákladní — An-124, C-17, C-5, Il-76",icFighter:"stíhačky a cvičné proudové",
  icGlider:"kluzáky a motorové kluzáky",icTilt:"konvertoplány — V-22, AW609",icHeli:"vrtulníky",
  icBalloon:"balony a vzducholodě",icDrone:"drony",icGeneric:"nezjištěný typ",icNoTrack:"neznámý směr letu"};
const SH={"GENERIC":[10,0,0,12,3,1,13,-8,3,-5,3,-7,0,-12,-3,-7,-3,-5,-13,-8,-3,1,0],"NARROW":[7,0,0,14,2,11,2,-10,1,-14,-1,-14,-2,-10,-2,11,4,1,2,4,13,-3,13,-5,2,-1,4,1,4,1,6,0,6,-3,4,-3,4,1,1,-9,6,-12,6,-14,1,-12,0],"WIDE":[7,0,0,14,3,10,3,-10,1,-14,-1,-14,-3,-10,-3,10,4,1,3,4,15,-4,15,-6,3,-1,4,1,5,2,8,1,8,-3,5,-3,4,1,1,-9,7,-12,7,-14,1,-12,0],"QUAD":[7,0,0,14,3,10,3,-10,1,-14,-1,-14,-3,-10,-3,10,4,1,3,4,15,-4,15,-6,3,-1,4,1,5,2,7,1,7,-2,5,-2,4,1,9,-1,11,-2,11,-5,9,-5,4,1,1,-9,7,-12,7,-14,1,-12,0],"REGIONAL":[7,0,0,13,2,10,2,-9,1,-13,-1,-13,-2,-9,-2,10,4,1,2,3,11,-2,11,-4,2,-1,4,1,2,-5,4,-5,4,-9,2,-9,4,1,0,-11,6,-12,6,-13,0,-13,0],"BIZJET":[7,0,0,12,2,8,2,-8,1,-12,-1,-12,-2,-8,-2,8,4,1,2,2,10,-4,10,-5,2,-1,4,1,2,-5,4,-5,4,-8,2,-8,4,1,0,-10,5,-12,5,-13,0,-12,0],"TWINPROP":[6,0,0,13,2,10,2,-11,0,-13,-2,-11,-2,10,4,1,2,3,14,2,14,-1,2,-1,4,1,4,6,6,6,6,-1,4,-1,4,1,1,-9,6,-10,6,-12,1,-12,0],"LIGHT":[6,0,0,10,2,8,2,-9,0,-11,-2,-9,-2,8,4,1,2,5,14,5,14,2,2,2,4,1,1,-8,5,-8,5,-10,1,-10,0],"MILTRANS":[6,0,0,13,3,10,3,-10,0,-13,-3,-10,-3,10,4,1,3,4,16,3,16,0,3,0,4,1,5,7,7,7,7,0,5,0,4,1,10,6,12,6,12,0,10,0,4,1,1,-9,7,-10,7,-12,1,-12,0],"FREIGHTER":[7,0,0,14,3,10,3,-10,1,-14,-1,-14,-3,-10,-3,10,4,1,3,5,16,-1,16,-3,3,1,4,1,6,3,8,2,8,-1,6,-1,4,1,11,1,13,0,13,-3,11,-3,4,1,0,-11,7,-13,7,-14,0,-14,0],"FIGHTER":[6,0,0,13,2,8,2,-11,1,-13,-1,-13,-2,-11,3,1,1,6,10,-6,1,-6,3,1,1,-8,5,-12,1,-12,0],"GLIDER":[6,0,0,10,1,8,1,-10,0,-11,-1,-10,-1,8,4,1,1,3,17,2,17,1,1,1,4,1,0,-9,4,-9,4,-10,0,-10,0],"TILT":[6,0,0,12,2,9,2,-10,0,-12,-2,-10,-2,9,4,1,2,2,10,2,10,-1,2,-1,4,1,1,-8,5,-9,5,-11,1,-11,0]};
const ICONS=[ // [shape, scale, key] - order and sizes as on the display
 ["NARROW",1,"icNarrow"],["WIDE",1.25,"icWide"],["QUAD",1.4,"icQuad"],["REGIONAL",.95,"icRegional"],
 ["BIZJET",.85,"icBizjet"],["TWINPROP",.9,"icTwinprop"],["LIGHT",.8,"icLight"],["MILTRANS",1.15,"icMiltrans"],
 ["FREIGHTER",1.4,"icFreighter"],["FIGHTER",.9,"icFighter"],["GLIDER",.85,"icGlider"],["TILT",1,"icTilt"],
 ["HELI",1,"icHeli"],["BALLOON",1,"icBalloon"],["DRONE",1,"icDrone"],["GENERIC",1,"icGeneric"],["UNKNOWN",1,"icNoTrack"]];
function iconSvg(name,k){
 const c="#e8edf4";let b="";
 if(name=="HELI")b="<circle r='11' fill='none' stroke='"+c+"' stroke-width='1.3'/><circle r='4' fill='"+c+"'/><path d='M0 0V13M-3 13H3' stroke='"+c+"' stroke-width='2'/>";
 else if(name=="BALLOON")b="<circle cy='-3' r='7' fill='"+c+"'/><path d='M-3 3L-2 7M3 3L2 7' stroke='"+c+"'/><rect x='-2' y='7' width='5' height='3' fill='"+c+"'/>";
 else if(name=="DRONE")b="<path d='M-7-7L7 7M-7 7L7-7' stroke='"+c+"' stroke-width='1.3'/>"+[[-7,-7],[7,-7],[-7,7],[7,7]].map(p=>"<circle cx='"+p[0]+"' cy='"+p[1]+"' r='3' fill='none' stroke='"+c+"'/>").join("")+"<rect x='-2' y='-2' width='5' height='5' fill='"+c+"'/>";
 else if(name=="UNKNOWN")b="<circle r='7' fill='none' stroke='"+c+"' stroke-width='1.3'/><circle r='2' fill='"+c+"'/>";
 else{const a=SH[name];let i=0;while(a[i]){const n=a[i],m=a[i+1],v=a.slice(i+2,i+2+2*n);
  for(const sg of (m?[1,-1]:[1]))b+="<polygon fill='"+c+"' points='"+v.map((x,j)=>j%2?(-x*k).toFixed(1):(sg*x*k).toFixed(1)).reduce((o,x,j)=>o+(j%2?","+x+" ":x),"")+"'/>";
  i+=2+2*n;}}
 return "<svg viewBox='-21 -21 42 42' width='42' height='42' style='flex:0 0 42px'>"+b+"</svg>";
}
function iconLegend(){
 $("iconLegend").innerHTML=ICONS.map(([n,k,key])=>"<div class='row' style='gap:10px;justify-content:flex-start'>"+iconSvg(n,k)+
  "<span style='font-size:14px'>"+t(ICN[key])+"</span></div>").join("");
}

// --- Home Assistant: adresa a hotovy blok configuration.yaml -------------------
function haInfo(){
 const host=location.hostname||'espd35meteoradar.local';
 const url="http://"+host+"/api/ha";
 $("haUrl").textContent=url;$("haLink").href=url;
 const q='"';
 const S=(n,v,u,dc,extra)=>"      - name: "+q+t(n)+q+"\n        unique_id: "+n.toLowerCase().replace(/[^a-z0-9]+/g,"_")+
  "\n        value_template: "+q+"{{ "+v+" }}"+q+"\n"+(u?"        unit_of_measurement: "+q+u+q+"\n":"")+
  (dc?"        device_class: "+dc+"\n":"")+(extra||"");
 const av=k=>"        availability: "+q+"{{ value_json."+k+" is not none }}"+q+"\n";
 $("haYaml").textContent=
  "rest:\n  - resource: "+url+"\n    scan_interval: 60\n    sensor:\n"+
  S("MPR Teplota","value_json.temperature","°C","temperature","        state_class: measurement\n"+av("temperature"))+
  S("MPR Vitr","value_json.wind_kmh","km/h","wind_speed","        state_class: measurement\n"+av("wind_kmh"))+
  S("MPR Srazky","value_json.precip_mm","mm","precipitation",av("precip_mm"))+
  S("MPR Bourka max dBZ","value_json.storm.max_dbz | default(0)","dBZ")+
  S("MPR Bourka vzdalenost","value_json.storm.nearest_km","km","distance",
   "        availability: "+q+"{{ value_json.storm.nearest_km is defined and value_json.storm.nearest_km is not none }}"+q+"\n")+
  S("MPR Letadla v okoli","value_json.aircraft.count")+
  S("MPR Nejblizsi letadlo","value_json.aircraft.callsign | default('-', true)",null,null,
   "        json_attributes_path: "+q+"$.aircraft"+q+"\n        json_attributes:\n"+
   ["distance_km","altitude_m","speed_kmh","type","kind","bearing","in_window"].map(a=>"          - "+a+"\n").join(""))+
  S("MPR Cena elektriny","value_json.price_czk_kwh","CZK/kWh",null,
   "        availability: "+q+"{{ value_json.price_czk_kwh is defined }}"+q+"\n")+
  "    binary_sensor:\n"+
  "      - name: "+q+t("MPR Bourka")+q+"\n        unique_id: mpr_bourka\n        value_template: "+q+"{{ 'on' if value_json.storm.active else 'off' }}"+q+"\n        device_class: safety\n"+
  "      - name: "+q+t("MPR Letadlo nablizku")+q+"\n        unique_id: mpr_letadlo_nablizku\n        value_template: "+q+"{{ 'on' if value_json.aircraft.alert else 'off' }}"+q+"\n";
}
function copyHa(){
 const t=$("haYaml").textContent;
 if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(t).then(()=>toast(t('Zkopírováno')));return;}
 // Obycejne http v siti neni "secure context" a clipboard API tam chybi.
 const r=document.createRange();r.selectNodeContents($("haYaml"));
 const sel=window.getSelection();sel.removeAllRanges();sel.addRange(r);
 try{document.execCommand("copy");toast(t('Zkopírováno'));}catch(e){}
}

// --- Ulozene WiFi site (0.8.0) ---------------------------------------------------
function wifiSlots(nets){
  nets=nets||[];
  $('wslots').innerHTML=[0,1,2].map(i=>{const n=nets[i]||{};
    return `<div class="row"><label>${i+1}.</label>
      <input type="text" id="ws${i}" placeholder="${t('název sítě')}" value="${(n.ssid||'').replace(/"/g,'&quot;')}">
      <input type="password" id="wp${i}" placeholder="${n.hasPass?t('heslo uložené'):t('heslo')}"></div>`;}).join('');
}
async function saveNets(){
  const nets=[0,1,2].map(i=>({ssid:$('ws'+i).value.trim(),pass:$('wp'+i).value}));
  try{await api('/api/wifilist',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({nets})});toast(t('Sítě uloženy'));loadStat();}
  catch(e){toast(t('Chyba: {0}',e.message));}
}

async function loadStat(){
  let s;try{s=await api('/api/status')}catch(e){$('hdr').textContent=t('deska neodpovídá');return;}
  $('hdr').textContent=`${t('verze {0}',s.version)} · ${s.ap?t('přístupový bod'):t('síť {0}',s.ssid)} · http://${s.ap?s.ip:s.host+'.local'}/`;
  const up=s.uptime,d=Math.floor(up/86400),h=Math.floor(up%86400/3600),m=Math.floor(up%3600/60);
  const kv=(a,b)=>`<div class="kv"><span>${a}</span><span>${b}</span></div>`;
  $('stat').innerHTML=
    kv(t('Čas v desce'),`${s.time} (${s.clockSource})`)+
    kv(t('Adresa'),s.ip)+
    kv(t('Signál'),s.ap?'—':s.rssi+' dBm')+
    kv(t('Běží'),(d?d+' d ':'')+h+' h '+m+' min')+
    kv(t('Volná paměť'),t('{0} kB interní · {1} kB PSRAM',Math.round(s.heapInternal/1024),Math.round(s.heapPsram/1024)))+
    kv(t('Letadla'),s.sources.adsb)+
    kv(t('Meteoradar'),s.sources.radar)+
    kv(t('Předpověď'),s.sources.forecast)+
    kv(t('Cena elektřiny'),s.sources.price)+
    kv(t('Výroba elektřiny'),s.sources.mix)+
    kv(t('Časové pásmo'),s.timeZone||'—')+
    kv(t('Bouřka'),s.sources.storm||'—')+
    kv(t('Letadla poblíž'),s.sources.planeWatch||'—')+
    kv(t('Režim'),s.isNight?t('noční'):t('denní'))+
    kv(t('Střídání obrazovek'), !s.rotateSec ? t('vypnuto')
        : s.rotatePauseLeft ? t('po {0} s — pozastaveno ještě {1} s',s.rotateSec,s.rotatePauseLeft)
        : t('po {0} s',s.rotateSec));
  if(!loadStat._nets){loadStat._nets=1;wifiSlots(s.wifiNets);}
}

function collect(){
  let mask=0;for(let i=0;i<6;i++)if($('sc'+i).checked)mask|=1<<i;
  const o={lat:+$('lat').value,lon:+$('lon').value,
    brightDay:+$('bd').value,brightNight:+$('bn').value,
    nightAuto:$('na').checked,nightOffset:+$('no').value,
    screenMask:mask,autoRotateSec:+$('rot').value,secondsStyle:+$('sec').value,
    metric:$('mu').checked,topBearing:+$('tb').value,
    altMinFt:altFilterFt('alo'),altMaxFt:altFilterFt('ahi'),
    onlyWithCallsign:$('oc').checked,squawkAlert:$('sa').checked,
    watchCallsign:$('wc').value,
    meteoLegend:$('ml').checked,
    priceFee:Math.max(0,Math.min(20000,Math.round(+$('pf').value||0))),
    priceVat:Math.max(0,Math.min(100,Math.round(+$('pv').value||0))),
    mixCountry:$('mc').value,
    // 0.8.x
    clockFont:+$('cf').value, radarSrc:+$('rs').value,
    stormOnly:$('so').checked, stormDbz:+$('sd').value, stormKm:+$('sk').value,
    planeAlert:$('pa').checked, planeAlertKm:+$('pk').value, planeAlertSec:+$('ps').value,
    windowOn:$('wo').checked, windowWidth:+$('ww').value, windowKm:+$('wk').value,
    haEnabled:$('ha').checked, lang:+$('lg').value,
    altBandsFt:[0,1,2,3].map(bandFt),
    altColors:[0,1,2,3,4].map(i=>to565($('ac'+i).value))};
  return o;
}

// Ceniky uvadeji slozky v Kc/kWh, pole je v Kc/MWh - prepocet hned pod nim,
// aby bylo videt, ze 1500 je 1,50 Kc/kWh a ne 1500 Kc.
function feeHint(){const v=+$('pf').value||0;
  $('pfk').textContent=t('Odpovídá {0} Kč/kWh bez DPH.',(v/1000).toFixed(2).replace('.',','));}

async function save(){
  try{await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(collect())});
    const relang=(+$('lg').value)!==(CFG.lang|0);
    toast(t('Uloženo'));await loadCfg();if(relang){loadStat._nets=0;loadStat();}}
  catch(e){toast(t('Chyba: {0}',e.message));}
}
async function scr(step){try{await api('/api/screen',{method:'POST',
  headers:{'Content-Type':'application/json'},body:JSON.stringify({step})});}catch(e){toast(e.message)}}
async function rng(step){try{await api('/api/range',{method:'POST',
  headers:{'Content-Type':'application/json'},body:JSON.stringify({step})});}catch(e){toast(e.message)}}

async function geo(){
  const q=$('city').value.trim();if(q.length<2)return;
  $('geores').innerHTML='<p class="muted">'+t('hledám…')+'</p>';
  try{const r=await api('/api/geocode?q='+encodeURIComponent(q));
    const l=(r.results||[]);
    if(!l.length){$('geores').innerHTML='<p class="muted">'+t('nic nenalezeno')+'</p>';return;}
    $('geores').innerHTML='<div id="nets">'+l.map(p=>
      `<div onclick="pick(${p.latitude},${p.longitude})"><span>${p.name}${p.admin1?', '+p.admin1:''} (${p.country_code||''})</span>
       <span class="muted">${p.latitude.toFixed(3)}, ${p.longitude.toFixed(3)}</span></div>`).join('')+'</div>';
  }catch(e){$('geores').innerHTML='<p class="muted">'+t('nedostupné: {0}',e.message)+'</p>';}
}
function pick(a,b){$('lat').value=a.toFixed(4);$('lon').value=b.toFixed(4);
  $('geores').innerHTML='';toast(t('Poloha vyplněna, nezapomeňte uložit'));}

async function scan(){
  $('nets').innerHTML='<p class="muted">'+t('hledám sítě…')+'</p>';
  try{const l=await api('/api/scan');
    $('nets').innerHTML=l.length?l.map(n=>
      `<div onclick="selnet('${n.ssid.replace(/'/g,"\\'")}')"><span>${n.ssid||t('(skrytá)')}</span>
       <span class="muted">${n.rssi} dBm${n.open?t(' · otevřená'):''}</span></div>`).join('')
      :'<p class="muted">'+t('žádná síť')+'</p>';
  }catch(e){$('nets').innerHTML='<p class="muted">'+e.message+'</p>';}
}
function selnet(s){$('ss').value=s;$('pw').focus();}
async function wifi(){
  try{await api('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:$('ss').value,pass:$('pw').value})});
    toast(t('Připojuji — sledujte displej'));}catch(e){toast(t('Chyba: {0}',e.message));}
}

async function reboot(){
  if(!confirm(t('Restartovat desku?')))return;
  try{await api('/api/reboot',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({password:pw()})});toast(t('Restartuji…'));}catch(e){toast(t('Chyba: {0}',e.message));}
}
async function factory(){
  if(!confirm(t('Smazat VŠECHNO nastavení včetně WiFi a restartovat?')))return;
  try{await api('/api/reset',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({password:pw()})});toast(t('Mažu…'));}catch(e){toast(t('Chyba: {0}',e.message));}
}
// Zmena hesla ma vlastni koncovy bod a chce i soucasne heslo - driv se nove
// heslo posilalo s kazdym Ulozit z tehoz pole, ktere slouzilo k povoleni akci.
async function chpw(){
  const np=$('np').value;
  if(!np&&!confirm(t('Vypnout ochranu heslem?')))return;
  try{await api('/api/password',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({password:pw(),newPassword:np})});
    $('np').value='';$('ap').value='';toast(np?t('Heslo změněno'):t('Ochrana heslem vypnuta'));await loadCfg();}
  catch(e){toast(t('Chyba: {0}',e.message));}
}
async function imprt(){
  const f=$('imp').files[0];if(!f){toast(t('Vyberte soubor'));return;}
  try{const o=JSON.parse(await f.text());o.password=pw();
    await api('/api/import',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify(o)});toast(t('Obnoveno, restartuji…'));}
  catch(e){toast(t('Chyba: {0}',e.message));}
}

function flash(){
  const f=$('fw').files[0];if(!f){toast(t('Vyberte soubor'));return;}
  if(!confirm(t('Nahrát {0} ({1} kB)?',f.name,Math.round(f.size/1024))))return;
  const fd=new FormData();fd.append('firmware',f);
  const x=new XMLHttpRequest();
  // Velikost jde v dotazu, aby deska znala celek a mohla kreslit procenta.
  x.open('POST','/update?size='+f.size);
  $('bar').style.display='block';
  x.upload.onprogress=e=>{if(e.lengthComputable)
    $('bar').firstElementChild.style.width=Math.round(e.loaded/e.total*100)+'%';};
  x.onload=()=>toast(x.status===200?t('Hotovo, deska se restartuje'):t('Aktualizace selhala'));
  x.onerror=()=>toast(t('Spojení přerušeno'));
  x.send(fd);
}

buildTabs();
loadCfg().catch(e=>toast(t('Nelze načíst nastavení: {0}',e.message)));
loadStat();setInterval(loadStat,5000);
</script>
</body>
</html>
)HTMLPAGE";
