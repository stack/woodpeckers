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

/// The level severity of a log message.
typedef enum _LogLevel {
    LogLevelVerbose, ///< A highly specific debug log message
    LogLevelDebug,   ///< A debug log message
    LogLevelInfo,    ///< An information log message
    LogLevelWarning, ///< A warning log message
    LogLevelError,   ///< An error log message
} LogLevel;


// MARK: - Callbacks

/**
 * When callback logging is enabled, this callback is called with logging information.
 * \param level The serverity of the message.
 * \param tag A section identifier of the message.
 * \param message The full log message.
 */
typedef void (* LogCallback)(LogLevel level, const char * NONNULL tag, const char * NONNULL message);


// MARK: - Initialization

/**
 * Enable or disable callback logging.
 * \param enabled `true` to enable callback logging, otherwise `false`.
 * \param callback The callback function to be called for a log line.
 */
void LogEnableCallbackOutput(bool enabled, LogCallback NULLABLE callback);

/**
 * Enable or disable console output.
 * \param enabled `true` to enable logging to the console, otherwise `false`.
 */
void LogEnableConsoleOutput(bool enabled);

/**
 * Enable or disable system output.
 * \param enabled `true` to enable logging to system log service, otherwise `false`.
 */
void LogEnableSystemOutput(bool enabled);

/**
 * Initialize the logging subsystem.
 * \param level The log level to filter to.
 */
void LogSetUp(LogLevel level);


// MARK: - Logging

/**
 * Log a message.
 * \param level The severity of the message.
 * \param tag A section identifier of the message.
 * \param format A format string for producing the message.
 * \param ... Arguments to be formatted in the message.
 */
void Log(LogLevel level, const char * NONNULL tag, const char * NONNULL format, ...);

/**
 * Log a message with a compiled argument list.
 * \param level The severity of the message.
 * \param tag A section identifier of the message.
 * \param format A format string for producing the message.
 * \param args The argument list to be formatted.
 */
void LogVA(LogLevel level, const char * NONNULL tag, const char * NONNULL format, va_list args);

/// A helper macro to write a debug level log.
#define LogD(T, ...) Log(LogLevelDebug, (T), __VA_ARGS__)

/// A helper macro to write an error level log.
#define LogE(T, ...) Log(LogLevelError, (T), __VA_ARGS__)

/// A helper macro to write an info level log.
#define LogI(T, ...) Log(LogLevelInfo, (T), __VA_ARGS__)

/// A helper macro to write a verbose level log.
#define LogV(T, ...) Log(LogLevelVerbose, (T), __VA_ARGS__)

/// A helper macro to write a warning level log.
#define LogW(T, ...) Log(LogLevelWarning, (T), __VA_ARGS__)


// MARK: - System-specific Logging

/**
 * Log a message as an error with a given `errno`.
 * \param tag A section identifier of the message.
 * \param errorNumber The `errno` to be logged.
 * \param format A format string for producing the message.
 * \param ... Arguments to be formatted in the message.
 */
void LogErrno(const char * NONNULL tag, int errorNumber, const char * NONNULL format, ...);

END_DECLS

#endif /* LOG_H */
