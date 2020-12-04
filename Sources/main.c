//
//  main.c
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-27.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "config.h"

#include "Macros.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Configuration.h"
#include "Controller.h"
#include "Log.h"


// MARK: - Constants & Globals

#define MAX_OUTPUTS 16
#define TAG "Main"

static struct option Options[] = {
    { "version", no_argument,       NULL, 'v' },
    { "help",    no_argument,       NULL, 'h' },
    { "config",  required_argument, NULL, 'c' },
    { "debug",   no_argument,       NULL, 'd' },
    { NULL,      0,                 NULL, 0   }
};


// MARK: - Prototypes

static void PrintUsage(void);
static void PrintVersion(void);


// MARK: - Main

int main(int argc, char **argv) {
    // Parse options
    bool debugMode = false;
    char *configPath = NULL;

    while (true) {
        int result = getopt_long(argc, argv, "vhc:d", Options, NULL);

        if (result == -1) {
            break;
        }

        switch (result) {
            case 'v':
                PrintVersion();
                return EXIT_SUCCESS;
                break;
            case 'h':
                PrintUsage();
                return EXIT_SUCCESS;
                break;
            case 'c':
                SAFE_DESTROY(configPath, free);
                configPath = strdup(optarg);
                break;
            case 'd':
                debugMode = true;
                break;

        }
    }

    // Sanity check
    if (configPath == NULL) {
        fprintf(stderr, "A config file is required\n");
        return EXIT_FAILURE;
    }

    // Set up logging
    if (debugMode) {
        LogEnableConsoleOutput(true);
        LogEnableSystemOutput(false);
    } else {
        LogEnableConsoleOutput(false);
        LogEnableSystemOutput(true);
    }

    LogSetUp(LogLevelVerbose);

    LogI(TAG, "Woodpeckers %s", PROJECT_VERSION);

    // Load the configuration file
    ConfigurationRef configuration = ConfigurationCreateFromFile(configPath);

    if (configuration == NULL) {
        LogE(TAG, "Failed to load configuration from %s", configPath);
        return EXIT_FAILURE;
    }

    LogI(TAG, "Loaded configuration from %s", configPath);

    // Build the controller
    ControllerRef controller = ControllerCreate();

    ControllerSetMinWait(controller, ConfigurationGetMinWait(configuration));
    ControllerSetMaxWait(controller, ConfigurationGetMaxWait(configuration));
    ControllerSetMinPecks(controller, ConfigurationGetMinPecks(configuration));
    ControllerSetMaxPecks(controller, ConfigurationGetMaxPecks(configuration));
    ControllerSetPeckWait(controller, ConfigurationGetPeckWait(configuration));

    size_t totalOutputs = ConfigurationGetTotalOutputs(configuration);

    for (size_t idx = 0; idx < totalOutputs; idx++) {
        const char *name = ConfigurationGetOutputName(configuration, idx);
        ConfigurationOutputType type = ConfigurationGetOutputType(configuration, idx);

        const char *path = NULL;
        int pin = -1;

        bool success = false;

        switch (type) {
            case ConfigurationOutputTypeFile:
                path = ConfigurationGetOutputPath(configuration, idx);
                success = ControllerAddFileOutput(controller, name, path);
                break;
            case ConfigurationOutputTypeGPIO:
                pin = ConfigurationGetOutputPin(configuration, idx);
                success = ControllerAddGPIOOutput(controller, name, pin);
                break;
            case ConfigurationOutputTypeMemory:
                success = ControllerAddMemoryOutput(controller, name);
                break;
            default:
                LogE(TAG, "Unhandled configuration output type: %i", type);
                break;
        }

        if (!success) {
            LogE(TAG, "Failed to add output \"%s\". Aborting.", name);
            return EXIT_FAILURE;
        }
    }

    size_t totalBirds = ConfigurationGetTotalBirds(configuration);

    for (size_t birdIdx = 0; birdIdx < totalBirds; birdIdx++) {
        const char *name = ConfigurationGetBirdName(configuration, birdIdx);

        size_t totalStatics = ConfigurationGetBirdTotalStatics(configuration, birdIdx);
        size_t totalBacks = ConfigurationGetBirdTotalBacks(configuration, birdIdx);
        size_t totalForwards = ConfigurationGetBirdTotalForwards(configuration, birdIdx);

        const char *statics[MAX_OUTPUTS];
        const char *backs[MAX_OUTPUTS];
        const char *forwards[MAX_OUTPUTS];

        for (size_t idx = 0; idx < totalStatics; idx++) {
            statics[idx] = ConfigurationGetBirdStatic(configuration, birdIdx, idx);
        }

        for (size_t idx = 0; idx < totalBacks; idx++) {
            backs[idx] = ConfigurationGetBirdBack(configuration, birdIdx, idx);
        }

        for (size_t idx = 0; idx < totalForwards; idx++) {
            forwards[idx] = ConfigurationGetBirdForward(configuration, birdIdx, idx);
        }

        bool success = ControllerAddBird(controller, name, statics, totalStatics, backs, totalBacks, forwards, totalForwards);

        if (!success) {
            LogE(TAG, "Failed to add bird \"%s\". Aborting.", name);
            return EXIT_FAILURE;
        }
    }

    SAFE_DESTROY(configuration, ConfigurationDestroy);

    // Run forever
    ControllerRun(controller);

    // Clean up
    ControllerDestroy(controller);

    return EXIT_SUCCESS;
}


// MARK: - Utilities

static void PrintUsage() {
    printf("Usage: Woodpeckers [options]\n");
    printf("    -v, --version             Print the version number\n");
    printf("    -h, --help                Print this help message\n");
    printf("    -c, --config=CONFIG       Path to the required config file\n");
    printf("    -d, --debug               Run in debug mode\n");
}

static void PrintVersion() {
    printf("%s\n", PROJECT_VERSION);
}
