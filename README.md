![ESPD-3.5](https://github.com/LaskaKit/ESPD-35/blob/main/img/003.jpg)

# ESPD 3.5" - Development board with ESP32 and great TFT display

Don't you need a low-power board, but appreciate an active TFT display with a great viewing angle and quick connection of additional sensors and modules? What about ESPD-3.5"? Although it is a development kit, it is also suitable for fitting into the final product. The board has 4 mounting holes.

The 3.5" display has a resolution of 480x320 px and the number of colors of individual pixels can be up to 65535, and it also has a touch and includes a microSD card slot where you can store a lot of pictures and icons - instructions on how to do [this can be found here](https://blog.laskakit.cz/jak-nahrat-fotku-ikonu-do-esp32-a-zobrazit-na-tft-displeji/).

In addition to the SD card slot, the board has a module with ESP32 supporting Wi-Fi and Bluetooth connectivity, a built-in programmer with CH9102, a charging circuit for a battery with a charging current of 400mA, 2x uŠup connector for connecting sensors (for example [SHT40 ](https://www.laskakit.cz/laskakit-sht40-senzor-teploty-a-vlhkosti-vzduchu/)- temperature/humidity, [SCD41 ](https://www.laskakit.cz/laskakit-scd41-senzor-co2--teploty-a-vlhkosti-vzduchu/)- CO2/temperature/humidity or [BME280](https://www.laskakit.cz/arduino-senzor-tlaku--teploty-a-vlhkosti-bme280/) - pressure/temperature/humidity).

![ESPD-3.5 pinout](https://github.com/LaskaKit/ESPD-35/blob/main/img/ESPD-3.5-pinout.jpg)

The ESPD3.5" contains two buttons - RESET and Power ON/OFF - the button that turns the ESPD3.5" off (long press) and on (short press). The button is connected to GPIO35. You might also appreciate the 2x13 pin cavity rail where we connected the remaining ESP32 GPIO.

The USB-C connector is used for both programming and charging the Lipolka. We also left a [place on the board for a 580mAh Lipol battery](https://www.laskakit.cz/geb-lipol-baterie-801454-580mah-3-7v-jst-ph-2-0/) - so it won't get in the way anywhere else.

To measure the battery voltage, we also fitted a voltage divider connected to the battery and GPIO34 - don't forget to power the solder bridge.

We have prepared some sample codes: https://github.com/LaskaKit/ESPD-35/tree/main/SW 

And also a box to print on a 3D printer: https://github.com/LaskaKit/ESPD-35/tree/main/3D

![ESPD-3.5](https://github.com/LaskaKit/ESPD-35/blob/main/img/001.jpg)
![ESPD-3.5](https://github.com/LaskaKit/ESPD-35/blob/main/img/002.jpg)
