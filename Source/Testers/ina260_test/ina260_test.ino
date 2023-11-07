#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA260.h> //https://github.com/adafruit/Adafruit_INA260
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // display width, in pixels
#define SCREEN_HEIGHT 64 // display height, in pixels
#define OLED_RESET    4 // set to unused pin
#define SCREEN_ADDRESS 0x3D // OLED I2C address

Adafruit_INA260 ina260 = Adafruit_INA260();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  // Wait until serial port is opened
  while (!Serial) { delay(10); }

  Serial.println("Adafruit INA260 Test");

  if (!ina260.begin()) {
    Serial.println("Couldn't find INA260 chip");
    while (1);
  }
  Serial.println("Found INA260 chip");

  // set the number of samples to average
  ina260.setAveragingCount(INA260_COUNT_16);

  // set the time over which to measure the current and bus voltage
  ina260.setVoltageConversionTime(INA260_TIME_1_1_ms);
  ina260.setCurrentConversionTime(INA260_TIME_1_1_ms);

  //  SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1); // Don't proceed, wait forever
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  float current = ina260.readCurrent();
  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" mA");

  Serial.print("Bus Voltage: ");
  Serial.print(ina260.readBusVoltage());
  Serial.println(" mV");

  Serial.print("Power: ");
  Serial.print(ina260.readPower());
  Serial.println(" mW");

  Serial.println();


  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  display.setCursor(15,5);
  display.print(current,1);
  display.print(" mA");
  display.display();


  delay(200);
}
