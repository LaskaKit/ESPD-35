/* 
* This example code is used for LaskaKit ESPD-3.5 ESP32 3.5 TFT ILI9488 Touch v3.0 https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
* with I2S decoder wit I2S DAC and amplifier MAX98357A on board
* ESPD-3.5 board reads stream from internet radio and playing on speaker.
 * 
 * How to steps:
 * 1. Copy file Setup303_ILI9488_ESPD-3_5_v3.h from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/  
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h 
      a. comment: #include <User_Setup.h> 
      b. add: #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 V3
 * 
 * Board constants:
      TFT_BL          - LED back-light use: analogWrite(TFT_BL, TFT_BL_PWM);
      POWER_OFF_PIN   - Pull LOW to switch board off
      TOUCH_INT       - Touch interrupt pin
    * I2C (µŠup and devices (only from v3)):
      I2C_SDA         - Data pin 
      I2C_SCL         - Clock pin
    * SPI (µŠup (only from v3) and SD card):
      SPI_MISO        - MISO pin
      SPI_MOSI        - MOSI pin
      SPI_SCK         - Clock pin
      SPI_USUP_CS     - µŠup Chip Select pin (only from v3)
      SPI_SD_CS       - SD Card Chip Select pin
    * I2S (only from v3):
      I2S_LRC         - Word select a.k.a. left-right clock pin
      I2S_DOUT        - Serial data pin
      I2S_BCLK        - Serial clock a.k.a. bit clock pin
    * Battery mesurement:
      BAT_PIN         - Battery voltage mesurement
      deviderRatio    - Voltage devider ratio on ADC pin 1MOhm + 1.3MOhm
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <TFT_eSPI.h>               // Hardware-specific library
#include <SPI.h>
#include "WiFi.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"

// Replace with your network credentials
const char* ssid = "xxxxx";
const char* password = "xxxxx";

// TFT SPI
#define TFT_BL_PWM 255 // Backlight brightness 0-255
#define TFT_DISPLAY_RESOLUTION_X 480
#define TFT_DISPLAY_RESOLUTION_Y 320

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height
Audio audio;

void displayInit() {
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {} // Wait until serial is ok
  delay(200);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SD.begin(SPI_SD_CS);

  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi ssid: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int i = 0;
  int a = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    a++;
    i++;
    if (i == 10) {
      i = 0;
      Serial.println(".");
    } else {
      Serial.print("."); 
    }
  }
  Serial.println("Wi-Fi connected successfully");

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // default 0...21
//  or alternative
//  audio.setVolumeSteps(64); // max 255
//  audio.setVolume(63);
//
//  *** radio streams ***
//  audio.connecttohost("http://stream.antennethueringen.de/live/aac-64/stream.antennethueringen.de/"); // aac
//  audio.connecttohost("http://mcrscast.mcr.iol.pt/cidadefm");                                         // mp3
//  audio.connecttohost("http://www.wdr.de/wdrlive/media/einslive.m3u");                                // m3u
//  audio.connecttohost("https://stream.srg-ssr.ch/rsp/aacp_48.asx");                                   // asx
  audio.connecttohost("http://tuner.classical102.com/listen.pls");                                    // pls
//  audio.connecttohost("http://stream.radioparadise.com/flac");                                        // flac
//  audio.connecttohost("http://stream.sing-sing-bis.org:8000/singsingFlac");                           // flac (ogg)
//  audio.connecttohost("http://s1.knixx.fm:5347/dein_webradio_vbr.opus");                              // opus (ogg)
//  audio.connecttohost("http://stream2.dancewave.online:8080/dance.ogg");                              // vorbis (ogg)
//  audio.connecttohost("http://26373.live.streamtheworld.com:3690/XHQQ_FMAAC/HLSTS/playlist.m3u8");    // HLS
//  audio.connecttohost("http://eldoradolive02.akamaized.net/hls/live/2043453/eldorado/master.m3u8");   // HLS (ts)
//  *** web files ***
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Pink-Panther.wav");        // wav
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Santiano-Wellerman.flac"); // flac
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Olsen-Banden.mp3");        // mp3
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Miss-Marple.m4a");         // m4a (aac)
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/Collide.ogg");             // vorbis
//  audio.connecttohost("https://github.com/schreibfaul1/ESP32-audioI2S/raw/master/additional_info/Testfiles/sample.opus");             // opus
//  *** local files ***
//  audio.connecttoFS(SD, "/Pink-Panther.wav");     // SD
//  audio.connecttoFS(SPIFFS, "/test.wav"); // SPIFFS

//  audio.connecttospeech("LaskaKit, by Makers for Makers, bastlime s laskou,", "en"); // Google TTS

  displayInit();
	analogWrite(TFT_BL, TFT_BL_PWM);      // Set brightness of backlight
}

void loop() {
  vTaskDelay(1);
  audio.loop();

	tft.setTextSize(1);
	tft.setTextFont(4);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
  tft.drawString("I2S MAX98357 Example" , TFT_DISPLAY_RESOLUTION_X / 2, (TFT_DISPLAY_RESOLUTION_Y / 2));

  delay(100);
}