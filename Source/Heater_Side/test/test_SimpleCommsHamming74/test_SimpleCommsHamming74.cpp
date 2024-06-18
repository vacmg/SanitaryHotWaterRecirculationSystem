#define SC_USE_HAMMING_7_4_CORRECTION_CODE 1

#include <SimpleComms.h>
#include <unity.h>


void testAvailable()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header");
    TEST_ASSERT_EQUAL(0, comms.available());
    stream.print("Hello");
    TEST_ASSERT_EQUAL(5, comms.available());
    stream.print(" ");
    TEST_ASSERT_EQUAL(6, comms.available());
    char c = stream.read();
    TEST_ASSERT_EQUAL('H', c);
    TEST_ASSERT_EQUAL(5, comms.available());
}

void testSendCommandAverage()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    int expected = 25;
    const char* args[] = {"arg1", "arg2"};
    TEST_ASSERT_EQUAL(expected, comms.sendCommand("command", args, 2));
    TEST_ASSERT_EQUAL(0, strcmp("Header_command$arg1$arg2$", stream.str().c_str()));
    TEST_ASSERT_EQUAL(expected, comms.available());
}

void testSendCommandBig()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_Header_Header_Header_Header_Header_Header_Header_Header_Header_Header_");
    int expected = -1;
    const char* args[] = {"arg1", "arg2"};
    TEST_ASSERT_EQUAL(expected, comms.sendCommand("command", args, 2));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
    TEST_ASSERT_EQUAL(0, comms.available());
}

void testSendCommandArgsBig()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    int expected = -1;
    const char* args[] = {"arg1", "arg2arg2arg2arg2arg2arg2arg2arg2arg2arg2arg2arg2arg2"};
    TEST_ASSERT_EQUAL(expected, comms.sendCommand("command", args, 2));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
    TEST_ASSERT_EQUAL(0, comms.available());
}

void testSendCommandNoCommand()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    const char* args[] = {"arg1", "arg2"};
    TEST_ASSERT_EQUAL(-1, comms.sendCommand("", args, 2));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
    TEST_ASSERT_EQUAL(0, comms.available());
}

void testSendCommandNullCommand()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    const char* args[] = {"arg1", "arg2"};
    TEST_ASSERT_EQUAL(-1, comms.sendCommand(NULL, args, 2));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
    TEST_ASSERT_EQUAL(0, comms.available());
}

void testSendCommandNoArgs()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    const char* args[] = {};
    TEST_ASSERT_EQUAL(15, comms.sendCommand("command", args, 0));
    TEST_ASSERT_EQUAL(0, strcmp("Header_command$", stream.str().c_str()));
    TEST_ASSERT_EQUAL(15, comms.available());
}

void testSendCommandNullArgs()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    TEST_ASSERT_EQUAL(15, comms.sendCommand("command", NULL, 0));
    TEST_ASSERT_EQUAL(0, strcmp("Header_command$", stream.str().c_str()));
    TEST_ASSERT_EQUAL(15, comms.available());
}

void testGetNextCommandNoBuffer()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    stream.print("Header_command$arg1$arg2$");
    TEST_ASSERT_EQUAL(-1, comms.getNextCommand(NULL, 0));
    TEST_ASSERT_EQUAL(0, strcmp("Header_command$arg1$arg2$", stream.str().c_str()));
}

void testGetNextCommandAverage()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    char buffer[64];
    stream.print("Header_command$arg1$arg2$");

    TEST_ASSERT_EQUAL(7, comms.getNextCommand(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("command", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("arg1$arg2$", stream.str().c_str()));
}

void testGetNextCommandNoArgs()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    char buffer[64];
    stream.print("Header_command$");

    TEST_ASSERT_EQUAL(7, comms.getNextCommand(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("command", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
}

void testGetNextCommandNoArgsMoreDataAfter()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    char buffer[64];
    stream.print("Header_command$Header_command2$");

    TEST_ASSERT_EQUAL(7, comms.getNextCommand(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("command", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("Header_command2$", stream.str().c_str()));

    TEST_ASSERT_EQUAL(8, comms.getNextCommand(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("command2", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
}

void testGetNextCommandBig()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    char buffer[64];
    stream.print("Header_command123456789012345678901234567890123456789012345678901234567890$arg1$arg2$");

    TEST_ASSERT_EQUAL(-1, comms.getNextCommand(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("890$arg1$arg2$", stream.str().c_str()));
}

void testGetNextCommandBigMoreDataAfter()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    char buffer[64];
    stream.print("Header_command123456789012345678901234567890123456789012345678901234567890$arg1$arg2$Header_command2$");

    TEST_ASSERT_EQUAL(-1, comms.getNextCommand(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("890$arg1$arg2$Header_command2$", stream.str().c_str()));

    TEST_ASSERT_EQUAL(8, comms.getNextCommand(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("command2", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
}

void testGetNextArgumentNoBuffer()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    stream.print("arg1$arg2$");
    TEST_ASSERT_EQUAL(-1, comms.getNextArgument(NULL, 0));
    TEST_ASSERT_EQUAL(0, strcmp("arg1$arg2$", stream.str().c_str()));
}

void testGetNextArgumentAverage()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    char buffer[64];
    stream.print("arg1$arg2$");

    TEST_ASSERT_EQUAL(4, comms.getNextArgument(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("arg1", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("arg2$", stream.str().c_str()));

    TEST_ASSERT_EQUAL(4, comms.getNextArgument(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("arg2", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("", stream.str().c_str()));
}

void testGetNextArgumentMoreDataAfter()
{
    StringStream stream;
    SimpleComms comms(&stream, "Header_");
    char buffer[64];
    stream.print("arg1$arg2$Header_command2$");

    TEST_ASSERT_EQUAL(4, comms.getNextArgument(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("arg1", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("arg2$Header_command2$", stream.str().c_str()));

    TEST_ASSERT_EQUAL(4, comms.getNextArgument(buffer, 64));
    TEST_ASSERT_EQUAL(0, strcmp("arg2", buffer));
    TEST_ASSERT_EQUAL(0, strcmp("Header_command2$", stream.str().c_str()));
}


void setUp(void)
{

}

void tearDown(void)
{

}


void setup()
{
    UNITY_BEGIN();
    RUN_TEST(testAvailable);

    RUN_TEST(testSendCommandAverage);
    RUN_TEST(testSendCommandBig);
    RUN_TEST(testSendCommandArgsBig);
    RUN_TEST(testSendCommandNoCommand);
    RUN_TEST(testSendCommandNullCommand);
    RUN_TEST(testSendCommandNoArgs);
    RUN_TEST(testSendCommandNullArgs);

    RUN_TEST(testGetNextCommandNoBuffer);
    RUN_TEST(testGetNextCommandAverage);
    RUN_TEST(testGetNextCommandNoArgs);
    RUN_TEST(testGetNextCommandNoArgsMoreDataAfter);
    RUN_TEST(testGetNextCommandBig);
    RUN_TEST(testGetNextCommandBigMoreDataAfter);

    RUN_TEST(testGetNextArgumentAverage);
    RUN_TEST(testGetNextArgumentMoreDataAfter);
    RUN_TEST(testGetNextArgumentNoBuffer);

    UNITY_END();
}

void loop()
{

}
