#include <Arduino.h>
#include "Config.h"
#include "SimpleComms.h"
#include <MAX_RS485.h>

#if !MOCK_SENSORS
    #include <OneWire.h>
    #include <DallasTemperature.h>
#endif


const uint8_t RECEIVER_ENABLE_PIN =  10;  // HIGH = Driver / LOW = Receptor
const uint8_t DRIVE_ENABLE_PIN =  9;  // HIGH = Driver / LOW = Receptor
const uint8_t pumpRelayPin = 7; // Pump relay pin

#if !MOCK_SENSORS
    const uint8_t TEMP_SENSOR = 12;

    OneWire oneWire(TEMP_SENSOR); // Create Onewire instance for temp sensor
    DallasTemperature tempSensor(&oneWire); // Create temp sensor instance
#endif

MAX_RS485 rs485(&Serial1, RECEIVER_ENABLE_PIN, DRIVE_ENABLE_PIN); // module constructor
SimpleComms comms(&rs485, HEADER);

bool pumpEnabled = false;
unsigned long pumpPMillis = 0;

char commsBuffer[SC_MAX_MESSAGE_SIZE+1] = "";


const char* formattedTime(long milliseconds, char* buff)
{
    if(milliseconds<0)
    {
        strcpy(buff, "None");
        return buff;
    }

    long ms = milliseconds%1000;
    long s = milliseconds/1000;
    long m = s/60;
    long h = m/60;
    m = m%60;
    s = s%60;

    sprintf(buff,"%lih %lim %lis %lims (%lims)",h,m,s,ms,milliseconds);
    return buff;
}

float getTemp()
{
    #if MOCK_SENSORS
        return 63.38;
    #else
        tempSensor.requestTemperatures(); // Request temp
        return tempSensor.getTempCByIndex(0); // Obtain temp
    #endif
}

void waitForValveConnection()
{
    #if !DISABLE_WATCHDOGS
        unsigned long connectionPMillis = millis();
        bool connected = false;

        debugln(F("Waiting for connection from valve MCU..."));

        while(!connected && (millis() - connectionPMillis < INIT_CONNECTION_TIMEOUT))
        {
            wdt_reset();
            delay(20);

            if(comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0 && strcmp(commsBuffer, WTDRSTCMD) == 0)
            {
                debugln(F("Got a message from valve MCU, answering with OK..."));
                comms.sendCommand(OKCMD, nullptr, 0);
                connected = true;
                wdt_reset();
                debugln(F("Connection established with valve MCU"));
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
    #endif
}

void autoDisablePumpIfTimeout()
{
    if(pumpEnabled && millis() - pumpPMillis > AUTO_DISABLE_PUMP_TIMEOUT)
    {
        pumpEnabled = false;
        digitalWrite(pumpRelayPin,RELAY_DISABLED);

        debug(F("ERROR: TIMEOUT REACHED FOR PUMP. (Elapsed time = "));
        debug(formattedTime(millis() - pumpPMillis, commsBuffer)); debug(F(" > Timeout = "));
        debug(formattedTime(AUTO_DISABLE_PUMP_TIMEOUT, commsBuffer));debugln(F(")\nDISCONECTING IT..."));

        // TODO send error message to the other MCU

        debugln(F("Rebooting both MCUs"));
        rebootLoop();
    }
}

void handleRS485Event()
{
    if(comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0)
    {
        #if !DISABLE_WATCHDOGS
        if(strcmp(commsBuffer, WTDRSTCMD) == 0)
        {
            wdt_reset();
            #if DEBUGWATCHDOG
            debugln(F("Watchdog Reset CMD PARSED"));
            debugln(F("Sending OK CMD"));
            #endif

            comms.sendCommand(OKCMD, nullptr, 0);
        }
        else
        #endif
        if(strcmp(commsBuffer, pumpCMD) == 0)
        {
            debugln(F("PUMP CMD PARSED"));

            if(comms.getNextArgument(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0)
            {
                debug(F("Enable:\t")); debugln(commsBuffer);

                if(atoi(commsBuffer))
                {
                    pumpPMillis = millis();
                    pumpEnabled = true;
                    digitalWrite(pumpRelayPin,RELAY_ENABLED);

                    debug(F("Starting pump at millis() = ")); debugln(pumpPMillis);
                    debug(F("Max pump on time: ")); debugln(formattedTime(AUTO_DISABLE_PUMP_TIMEOUT, commsBuffer));
                }
                else
                {
                    debug(F("Stopping pump at millis() = ")); debugln(millis());
                    digitalWrite(pumpRelayPin,RELAY_DISABLED);

                    if(pumpEnabled)
                    {
                        debug(F("Elapsed time = "));
                        debugln(formattedTime(millis() - pumpPMillis, commsBuffer));
                    }
                    pumpEnabled = false;
                }

                debugln(F("Sending OK CMD"));
                comms.sendCommand(OKCMD, nullptr, 0);
            }
        }
        else if(strcmp(commsBuffer, tempCMD) == 0)
        {
            debugln(F("TEMP CMD PARSED"));

            int temp = (int)getTemp();
            debug(F("Current temp: ")); debugln(temp);

            char tempStr[10];
            sprintf(tempStr, "%d", temp);

            debug(F("Sending TEMP CMD ANSWER: ")); debugln(tempStr);
            comms.sendCommand(tempCMD, (const char**)&tempStr, 1);

        }
        else
        {
            debug(F("WARNING: Unknown command: ")); debugln(commsBuffer);
        }
        debugln();
    }
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
    Serial.begin(SERIAL_USB_BAUD_RATE); // Used for debug purposes
    delay(3000);
    Serial.println(F("\n------------------------------------------"  ));
    Serial.println(F(  "|                SHWRS-HS                |"  ));
    Serial.println(F(  "|                 " VS "                 |"  ));
    Serial.println(F(  "------------------------------------------\n"));

    #if !MOCK_SENSORS
        tempSensor.begin();
    #endif

    #if SC_USE_HAMMING_7_4_CORRECTION_CODE
        Serial.println(F("INFO: HAMMING 7,4 CORRECTION CODE ENABLED FOR RS485 COMMUNICATION OVER SERIAL1"));
        rs485.begin(RS485_SERIAL_BAUD_RATE, RECEIVED_MESSAGE_TIMEOUT, SERIAL_7N1); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function), third argument is hardware serial options
    #else
        rs485.begin(RS485_SERIAL_BAUD_RATE, RECEIVED_MESSAGE_TIMEOUT); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)
    #endif

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
