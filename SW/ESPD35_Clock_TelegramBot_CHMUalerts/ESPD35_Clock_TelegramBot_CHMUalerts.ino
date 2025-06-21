/* 
* ESPD-3.5" - using of telegram bot and clock with ESPD-35
* Written by chiptron.cz for laskakit.cz (2023, updated 2025)
* ESPD35: https://www.laskakit.cz/laskakit-espd-35-esp32-3-5-tft-ili9488-touch/
* TMEP.cz
 * 
 * How to steps:
 * 1. Copy file from https://github.com/LaskaKit/ESPD-35/tree/main/SW to Arduino/libraries/TFT_eSPI/User_Setups/
 *    - for version v2.3 and before:  Setup300_ILI9488_ESPD-3_5_v2.h 
 *    - for version v3 and above:     Setup303_ILI9488_ESPD-3_5_v3.h
 * 2. in Arduino/libraries/TFT_eSPI/User_Setup_Select.h 
      a. comment: #include <User_Setup.h> 
      b. add: 
          - for version v2.3 and before:  #include <User_Setups/Setup300_ILI9488_ESPD-3_5_v2.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 
          - for version v3 and above:     #include <User_Setups/Setup303_ILI9488_ESPD-3_5_v3.h>  // Setup file for LaskaKit ESPD-3.5" 320x480, ILI9488 V3
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
 * Used library:
 * AsyncTelegram - https://github.com/cotestatnt/AsyncTelegram
 * Font convertor: https://oleddisplay.squix.ch/
 * ArduinoJson: https://arduinojson.org/
 * TFT_eSPI - https://github.com/Bodmer/TFT_eSPI
 *
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
*/

#include <WiFi.h>

// TFT Display
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

// NTP
#include <WiFiUdp.h>
#include <NTPClient.h>

// HTTP Client
#include <HTTPClient.h>

// Fonts
#include "OpenSansSB_40px.h"
#include "OpenSansSB_60px.h"
#include "DSEG14Classic_100px_bold.h"
#include "DSEG14Classic_80px_bold.h"
#include "DSEG14Classic_60px_bold.h"

// Telegram BOT
#include "AsyncTelegram.h"

// CHMU Alerts from TMEP.cz 
#include <ArduinoJson.h>

#define TFT_BL_PWM 255 // Backlight brightness 0-255

// Wi-Fi credentials
const char *ssid     = "xxx";
const char *password = "yyy";
const char* token = "zzz";   	// REPLACE myToken WITH YOUR TELEGRAM BOT TOKEN

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.cz.pool.ntp.org", 7200, 60000); // Czech NTP server

// Telegram BOT
AsyncTelegram myBot;
TBMessage text_msg;

// TFT display
TFT_eSPI tft = TFT_eSPI();
unsigned long lastTimeBotRan;
int updtDisplayDelay = 10000; // update the time each 10s

// CHMU from TMEP.cz
// test json with random values
//"https://tmep.cz/vystup-json.php?test=1";
const char* json_url = "www";
int chmuAlertColor = TFT_WHITE;
float teplota;
int vlhkost;

void setup()
{
  Serial.begin(115200);

  displayInit();

  wifiInit();

  updtDisplay();

  telegramBotInit();

}
void loop()
{ 
  // update the time 
  if (millis() > lastTimeBotRan + updtDisplayDelay)  
  {
    updtDisplay();
    lastTimeBotRan = millis();
  }

  // Telegram BOT
	// if there is an incoming message...
	if (myBot.getNewMessage(text_msg)) 
  {
		if (text_msg.text.equalsIgnoreCase("/clear")) 
    {      
      // clear message
      myBot.sendMessage(text_msg, "Message cleared"); // notify the sender
			text_msg.text = "";
		}
		else if (text_msg.text.equalsIgnoreCase("/info")) 
    {        
      // if the received message is "/info"...
      String reply;
      reply = "Info: \n";
      reply = "RSSI: ";
      reply += WiFi.RSSI();
			myBot.sendMessage(text_msg, reply); // notify the sender
      // clear message
      text_msg.text = "";
		}
		else 
    {                                                    
			// generate the message for the sender
			String reply;
			reply = "Welcome ";
			reply += text_msg.sender.username;
			reply += "\n Command received";
			myBot.sendMessage(text_msg, reply); // and send it
		}
    
    //update display
    updtDisplay();
	}
}


void updtDisplay()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  // CHMU message
  String chmuAlert = chmuWarnings();

  // check the CMHU Alerts and Telegram bot message
  if((chmuAlert == "null") && (text_msg.text == NULL))
  {
    tft.setCursor(60,120);
    tft.setFreeFont(&DSEG14_Classic_Bold_100); 
  }
  else
  {
    tft.setCursor(10,110);
    tft.setFreeFont(&DSEG14_Classic_Bold_80);     
  }
  
  // time from NTP server
  char buffer[10];
  timeClient.update();
  sprintf(buffer, "%02d:%02d", timeClient.getHours(), timeClient.getMinutes());
  tft.print(buffer);

  // TMEP temperature
  if((chmuAlert == "null") && (text_msg.text == NULL))
  {
    tft.setFreeFont(&DSEG14_Classic_Bold_100);
    tft.setTextColor(TFT_RED);
    tft.setCursor(10,280);
    tft.print(String(teplota, 1)); // temperature
    tft.setTextColor(TFT_BLUE);
    tft.setCursor(300,280);
    tft.print(String(vlhkost)); // humidity
  }
  else
  {
    tft.setFreeFont(&DSEG14_Classic_Bold_60); 
    tft.setTextColor(TFT_RED);
    tft.setCursor(310,70);
    tft.print(String(teplota, 1)); // temperature
    tft.setTextColor(TFT_BLUE);
    tft.setCursor(310,140);
    tft.print(String(vlhkost)); // humidity
  }

	// print the message from TelegramBot
  if(text_msg.text != NULL)
  {
    // print message from telegram bot
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0,180);
    tft.setFreeFont(&OpenSansSB_40px); 
    tft.print(fixCzechCharacters(text_msg.text)); 
  }

	// print the message from CHMU
  if(chmuAlert != "null")
  {
    // print message from CHMI
    tft.setTextColor(chmuAlertColor);
    tft.setCursor(0,310);
    tft.setFreeFont(&OpenSansSB_40px); 
    tft.print("CHMU: ");
    tft.print(chmuAlert);
  }
}

void displayInit()
{
  tft.init();
  tft.setRotation(1);
  analogWrite(TFT_BL, TFT_BL_PWM);      // Set brightness of backlight

}

void telegramBotInit()
{
  // To ensure certificate validation, WiFiClientSecure needs time updated
  // myBot.setInsecure(false);
  myBot.setClock("CET-1CEST,M3.5.0,M10.5.0/3");
	
	// Set the Telegram bot properies
	myBot.setUpdateTime(1000);
	myBot.setTelegramToken(token);

  // Check if all things are ok
	Serial.print("\nTest Telegram connection... ");
	myBot.begin() ? Serial.println("OK") : Serial.println("NOK");
}

String chmuWarnings()
{
  String payload = httpGETRequest(json_url);
  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }
  Serial.println("CHMI json: ");
  
  String color = fixCzechCharacters(doc["chmi"][0]["awareness_level"]);
  String chmuAlertMsg = fixCzechCharacters(doc["chmi"][0]["event"]);
  Serial.print("Level "); Serial.println(color);
  Serial.print("Event "); Serial.println(chmuAlertMsg);

  float temp = doc["teplota"];
  teplota = temp;
  int hum = doc["vlhkost"];
  vlhkost = hum;
  Serial.print("Teplota: "); Serial.print(temp); Serial.println(" degC");
  Serial.print("Vlhkost: "); Serial.print(hum); Serial.println(" % Rh");
  
  if(color == "red")
    chmuAlertColor = TFT_RED;
  else if (color == "green")
    chmuAlertColor = TFT_GREEN;
  else if (color == "orange")
    chmuAlertColor = TFT_ORANGE;
  else
    chmuAlertColor = TFT_YELLOW;

  return (chmuAlertMsg);
}

void wifiInit()
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting.");
 
  while ( WiFi.status() != WL_CONNECTED ) 
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("connected");
}

String httpGETRequest(const char* serverName) 
{
  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

// replace the czech characters 
String fixCzechCharacters(String data) 
{
	data.replace("Ě", "E");
	data.replace("Š", "S");
	data.replace("Č", "C");
	data.replace("Ř", "R");
	data.replace("Ž", "Z");
	data.replace("Ý", "Y");
	data.replace("Á", "A");
	data.replace("Í", "I");
	data.replace("É", "E");
	data.replace("Ů", "U");
	data.replace("Ú", "U");
	data.replace("Ď", "D");
	data.replace("Ť", "T");
	data.replace("Ň", "N");
	data.replace("ě", "e");
	data.replace("š", "s");
	data.replace("č", "c");
	data.replace("ř", "r");
	data.replace("ž", "z");
	data.replace("ý", "y");
	data.replace("á", "a");
	data.replace("í", "i");
	data.replace("é", "e");
	data.replace("ů", "u");
	data.replace("ú", "u");
	data.replace("ď", "d");
	data.replace("ť", "t");
	data.replace("ň", "n");
	data.replace("°C", "");
	return data;
}
