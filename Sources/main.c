//
//  main.c
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-27.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "config.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Configuration.h"
#include "Macros.h"
#include "Log.h"

// MARK: - Constants & Globals

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
    ConfigurationSetDumpParseEvents(true);
    ConfigurationRef configuration = ConfigurationCreateFromFile(configPath);

    if (configuration == NULL) {
        LogE(TAG, "Failed to load configuration from %s", configPath);
        return EXIT_FAILURE;
    }

    LogI(TAG, "Loaded configuration from %s", configPath);

    // Clean up
    SAFE_DESTROY(configuration, ConfigurationDestroy);

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
