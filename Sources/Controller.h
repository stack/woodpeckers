//
//  Controller.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-12-01.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "Macros.h"

#include <inttypes.h>
#include <stdint.h>


BEGIN_DECLS


// MARK: - Constants & Globals

/// The Controller object
typedef struct _Controller * ControllerRef;


// MARK: - Lifecycle Methods

/**
 * Create a Controller instance.
 * \return The the new instance.
 */
ControllerRef NONNULL ControllerCreate(void);

/**
 * Destroy an instance of a Controller.
 * \param controller The instance to destroy.
 */
void ControllerDestroy(ControllerRef NONNULL controller);


// MARK: - Running

/**
 * Run the given configuration.
 * \param controller The instance to run.
 * \note This will block until the run is complete.
 */
void ControllerRun(ControllerRef NONNULL controller);


// MARK: - Properties

/**
 * Set the minimum wait time between peck sequences.
 * \param controller The instance to modify.
 * \param value The minimum time in milliseconds to wait between peck sequences.
 */
void ControllerSetMinWait(ControllerRef NONNULL controller, uint32_t value);

/**
 * Set the maximum wait time between peck sequences.
 * \param controller The instance to modify.
 * \param value The maximum time in milliseconds to wait between peck sequences.
 */
void ControllerSetMaxWait(ControllerRef NONNULL controller, uint32_t value);

/**
 * Set the minimum number of pecks to perform in a sequence.
 * \param controller The instance to modify.
 * \param value The minimum number of pecks to perform in a sequence.
 */
void ControllerSetMinPecks(ControllerRef NONNULL controller, uint32_t value);

/**
 * Set the maximum number of pecks to perform in a sequence.
 * \param controller The instance to modify.
 * \param value The maximum number of pecks to perform in a sequence.
 */
void ControllerSetMaxPecks(ControllerRef NONNULL controller, uint32_t value);

/**
 * Set the time between peck movements.
 * \param controller The instance to modify.
 * \param value The time in milliseconds between peck movements.
 */
void ControllerSetPeckWait(ControllerRef NONNULL controller, uint32_t value);

END_DECLS

#endif /* CONTROLLER_H */
