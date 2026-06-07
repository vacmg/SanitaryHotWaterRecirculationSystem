#include "Errors.h"
#include "Config.h"
#include "Globals.h"
#include "Leds.h"
#include "Utils.h"
#include "FSM.h"
#include <avr/pgmspace.h>
#include <math.h>
#include <string.h>

enum ErrorMessageRenderMode : uint8_t
{
    ERROR_MESSAGE_RENDER_FLASH_STRING = 0,
    ERROR_MESSAGE_RENDER_INT_VALUE,
    ERROR_MESSAGE_RENDER_PRESSURE_RANGE
};

static constexpr char errorNameNoError[] PROGMEM = "NO_ERROR";
static constexpr char errorNameTempSensorInvalidValue[] PROGMEM = "ERROR_TEMP_SENSOR_INVALID_VALUE";
static constexpr char errorNamePressureSensorInvalidValue[] PROGMEM = "ERROR_PRESSURE_SENSOR_INVALID_VALUE";
static constexpr char errorNameCommsConnectionNotEstablished[] PROGMEM = "ERROR_COMMS_CONNECTION_NOT_ESTABLISHED";
static constexpr char errorNameCommsNoResponse[] PROGMEM = "ERROR_COMMS_NO_RESPONSE";
static constexpr char errorNameCommsUnexpectedMessage[] PROGMEM = "ERROR_COMMS_UNEXPECTED_MESSAGE";
static constexpr char errorNameHeaterMcuError[] PROGMEM = "ERROR_HEATER_MCU_ERROR";
static constexpr char errorNameUnknown[] PROGMEM = "Unknown Error";

static const char* const errorNameTable[] PROGMEM = {
    errorNameNoError,
    errorNameTempSensorInvalidValue,
    errorNamePressureSensorInvalidValue,
    errorNameCommsConnectionNotEstablished,
    errorNameCommsNoResponse,
    errorNameCommsUnexpectedMessage,
    errorNameHeaterMcuError
};

static constexpr char errorMsgTemplateNone[] PROGMEM = "No persisted detail";
static constexpr char errorMsgTemplateValveTempTooLow[] PROGMEM = "VALVE TEMP IS TOO LOW (%d)";
static constexpr char errorMsgTemplateValveTempTooHigh[] PROGMEM = "VALVE TEMP IS TOO HIGH (%d)";
static constexpr char errorMsgTemplateHeaterTempTooLow[] PROGMEM = "HEATER TEMP IS TOO LOW (%d)";
static constexpr char errorMsgTemplateHeaterTempTooHigh[] PROGMEM = "HEATER TEMP IS TOO HIGH (%d)";
static constexpr char errorMsgTemplatePressureCurrentOutsideRange[] PROGMEM = "PRESSURE CURRENT (%dmA) IS OUTSIDE THE RANGE (%d, %d)mA";
static constexpr char errorMsgTemplateTimeoutSetPump[] PROGMEM = "Timeout receiving setPump response";
static constexpr char errorMsgTemplateParseHeaderSetPump[] PROGMEM = "Unable to parse message header in setPump";
static constexpr char errorMsgTemplateUnexpectedSetPump[] PROGMEM = "Unexpected response from the HEATER MCU at setPump: <not persisted>";
static constexpr char errorMsgTemplateTimeoutSetPumpTimeout[] PROGMEM = "Timeout receiving setPumpTimeout response";
static constexpr char errorMsgTemplateParseHeaderSetPumpTimeout[] PROGMEM = "Unable to parse message header in setPumpTimeout";
static constexpr char errorMsgTemplateUnexpectedSetPumpTimeout[] PROGMEM = "Unexpected response from the HEATER MCU at setPumpTimeout: <not persisted>";
static constexpr char errorMsgTemplateTimeoutGetTemp[] PROGMEM = "Timeout receiving getTemp response";
static constexpr char errorMsgTemplateParseHeaderGetTemp[] PROGMEM = "Unable to parse message header in getTemp";
static constexpr char errorMsgTemplateUnexpectedGetHeaterTemp[] PROGMEM = "Unexpected response from the HEATER MCU at getHeaterTemp: <not persisted>";
static constexpr char errorMsgTemplateNoHeaterTempValue[] PROGMEM = "No temperature value received from the HEATER MCU";
static constexpr char errorMsgTemplateTimeoutConnectHeater[] PROGMEM = "Timeout connecting to the HEATER MCU";
static constexpr char errorMsgTemplateUnexpectedConnectHeater[] PROGMEM = "Unexpected response from the HEATER MCU at connectToHeater: <not persisted>";
static constexpr char errorMsgTemplateTimeoutWtdRst[] PROGMEM = "Timeout receiving WTD-RST response";
static constexpr char errorMsgTemplateParseHeaderResetWatchdogs[] PROGMEM = "Unable to parse message header in resetWatchdogs";
static constexpr char errorMsgTemplateUnexpectedResetWatchdogs[] PROGMEM = "Unexpected response from the HEATER MCU at resetWatchdogs: <not persisted>";
static constexpr char errorMsgTemplateRemoteHeaterDetail[] PROGMEM = "HEATER MCU returned an error detail: <not persisted>";
static constexpr char errorMsgTemplateInfoSystemResetTimeout[] PROGMEM = "INFO: Restarting the system due to SYSTEM_RESET_PERIOD timeout";
static constexpr char errorMsgTemplateInfoRebootUserCommand[] PROGMEM = "Rebooting by user command";
static constexpr char errorMsgTemplateInfoRebootResetDisableFallback[] PROGMEM = "Rebooting to complete reset and disable fallback mode";
static constexpr char errorMsgTemplateInfoRebootResetEnableFallback[] PROGMEM = "Rebooting to complete reset and enable fallback mode";
static constexpr char errorMsgTemplateInfoRebootButtonLongPress[] PROGMEM = "Rebooting by user command (button long press)";

static const char* const errorMessageTemplateTable[] PROGMEM = {
    errorMsgTemplateNone,
    errorMsgTemplateValveTempTooLow,
    errorMsgTemplateValveTempTooHigh,
    errorMsgTemplateHeaterTempTooLow,
    errorMsgTemplateHeaterTempTooHigh,
    errorMsgTemplatePressureCurrentOutsideRange,
    errorMsgTemplateTimeoutSetPump,
    errorMsgTemplateParseHeaderSetPump,
    errorMsgTemplateUnexpectedSetPump,
    errorMsgTemplateTimeoutSetPumpTimeout,
    errorMsgTemplateParseHeaderSetPumpTimeout,
    errorMsgTemplateUnexpectedSetPumpTimeout,
    errorMsgTemplateTimeoutGetTemp,
    errorMsgTemplateParseHeaderGetTemp,
    errorMsgTemplateUnexpectedGetHeaterTemp,
    errorMsgTemplateNoHeaterTempValue,
    errorMsgTemplateTimeoutConnectHeater,
    errorMsgTemplateUnexpectedConnectHeater,
    errorMsgTemplateTimeoutWtdRst,
    errorMsgTemplateParseHeaderResetWatchdogs,
    errorMsgTemplateUnexpectedResetWatchdogs,
    errorMsgTemplateRemoteHeaterDetail,
    errorMsgTemplateInfoSystemResetTimeout,
    errorMsgTemplateInfoRebootUserCommand,
    errorMsgTemplateInfoRebootResetDisableFallback,
    errorMsgTemplateInfoRebootResetEnableFallback,
    errorMsgTemplateInfoRebootButtonLongPress
};

static const uint8_t errorMessageRenderModeTable[] PROGMEM = {
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_INT_VALUE,
    ERROR_MESSAGE_RENDER_INT_VALUE,
    ERROR_MESSAGE_RENDER_INT_VALUE,
    ERROR_MESSAGE_RENDER_INT_VALUE,
    ERROR_MESSAGE_RENDER_PRESSURE_RANGE,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING,
    ERROR_MESSAGE_RENDER_FLASH_STRING
};

static_assert((sizeof(errorMessageTemplateTable) / sizeof(errorMessageTemplateTable[0])) == ERROR_MSG_TEMPLATE_COUNT, "errorMessageTemplateTable must match ErrorMessageTemplate enum");
static_assert((sizeof(errorMessageRenderModeTable) / sizeof(errorMessageRenderModeTable[0])) == ERROR_MSG_TEMPLATE_COUNT, "errorMessageRenderModeTable must match ErrorMessageTemplate enum");
static_assert((sizeof(errorNameTable) / sizeof(errorNameTable[0])) == ENUM_LEN, "errorNameTable must match ErrorCode enum");

static int errorAddressForIndex(const uint8_t i)
{
    return EEPROM_ERROR_ENTRIES_START_ADDRESS + i * static_cast<int>(sizeof(EEPROMError));
}

static EEPROMError getEmptyErrorRecord()
{
    EEPROMError err = {0, static_cast<uint8_t>(NO_ERROR), static_cast<uint8_t>(ERROR_MSG_TEMPLATE_NONE), 0, NAN, ""};
    return err;
}

static ErrorMessageTemplate normalizeMessageTemplate(ErrorMessageTemplate messageTemplate)
{
    const uint8_t index = static_cast<uint8_t>(messageTemplate);
    if(index >= ERROR_MSG_TEMPLATE_COUNT)
    {
        return ERROR_MSG_TEMPLATE_NONE;
    }
    return messageTemplate;
}

static PGM_P getMessageTemplateFormat(ErrorMessageTemplate messageTemplate)
{
    const ErrorMessageTemplate normalizedTemplate = normalizeMessageTemplate(messageTemplate);
    return static_cast<PGM_P>(pgm_read_ptr(&errorMessageTemplateTable[static_cast<uint8_t>(normalizedTemplate)]));
}

static ErrorMessageRenderMode getMessageRenderMode(ErrorMessageTemplate messageTemplate)
{
    const ErrorMessageTemplate normalizedTemplate = normalizeMessageTemplate(messageTemplate);
    return static_cast<ErrorMessageRenderMode>(pgm_read_byte(&errorMessageRenderModeTable[static_cast<uint8_t>(normalizedTemplate)]));
}

static PGM_P getErrorNameText(ErrorCode error)
{
    const uint8_t index = static_cast<uint8_t>(error);
    if(index >= ENUM_LEN)
    {
        return errorNameUnknown;
    }

    return static_cast<PGM_P>(pgm_read_ptr(&errorNameTable[index]));
}

static void printErrorName(ErrorCode error)
{
    Serial.print(reinterpret_cast<const __FlashStringHelper*>(getErrorNameText(error)));
}

static void copyFileBasename(char* dest, const char* file)
{
    const char* basename = file;
    const char* slash = strrchr(file, '/');
    if(slash != nullptr)
    {
        basename = slash + 1;
    }

    strncpy(dest, basename, EEPROM_ERROR_FILE_SIZE - 1);
    dest[EEPROM_ERROR_FILE_SIZE - 1] = '\0';
}

static void initializeErrorStorageIfNecessary()
{
    EEPROMErrorHeader header;
    EEPROM.get(EEPROM_ERROR_HEADER_ADDRESS, header);

    const bool invalidHeader =
        header.magic != EEPROM_ERROR_HEADER_MAGIC ||
        header.layoutVersion != EEPROM_ERROR_LAYOUT_VERSION
        #if EEPROM_CLEAR_ON_BUILD_ID_CHANGE
        || header.buildId != EEPROM_BUILD_ID
        #endif
        ;

    if(!invalidHeader)
    {
        return;
    }

    EEPROMError empty = getEmptyErrorRecord();
    for(uint8_t i = 0; i < EEPROM_ERROR_MEMORY_ITEMS; i++)
    {
        EEPROM.put(errorAddressForIndex(i), empty);
    }

    EEPROMErrorHeader newHeader = {EEPROM_ERROR_HEADER_MAGIC, EEPROM_ERROR_LAYOUT_VERSION, EEPROM_BUILD_ID};
    EEPROM.put(EEPROM_ERROR_HEADER_ADDRESS, newHeader);
    EEPROM.put(EEPROM_ERROR_COUNTER_ADDRESS, static_cast<uint16_t>(0));
}

static uint16_t readRaiseErrorCounterFromEeprom()
{
    uint16_t counter = 0;
    EEPROM.get(EEPROM_ERROR_COUNTER_ADDRESS, counter);
    return counter;
}

static uint16_t incrementRaiseErrorCounterInEeprom()
{
    uint16_t counter = readRaiseErrorCounterFromEeprom();
    if(counter < UINT16_MAX)
    {
        counter++;
    }
    EEPROM.put(EEPROM_ERROR_COUNTER_ADDRESS, counter);
    return counter;
}


static void printReconstructedMessage(ErrorMessageTemplate messageTemplate, float value)
{
    const ErrorMessageTemplate normalizedTemplate = normalizeMessageTemplate(messageTemplate);
    PGM_P format = getMessageTemplateFormat(normalizedTemplate);
    const ErrorMessageRenderMode renderMode = getMessageRenderMode(normalizedTemplate);

    switch(renderMode)
    {
        case ERROR_MESSAGE_RENDER_INT_VALUE:
        {
            char buffer[ERROR_MESSAGE_SIZE];
            snprintf_P(buffer, sizeof(buffer), format, static_cast<int>(value));
            Serial.print(buffer);
            break;
        }
        case ERROR_MESSAGE_RENDER_PRESSURE_RANGE:
        {
            char buffer[ERROR_MESSAGE_SIZE];
            snprintf_P(
                buffer,
                sizeof(buffer),
                format,
                static_cast<int>(value),
                static_cast<int>(MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA),
                static_cast<int>(MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA)
            );
            Serial.print(buffer);
            break;
        }
        case ERROR_MESSAGE_RENDER_FLASH_STRING:
        default:
            Serial.print(reinterpret_cast<const __FlashStringHelper*>(format));
            break;
    }
}

static void printReconstructedMessage(const EEPROMError& eepromError)
{
    printReconstructedMessage(static_cast<ErrorMessageTemplate>(eepromError.messageTemplate), eepromError.value);
}

void invalidateErrorData()
{
    initializeErrorStorageIfNecessary();
    EEPROMError eepromError = getEmptyErrorRecord();
    for(uint8_t i = 0; i < EEPROM_ERROR_MEMORY_ITEMS; i++)
    {
        EEPROM.put(errorAddressForIndex(i), eepromError);
    }
    EEPROM.put(EEPROM_ERROR_COUNTER_ADDRESS, static_cast<uint16_t>(0));
}

uint16_t getRaiseErrorCounter()
{
    initializeErrorStorageIfNecessary();
    return readRaiseErrorCounterFromEeprom();
}

void resetRaiseErrorCounter()
{
    initializeErrorStorageIfNecessary();
    EEPROM.put(EEPROM_ERROR_COUNTER_ADDRESS, static_cast<uint16_t>(0));
}

void resetRaiseErrorCounterIfTimeoutElapsed()
{
    static bool alreadyReset = false;
    if(alreadyReset || millis() <= FALLBACK_ERROR_COUNTER_RESET_TIMEOUT_MS)
    {
        return;
    }

    Serial.print(F("Resetting raiseError counter after timeout. Previous value: "));
    Serial.println(getRaiseErrorCounter());
    resetRaiseErrorCounter();
    alreadyReset = true;
}

void printErrorData()
{
    initializeErrorStorageIfNecessary();

    EEPROMError eepromError;
    bool anyError = false;
    for(uint8_t i = 0; i < EEPROM_ERROR_MEMORY_ITEMS; i++)
    {
        EEPROM.get(errorAddressForIndex(i), eepromError);
        if(eepromError.activeError)
        {
            anyError = true;
            Serial.print(F("["));
            Serial.print(i);
            Serial.print(F("] "));
            printErrorName(static_cast<ErrorCode>(eepromError.errorCode));
            Serial.print(F(" @ "));
            Serial.print(eepromError.file);
            Serial.print(F(":"));
            Serial.print(eepromError.line);
            Serial.print(F(" - "));
            printReconstructedMessage(eepromError);
            Serial.println();
        }
    }

    if(!anyError)
    {
        Serial.println(F("No errors found"));
    }
}

void toggleFallbackMode(bool enableFallBackMode)
{
    if(enableFallBackMode)
    {
        if(currentMode == ErrorFallBackMode)
        {
            debugln(F("Warning: FallBack Mode already enabled"));
            return;
        }
        debug(F("Enabling Error FallBack Mode from mode "));debugln(modeToString(currentMode));
        EEPROM.put(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS, true);
        EEPROM.put(EEPROM_MODE_ADDRESS, currentMode);
        changeMode(ErrorFallBackMode);
        stepFSM();
    }
    else
    {
        debugln(F("Disabling Error FallBack Mode"));
        Mode mode;
        EEPROM.get(EEPROM_MODE_ADDRESS, mode);
        if(mode >= NUM_OF_MODES)
        {
            Serial.println(F("ERROR: Invalid Mode stored in EEPROM, setting to OnPressureTrigger"));
            mode = OnPressureTrigger;
        }
        changeMode(mode);
        EEPROM.put(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS, false);
        resetRaiseErrorCounter();
    }
}

[[noreturn]] void raiseErrorImpl(ErrorCode error, ErrorMessageTemplate messageTemplate, float value, const char* file, uint16_t line)
{
    initializeErrorStorageIfNecessary();

    #if !DISABLE_WATCHDOGS
        wdt_reset();
    #endif
    Serial.println(F("\n-----------------------------------------"));
    Serial.println(F("-----------------------------------------"));
    Serial.print(F("ERROR RAISED: "));
    printErrorName(error);
    Serial.print(F(": "));
    printReconstructedMessage(messageTemplate, value);
    Serial.println();
    Serial.println(F("-----------------------------------------"));
    Serial.println(F("-----------------------------------------\n"));

    writeColor(ERROR_FALLBACK_COLOR);

    if(error != NO_ERROR)
    {
        const uint16_t raiseErrorCounter = incrementRaiseErrorCounterInEeprom();

        #if !EEPROM_DONT_WRITE_ERRORS
        bool errorSaved = false;
        for(uint8_t i = 0; !errorSaved && i < EEPROM_ERROR_MEMORY_ITEMS; i++)
        {
            EEPROMError eepromError;
            EEPROM.get(errorAddressForIndex(i), eepromError);

            if(!eepromError.activeError)
            {
                eepromError.activeError = 1;
                eepromError.errorCode = static_cast<uint8_t>(error);
                eepromError.messageTemplate = static_cast<uint8_t>(messageTemplate);
                eepromError.line = line;
                eepromError.value = value;
                copyFileBasename(eepromError.file, file);
                EEPROM.put(errorAddressForIndex(i), eepromError);
                errorSaved = true;
            }
        }
        if(!errorSaved)
        {
            Serial.println(F("ERROR: Error memory is full"));
        }
        #endif

        Serial.print(F("raiseError counter: "));
        Serial.print(raiseErrorCounter);
        Serial.print(F(" (fallback threshold: >= "));
        Serial.print(ERROR_COUNT_TO_ENABLE_FALLBACK_MODE);
        Serial.println(F(")"));

        if(raiseErrorCounter >= ERROR_COUNT_TO_ENABLE_FALLBACK_MODE)
        {
            toggleFallbackMode(true);
        }
    }

    Serial.println(F("Rebooting..."));
    rebootLoop();
}

void handleHeaterError(int retryCount, char* buff)
{
    if(retryCount>=COMMS_MAX_RETRIES)
    {
        if(buff != nullptr && comms.getNextArgument(buff, SC_MAX_MESSAGE_SIZE) > 0)
        {
            Serial.print(F("HEATER MCU detail before reboot: "));
            Serial.println(buff);
        }
        raiseErrorWithTemplate(ERROR_HEATER_MCU_ERROR, ERROR_MSG_TEMPLATE_REMOTE_HEATER_DETAIL);
    }
}
