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
//  Jazyk je cesky. Zdrojovy projekt petus/MeteoPlaneRadar veze obe jazykove
//  sady a prepina je v JavaScriptu; tady je anglicka vetev zamerne vynechana.
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
.row .hint{flex-basis:100%;color:var(--dim);font-size:12.5px;margin:-4px 0 4px}
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
<p class="muted" style="margin:0 0 12px">Změny se na displeji projeví až po klepnutí na <b>Uložit nastavení</b> dole.</p>

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
  <div class="row hint">Změna se projeví hned — deska si znovu stáhne předpověď i mapu. Restart není potřeba.</div>
</div>

<div class="card">
  <h2>Displej</h2>
  <div class="row"><label>Jas ve dne <b id="bdv"></b>%</label>
    <input type="range" id="bd" min="10" max="100" oninput="bdv.textContent=this.value"></div>
  <div class="row"><label>Jas v noci <b id="bnv"></b>%</label>
    <input type="range" id="bn" min="10" max="100" oninput="bnv.textContent=this.value"></div>
  <div class="row"><label for="na">Přepínat automaticky podle slunce</label>
    <input type="checkbox" id="na"></div>
  <div class="row hint">Časy východu a západu přicházejí spolu s předpovědí, takže automatika nestojí žádný požadavek navíc.</div>
  <div class="row"><label for="no">Posun proti slunci (min)</label>
    <input type="number" id="no" min="-120" max="120" step="5"></div>
  <div class="row hint">Kladná hodnota začne noc dřív a ukončí ji později — pro pokoj, který se stmívá dřív než obloha.</div>
</div>
</section>

<section id="t-obrazovky" hidden>
<div class="card">
  <h2>Obrazovky</h2>
  <div class="grid" id="scrs"></div>
  <div class="row" style="margin-top:10px"><label for="rot">Střídat automaticky po (s)</label>
    <input type="number" id="rot" min="0" max="3600" step="10"></div>
  <div class="row hint">0 = vypnuto. Swipe nebo dlouhý stisk střídání dočasně pozastaví (desetinásobek intervalu, nejméně 30 s a nejvýše 10 minut) — kolik zbývá, je vidět v záložce Stav. Nastavení vypnout nejde. Změna se projeví hned; když vypnete právě zobrazenou obrazovku, deska přepne na první zapnutou.</div>
  <div class="row"><label for="sec">Běh sekund na hodinách (elipsa)</label>
    <select id="sec">
      <option value="0">vypnutý</option>
      <option value="1">tečky po obvodu</option>
      <option value="2">čára</option>
      <option value="3">kometa</option>
    </select></div>
</div>
</section>

<section id="t-letadla" hidden>
<div class="card">
  <h2>Letadla</h2>
  <div class="row"><label for="mu">Metrické jednotky (m, km/h)</label>
    <input type="checkbox" id="mu"></div>
  <div class="row"><label for="tb">Nahoře na radaru je</label>
    <select id="tb"></select></div>
  <div class="row hint">Směr, kterým se díváte z okna. Letadla na displeji pak míří stejně jako ta za sklem.</div>
  <div class="row"><label>Výškové pásmo (ft)</label>
    <input type="number" id="alo" min="0" max="60000" step="500">
    <input type="number" id="ahi" min="0" max="60000" step="500"></div>
  <div class="row"><label for="oc">Jen letadla s volacím znakem</label>
    <input type="checkbox" id="oc"></div>
  <div class="row"><label for="sa">Hlásit nouzový squawk (7500/7600/7700)</label>
    <input type="checkbox" id="sa"></div>
  <div class="row hint">Letadlo v nouzi se zobrazí i mimo nastavené výškové pásmo.</div>
  <div class="row"><label for="wc">Hlídaný volací znak</label>
    <input type="text" id="wc" maxlength="8" placeholder="např. CSA"></div>
  <div class="row hint">Porovnává se na začátek znaku, takže „CSA“ chytí všechny lety CSA.</div>
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
  <div class="row hint">Deska opustí přístupový bod, takže se tahle stránka odpojí. Po připojení ji najdete na adrese uvedené nahoře.</div>
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
  <div class="row"><label for="ap">Heslo správce (prázdné = bez ochrany)</label>
    <input type="password" id="ap" placeholder="beze změny"></div>
  <div class="row hint">Chrání aktualizaci firmwaru, obnovu ze zálohy a tovární reset. Je to ochrana před domácností, ne před útočníkem na síti.</div>
  <div class="row">
    <button onclick="location.href='/api/export'">Stáhnout zálohu</button>
    <input type="file" id="imp" accept=".json" style="flex:0 1 200px">
    <button onclick="imprt()">Obnovit ze zálohy</button>
  </div>
  <div class="row" style="margin-top:10px">
    <button onclick="reboot()">Restartovat</button>
    <button class="danger" onclick="factory()">Tovární reset</button>
  </div>
  <div class="row hint">Běžná změna nastavení restart nevyžaduje — projeví se hned. Restart je tu jen pro případ, že ho opravdu chcete.</div>
</div>
</section>

<!-- Ulozit patri POD sekce, ne do nektere z nich. Prvni verze ho mela uvnitr
     zalozky Sprava, takze se skryl spolu s ni - a nastaveni ze zbylych
     zalozek neslo ulozit, aniz by uzivatel vedel, ze ma prejit jinam. -->
<div id="savebar" style="position:sticky;bottom:0;padding:10px 0;background:var(--bg)">
  <button class="primary" style="width:100%" onclick="save()">Uložit nastavení</button>
</div>

<p class="muted" style="text-align:center">
  Data: <a style="color:var(--acc)" href="https://adsb.fi">adsb.fi</a> ·
  <a style="color:var(--acc)" href="https://opendata.chmi.cz">ČHMÚ</a> ·
  <a style="color:var(--acc)" href="https://open-meteo.com">Open-Meteo</a> ·
  <a style="color:var(--acc)" href="https://adsb.lol">adsb.lol</a><br>
  Jen pro osobní nekomerční použití.
</p>
</main>
<div id="toast"></div>

<script>
const $=id=>document.getElementById(id);

// --- Zalozky ---------------------------------------------------------------
// Zvolena zalozka se pamatuje v adrese (#letadla), takze po ulozeni nebo po
// obnoveni stranky zustanete tam, kde jste byli.
const TABS=[['stav','Stav'],['misto','Poloha a displej'],['obrazovky','Obrazovky'],
            ['letadla','Letadla'],['sit','WiFi'],['sprava','Správa']];
function showTab(id){
  if(!TABS.some(t=>t[0]===id)) id=TABS[0][0];
  TABS.forEach(([k])=>{
    const sec=$('t-'+k); if(sec) sec.hidden=(k!==id);
    const b=$('tab-'+k); if(b) b.setAttribute('aria-selected', k===id);
  });
  // Ulozit je na VSECH zalozkach. Skryvat ho tam, kde zrovna "neni co
  // ulozit", znamena, ze ho uzivatel jednou nenajde a bude si myslet, ze se
  // nastaveni ukladaji sama.
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
    throw new Error(e||('HTTP '+r.status));}
  // Vsechny koncove body vraceji JSON; prazdna odpoved se bere jako {}.
  return r.json().catch(()=>({}));}
function pw(){return $('ap').value;}

const DIRS=[['S',0],['SV',45],['V',90],['JV',135],['J',180],['JZ',225],['Z',270],['SZ',315]];

async function loadCfg(){
  CFG=await api('/api/config');
  $('lat').value=(+CFG.lat).toFixed(4);
  $('lon').value=(+CFG.lon).toFixed(4);
  $('bd').value=CFG.brightDay; $('bdv').textContent=CFG.brightDay;
  $('bn').value=CFG.brightNight; $('bnv').textContent=CFG.brightNight;
  $('na').checked=CFG.nightAuto; $('no').value=CFG.nightOffset;
  $('rot').value=CFG.autoRotateSec; $('sec').value=CFG.secondsStyle;
  $('mu').checked=CFG.metric;
  $('alo').value=CFG.altMinFt; $('ahi').value=CFG.altMaxFt;
  $('oc').checked=CFG.onlyWithCallsign; $('sa').checked=CFG.squawkAlert;
  $('wc').value=CFG.watchCallsign||'';
  $('tb').innerHTML=DIRS.map(d=>`<option value="${d[1]}">${d[0]} (${d[1]}°)</option>`).join('');
  $('tb').value=CFG.topBearing;
  const names=['Hodiny','Letadla','Meteoradar','Předpověď'];
  $('scrs').innerHTML=names.map((n,i)=>
    `<label class="row" style="margin:0"><input type="checkbox" id="sc${i}"
      ${(CFG.screenMask>>i)&1?'checked':''}> ${n}</label>`).join('');
  $('wifinow').textContent=CFG.wifiSsid?('Uložená síť: '+CFG.wifiSsid):'Žádná síť není uložená.';
  if(CFG.hasPassword)$('ap').placeholder='heslo je nastavené — beze změny';
}

async function loadStat(){
  let s;try{s=await api('/api/status')}catch(e){$('hdr').textContent='deska neodpovídá';return;}
  $('hdr').textContent=`verze ${s.version} · ${s.ap?'přístupový bod':'síť '+s.ssid} · http://${s.ap?s.ip:s.host+'.local'}/`;
  const up=s.uptime,d=Math.floor(up/86400),h=Math.floor(up%86400/3600),m=Math.floor(up%3600/60);
  const kv=(a,b)=>`<div class="kv"><span>${a}</span><span>${b}</span></div>`;
  $('stat').innerHTML=
    kv('Čas v desce',`${s.time} (${s.clockSource})`)+
    kv('Adresa',s.ip)+
    kv('Signál',s.ap?'—':s.rssi+' dBm')+
    kv('Běží',(d?d+' d ':'')+h+' h '+m+' min')+
    kv('Volná paměť',`${Math.round(s.heapInternal/1024)} kB interní · ${Math.round(s.heapPsram/1024)} kB PSRAM`)+
    kv('Letadla',s.sources.adsb)+
    kv('Meteoradar',s.sources.radar)+
    kv('Předpověď',s.sources.forecast)+
    kv('Režim',s.isNight?'noční':'denní')+
    kv('Střídání obrazovek', !s.rotateSec ? 'vypnuto'
        : s.rotatePauseLeft ? `po ${s.rotateSec} s — pozastaveno ještě ${s.rotatePauseLeft} s`
        : `po ${s.rotateSec} s`);
}

function collect(){
  let mask=0;for(let i=0;i<4;i++)if($('sc'+i).checked)mask|=1<<i;
  const o={lat:+$('lat').value,lon:+$('lon').value,
    brightDay:+$('bd').value,brightNight:+$('bn').value,
    nightAuto:$('na').checked,nightOffset:+$('no').value,
    screenMask:mask,autoRotateSec:+$('rot').value,secondsStyle:+$('sec').value,
    metric:$('mu').checked,topBearing:+$('tb').value,
    altMinFt:+$('alo').value,altMaxFt:+$('ahi').value,
    onlyWithCallsign:$('oc').checked,squawkAlert:$('sa').checked,
    watchCallsign:$('wc').value};
  if($('ap').value)o.password=$('ap').value;
  return o;
}

async function save(){
  try{await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(collect())});
    toast('Uloženo');await loadCfg();}
  catch(e){toast('Chyba: '+e.message);}
}
async function scr(step){try{await api('/api/screen',{method:'POST',
  headers:{'Content-Type':'application/json'},body:JSON.stringify({step})});}catch(e){toast(e.message)}}
async function rng(step){try{await api('/api/range',{method:'POST',
  headers:{'Content-Type':'application/json'},body:JSON.stringify({step})});}catch(e){toast(e.message)}}

async function geo(){
  const q=$('city').value.trim();if(q.length<2)return;
  $('geores').innerHTML='<p class="muted">hledám…</p>';
  try{const r=await api('/api/geocode?q='+encodeURIComponent(q));
    const l=(r.results||[]);
    if(!l.length){$('geores').innerHTML='<p class="muted">nic nenalezeno</p>';return;}
    $('geores').innerHTML='<div id="nets">'+l.map(p=>
      `<div onclick="pick(${p.latitude},${p.longitude})"><span>${p.name}${p.admin1?', '+p.admin1:''} (${p.country_code||''})</span>
       <span class="muted">${p.latitude.toFixed(3)}, ${p.longitude.toFixed(3)}</span></div>`).join('')+'</div>';
  }catch(e){$('geores').innerHTML='<p class="muted">nedostupné: '+e.message+'</p>';}
}
function pick(a,b){$('lat').value=a.toFixed(4);$('lon').value=b.toFixed(4);
  $('geores').innerHTML='';toast('Poloha vyplněna, nezapomeňte uložit');}

async function scan(){
  $('nets').innerHTML='<p class="muted">hledám sítě…</p>';
  try{const l=await api('/api/scan');
    $('nets').innerHTML=l.length?l.map(n=>
      `<div onclick="selnet('${n.ssid.replace(/'/g,"\\'")}')"><span>${n.ssid||'(skrytá)'}</span>
       <span class="muted">${n.rssi} dBm${n.open?' · otevřená':''}</span></div>`).join('')
      :'<p class="muted">žádná síť</p>';
  }catch(e){$('nets').innerHTML='<p class="muted">'+e.message+'</p>';}
}
function selnet(s){$('ss').value=s;$('pw').focus();}
async function wifi(){
  try{await api('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:$('ss').value,pass:$('pw').value})});
    toast('Připojuji — sledujte displej');}catch(e){toast('Chyba: '+e.message);}
}

async function reboot(){
  if(!confirm('Restartovat desku?'))return;
  try{await api('/api/reboot',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({password:pw()})});toast('Restartuji…');}catch(e){toast('Chyba: '+e.message);}
}
async function factory(){
  if(!confirm('Smazat VŠECHNO nastavení včetně WiFi a restartovat?'))return;
  try{await api('/api/reset',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({password:pw()})});toast('Mažu…');}catch(e){toast('Chyba: '+e.message);}
}
async function imprt(){
  const f=$('imp').files[0];if(!f){toast('Vyberte soubor');return;}
  try{const o=JSON.parse(await f.text());o.password=pw();
    await api('/api/import',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify(o)});toast('Obnoveno, restartuji…');}
  catch(e){toast('Chyba: '+e.message);}
}

function flash(){
  const f=$('fw').files[0];if(!f){toast('Vyberte soubor');return;}
  if(!confirm('Nahrát '+f.name+' ('+Math.round(f.size/1024)+' kB)?'))return;
  const fd=new FormData();fd.append('firmware',f);
  const x=new XMLHttpRequest();
  // Velikost jde v dotazu, aby deska znala celek a mohla kreslit procenta.
  x.open('POST','/update?size='+f.size);
  $('bar').style.display='block';
  x.upload.onprogress=e=>{if(e.lengthComputable)
    $('bar').firstElementChild.style.width=Math.round(e.loaded/e.total*100)+'%';};
  x.onload=()=>toast(x.status===200?'Hotovo, deska se restartuje':'Aktualizace selhala');
  x.onerror=()=>toast('Spojení přerušeno');
  x.send(fd);
}

buildTabs();
loadCfg().catch(e=>toast('Nelze načíst nastavení: '+e.message));
loadStat();setInterval(loadStat,5000);
</script>
</body>
</html>
)HTMLPAGE";
