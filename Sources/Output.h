//
//  Output.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-12-01.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#ifndef OUTPUT_H
#define OUTPUT_H

#include "Macros.h"

#include <stdbool.h>


BEGIN_DECLS


// MARK: - Constants & Globals

/// The Output object
typedef struct _Output * OutputRef;


// MARK: - Lifecycle Methods

/**
 * Create an output that targets a file.
 * \param name The name of the output.
 * \param path The path to the file to target.
 * \return An output instance.
 */
OutputRef NONNULL OutputCreateFile(const char * NONNULL name, const char * NONNULL path);

/**
 * Create an output that targets a GPIO pin.
 * \param name The name of the output.
 * \param pin The GPIO pin.
 * \return An output instance.
 */
OutputRef NONNULL OutputCreateGPIO(const char * NONNULL name, int pin);

/**
 * Create an output that is stored in memory.
 * \param name The name of the output.
 * \return An output instance.
 */
OutputRef NONNULL OutputCreateMemory(const char * NONNULL name);

/**
 * Destroy an instance of an Output.
 * \param output The instance to destroy.
 */
void OutputDestroy(OutputRef NONNULL output);


// MARK: - Set Up & Tear Down

/**
 * Perform any set up an output may need.
 * \param output The instance to set up.
 * \return `true` if the set up succeeded, otherwise `false`.
 */
bool OutputSetUp(OutputRef NONNULL output);

/**
 * Perform any tear down an output may need.
 * param output The instance to tear down.
 */
void OutputTearDown(OutputRef NONNULL output);


// MARK: - Properties

/**
 * Get the name of the output.
 * \param output The instance to inspect.
 * \return The name of the output.
 */
const char * NONNULL OutputGetName(const OutputRef NONNULL output);

/**
 * Get the current value of the output.
 * \param output The instance to inspect.
 * \return `true` if the output is set, otherwise `false`.
 */
bool OutputGetValue(const OutputRef NONNULL output);

/**
 * Set the current value of the output.
 * \param output The instance to modify.
 * \param value `true` to set the output, otherwise `false`.
 */
void OutputSetValue(OutputRef NONNULL output, bool value);

END_DECLS

#endif /* OUTPUT_H */
