//
//  Configuration.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-25.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "Macros.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


BEGIN_DECLS


// MARK: - Constants & Globals

/// The Configuration object
typedef struct _Configuration * ConfigurationRef;

/// The output type
typedef enum _ConfigurationOutputType {
    ConfigurationOutputTypeUnknown = 0, ///< The output is unknown
    ConfigurationOutputTypeMemory,      ///< The output is memory-based
    ConfigurationOutputTypeFile,        ///< The output is file-based
    ConfigurationOutputTypeGPIO,        ///< The output is GPIO-based
} ConfigurationOutputType;


// MARK: - Lifecycle Methods

/**
 * Create a Configuration with the default settings.
 * \return A new Configuration instance.
 */
ConfigurationRef NONNULL ConfigurationCreate(void);

/**
 * Create a Configuration with values from a given YAML file.
 * \param path The path to load the settings from.
 * \return A new Configuration instance, or `NULL` if an error occurred.
 */
ConfigurationRef NULLABLE ConfigurationCreateFromFile(const char * NONNULL path);

/**
 * Create a Configuration with values from a given YAML string.
 * \param value The settings to load.
 * \return A new Configuration instance, or `NULL` if an error occurred.
 */
ConfigurationRef NULLABLE ConfigurationCreateFromString(const char * NONNULL value);

/**
 * Destroy a Configuration instance.
 * \param configuration The instance to destroy.
 */
void ConfigurationDestroy(ConfigurationRef NONNULL configuration);


// MARK: - Settings

/**
 * Get the minimum wait time between peck sequences.
 * \param configuration The instance to inspect.
 * \return The minimum time in milliseconds to wait between peck sequences.
 */
uint32_t ConfigurationGetMinWait(const ConfigurationRef NONNULL configuration);

/**
 * Get the maximum wait time between peck sequences.
 * \param configuration The instance to inspect.
 * \return The maximum time in milliseconds to wait between peck sequences.
 */
uint32_t ConfigurationGetMaxWait(const ConfigurationRef NONNULL configuration);

/**
 * Get the minimum number of pecks to perform in a sequence.
 * \param configuration The instance to inspect.
 * \return The minimum number of pecks to perform in a sequence.
 */
uint32_t ConfigurationGetMinPecks(const ConfigurationRef NONNULL configuration);

/**
 * Get the maximum number of pecks to perform in a sequence.
 * \param configuration The instance to inspect.
 * \return The maximum number of pecks to perform in a sequence.
 */
uint32_t ConfigurationGetMaxPecks(const ConfigurationRef NONNULL configuration);

/**
 * Get the time between peck movements.
 * \param configuration The instance to inspect.
 * \return The time in milliseconds between peck movements in a sequence.
 */
uint32_t ConfigurationGetPeckWait(const ConfigurationRef NONNULL configuration);


// MARK: - Outputs

/**
 * Get the name of an output at the given index.
 * \param configuration The instance to inspect.
 * \param idx The index of the output.
 * \return The name of the output, or `NULL` if the output is invalid.
 */
const char * NULLABLE ConfigurationGetOutputName(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the path of an output at the given index.
 * \param configuration The instance to inspect.
 * \param idx The index of the output.
 * \return The path of the output, or `NULL` if the output is invalid.
 */
const char * NULLABLE ConfigurationGetOutputPath(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the pin of an output at the given index.
 * \param configuration The instance to inspect.
 * \param idx The index of the output.
 * \return The pin of the output, or `NULL` if the output is invalid.
 */
int ConfigurationGetOutputPin(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the type of an output at the given index.
 * \param configuration The instance to inspect.
 * \param idx The index of the output.
 * \return The type of the output.
 */
ConfigurationOutputType ConfigurationGetOutputType(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the total number of outputs in the configuration.
 * \param configuration The instance to inspect.
 * \return The total number of outputs.
 */
size_t ConfigurationGetTotalOutputs(const ConfigurationRef NONNULL configuration);


// MARK: - Birds

/**
 * Get the name of the output to use for a bird back position at the given index.
 * \param configuration The instance to inspect.
 * \param birdIdx The index of the bird in the configuration.
 * \param idx The index of the name.
 * \return The name of the output, or `NULL` if the output is invalid.
 */
const char * NULLABLE ConfigurationGetBirdBack(const ConfigurationRef NONNULL configuration, size_t birdIdx, size_t idx);

/**
 * Get the total number of output names for a bird back position.
 * \param configuration The instance to inspect.
 * \param idx The index of the bird in the configuration.
 * \return The number of output names, or 0 if the output is invalid.
 */
size_t ConfigurationGetBirdTotalBacks(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the name of the output to use for a bird forward position at the given index.
 * \param configuration The instance to inspect.
 * \param birdIdx The index of the bird in the configuration.
 * \param idx The index of the name.
 * \return The name of the output, or `NULL` if the output is invalid.
 */
const char * NULLABLE ConfigurationGetBirdForward(const ConfigurationRef NONNULL configuration, size_t birdIdx, size_t idx);

/**
 * Get the total number of output names for a bird forward position.
 * \param configuration The instance to inspect.
 * \param idx The index of the bird in the configuration.
 * \return The number of output names, or 0 if the output is invalid.
 */
size_t ConfigurationGetBirdTotalForwards(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the name of the bird at the given index.
 * \param configuration The instance to inspect.
 * \param idx The index of the bird.
 * \return The name of the bird, or `NULL` if the bird is invalid.
 */
const char * NULLABLE ConfigurationGetBirdName(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the name of the output to use for a bird static position at the given index.
 * \param configuration The instance to inspect.
 * \param birdIdx The index of the bird in the configuration.
 * \param idx The index of the name.
 * \return The name of the output, or `NULL` if the output is invalid.
 */
const char * NULLABLE ConfigurationGetBirdStatic(const ConfigurationRef NONNULL configuration, size_t birdIdx, size_t idx);

/**
 * Get the total number of output names for a bird static position.
 * \param configuration The instance to inspect.
 * \param idx The index of the bird in the configuration.
 * \return The number of output names, or 0 if the output is invalid.
 */
size_t ConfigurationGetBirdTotalStatics(const ConfigurationRef NONNULL configuration, size_t idx);

/**
 * Get the total number of birds in the configuration.
 * \param configuration The instance to inspect.
 * \return The number of birds.
 */
size_t ConfigurationGetTotalBirds(const ConfigurationRef NONNULL configuration);


// MARK: - Debug

/**
 * Dump the parsing events to log for debugging purposes.
 * \param dump `true` to dump logs, otherwise `false`.
 */
void ConfigurationSetDumpParseEvents(bool dump);

END_DECLS

#endif /* CONFIGURATION_H */
