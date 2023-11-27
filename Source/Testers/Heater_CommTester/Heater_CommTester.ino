/**
 * This example shows how to send & receive a message between 2 modules
 */

#include <MAX_RS485.h>

const int ledPin =  13;  // Built-in LED
const int receiverEnablePin =  5;  // HIGH = Driver / LOW = Receptor
const int driveEnablePin =  4;  // HIGH = Driver / LOW = Receptor
const int rxPin = 6; // Serial data in pin
const int txPin = 3; // Serial data out pin

// Command structure: "SHWRS_{CMD$}[ARG$]*"
char HEADER[] = "SHWRS_"; // This string is prepended to the message and used to discard leftover bytes from previous messages
const char pumpCMD[] = "PUMP";
const char tempCMD[] = "TEMP";
const char OKCMD[] = "OK";

const int RECEIVED_MESSAGE_TIMEOUT = 100; //ms

MAX_RS485 rs485(rxPin, txPin, receiverEnablePin, driveEnablePin); // module constructor

void setup() 
{
  Serial.begin(9600); 
  Serial.setTimeout(100);
  delay(1000);

  rs485.begin(9600,RECEIVED_MESSAGE_TIMEOUT); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  delay(1000);
  Serial.println(F("\nPress p0 or p1 to send pump command\nor press t to send temperature command\nor press c to clear the input buffer"));
}

void serialEvent()
{
  char buffer[17];
  size_t len = Serial.readBytesUntil('\n',buffer,16);
  buffer[len] = '\0';

  Serial.print(F("The user input: ")); Serial.println(buffer);

  if(strcmp(buffer,"t") == 0)
  {
    Serial.println(F("Sending command: SHWRS_TEMP$"));
    rs485.print(F("SHWRS_TEMP$"));
  } 
  else if(strcmp(buffer,"p0") == 0)
  {
    Serial.println(F("Sending command: SHWRS_PUMP$0$"));
    rs485.print(F("SHWRS_PUMP$0$"));
  }
  else if(strcmp(buffer,"p1") == 0)
  {
    Serial.println(F("Sending command: SHWRS_PUMP$1$"));
    rs485.print(F("SHWRS_PUMP$1$"));
  }
  else if (strcmp(buffer,"c") == 0)
  {
    Serial.print(F("Clearing reception buffer:\t\""));
    while(rs485.available())
    {
        Serial.print((char)rs485.read());
    }
    Serial.print(F("\"\n"));
  }

  delay(2*RECEIVED_MESSAGE_TIMEOUT);

  if(rs485.available())
  {
    Serial.print(F("Got an answer:\t\""));
    while(rs485.available())
    {
        Serial.print((char)rs485.read());
    }
    Serial.print(F("\"\n"));
  }
}
 
void loop() 
{ 
  
}
