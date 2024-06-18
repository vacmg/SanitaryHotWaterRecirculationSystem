//
// Created by Victor on 17/06/2024.
//

#include "SimpleComms.h"

SimpleComms::SimpleComms(Stream* channel, const char* header)
{
    #if SC_USE_HAMMING_7_4_CORRECTION_CODE
        this->channel = new HammingStream<7, 4>(channel);
    #else
        this->channel = channel;
    #endif
    this->header = strdup(header);
}

int SimpleComms::getNextCommand(char* buffer, int bufferSize)
{
    if(buffer == nullptr)
        return -1;

    if(!available())
        return 0;

    if(!channel->find(header))
        return -1;

    int dataSize = (int)channel->readBytesUntil('$', buffer, bufferSize);
    if(dataSize >= bufferSize)
    {
        buffer[0] = '\0';
        return -1;
    }
    buffer[dataSize] = '\0';

    return dataSize;
}

int SimpleComms::getNextArgument(char* buffer, int bufferSize)
{
    if(buffer == nullptr)
        return -1;

    if(!available())
        return 0;

    int dataSize = (int)channel->readBytesUntil('$', buffer, bufferSize-1);
    if(dataSize >= bufferSize)
    {
        buffer[0] = '\0';
        return -1;
    }
    buffer[dataSize] = '\0';

    return dataSize;
}

int SimpleComms::sendCommand(const char* command, const char** arguments, int numArguments)
{
    if(command == nullptr || command[0] == '\0')
        return -1;

    char dollar[2] = "$";
    char buffer[SC_MAX_MESSAGE_SIZE+1];

    int size = snprintf(buffer, SC_MAX_MESSAGE_SIZE, "%s%s$", header, command);

    for (int i = 0; i < numArguments; i++)
    {
        size_t argLen = strlen(arguments[i]);
        if(size + argLen + 1 > SC_MAX_MESSAGE_SIZE)
            return -1;

        strncat(buffer, arguments[i], SC_MAX_MESSAGE_SIZE - size);
        size += (int)argLen;
        strncat(buffer, dollar, SC_MAX_MESSAGE_SIZE - size);
        size++;
    }
    buffer[min(size,SC_MAX_MESSAGE_SIZE)] = '\0';

    channel->print(buffer);

    return size;
}

int SimpleComms::available()
{
    return channel->available();
}

int SimpleComms::availableForWrite()
{
    return channel->availableForWrite();
}
