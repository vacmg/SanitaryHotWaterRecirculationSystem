/**
 * This is the source code of the part of the system which is near to the heater source.
 */

#include "Config.h"
#include<avr/wdt.h> /* Header for watchdog timers in AVR */
#include <MAX_RS485.h>

#if !MOCK_SENSORS
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

const uint8_t RECEIVER_ENABLE_PIN =  10;  // HIGH = Driver / LOW = Receptor
const uint8_t DRIVE_ENABLE_PIN =  9;  // HIGH = Driver / LOW = Receptor
const uint8_t RX_PIN = 11; // Serial data in pin
const uint8_t TX_PIN = 8; // Serial data out pin
const uint8_t pumpRelayPin = 7; // Pump relay pin

#if !MOCK_SENSORS
const uint8_t TEMP_SENSOR = 12;


OneWire ourWire(TEMP_SENSOR); // Create Onewire instance for temp sensor
DallasTemperature tempSensor(&ourWire); // Create temp sensor instance
#endif

MAX_RS485 rs485(RX_PIN, TX_PIN, RECEIVER_ENABLE_PIN, DRIVE_ENABLE_PIN); // module constructor

char cmdBuffer[17] = "";
bool pumpEnabled = false;
unsigned long pumpPMillis = 0;


float getTemp()
{
  #if MOCK_SENSORS
    return 63.38;
  #else
    tempSensor.requestTemperatures(); // Request temp
    return tempSensor.getTempCByIndex(0); // Obtain temp
  #endif
}


void autoDisablePumpIfTimeout()
{
  if(pumpEnabled && millis() - pumpPMillis > AUTO_DISABLE_PUMP_TIMEOUT)
  {
    pumpEnabled = false;
    digitalWrite(pumpRelayPin,RELAY_DISABLED);

    debug(F("ERROR: TIMEOUT REACHED FOR PUMP. (Elapsed time = "));
    debug(millis() - pumpPMillis); debug(F("ms > Timeout = "));
    debug(AUTO_DISABLE_PUMP_TIMEOUT);debugln(F("ms)\nDISCONECTING IT..."));

    debugln(F("Rebooting both MCUs"));
    rebootLoop();
  }
}


void handleRS485Event()
{
  if(rs485.available() && rs485.find(HEADER))
  {
    size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
    cmdBuffer[dataSize] = '\0';
    //debug(F("Command:\t")); debugln(cmdBuffer);

    #if !DISABLE_WATCHDOGS
    if(strcmp(cmdBuffer,WTDRSTCMD) == 0)
    {
      wdt_reset();
      #if DEBUGWATCHDOG
      debugln(F("Watchdog Reset CMD PARSED"));
      #endif

      sprintf(cmdBuffer,"%s%s$",HEADER,OKCMD);

      #if DEBUGWATCHDOG
      debug(F("Sending OK CMD: ")); debugln(cmdBuffer); debugln();
      #endif

      rs485.print(cmdBuffer);
    }
    else
    #endif
    if(strcmp(cmdBuffer,pumpCMD)==0)
    {
      debugln(F("PUMP CMD PARSED"));

      size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
      cmdBuffer[dataSize] = '\0';
      debug(F("Enable:\t")); debugln(cmdBuffer);

      if(atoi(cmdBuffer))
      {
        pumpPMillis = millis();
        pumpEnabled = true;
        digitalWrite(pumpRelayPin,RELAY_ENABLED);

        debug(F("Starting pump at millis() = ")); debugln(pumpPMillis); 
        debug(F("Max pump on time: ")); debug(AUTO_DISABLE_PUMP_TIMEOUT); debugln(F("ms\n"));
      }
      else
      {
        digitalWrite(pumpRelayPin,RELAY_DISABLED);

        debug(F("Stoping pump... ")); 
        #if DEBUG
          if(pumpEnabled)
          {
            debug(F("Elapsed time = "));
            debug(millis() - pumpPMillis);debug(F("ms"));
          }
        #endif
        debugln('\n');

        pumpEnabled = false;
      }
      
      sprintf(cmdBuffer,"%s%s$",HEADER,OKCMD);

      debug(F("Sending OK CMD: ")); debugln(cmdBuffer);  debugln();

      rs485.print(cmdBuffer);
    }
    else if(strcmp(cmdBuffer,tempCMD)==0)
    {
      debugln(F("TEMP CMD PARSED"));

      int temp = (int)getTemp();

      sprintf(cmdBuffer,"%s%s$%d$",HEADER,tempCMD,temp);

      debug(F("Sending TEMP CMD ANSWER: ")); debugln(cmdBuffer); debugln();

      rs485.print(cmdBuffer);
    }
    else
    {
      debugln(F("WARNING: Unknown Command parsed"));
    }
    if(rs485.available())
    {
      debug(F("WARNING: Discarding extra RS485 input data:\t\""));
      while(rs485.available())
      {
        #if DEBUG
          debug((char)rs485.read());
        #else
          rs485.read();
        #endif
      }
      debug(F("\"\n\n"));
    }
  }
}


void waitForValveConnection()
{
  #if !DISABLE_WATCHDOGS

  char cmdBuffer[17];
  unsigned long connectionPMillis = millis();
  bool connected = false;

  debugln(F("Waiting for connection from valve MCU..."));

  while(!connected && millis() - connectionPMillis < INIT_CONNECTION_TIMEOUT)
  {
    wdt_reset();
    delay(20);
    if(rs485.available() && rs485.find(HEADER))
    {
      size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
      cmdBuffer[dataSize] = '\0';
      //debug(F("Command:\t")); debugln(cmdBuffer);

      if(strcmp(cmdBuffer,WTDRSTCMD) == 0)
      {
        #if DEBUGWATCHDOG
        debugln(F("Watchdog Reset CMD PARSED"));
        #endif

        connected = true;

        sprintf(cmdBuffer,"%s%s$",HEADER,OKCMD);

        #if DEBUGWATCHDOG
        debug(F("Sending OK CMD: ")); debugln(cmdBuffer);
        #endif

        rs485.print(cmdBuffer);
      }
    }
  }

  if(!connected)
  {
    Serial.println(F("\n-----------------------------------------"));
    Serial.println(F("-----------------------------------------"));
    Serial.println(F("ERROR: Cannot connect to valve MCU"));
    Serial.println(F("-----------------------------------------"));
    Serial.println(F("-----------------------------------------\n"));

    Serial.print(F("Rebooting"));
    rebootLoop();
  }
  debugln(F("Connected to valve MCU!!!"));
  delay(1500);

  #endif
}


void setup() 
{
  wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds */
  #if !DISABLE_WATCHDOGS
    delay(10000); /* Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */
  #endif

  pinMode(pumpRelayPin,OUTPUT);
  digitalWrite(pumpRelayPin,RELAY_DISABLED);
  Serial.begin(9600); // Used for debug purposes
  delay(3000);
  Serial.println(F("\n------------------------------------------"  ));
  Serial.println(F(  "|                SHWRS-HS                |"  ));
  Serial.println(F(  "|                V1.0.0R1                |"  ));
  Serial.println(F(  "------------------------------------------\n"));
  
  #if !MOCK_SENSORS
    tempSensor.begin();
  #endif

  rs485.begin(9600,RECEIVED_MESSAGE_TIMEOUT); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  delay(1000);

  waitForValveConnection();

  Serial.println(F("Heater side system successfully started!!!"));
  
  #if MOCK_SENSORS
    Serial.println(F("WARNING: SENSOR MOCKING ENABLED"));
  #endif

  delay(1000);
}


void loop() 
{ 
  handleRS485Event();
  autoDisablePumpIfTimeout();

  delay(100);
}
