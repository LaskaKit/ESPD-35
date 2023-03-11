# Jak pracovat s displejem

1. Stáhnout [CH343SER.EXE](http://www.wch-ic.com/search?t=all&q=CH9102) a nainstalovat. Návod i na našem [blogu](https://blog.laskakit.cz/instalace-ovladace-prevodniku-usb-na-uart-ch340/).

2. Zapojit displej do PC a zapnout tlačítkem na boku (**stisk pro zapnutí, dlouhý stisk pro vypnutí**).

3. Start – Systém – Správce zařízení (může být potřeba oprávnění správce) najít položku Porty (COM a LPT).

    ![COM_port](../img/COM_port.jpg)

## Arduino IDE
1. Stáhnout Github repository a otevřít příklad, který chcete zkusit ze složky [SW](../SW).
2. Otevřít příklad pomocí Arduino IDE a nastavit správný COM port a typ desky.
   
    ![ArduinoIDE_set](../img/ArduinoIDE_set.png)

3. Stáhnout nezbytné knihovny.
4. Knihovnu TFE_eSPI je vždy potřeba nastavit podle konkrétního displeje. Pro tento displej je už konfigurační soubor připravený ([Setup300_ILI9488_ESPD-3_5.h](Setup300_ILI9488_ESPD-3_5.h)).
Musí být pouze zkopírován do složky knihovny: Arduino\libraries\TFT_eSPI\User_Setups.
5. V souboru Arduino\libraries\TFT_eSPI\User_Setup_Select.h je poté potřeba přidat řádek: `#include <User_Setups/Setup300_ILI9488_ESPD-3_5.h>` do sekce: `#ifndef USER_SETUP_LOADED` a zakomentovat `#include <User_Setup.h>`, jak je znázorněno v obrázku níže.
   
   ![User_setup](../img/User_setup.png)

6. Nahrát kód.
## Platform IO
1. Stáhnout Github repository a otevřít příklad, který chcete zkusit ze složky [SW](../SW).
2. Otevřít příklad pomocí Arduino IDE a nastavit správný COM port a typ desky. COM port by měl být nastaven automaticky.
3. Knihovny se stáhnou automaticky díky lib_deps v souboru platformio.ini.
4. Knihovnu TFE_eSPI je vždy potřeba nastavit podle konkrétního displeje. V Platform IO můžeme opět použít build flags, takže není potřeba upravovat žádné soubory. Všechno je už nastaveno v platformio.ini v každém příkladu.
    <p align="center">
    <img src="../img/PlatformIO_set.png" height="500">
    </p>
5. Nahrát kód.