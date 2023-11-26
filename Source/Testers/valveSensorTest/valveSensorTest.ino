#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define ENABLE_TEMP 1

#if ENABLE_TEMP

#include <OneWire.h>                
#include <DallasTemperature.h>

#endif

#define SCREEN_WIDTH 128 // display width, in pixels
#define SCREEN_HEIGHT 64 // display height, in pixels
#define OLED_RESET    4 // set to unused pin
#define SCREEN_ADDRESS 0x3D // OLED I2C address

#define Y_OFFSET 20
#define Tperiod 1000

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PRESSURE_SENSOR A0
#define TEMP_SENSOR 12

#if ENABLE_TEMP
OneWire ourWire(TEMP_SENSOR);                //Se establece el pin 2  como bus OneWire
DallasTemperature tempSensor(&ourWire); //Se declara una variable u objeto para nuestro sensor

float temp = NAN;
long Tpmillis = 0;
#endif

void setup() {
  Serial.begin(115200);
  // Wait until serial port is opened
  while (!Serial) { delay(10); }

  analogReference(INTERNAL);

  #if ENABLE_TEMP
  tempSensor.begin();
  #endif

  //  SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1); // Don't proceed, wait forever
  }
  display.clearDisplay();
  display.display();
}



void loop() {
  int adcData = analogRead(A0);

  //adcData = random(0,1023);

  double voltage = (adcData * 1.1)/1024;
  double current = (voltage*1000) / 51;

  #if ENABLE_TEMP
  if(millis() - Tpmillis > Tperiod)
  {
    tempSensor.requestTemperatures();   //Se envía el comando para leer la temperatura
    temp= tempSensor.getTempCByIndex(0); //Se obtiene la temperatura en ºC
    Tpmillis = millis();
  }
  #endif

  Serial.print("ADC RAW: ");
  Serial.print(adcData);
  Serial.println();                

  Serial.print("Resistor Voltage: ");
  Serial.print(voltage);
  Serial.println(" V");

  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" mA");

  #if ENABLE_TEMP
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.println(" C");  
  #endif

  Serial.println();


  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  int x = 5;
  int y = 5;

  display.setCursor(x,y);
  display.print("V:");
  display.print(voltage,2);
  display.print(" V");

  y+=Y_OFFSET;

  display.setCursor(x,y);
  display.print("I:");
  display.print(current,2);
  display.print(" mA");

  #if ENABLE_TEMP

  y+=Y_OFFSET;

  display.setCursor(x,y);
  display.print("T:");
  display.print(temp,2);
  display.print(" C");
  
  #endif
  
  display.display();

  delay(50);
}
