//
//  ConfigurationTest.cpp
//  Woodpeckers Tests
//
//  Created by Stephen H. Gerstacker on 2020-11-27.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include <gtest/gtest.h>

#include <sstream>

#include <Configuration.h>
#include <Log.h>

class ConfigurationTest : public ::testing::Test {

    protected:

    static void LogMessage(LogLevel level, const char *tag, const char *message) {
        std::cerr << "[          ] [" << tag << "/" << message << std::endl;
    }

    void SetUp() override {
        configuration = nullptr;

        LogEnableCallbackOutput(true, LogMessage);
        LogEnableConsoleOutput(false);
        LogEnableSystemOutput(false);
    }

    void TearDown() override {
        SAFE_DESTROY(configuration, ConfigurationDestroy);
    }

    ConfigurationRef configuration;

};

TEST_F(ConfigurationTest, HasDefaultSettings) {
    configuration = ConfigurationCreate();

    uint32_t value = ConfigurationGetMinWait(configuration);
    ASSERT_EQ(value, 1000);

    value = ConfigurationGetMaxWait(configuration);
    ASSERT_EQ(value, 4000);

    value = ConfigurationGetMinPecks(configuration);
    ASSERT_EQ(value, 1);

    value = ConfigurationGetMaxPecks(configuration);
    ASSERT_EQ(value, 3);

    value = ConfigurationGetPeckWait(configuration);
    ASSERT_EQ(value, 500);
}

TEST_F(ConfigurationTest, HasDefaultOutputs) {
    configuration = ConfigurationCreate();

    size_t value = ConfigurationGetTotalOutputs(configuration);
    ASSERT_EQ(value, 0);
}

TEST_F(ConfigurationTest, ParsesNothing) {
    const char *stringValue = "%YAML 1.2\n---\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_NE(configuration, nullptr);
}

TEST_F(ConfigurationTest, ParsesSettings) {
    const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Settings:\n"
        "  MinWait: 2000\n"
        "  MaxWait: 5000\n"
        "  MinPecks: 2\n"
        "  MaxPecks: 4\n"
        "  PeckWait: 1000\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_NE(configuration, nullptr);

    uint32_t value = ConfigurationGetMinWait(configuration);
    ASSERT_EQ(value, 2000);

    value = ConfigurationGetMaxWait(configuration);
    ASSERT_EQ(value, 5000);

    value = ConfigurationGetMinPecks(configuration);
    ASSERT_EQ(value, 2);

    value = ConfigurationGetMaxPecks(configuration);
    ASSERT_EQ(value, 4);

    value = ConfigurationGetPeckWait(configuration);
    ASSERT_EQ(value, 1000);
}

TEST_F(ConfigurationTest, ParsesOutputs) {
    const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Outputs:\n"
        "  - Memory Output:\n"
        "    Type: Memory\n"
        "  - File Output:\n"
        "    Type: File\n"
        "    Path: /path/to/output\n"
        "  - GPIO Output:\n"
        "    Type: GPIO\n"
        "    Pin: 42\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_NE(configuration, nullptr);

    size_t count = ConfigurationGetTotalOutputs(configuration);
    ASSERT_EQ(count, 3);

    ConfigurationOutputType type = ConfigurationGetOutputType(configuration, 0);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    const char *name = ConfigurationGetOutputName(configuration, 0);
    ASSERT_STREQ(name, "Memory Output");

    type = ConfigurationGetOutputType(configuration, 1);
    ASSERT_EQ(type, ConfigurationOutputTypeFile);

    name = ConfigurationGetOutputName(configuration, 1);
    ASSERT_STREQ(name, "File Output");

    const char *path = ConfigurationGetOutputPath(configuration, 1);
    ASSERT_STREQ(path, "/path/to/output");

    type = ConfigurationGetOutputType(configuration, 2);
    ASSERT_EQ(type, ConfigurationOutputTypeGPIO);

    name = ConfigurationGetOutputName(configuration, 2);
    ASSERT_STREQ(name, "GPIO Output");

    int pin = ConfigurationGetOutputPin(configuration, 2);
    ASSERT_EQ(pin, 42);
}

TEST_F(ConfigurationTest, FailsToParseOutputEmptyType) {
    const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Outputs:\n"
        "  - Memory Output:\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_EQ(configuration, nullptr);
}

TEST_F(ConfigurationTest, FailsToParseOutputUnknownType) {
    const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Outputs:\n"
        "  - Memory Output:\n"
        "    Type: Blap\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_EQ(configuration, nullptr);
}

TEST_F(ConfigurationTest, FailsToParseOutputUnknownKey) {
    const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Outputs:\n"
        "  - Memory Output:\n"
        "    Type: Memory\n"
        "    Foo: Bar\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_EQ(configuration, nullptr);
}

TEST_F(ConfigurationTest, FailsToParseFileOutputWithoutPath) {
   const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Outputs:\n"
        "  - File Output:\n"
        "    Type: File\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_EQ(configuration, nullptr); 
}

TEST_F(ConfigurationTest, FailsToParseGPIOOutputWithoutPin) {
   const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Outputs:\n"
        "  - GPIO Output:\n"
        "    Type: GPIO\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_EQ(configuration, nullptr); 
}

TEST_F(ConfigurationTest, ParsesBirds) {
    const char *stringValue = 
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Birds:\n"
        "  - Left:\n"
        "    Static:\n"
        "      - One\n"
        "    Back:\n"
        "      - Two\n"
        "      - Three\n"
        "    Forward:\n"
        "      - Four\n"
        "      - Five\n"
        "  - Right:\n"
        "    Static:\n"
        "      - Six\n"
        "      - Seven\n"
        "    Back:\n"
        "      - Eight\n"
        "    Forward:\n"
        "      - Nine\n"
        "      - Ten\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_NE(configuration, nullptr);

    size_t total = ConfigurationGetTotalBirds(configuration);
    ASSERT_EQ(total, 2);

    const char *name = ConfigurationGetBirdName(configuration, 0);
    ASSERT_STREQ(name, "Left");

    total = ConfigurationGetBirdTotalStatics(configuration, 0);
    ASSERT_EQ(total, 1);

    name = ConfigurationGetBirdStatic(configuration, 0, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "One");

    total = ConfigurationGetBirdTotalBacks(configuration, 0);
    ASSERT_EQ(total, 2);

    name = ConfigurationGetBirdBack(configuration, 0, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Two");

    name = ConfigurationGetBirdBack(configuration, 0, 1);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Three");

    total = ConfigurationGetBirdTotalForwards(configuration, 0);
    ASSERT_EQ(total, 2);

    name = ConfigurationGetBirdForward(configuration, 0, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Four");

    name = ConfigurationGetBirdForward(configuration, 0, 1);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Five");

    name = ConfigurationGetBirdName(configuration, 1);
    ASSERT_STREQ(name, "Right");

    total = ConfigurationGetBirdTotalStatics(configuration, 1);
    ASSERT_EQ(total, 2);

    name = ConfigurationGetBirdStatic(configuration, 1, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Six");

    name = ConfigurationGetBirdStatic(configuration, 1, 1);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Seven");

    total = ConfigurationGetBirdTotalBacks(configuration, 1);
    ASSERT_EQ(total, 1);

    name = ConfigurationGetBirdBack(configuration, 1, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Eight");

    total = ConfigurationGetBirdTotalForwards(configuration, 1);
    ASSERT_EQ(total, 2);

    name = ConfigurationGetBirdForward(configuration, 1, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Nine");

    name = ConfigurationGetBirdForward(configuration, 1, 1);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Ten");
}

TEST_F(ConfigurationTest, ParseEverything) {
    const char *stringValue =
        "%YAML 1.2\n"
        "---\n"
        "\n"
        "Settings:\n"
        "  MinWait: 1000\n"
        "  MaxWait: 4000\n"
        "  MinPecks: 1\n"
        "  MaxPecks: 3\n"
        "  PeckWait: 500\n"
        "\n"
        "Outputs:\n"
        "  - Left Static 1:\n"
        "    Type: Memory\n"
        "  - Left Static 2:\n"
        "    Type: Memory\n"
        "  - Left Back 1:\n"
        "    Type: Memory\n"
        "  - Left Forward 1:\n"
        "    Type: Memory\n"
        "  - Right Static 1:\n"
        "    Type: Memory\n"
        "  - Right Static 2:\n"
        "    Type: Memory\n"
        "  - Right Back 1:\n"
        "    Type: Memory\n"
        "  - Right Forward 1:\n"
        "    Type: Memory\n"
        "\n"
        "Birds:\n"
        "  - Left:\n"
        "    Static:\n"
        "      - Left Static 1\n"
        "      - Left Static 2\n"
        "    Back:\n"
        "      - Left Back 1\n"
        "    Forward:\n"
        "      - Left Forward 1\n"
        "  - Right:\n"
        "    Static:\n"
        "      - Right Static 1\n"
        "      - Right Static 2\n"
        "    Back:\n"
        "      - Right Back 1\n"
        "    Forward:\n"
        "      - Right Forward 1\n";

    configuration = ConfigurationCreateFromString(stringValue);
    ASSERT_NE(configuration, nullptr);

    // Settings

    uint32_t value = ConfigurationGetMinWait(configuration);
    ASSERT_EQ(value, 1000);

    value = ConfigurationGetMaxWait(configuration);
    ASSERT_EQ(value, 4000);

    value = ConfigurationGetMinPecks(configuration);
    ASSERT_EQ(value, 1);

    value = ConfigurationGetMaxPecks(configuration);
    ASSERT_EQ(value, 3);

    value = ConfigurationGetPeckWait(configuration);
    ASSERT_EQ(value, 500);

    // Outputs

    size_t count = ConfigurationGetTotalOutputs(configuration);
    ASSERT_EQ(count, 8);

    ConfigurationOutputType type = ConfigurationGetOutputType(configuration, 0);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    const char *name = ConfigurationGetOutputName(configuration, 0);
    ASSERT_STREQ(name, "Left Static 1");

    type = ConfigurationGetOutputType(configuration, 1);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    name = ConfigurationGetOutputName(configuration, 1);
    ASSERT_STREQ(name, "Left Static 2");

    type = ConfigurationGetOutputType(configuration, 2);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    name = ConfigurationGetOutputName(configuration, 2);
    ASSERT_STREQ(name, "Left Back 1");

    type = ConfigurationGetOutputType(configuration, 3);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    name = ConfigurationGetOutputName(configuration, 3);
    ASSERT_STREQ(name, "Left Forward 1");

    type = ConfigurationGetOutputType(configuration, 4);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    name = ConfigurationGetOutputName(configuration, 4);
    ASSERT_STREQ(name, "Right Static 1");

    type = ConfigurationGetOutputType(configuration, 5);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    name = ConfigurationGetOutputName(configuration, 5);
    ASSERT_STREQ(name, "Right Static 2");

    type = ConfigurationGetOutputType(configuration, 6);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    name = ConfigurationGetOutputName(configuration, 6);
    ASSERT_STREQ(name, "Right Back 1");

    type = ConfigurationGetOutputType(configuration, 7);
    ASSERT_EQ(type, ConfigurationOutputTypeMemory);

    name = ConfigurationGetOutputName(configuration, 7);
    ASSERT_STREQ(name, "Right Forward 1");

    // Birds

    size_t total = ConfigurationGetTotalBirds(configuration);
    ASSERT_EQ(total, 2);

    name = ConfigurationGetBirdName(configuration, 0);
    ASSERT_STREQ(name, "Left");

    total = ConfigurationGetBirdTotalStatics(configuration, 0);
    ASSERT_EQ(total, 2);

    name = ConfigurationGetBirdStatic(configuration, 0, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Left Static 1");

    name = ConfigurationGetBirdStatic(configuration, 0, 1);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Left Static 2");

    total = ConfigurationGetBirdTotalBacks(configuration, 0);
    ASSERT_EQ(total, 1);

    name = ConfigurationGetBirdBack(configuration, 0, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Left Back 1");

    total = ConfigurationGetBirdTotalForwards(configuration, 0);
    ASSERT_EQ(total, 1);

    name = ConfigurationGetBirdForward(configuration, 0, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Left Forward 1");

    name = ConfigurationGetBirdName(configuration, 1);
    ASSERT_STREQ(name, "Right");

    total = ConfigurationGetBirdTotalStatics(configuration, 1);
    ASSERT_EQ(total, 2);

    name = ConfigurationGetBirdStatic(configuration, 1, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Right Static 1");

    name = ConfigurationGetBirdStatic(configuration, 1, 1);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Right Static 2");

    total = ConfigurationGetBirdTotalBacks(configuration, 1);
    ASSERT_EQ(total, 1);

    name = ConfigurationGetBirdBack(configuration, 1, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Right Back 1");

    total = ConfigurationGetBirdTotalForwards(configuration, 1);
    ASSERT_EQ(total, 1);

    name = ConfigurationGetBirdForward(configuration, 1, 0);
    ASSERT_NE(name, nullptr);
    ASSERT_STREQ(name, "Right Forward 1");
}
