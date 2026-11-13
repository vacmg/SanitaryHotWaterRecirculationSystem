//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Utils.h"
#include "Static.h"
#include "Sensors.h"

char* strncpy_F(char* dest, const __FlashStringHelper* src, size_t size)
{
    PGM_P p = reinterpret_cast<PGM_P>(src);
    for (size_t i = 0; i<size; i++)
    {
        char c = pgm_read_byte(p++);

        dest[i] = c;
        if (c == '\0')
        {
            break;
        }
    }

    dest[size-1] = '\0';
    return dest;
}

double fmap(double x, double in_min, double in_max, double out_min, double out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

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

void printSystemInfo()
{
    char buff[64];
    Serial.print(F(    "Remaining time until next restart: ")); Serial.println(formattedTime(SYSTEM_RESET_PERIOD - static_cast<long>(millis()), buff));
    Serial.print(F(    "Watchdogs reset period: ")); Serial.println(formattedTime(WATCHDOG_RESET_PERIOD, buff));
    Serial.print(F(    "FallBack Mode ")); Serial.println(currentMode==ErrorFallBackMode?"Enabled":"Disabled");
    Serial.print(F(    "Comms max retries: ")); Serial.println(COMMS_MAX_RETRIES);
    Serial.print(F(    "Error message max length: ")); Serial.println(ERROR_MESSAGE_SIZE);
    Serial.print(F(    "Comms message max length: ")); Serial.println(SC_MAX_MESSAGE_SIZE);
    Serial.print(F(    "Mode: ")); Serial.println(modeToString(currentMode));
    Serial.print(F(    "Status: ")); Serial.println(statusToString(currentStatus));
    Serial.print(F(    "Hot Start: ")); Serial.println(hotStart?F("ACTIVE"):F("INACTIVE"));
}

void printSensorsInfo()
{
    Serial.println(F("\n-----------------------------------------"  ));
    printSystemInfo();
    Serial.println(F(  "Sensor list:\n"));
#if MOCK_SENSORS
    Serial.println(F("Valve pressure sensor: MOCKED"));
    Serial.print(F(  "Valve temperature sensor: MOCKED to ")); Serial.print(valveTemp);Serial.println(F("ºC"));
#else
    Serial.print(F(  "Valve pressure sensor: ")); Serial.print(getValvePressure(true));Serial.println(F("BAR"));
    Serial.print(F(  "Valve temperature sensor: ")); Serial.print(getValveTemp(true));Serial.println(F("ºC"));
#endif
    int heaterTemp = getHeaterTemp(true);
    Serial.print(F(    "Heater temperature sensor: ")); Serial.print(heaterTemp);Serial.println(F("ºC"));
    Serial.print(F(    "Desired temperature: ")); Serial.print(getDesiredTemp(heaterTemp));Serial.println(F("ºC"));

    Serial.println(F(  "-----------------------------------------\n"));
}
