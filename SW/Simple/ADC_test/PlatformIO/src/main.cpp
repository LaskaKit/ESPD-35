#include <Arduino.h>
#include <ESP32AnalogRead.h>
#include <TFT_eSPI.h>               // Hardware-specific library
#include <SPI.h>

#define ADC 34                      // Battery voltage mesurement
#define deviderRatio 1.3

// TFT SPI
#define TFT_LED 33			// TFT backlight pin
#define TFT_LED_PWM 100 // dutyCycle 0-255 last minimum was 15
#define TFT_DISPLAY_RESOLUTION_X 480
#define TFT_DISPLAY_RESOLUTION_Y 320

ESP32AnalogRead adc;
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height

void displayInit()
{
	tft.init();
	tft.setRotation(1);
	tft.fillScreen(TFT_BLACK);
}

void setup() {
  ledcSetup(1, 5000, 8);		 // ledChannel, freq, resolution
	ledcAttachPin(TFT_LED, 1); // ledPin, ledChannel
	ledcWrite(1, TFT_LED_PWM); // dutyCycle 0-255
  displayInit();
  adc.attach(ADC);
}

void loop() {
	tft.setTextSize(1);
	tft.setTextFont(4);
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
  tft.drawString("Battery voltage is " + String(adc.readVoltage() * deviderRatio) + " V", TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y / 2);
  delay(100);
}