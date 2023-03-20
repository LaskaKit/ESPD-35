/* 
 * LaskaKit ESPDisplay for Weather Station. 
 * Thingspeak edition
 * Read Temperature, Humidity and pressure from Thingspeak and show on the display
 * For settings see config.h
 * 
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 * 
 * Miles Burton DS18B20 library
 * https://github.com/milesburton/Arduino-Temperature-Control-Library
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include <TFT_eSPI.h>               // Hardware-specific library
#include <SPI.h>
#include <Wire.h>
#include <ArduinoJson.h>            // JSON library
#include <Adafruit_SHT4x.h>
#include <ESP32AnalogRead.h>
#include "time.h"
#include "sntp.h"
//#include "squares.h"              // Gauges bigger, not fully visible
#include "squares1.h"               // Gauges smaller, fully visible
#include "config.h"                 // change to config.h and fill the file.
#include "iot_iconset_16x16.h"      // WIFI and battery icons

#define ADC 34                      // Battery voltage mesurement
#define SDA 21
#define SCL 22
#define USE_STATIC_IP false         // if we want to use a static IP address
#define deviderRatio 1.3

// if we want to use a static IP address
#if USE_STATIC_IP
  IPAddress ip(192,168,100,244);      // pick your own IP outside the DHCP range of your router
  IPAddress gateway(192,168,100,1);   // watch out, these are comma's not dots
  IPAddress subnet(255,255,255,0);
#endif  


#define TFT_DISPLAY_RESOLUTION_X 320
#define TFT_DISPLAY_RESOLUTION_Y 480

// TFT SPI
#define TFT_LED   33                  // TFT backlight pin

// Define the colors, color picker here: https://ee-programming-notepad.blogspot.com/2016/10/16-bit-color-generator-picker.html
#define TFT_TEXT_COLOR              0xFFFF  // white     0xFFFF
#define TFT_BACKGROUND_COLOR        0x0000  // black     0x00A6  // dark blue 0x00A6
#define TFT_TILE_SHADOW_COLOR       0x0000  // black     0x0000
#define TFT_TILE_BACKGROUND_COLOR_1 0x0700  // green     0x0700
#define TFT_TILE_BACKGROUND_COLOR_2 0x3314  // blue      0x3314
#define TFT_TILE_BACKGROUND_COLOR_3 0xDEC0  // yellow    0xDEC0
#define TFT_TILE_BACKGROUND_COLOR_4 0xD000  // red       0xD000
#define TFT_LED_PWM                 100     // dutyCycle 0-255 last minimum was 15

#define TFT_TIME_Y_OFFSET           50
#define TFT_SQUARE_Y_OFFSET         150
#define TFT_SQUARE_SIZE             152
#define TFT_SQUARE_FRAME_OFFSET     5
#define TFT_SQUARE_MIDLE_SPACE      6
#define TFT_VALUE_PADDING_X         15
#define TFT_VALUE_PADDING_Y         20

#define TFT_SQUARE_POS1_X           TFT_SQUARE_FRAME_OFFSET
#define TFT_SQUARE_POS1_Y           TFT_SQUARE_Y_OFFSET
#define TFT_SQUARE_POS2_X           TFT_SQUARE_FRAME_OFFSET + TFT_SQUARE_MIDLE_SPACE + TFT_SQUARE_SIZE
#define TFT_SQUARE_POS2_Y           TFT_SQUARE_Y_OFFSET
#define TFT_SQUARE_POS3_X           TFT_SQUARE_FRAME_OFFSET
#define TFT_SQUARE_POS3_Y           TFT_SQUARE_SIZE + TFT_SQUARE_Y_OFFSET + TFT_SQUARE_FRAME_OFFSET
#define TFT_SQUARE_POS4_X           TFT_SQUARE_FRAME_OFFSET + TFT_SQUARE_MIDLE_SPACE + TFT_SQUARE_SIZE
#define TFT_SQUARE_POS4_Y           TFT_SQUARE_SIZE + TFT_SQUARE_Y_OFFSET + TFT_SQUARE_FRAME_OFFSET

#define TFT_TEXT_POS1_X             TFT_SQUARE_POS1_X + TFT_VALUE_PADDING_X
#define TFT_TEXT_POS1_Y             TFT_SQUARE_POS1_Y + TFT_VALUE_PADDING_Y
#define TFT_TEXT_POS2_X             TFT_SQUARE_POS2_X + TFT_VALUE_PADDING_X
#define TFT_TEXT_POS2_Y             TFT_SQUARE_POS2_Y + TFT_VALUE_PADDING_Y
#define TFT_TEXT_POS3_X             TFT_SQUARE_POS3_X + TFT_VALUE_PADDING_X
#define TFT_TEXT_POS3_Y             TFT_SQUARE_POS3_Y + TFT_VALUE_PADDING_Y
#define TFT_TEXT_POS4_X             TFT_SQUARE_POS4_X + TFT_VALUE_PADDING_X
#define TFT_TEXT_POS4_Y             TFT_SQUARE_POS4_Y + TFT_VALUE_PADDING_Y

#define BOOT_MESSAGE "booting..."
#define REFRESH_RATE_MS 60*1000
#define REFRESH_RATE_SHT40_MS 60*1000

// Time settings
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
// A list of rules for your zone could be obtained from https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";

TFT_eSPI display = TFT_eSPI();       // Invoke custom library
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
ESP32AnalogRead adc;


float temp;
float m_volt;
float temp_in;
float d_volt;
float temp_box;
int pressure;
int humidity;
int hum_in;
int32_t wifiSignal;
String date;
uint32_t nextRefresh;
uint32_t nextRefreshSHT40;

const char* host = "tmep.cz";
const int httpPort = 80;

void readChannel() {
  // Connect to the HOST and read data via GET method
  WiFiClient client; // Use WiFiClient class to create TCP connections
 
  Serial.print("Connecting to "); Serial.println(host);
  if (!client.connect(host, httpPort)) {
    // If you didn't get a connection to the server
    Serial.println("Connection failed");
    return;
  }
  Serial.println("Client connected");
 
  // Make an url
  char url[70];                                   // Make new array, 50 bytes long
  strncpy(url, &jsonurl[15], strlen(jsonurl)-14); // Remove "https://tmep.cz" and leave rest
 
  Serial.print("Requesting URL: "); Serial.println(url);
 
  // Make HTTP GET request
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
 
  // Workaroud for timeout
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
  // Read JSON
  String data;
  bool capture = false;
  String json;
  while (client.available()) {
    data = client.readStringUntil('\n'); // Loop over rows. "\r" not work. "\0" returns all at one
    //Serial.println(data);
    // First few rows is header, it ends with empty line.
    // This is unique ultrasimple, but working solution.
    // Start capture when { occurs and end capturing with }.
    if(data.startsWith("{", 0)) { // JSON starts with {. Start capturing.
      capture = true;
    }
    if(data.startsWith("}", 0)) { // JSON ends with }. Stop capturing.
      capture = false;
    }
    if(capture) {
      json = json + data; // Joining row by row together in one nice JSON part.
    }
  }
  json = json + "}"; // WTF, last bracket is missing :/ add it here - ugly!
  Serial.println(json);
 
  // Lets throw our json on ArduinoJson library and get results!
  StaticJsonDocument<1000> doc; 
  deserializeJson(doc, json);
  auto error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return;
  }
  // This is how our JSON looks. You can read any value of parameters
  // {
  //  "teplota": 24.87,
  //  "vlhkost": null,
  //  "cas": "2016-06-02 18:29:45",
  //  "umisteni": "Trutnov"
  // }
 
  //---------------- Teplota ----------------//
  temp = doc["teplota"]; // Get value of "teplota"
  Serial.println("Temperature: " + String(temp));
  //---------------- Tlak ----------------//
  pressure = doc["tlak"];
  Serial.println("Pressure: " + String(pressure));
   //---------------- Vhlkost ----------------//
  humidity = doc["vlhkost"];
  Serial.println("Humidity: " + String(humidity));
  //---------------- Baterie ----------------//
  m_volt = doc["napeti"];
  //m_volt = 3.99;
  m_volt = round(m_volt*100.0)/100.0;     // round to x,xx
  Serial.println("Meteo Battery voltage: " + String(m_volt));
  //---------------- Date ----------------//
  date = doc["cas"].as<String>();
  Serial.println("Datum aktualizaci: " + date);
}

uint8_t getWifiStrength() {
  int32_t strength = WiFi.RSSI();
  Serial.print("Wifi Strenght: " + String(strength) + "dB; ");

  uint8_t percentage;
  if(strength <= -100) {
    percentage = 0;
  } else if(strength >= -50) {  
    percentage = 100;
  } else {
    percentage = 2 * (strength + 100);
  }
  Serial.println(String(percentage) + "%");  //Signal strength in %  

  if (percentage >= 75) strength = 4;
  else if (percentage >= 50 && percentage < 75) strength = 3;
  else if (percentage >= 25 && percentage < 50) strength = 2;
  else if (percentage >= 10 && percentage < 25) strength = 1;
  else strength = 0;
  return strength;
}

uint8_t getIntBattery() {
  d_volt = adc.readVoltage() * deviderRatio;
  Serial.println("Battery voltage: " + String(d_volt) + "V");

  // Měření napětí baterie | Battery voltage measurement

  // Simple percentage converting
  if (d_volt >= 4.0) return 5;
  else if (d_volt >= 3.8 && d_volt < 4.0) return 4;
  else if (d_volt >= 3.73 && d_volt < 3.8) return 3;
  else if (d_volt >= 3.65 && d_volt < 3.73) return 2;
  else if (d_volt >= 3.6 && d_volt < 3.65) return 1;
  else if (d_volt < 3.6) return 0;
  else return 0;
}

void printTime() {
  char buff[6];
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
    return;
  }
  display.setTextSize(1);
  display.setTextFont(7);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextDatum(TC_DATUM);
  sprintf(buff, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  display.drawString(buff, (TFT_DISPLAY_RESOLUTION_X / 2), (TFT_TIME_Y_OFFSET));
}

void printWifi() {

  // logo laskakit
 //display.drawBitmap(TFT_DISPLAY_RESOLUTION_X/2-24, TFT_DISPLAY_RESOLUTION_Y/2-24, laskarduino_glcd_bmp, 48, 48, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);

  // WiFi signal
  int32_t wifiSignalMax = 4;
  int32_t offset = 6;
  
  display.drawBitmap(0, 0, wifi1_icon16x16, 16, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);  

  for (int32_t i = 1; i <= wifiSignalMax; i++)
      display.drawRect(i * offset - 6 + 18, 0, 4, 13, TFT_TEXT_COLOR);

  for (int32_t i = 1; i <= wifiSignal; i++)
      display.fillRect(i * offset - 6 + 18, 0, 4, 13, TFT_TEXT_COLOR);
}

void printBattery() {
  // Napeti baterie meteostanice
  String meteoBateryVoltage = "";
  meteoBateryVoltage = String(m_volt,2)  + "v";
  display.setTextSize(2); 
  display.setTextFont(1); 
  display.setTextColor(TFT_WHITE, TFT_BACKGROUND_COLOR);
  display.setTextDatum(TC_DATUM);
  display.drawString(meteoBateryVoltage, TFT_DISPLAY_RESOLUTION_X / 2, 0);
 
  // Napeti baterie
  uint8_t intBatteryPercentage = getIntBattery();
  switch (intBatteryPercentage) {
    case 5:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_100, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);
      break;
     case 4:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_80, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);
      break;
    case 3:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_60, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);
      break;
    case 2:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_40, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);
      break;
     case 1:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_20, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);
      break;
    case 0:
    display.drawBitmap(TFT_DISPLAY_RESOLUTION_X-27, 0, bat_0, 27, 16, TFT_TEXT_COLOR, TFT_BACKGROUND_COLOR);
      break;
    default:
    break;
  }
}

void printLastUpdate() {
  // datum a cas
  display.setTextSize(1);
  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BACKGROUND_COLOR);
  display.setTextDatum(MC_DATUM);
  display.drawString("Last update: " + date, TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y - 10);
}

void printValues() {
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_WHITE, 0x640E);
  display.setTextPadding(display.textWidth("-99.99`C", 4));
  if ((temp > 60) || (temp < -40))
  {
    display.drawString("- `C", TFT_TEXT_POS1_X, TFT_TEXT_POS1_Y, 4);
  }
  else
  {
    display.drawString(String(temp) + " `C", TFT_TEXT_POS1_X, TFT_TEXT_POS1_Y, 4);
  }
  display.setTextColor(TFT_WHITE, 0xB50F);
  display.setTextPadding(display.textWidth("-99.99`C", 4));
  if ((temp_in > 30) || (temp_in < -10))
  {
    display.drawString("- `C", TFT_TEXT_POS2_X, TFT_TEXT_POS2_Y, 4);
  }
  else
  {
    display.drawString(String(temp_in) + " `C", TFT_TEXT_POS2_X, TFT_TEXT_POS2_Y, 4);
  }
  display.setTextColor(TFT_WHITE, 0x82EB);
  display.setTextPadding(display.textWidth("9999hPa", 4));
  if ((pressure > 1100) || (pressure < 900))
  {
    display.drawString("- hPa", TFT_TEXT_POS3_X, TFT_TEXT_POS3_Y, 4);
  }
  else
  {
    display.drawString(String(pressure) + " hPa", TFT_TEXT_POS3_X, TFT_TEXT_POS3_Y, 4);
  }
  display.setTextColor(TFT_WHITE, 0x9DB7);
  display.setTextPadding(display.textWidth("100%", 4));
  if ((humidity > 100) || (humidity < 0))
  {
    display.drawString("- %Rh", TFT_TEXT_POS4_X, TFT_TEXT_POS4_Y, 4);
  }
  else
  {
    display.drawString(String(humidity) + " %Rh", TFT_TEXT_POS4_X, TFT_TEXT_POS4_Y, 4);
  }
}

void WiFiConnection() {
  // pripojeni k WiFi
  // Connecting to last using WiFi

  Serial.println();
  Serial.print("Connecting to...");
  Serial.println(ssid);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.print("Connecting to... ");
  display.println(ssid);

  #if USE_STATIC_IP
    if (!WiFi.config(ip, gateway, subnet)) {
      Serial.println("STA Failed to configure");
      display.println("STA Failed to configure");
    }
  #endif
  WiFi.begin(ssid, pass);

  int i = 0;
  int a = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    a++;
    i++;
    if (i == 10) {
      i = 0;
      Serial.println(".");
      display.println(".");
    } else {
      Serial.print("."); 
      display.print(".");
    }
  }
  Serial.println("");
  Serial.println("Wi-Fi connected successfully");
  display.println("");
  display.println("");
  display.println("Wi-Fi connected successfully");
}

void getSHT40() {

  sensors_event_t humidity, temp; // temperature and humidity variables
  sht4.getEvent(&humidity, &temp);

  temp_in = temp.temperature;
  hum_in= humidity.relative_humidity;
  Serial.println("Temperature in: " + String(temp_in) + " degC");
  Serial.println("Humidity in: " + String(hum_in) + " % rH");
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {} // Wait until serial is ok

  // configure backlight LED PWM functionalitites
  ledcSetup(1, 5000, 8);              // ledChannel, freq, resolution
  ledcAttachPin(TFT_LED, 1);          // ledPin, ledChannel
  ledcWrite(1, TFT_LED_PWM);          // dutyCycle 0-255

  // Time config
  configTzTime(time_zone, ntpServer1, ntpServer2);

  // setting ADC
  adc.attach(ADC);
  display.begin();
  display.setRotation(0);
  display.fillScreen(TFT_BLACK);
  display.setSwapBytes(true);
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);
  display.setTextFont(1);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setTextDatum(MC_DATUM);  
  display.drawString(BOOT_MESSAGE, display.width() / 2, display.height() / 2);
  // pripojeni k WiFi
  WiFiConnection();
  display.fillScreen(TFT_BLACK);

  if (! sht4.begin())  {
    Serial.println("SHT4x not found");
    Serial.println("Check the connection");
    //while (1) delay(1);
  }
 
  sht4.setPrecision(SHT4X_HIGH_PRECISION); // highest resolution
  sht4.setHeater(SHT4X_NO_HEATER); // no heater

  display.pushImage(TFT_SQUARE_POS1_X, TFT_SQUARE_POS1_Y, TFT_SQUARE_SIZE, TFT_SQUARE_SIZE, temp_out_pic);
  display.pushImage(TFT_SQUARE_POS2_X, TFT_SQUARE_POS2_Y, TFT_SQUARE_SIZE, TFT_SQUARE_SIZE, temp_pic);
  display.pushImage(TFT_SQUARE_POS3_X, TFT_SQUARE_POS3_Y, TFT_SQUARE_SIZE, TFT_SQUARE_SIZE, press_pic);
  display.pushImage(TFT_SQUARE_POS4_X, TFT_SQUARE_POS4_Y, TFT_SQUARE_SIZE, TFT_SQUARE_SIZE, hum_pic);

  wifiSignal = getWifiStrength();
  readChannel();
  printLastUpdate();
  printBattery();
  printWifi();
  printTime();
  printValues();
}

void loop() {
  if (millis() > nextRefreshSHT40) {
    nextRefreshSHT40 = millis() + REFRESH_RATE_SHT40_MS;
    getSHT40();
  }
  // Zmer hodnoty, prekresli displej a opakuj za nextRefresh milisekund
  if (millis() > nextRefresh) {
    nextRefresh = millis() + REFRESH_RATE_MS;

    wifiSignal = getWifiStrength();
    readChannel();
    printLastUpdate();
    printBattery();
    printWifi();
    printTime();
    printValues();
  }
}
