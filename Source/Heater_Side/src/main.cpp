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
long autoDisablePumpTimeout = AUTO_DISABLE_PUMP_TIMEOUT;

unsigned long tempRequestTempMillis = 0;
bool tempRequested = false;
unsigned long TEMP_WAIT_FROM_REQUEST_TO_READ; // Updated to the real value at setup
float temp = 0;

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

void requestTempIfNecessary()
{
    #if MOCK_SENSORS
    if((!tempRequested) && (millis() - tempRequestTempMillis > HEATER_TEMP_GATHERING_PERIOD))
    {
        tempRequestTempMillis = millis();
        tempSensor.requestTemperatures();
        tempRequested = true;
    }
    #endif
}

void getTempIfNecessary()
{
    #if MOCK_SENSORS
    if((tempRequested) && (millis() - tempRequestTempMillis > TEMP_WAIT_FROM_REQUEST_TO_READ))
    {
        temp = tempSensor.getTempCByIndex(0);
        tempRequested = false;
    }
    #endif
}

float getTemp()
{
    #if MOCK_SENSORS
        return 63.38;
    #else
        return temp;
    #endif
}

void waitForValveConnection()
{
    #if !DISABLE_WATCHDOGS
        unsigned long connectionPMillis = millis();
        bool connected = false;

        Serial.println(F("Waiting for connection from valve MCU..."));

        while(!connected && (millis() - connectionPMillis < INIT_CONNECTION_TIMEOUT))
        {
            wdt_reset();
            delay(20);

            int res = comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE);
            if(res > 0 && strcmp(commsBuffer, WTDRSTCMD) == 0)
            {
                debugln(F("Got a message from valve MCU, answering with OK..."));
                comms.sendCommand(OKCMD, nullptr, 0);
                connected = true;
                wdt_reset();
                Serial.println(F("Connection established with valve MCU"));
            }
            else if(res < 0)
            {
                Serial.println(F("WARNING: Message header could not be parsed and was discarded"));
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
    if(autoDisablePumpTimeout <= 0)
    {
        return;
    }

    if(pumpEnabled && millis() - pumpPMillis > autoDisablePumpTimeout)
    {
        pumpEnabled = false;
        digitalWrite(pumpRelayPin,RELAY_DISABLED);

        char buff[32];
        char buff2[32];
        sprintf(commsBuffer, "TIMEOUT REACHED FOR PUMP. (Elapsed time = %s > Timeout = %s)", formattedTime(static_cast<long>(millis() - pumpPMillis), buff), formattedTime(autoDisablePumpTimeout, buff2));
        Serial.print(F("ERROR: "));Serial.println(commsBuffer);Serial.println(F("DISCONNECTING IT..."));
        const char* argsPtr[] = {commsBuffer};

        comms.sendCommand(ERRCMD, argsPtr, 1);

        Serial.println(F("Rebooting both MCUs"));
        rebootLoop();
    }
}

void handleCommsEvent()
{
    int res = comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE);
    if(res > 0)
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

            #if DEBUGWATCHDOG
                debugln(F("Watchdog reset command processed"));
            #endif
        }
        else
        #endif
        if(strcmp(commsBuffer, pumpCMD) == 0)
        {
            debugln(F("PUMP CMD PARSED"));
            wdt_reset();

            if(comms.getNextArgument(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0)
            {
                debug(F("Enable:\t")); debugln(commsBuffer);

                if(atoi(commsBuffer))
                {
                    pumpPMillis = millis();
                    pumpEnabled = true;
                    digitalWrite(pumpRelayPin, RELAY_ENABLED);

                    debug(F("Starting pump at millis() = ")); debugln(pumpPMillis);
                    debug(F("Max pump on time: ")); debugln(formattedTime(autoDisablePumpTimeout, commsBuffer));
                }
                else
                {
                    debug(F("Stopping pump at millis() = ")); debugln(millis());
                    digitalWrite(pumpRelayPin, RELAY_DISABLED);

                    if(pumpEnabled)
                    {
                        debug(F("Elapsed time = "));
                        debugln(formattedTime(millis() - pumpPMillis, commsBuffer));
                    }
                    pumpEnabled = false;
                }

                debugln(F("Sending OK CMD"));
                comms.sendCommand(OKCMD, nullptr, 0);
                debugln(F("Pump command processed"));
            }
            else
            {
                sprintf(commsBuffer, "No argument found in PUMP command");
                Serial.print(F("ERROR: ")); Serial.println(commsBuffer);
                const char* argsPtr[] = {commsBuffer};
                comms.sendCommand(ERRCMD, argsPtr, 0);

                Serial.println(F("Rebooting both MCUs"));
                rebootLoop();
            }
        }
        else if(strcmp(commsBuffer, tempCMD) == 0)
        {
            debugln(F("TEMP CMD PARSED"));
            wdt_reset();

            int temp = (int)getTemp();
            debug(F("Current temp: ")); debugln(temp);

            char tempStr[10];
            sprintf(tempStr, "%d", temp);
            const char* argsPtr[] = {tempStr};

            debug(F("Sending TEMP CMD ANSWER: ")); debugln(tempStr);
            comms.sendCommand(tempCMD, argsPtr, 1);
            debugln(F("Temp sent"));

        }
        else if(strcmp(commsBuffer, setPumpTimeoutCMD) == 0)
        {
            debugln(F("DPT CMD PARSED"));
            wdt_reset();

            if(comms.getNextArgument(commsBuffer, SC_MAX_MESSAGE_SIZE))
            {
                autoDisablePumpTimeout = atol(commsBuffer);
                debug(F("New pump timeout: ")); debugln(autoDisablePumpTimeout);

                debugln(F("Sending OK CMD"));
                comms.sendCommand(OKCMD, nullptr, 0);
                debugln(F("Pump timeout updated"));
            }
            else
            {
                sprintf(commsBuffer, "No argument found in DPT command");
                Serial.print(F("ERROR: ")); Serial.println(commsBuffer);
                const char* argsPtr[] = {commsBuffer};
                comms.sendCommand(ERRCMD, argsPtr, 0);

                Serial.println(F("Rebooting both MCUs"));
                rebootLoop();
            }
        }
        else
        {
            Serial.print(F("WARNING: Unknown command: ")); Serial.println(commsBuffer);
        }
    }
    else if(res < 0)
    {
        Serial.println(F("WARNING: Message header could not be parsed and was discarded"));
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
        tempSensor.setWaitForConversion(false);
        TEMP_WAIT_FROM_REQUEST_TO_READ = DallasTemperature::millisToWaitForConversion(tempSensor.getResolution());
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
    requestTempIfNecessary();
    getTempIfNecessary();

    handleCommsEvent();
    autoDisablePumpIfTimeout();

    delay(100);
}
