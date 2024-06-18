//
// Created by Victor on 17/06/2024.
//

#ifndef HEATER_SIDE_SIMPLECOMMS_H
#define HEATER_SIDE_SIMPLECOMMS_H

#include <StreamUtils.h>
#include <Stream.h>

#define SC_MAX_MESSAGE_SIZE 63 // Real message size is 64, but the last byte is reserved for the null terminator
#ifndef SC_USE_HAMMING_7_4_CORRECTION_CODE
    #define SC_USE_HAMMING_7_4_CORRECTION_CODE 0 // For best performance if Serial is used, set the mode to SERIAL7N1
#endif

class SimpleComms
{
public:
    SimpleComms(Stream *channel, const char* header);

    /**
     * @brief Reads the next command from the channel. If there is no command available, it will return 0. If the command is too big, it will return -1 and discard the message. If the message does not contain a header, it will return -1 and discard the message.
     * @param buffer The buffer to store the command
     * @param bufferSize The size of the buffer
     * @return The number of bytes read (without the null terminator)
     */
    int getNextCommand(char* buffer, int bufferSize);

    /**
     * @brief Reads the next argument from the channel. If there is no argument available, it will return 0. If the argument is too big, it will return -1 and discard the message.
     * @param buffer The buffer to store the argument
     * @param bufferSize The size of the buffer
     * @return The number of bytes read (without the null terminator)
     */
    int getNextArgument(char* buffer, int bufferSize);

    /**
     * @brief Sends a command with its arguments. If the message is too big, it will return -1 and don't send anything.
     *
     * @param command The command to send
     * @param arguments An array of arguments
     * @param numArguments The number of arguments
     * @return int Returns the number of bytes sent or -1 if the message is too big
     */
    int sendCommand(const char* command, const char** arguments, int numArguments);

    /**
     * @brief Returns the number of bytes available for writing
     *
     * @return int
     */
    int availableForWrite();

    /**
     * @brief Returns the number of bytes available for reading
     *
     * @return int
     */
    int available();
private:
    #if USE_HAMMING_7_4_CORRECTION_CODE
        HammingStream<7, 4> *channel;
    #else
        Stream *channel;
    #endif

    char* header;
};


#endif //HEATER_SIDE_SIMPLECOMMS_H
