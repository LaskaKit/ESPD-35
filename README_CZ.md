![ESPD-3.5](https://github.com/LaskaKit/ESPD-35/blob/main/img/003.jpg)

# ESPD 3.5" - Vývojová deska s ESP32 a skvělým TFT displejem

Nepotřebuješ low-power desku, ale oceníš aktivní TFT displej se skvělým pozorovaním úhlem a rychlým připojením dalších čidel a modulů? ESPD-3.5". 
I když je to vývojový kit, je zároveň vhodný pro osazení do finálního produktu. Deska má 4 montážní díry. 

3.5" displej má rozlišení 480x320 px a počet barev jednotlivých pixelů může být až 65535 a navíc má i dotekovou 
ESPD-3.5" obsahuje slot na microSD kartu, kam si můžeš uložit spoustu obrázků a ikon - návod jak na to [najdeš tady](https://blog.laskakit.cz/jak-nahrat-fotku-ikonu-do-esp32-a-zobrazit-na-tft-displeji/).

Kromě slotu na SD kartu má deska samozřejmě modul s ESP32 podporující Wi-Fi i Bluetooth konektivitu, vestavěný programátor s CH9102, nabíjecí obvod pro akumulátor s nabíjecím
proudem 400mA, 2x konektor uŠup pro připojení čidel (například [SHT40](https://www.laskakit.cz/laskakit-sht40-senzor-teploty-a-vlhkosti-vzduchu/) - teplota/vlhkost, 
[SCD41](https://www.laskakit.cz/laskakit-scd41-senzor-co2--teploty-a-vlhkosti-vzduchu/) - CO2/teplota/vlhkost nebo [BME280](https://www.laskakit.cz/arduino-senzor-tlaku--teploty-a-vlhkosti-bme280/) - tlak/teplota/vlhkost).

![ESPD-3.5 pinout](https://github.com/LaskaKit/ESPD-35/blob/main/img/ESPD-3.5-pinout.jpg)

ESPD3.5" obsahuje dvě tlačítka - RESET a Power ON/OFF - tlačítko, kterým ESPD3.5" vypneš (dlouhý stisk) a zapneš (krátký stisk). Tlačítko je připojeno k GPIO35.
Ocenit bys mohl i dutinkovou lištu 2x13 pinů, kam jsme připojili zbylé GPIO ESP32.

USB-C konektor slouží jak k programování, tak i nabíjení Lipolky. 
Na desce jsme nechali i místo pro [580mAh Lipol akumulátor](https://www.laskakit.cz/geb-lipol-baterie-801454-580mah-3-7v-jst-ph-2-0/) - aby nikde jinde nepřekážel. 

K měření napětí akumulátoru jsme osadili i napěťový dělič připojený k akumulátoru a GPIO34 - nezapomeň propájet pájecí most. 

Připravili jsme několik vzorových kódů: https://github.com/LaskaKit/ESPD-35/tree/main/SW

A také krabičku k vytištěnní na 3D tiskárně: https://github.com/LaskaKit/ESPD-35/tree/main/3D

![ESPD-3.5](https://github.com/LaskaKit/ESPD-35/blob/main/img/001.jpg)
![ESPD-3.5](https://github.com/LaskaKit/ESPD-35/blob/main/img/002.jpg)
