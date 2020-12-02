//
//  Log.c
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-29.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "config.h"

#include "Log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#if TARGET_PLATFORM_APPLE
#include <os/log.h>
#elif TARGET_PLATFORM_LINUX
// TODO: syslog header here
#endif


// MARK: - Constants & Globals

static bool ConsoleOutputEnabled = true;
static bool CallbackOutputEnabled = false;
static bool SystemOutputEnabled = false;

static LogCallback Callback  = NULL;

static LogLevel GlobalLogLevel = LogLevelInfo;


// MARK: - Initialization

void LogEnableCallbackOutput(bool enabled, LogCallback NULLABLE callback) {
    CallbackOutputEnabled = enabled;

    if (enabled) {
        Callback = callback;
    } else {
        Callback = NULL;
    }
}

void LogEnableConsoleOutput(bool enabled) {
    ConsoleOutputEnabled = enabled;
}

void LogEnableSystemOutput(bool enabled) {
    SystemOutputEnabled = enabled;
}

void LogSetUp(LogLevel level) {
    GlobalLogLevel = level;
}


// MARK: - Prototypes

static char LogLevelToChar(LogLevel level);


// MARK: - Logging

void Log(LogLevel level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);

    LogVA(level, tag, format, args);

    va_end(args);
}

void LogVA(LogLevel level, const char *tag, const char *format, va_list args) {
    char messageBuffer[1024];
    vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);

    char levelChar = LogLevelToChar(level);

    struct timeval now;
    gettimeofday(&now, NULL);

    time_t nowTime = now.tv_sec;
    struct tm *nowLocal = localtime(&nowTime);

    char timeBuffer[64];
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", nowLocal);

    if (CallbackOutputEnabled && Callback != NULL) {
        Callback(level, tag, messageBuffer);
    }

    if (ConsoleOutputEnabled) {
        printf("%s.%06ld %c %-14s %s\n", timeBuffer, (long)now.tv_usec, levelChar, tag, messageBuffer);
    }

    if (SystemOutputEnabled) {
#if TARGET_PLATFORM_APPLE
        os_log_type_t logType;

        switch (level) {
            case LogLevelDebug:
            case LogLevelVerbose:
                logType = OS_LOG_TYPE_DEBUG;
                break;
            case LogLevelError:
            case LogLevelWarning:
                logType = OS_LOG_TYPE_ERROR;
                break;
            case LogLevelInfo:
                logType = OS_LOG_TYPE_INFO;
                break;
        }

        os_log_with_type(OS_LOG_DEFAULT, logType, "%c/%{public}-14s: %{public}s", levelChar, tag, messageBuffer);
#elif TARGET_PLATFORM_LINUX
        // TODO: syslog implementation here
#endif
    }
}

void LogErrno(const char * NONNULL tag, int errorNumber, const char * NONNULL format, ...) {
    char errorBuffer[1024];
    strerror_r(errorNumber, errorBuffer, sizeof(errorBuffer));

    va_list args;
    va_start(args, format);

    char messageBuffer[1024];
    vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);

    va_end(args);

    Log(LogLevelError, tag, "%s: (%i) %s", messageBuffer, errorNumber, errorBuffer);
}


// MARK: - Utilities

static char LogLevelToChar(LogLevel level) {
    switch (level) {
        case LogLevelDebug:
            return 'D';
            break;
        case LogLevelError:
            return 'E';
            break;
        case LogLevelInfo:
            return 'I';
            break;
        case LogLevelVerbose:
            return 'V';
            break;
        case LogLevelWarning:
            return 'W';
            break;
    }
}
