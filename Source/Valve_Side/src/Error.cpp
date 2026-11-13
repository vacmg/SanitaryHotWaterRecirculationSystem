//
// Created by varzoz on 20/6/26.
//

#include "Error.h"
#include "Color.h"
#include "Status.h"
#include "Utils.h"
#include "Static.h"

[[noreturn]] void raiseError(ErrorCode error, const char* message)
{
    #if !DISABLE_WATCHDOGS
        wdt_reset();
    #endif
    Serial.println(F("\n-----------------------------------------"));
    Serial.println(F("-----------------------------------------"));
    Serial.print(F("ERROR RAISED: ")); Serial.print(getErrorName(error)); if(message != nullptr) {Serial.print(F(": ")); Serial.print(message);} else {Serial.print(F("No aditional information provided"));} Serial.println();
    Serial.println(F("-----------------------------------------"));
    Serial.println(F("-----------------------------------------\n"));

    writeColor(ERROR_FALLBACK_COLOR);

    #if !EEPROM_DONT_WRITE_ERRORS
    if(error != NO_ERROR)
    {
        bool errorSaved = false;
        for(uint8_t i = 0; !errorSaved && i < EEPROM_ERROR_MEMORY_ITEMS; i++)
        {
            EEPROMError eepromError;
            EEPROM.get(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);

            if(!eepromError.activeError)
            {
                eepromError.activeError = true;
                eepromError.errorCode = error;
                if(message != nullptr)
                {
                    strncpy(eepromError.message, message, ERROR_MESSAGE_SIZE);
                }
                else
                {
                    strncpy_P(eepromError.message, PSTR("No message provided"), ERROR_MESSAGE_SIZE);
                }
                EEPROM.put(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
                errorSaved = true;
            }
        }
        if(!errorSaved)
        {
            Serial.println(F("ERROR: Error memory is full"));
        }

        toggleFallbackMode(true);
    }
    #endif

    Serial.println(F("Rebooting..."));
    rebootLoop();
}

[[noreturn]] void raiseError(ErrorCode error, const __FlashStringHelper* message)
{
    char buff[ERROR_MESSAGE_SIZE];

    strncpy_F(buff,message,ERROR_MESSAGE_SIZE);

    raiseError(error, buff);
}

void invalidateErrorData()
{
    EEPROMError eepromError;
    for(uint8_t i = 0; i < EEPROM_ERROR_MEMORY_ITEMS; i++)
    {
        EEPROM.get(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
        eepromError.activeError = false;
        EEPROM.put(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
    }
}

void printErrorData()
{
    EEPROMError eepromError;
    bool anyError = false;
    for(uint8_t i = 0; i < EEPROM_ERROR_MEMORY_ITEMS; i++)
    {
        EEPROM.get(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
        if(eepromError.activeError)
        {
            anyError = true;
            Serial.print(F("Error: ")); Serial.print(getErrorName(eepromError.errorCode)); Serial.print(F(" - ")); Serial.println(eepromError.message);
        }
    }

    if(!anyError)
    {
        Serial.println(F("No errors found"));
    }

}

void handleHeaterError(int retryCount, char* buff)
{
    if(retryCount>=COMMS_MAX_RETRIES)
    {
        if(comms.getNextArgument(buff, SC_MAX_MESSAGE_SIZE) > 0)
        {
            raiseError(ERROR_HEATER_MCU_ERROR, buff);
        }
        raiseError(ERROR_HEATER_MCU_ERROR);
    }
}
