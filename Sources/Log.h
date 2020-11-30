//
//  Log.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-29.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#ifndef LOG_H
#define LOG_H

#include "Macros.h"

#include <stdarg.h>
#include <stdbool.h>

BEGIN_DECLS


// MARK: - Constants & Globals

typedef enum _LogLevel {
    LogLevelVerbose,
    LogLevelDebug,
    LogLevelInfo,
    LogLevelWarning,
    LogLevelError,
} LogLevel;


// MARK: - Callbacks

typedef void (* LogCallback)(LogLevel level, const char * NONNULL tag, const char * NONNULL message);


// MARK: - Initialization

void LogEnableCallbackOutput(bool enabled, LogCallback NULLABLE callback);
void LogEnableConsoleOutput(bool enabled);
void LogEnableSystemOutput(bool enabled);

void LogSetUp(LogLevel level);


// MARK: - Logging

void Log(LogLevel level, const char * NONNULL tag, const char * NONNULL format, ...);
void LogVA(LogLevel level, const char * NONNULL tag, const char * NONNULL format, va_list args);

#define LogD(T, ...) Log(LogLevelDebug, (T), __VA_ARGS__)
#define LogE(T, ...) Log(LogLevelError, (T), __VA_ARGS__)
#define LogI(T, ...) Log(LogLevelInfo, (T), __VA_ARGS__)
#define LogV(T, ...) Log(LogLevelVerbose, (T), __VA_ARGS__)
#define LogW(T, ...) Log(LogLevelWarning, (T), __VA_ARGS__)


// MARK: - System-specific Logging

void LogErrno(const char * NONNULL tag, int errorNumber, const char * NONNULL format, ...);

END_DECLS

#endif /* LOG_H */
