//
//  ConfigurationTest.cpp
//  Woodpeckers Tests
//
//  Created by Stephen H. Gerstacker on 2020-11-27.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include <gtest/gtest.h>

#include <Configuration.h>

class ConfigurationTest : public ::testing::Test {

    protected:

    void SetUp() override {
        configuration = nullptr;
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
