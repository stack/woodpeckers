//
//  EventLoop.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-24.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// MARK: - Constants & Globals

/// A unique identifier for an event of a specific type
typedef uint16_t EventID;

/// The Event Loop object
typedef struct _EventLoop * EventLoopRef;


// MARK: - Lifecycle Methods

/**
 * Create a new Event Loop instance.
 * \return A new instance, or `NULL` if an error occurred.
 */
EventLoopRef EventLoopCreate(void);

/**
 * Destroy an Event Loop.
 * \param eventLoop The instance to destroy.
 */
void EventLoopDestroy(EventLoopRef eventLoop);


// MARK: - Event Loop Control

/**
 * Run the Event Loop.
 * \param eventLoop The Event Loop to run.
 */
void EventLoopRun(EventLoopRef eventLoop);

/**
 * Run the Event Loop through one iteration of events.
 * \param eventLoop The Event Loop to run.
 */
void EventLoopRunOnce(EventLoopRef eventLoop);

/**
 * Stop the Event Loop from processing any more events.
 * \param eventLoop The Event Loop to stop.
 */
void EventLoopStop(EventLoopRef eventLoop);


// MARK: - Timers

/**
 * Add a timer with the given ID to the Event Loop.
 * \param eventLoop The Event Loop to modify.
 * \param id The ID of the timer.
 * \param timeout The timeout in milliseconds for the timer.
 * \note The `id` zero is reserved.
 * \note Duplicate `id` values will be ignored.
 */
void EventLoopAddTimer(EventLoopRef eventLoop, EventID id, uint32_t timeout);

/**
 * Does the Event Loop have a timer with the given ID?
 * \param eventLoop The Event Loop to inspect.
 * \param id The ID of the timer.
 * \return `true` if the timer exists, otherwise `false`.
 */
bool EventLoopHasTimer(EventLoopRef eventLoop, EventID id);

/**
 * Remove a timer with the given ID from the Event Loop.
 * \param eventLoop The Event Loop to modify.
 * \param id The ID of the timer.
 * \note Non-existent IDs are ignored.
 */
void EventLoopRemoveTimer(EventLoopRef eventLoop, EventID id);

#endif /* EVENT_LOOP_H */
