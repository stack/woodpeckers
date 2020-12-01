//
//  EventLoop.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-24.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "Macros.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


BEGIN_DECLS


// MARK: - Constants & Globals

/// A unique identifier for an event of a specific type. `UINT32_MAX` is reserved.
typedef uint16_t EventID;

/// The Event Loop object
typedef struct _EventLoop * EventLoopRef;


// MARK: - Callbacks

/**
 * Called when a timer has fired.
 * \param eventLoop The Event Loop the timer fired from.
 * \param id The ID of the timer.
 * \param context The opaque callback context associated with the Event Loop.
 */
typedef void (* EventLoopTimerFiredCallback)(EventLoopRef NONNULL eventLoop, EventID id, void * NULLABLE context);

/**
 * Called when a user event has fired.
 * \param eventLoop The Event Loop the user event fired from.
 * \param id The ID of the user event.
 * \param context The opque callback context associated with the Event Loop.
 */
typedef void (* EventLoopUserEventFiredCallback)(EventLoopRef NONNULL eventLoop, EventID id, void * NULLABLE context);


// MARK: - Lifecycle Methods

/**
 * Create a new Event Loop instance.
 * \return A new instance, or `NULL` if an error occurred.
 */
EventLoopRef NONNULL EventLoopCreate(void);

/**
 * Destroy an Event Loop.
 * \param eventLoop The instance to destroy.
 */
void EventLoopDestroy(EventLoopRef NONNULL eventLoop);


// MARK: - Event Loop Control

/**
 * Run the Event Loop.
 * \param eventLoop The Event Loop to run.
 */
void EventLoopRun(EventLoopRef NONNULL eventLoop);

/**
 * Run the Event Loop through one iteration of events.
 * \param eventLoop The Event Loop to run.
 * \param timeout The time in milliseconds to time out, or  `-1` to wait indefinitely.
 */
void EventLoopRunOnce(EventLoopRef NONNULL eventLoop, int64_t timeout);

/**
 * Stop the Event Loop from processing any more events.
 * \param eventLoop The Event Loop to stop.
 */
void EventLoopStop(EventLoopRef NONNULL eventLoop);


// MARK: - Timers

/**
 * Add a timer with the given ID to the Event Loop.
 * \param eventLoop The Event Loop to modify.
 * \param id The ID of the timer.
 * \param timeout The timeout in milliseconds for the timer.
 * \param callback The callback to call when the timer has fired.
 * \note The `id` value of `UINT16_MAX` is reserved.
 * \note Duplicate `id` values will be ignored.
 */
void EventLoopAddTimer(EventLoopRef NONNULL eventLoop, EventID id, uint32_t timeout, EventLoopTimerFiredCallback NULLABLE callback);

/**
 * Does the Event Loop have a timer with the given ID?
 * \param eventLoop The Event Loop to inspect.
 * \param id The ID of the timer.
 * \return `true` if the timer exists, otherwise `false`.
 */
bool EventLoopHasTimer(EventLoopRef NONNULL eventLoop, EventID id);

/**
 * Remove a timer with the given ID from the Event Loop.
 * \param eventLoop The Event Loop to modify.
 * \param id The ID of the timer.
 * \note Non-existent IDs are ignored.
 */
void EventLoopRemoveTimer(EventLoopRef NONNULL eventLoop, EventID id);


// MARK: - User Events

/**
 * Add a user event with the given ID to the Event Loop.
 * \param eventLoop The Event Loop to modify.
 * \param id The ID of the user event.
 * \param callback The callback to call when the timer has fired.
 * \note The `id` value of `UINT16_MAX` is reserved.
 * \note Duplicate `id` values will be ignored.
 */
void EventLoopAddUserEvent(EventLoopRef NONNULL eventLoop, EventID id, EventLoopUserEventFiredCallback NULLABLE callback);

/**
 * Does the Event Loop have a user event with the given ID?
 * \param eventLoop The Event Loop to inspect.
 * \param id The ID of the user event.
 * \return `true` if the user event exists, otherwise `false`.
 */
bool EventLoopHasUserEvent(EventLoopRef NONNULL eventLoop, EventID id);

/**
 * Remove a user event with the given ID from the Event Loop.
 * \param eventLoop The Event Loop to modify.
 * \param id The ID of the user event.
 * \note Non-existent IDs are ignored.
 */
void EventLoopRemoveUserEvent(EventLoopRef NONNULL eventLoop, EventID id);

/**
 * Trigger the user event with the given ID.
 * \param eventLoop The Event Loop to modify.
 * \param id The ID of the user event.
 * \note Non-existent IDs are ignored.
 */
void EventLoopTriggerUserEvent(EventLoopRef NONNULL eventLoop, EventID id);


// MARK: - Callbacks

/**
 * Set the callback context associated with each callback method.
 * \param eventLoop The Event Loop to modify.
 * \param context The opaque pointer associated with each callback.
 */
void EventLoopSetCallbackContext(EventLoopRef NONNULL eventLoop, void * NULLABLE context);


END_DECLS

#endif /* EVENT_LOOP_H */
