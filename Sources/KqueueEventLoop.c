//
//  KqueueEventLoop.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-24.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "EventLoop.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Log.h"


// MARK: - Constants & Globals

#define EVENTS_STEP 5
#define EVENTS_TO_PROCESS 5
#define INTERNAL_EVENT_ID UINT16_MAX
#define TAG "EVENTLOOP"

typedef struct _Event {
    bool isActive;
    bool shouldDeactivate;

    EventID id;
    int16_t type;

    union {
        struct {
            uint32_t timeoutMs;
            EventLoopTimerFiredCallback timerFired;
        } timer;
        struct {
            EventLoopUserEventFiredCallback userEventFired;
        } user;
    };
} Event;

typedef struct _EventLoop {
    int kqueueFD;
    bool keepRunning;

    Event *timerEvents;
    size_t timerEventsCount;
    size_t timerEventsSize;

    Event *userEvents;
    size_t userEventsCount;
    size_t userEventsSize;

    void *callbackContext;
} EventLoop;


// MARK: - Prototypes

// Event Loop Controls
static void EventLoopHandleTimerEvent(EventLoopRef NONNULL self, Event * NONNULL event);
static void EventLoopHandleUserEvent(EventLoopRef NONNULL self, Event * NONNULL event);

// Callbacks
static void EventLoopHandleStopUserEvent(EventLoopRef NONNULL self, EventID id, void * NULLABLE context);

// Event Management
static void EventLoopDeactivateEvents(EventLoopRef NONNULL self);
static void EventLoopExpandEvents(EventLoopRef NONNULL self, Event * NONNULL * NONNULL events, size_t * NONNULL eventsSize);
static Event * EventLoopFindExistingEvent(EventLoopRef NONNULL self, EventID id, int16_t type);
static Event * EventLoopFindFreeEvent(EventLoopRef NONNULL self, int16_t type);
static bool EventLoopHasEvent(EventLoopRef NONNULL self, EventID id, int16_t type);


// MARK: - Lifecycle Methods

EventLoopRef EventLoopCreate() {
    // Build the object and initialize it
    EventLoopRef self = (EventLoopRef)calloc(1, sizeof(EventLoop));
    self->kqueueFD = -1;

    // Set up kqueue
    self->kqueueFD = kqueue();

    // Add the stop event
    EventLoopAddUserEvent(self, INTERNAL_EVENT_ID, EventLoopHandleStopUserEvent);

    return self;
}

void EventLoopDestroy(EventLoopRef self) {
    if (self->timerEvents != NULL) {
        Event *temp = self->timerEvents;
        self->timerEvents = NULL;
        self->timerEventsCount = 0;
        self->timerEventsSize = 0;

        free(temp);
    }

    if (self->userEvents != NULL) {
        Event *temp = self->userEvents;
        self->userEvents = NULL;
        self->userEventsCount = 0;
        self->userEventsSize = 0;
        
        free(temp);
    }

    if (self->kqueueFD != -1) {
        int temp = self->kqueueFD;
        self->kqueueFD = -1;

        close(temp);
    }

    free(self);
}


// MARK: - Event Loop Control

void EventLoopRun(EventLoopRef self) {
    self->keepRunning = true;

    while (self->keepRunning) {
        EventLoopRunOnce(self, -1);
    }
}

void EventLoopRunOnce(EventLoopRef self, int64_t timeout) {
    struct kevent events[EVENTS_TO_PROCESS];

    struct timespec timeoutValue;
    struct timespec *timeoutPointer;

    if (timeout == -1) {
        timeoutPointer = NULL;
    } else {
        timeoutValue.tv_sec = timeout / 1000LL;
        timeoutValue.tv_nsec = (timeout - (timeoutValue.tv_sec * 1000LL)) * 1000000LL;
        timeoutPointer = &timeoutValue;
    }

    int eventsAvailable = kevent(self->kqueueFD, NULL, 0, events, EVENTS_TO_PROCESS, timeoutPointer);

    if (eventsAvailable == -1) {
        LogErrno(TAG, errno, "Failed to get the next events");
        return;
    }

    for (int idx = 0; idx < eventsAvailable; idx++) {
        struct kevent *event = &(events[idx]);

        switch (event->filter) {
            case EVFILT_TIMER:
                EventLoopHandleTimerEvent(self, (Event *)event->udata);
                break;
            case EVFILT_USER:
                EventLoopHandleUserEvent(self, (Event *)event->udata);
                break;
            default:
                LogE(TAG, "Unhandled event filter: %i", event->filter);
                break;
        }
    }

    EventLoopDeactivateEvents(self);
}

void EventLoopStop(EventLoopRef self) {
    EventLoopTriggerUserEvent(self, INTERNAL_EVENT_ID);
}


// MARK: - Event Management

static void EventLoopDeactivateEvents(EventLoopRef self) {
    for (size_t idx = 0; idx < self->timerEventsSize; idx++) {
        Event *event = self->timerEvents + idx;

        if (event->shouldDeactivate) {
            memset(event, 0, sizeof(Event));
        }
    }

    for (size_t idx = 0; idx < self->userEventsSize; idx++) {
        Event *event = self->userEvents + idx;

        if (event->shouldDeactivate) {
            memset(event, 0, sizeof(Event));
        }
    }
}

static void EventLoopExpandEvents(EventLoopRef self, Event **events, size_t *eventsSize) {
    Event *currentEvents = *events;
    size_t currentSize = *eventsSize;

    currentEvents = (Event *)realloc(currentEvents, sizeof(Event) * (currentSize + EVENTS_STEP));
    memset(currentEvents + currentSize, 0, sizeof(Event) * EVENTS_STEP);

    *events = currentEvents;
    *eventsSize += EVENTS_STEP;
}

static Event * EventLoopFindExistingEvent(EventLoopRef self, EventID id, int16_t type) {
    Event *events;
    size_t size;

    switch (type) {
        case EVFILT_TIMER:
            events = self->timerEvents;
            size = self->timerEventsSize;
            break;
        case EVFILT_USER:
            events = self->userEvents;
            size = self->userEventsSize;
            break;
        default:
            events = NULL;
            size = 0;
            break;
    }

    for (size_t idx = 0; idx < size; idx++) {
        Event *event = events + idx;

        if (event->isActive && event->id == id) {
            return event;
        }
    }

    return NULL;
}

static Event *EventLoopFindFreeEvent(EventLoopRef self, int16_t type) {
    Event *events;
    size_t size;

    switch (type) {
        case EVFILT_TIMER:
            events = self->timerEvents;
            size = self->timerEventsSize;
            break;
        case EVFILT_USER:
            events = self->userEvents;
            size = self->userEventsSize;
            break;
        default:
            events = NULL;
            size = 0;
            break;
    }

    for (size_t idx = 0; idx < size; idx++) {
        Event *event = events + idx;

        if (event->isActive == false) {
            return event;
        }
    }

    return NULL;
}

static bool EventLoopHasEvent(EventLoopRef self, EventID id, int16_t type) {
    Event *events;
    size_t size;

    switch (type) {
        case EVFILT_TIMER:
            events = self->timerEvents;
            size = self->timerEventsSize;
            break;
        case EVFILT_USER:
            events = self->userEvents;
            size = self->userEventsSize;
            break;
        default:
            events = NULL;
            size = 0;
            break;
    }

    for (size_t idx = 0; idx < size; idx++) {
        Event *event = events + idx;

        if (event->isActive && event->id == id) {
            return true;
        }
    }

    return false;
}


// MARK: - Timers

void EventLoopAddTimer(EventLoopRef self, EventID id, uint32_t timeout, EventLoopTimerFiredCallback callback) {
    // Do nothing if the timer exists
    if (EventLoopHasEvent(self, id, EVFILT_TIMER)) {
        LogE(TAG, "Timer %" PRIu16 " already exists", id);
        return;
    }

    // Expand the events if needed
    if (self->timerEventsCount >= self->timerEventsSize) {
        EventLoopExpandEvents(self, &self->timerEvents, &self->timerEventsSize);
    }

    // Find the next available event
    Event *event = EventLoopFindFreeEvent(self, EVFILT_TIMER);

    if (event == NULL) {
        LogE(TAG, "Failed to find free space for timer event %" PRIu16, id);
        return;
    }

    // Add the event to kqueue
    struct kevent timerEvent;
    EV_SET(&timerEvent, id, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_CRITICAL, timeout, event);

    int result = kevent(self->kqueueFD, &timerEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add timer event %" PRIu16 " to kqueue", id);
        return;
    }

    // Finalize the event
    event->id = id;
    event->type = EVFILT_TIMER;
    event->timer.timeoutMs = timeout;
    event->timer.timerFired = callback;

    event->isActive = true;
    self->timerEventsCount += 1;
}

static void EventLoopHandleTimerEvent(EventLoopRef self, Event *event) {
    // This event may have been dropped, so skip it
    if (!event->isActive || event->shouldDeactivate) {
        return;
    }

    // Call the callback
    if (event->timer.timerFired != NULL) {
        event->timer.timerFired(self, event->id, self->callbackContext);
    }
}

bool EventLoopHasTimer(EventLoopRef self, EventID id) {
    bool result = EventLoopHasEvent(self, id, EVFILT_TIMER);
    return result;
}

void EventLoopRemoveTimer(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EVFILT_TIMER);

    if (event == NULL) {
        LogE(TAG, "Cannot remove timer %" PRIu16 ", which does not exist", id);
        return;
    }

    struct kevent timerEvent;
    EV_SET(&timerEvent, id, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);

    int result = kevent(self->kqueueFD, &timerEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to remove timer event %" PRIu16 " from kqueue", id);
    }

    event->shouldDeactivate = true;
} 


// MARK: - User Events

void EventLoopAddUserEvent(EventLoopRef self, EventID id, EventLoopUserEventFiredCallback callback) {
        // Do nothing if the user event exists
    if (EventLoopHasEvent(self, id, EVFILT_USER)) {
        LogE(TAG, "User event %" PRIu16 " already exists", id);
        return;
    }

    // Expand the events if needed
    if (self->userEventsCount >= self->userEventsSize) {
        EventLoopExpandEvents(self, &self->userEvents, &self->userEventsSize);
    }

    // Find the next available event
    Event *event = EventLoopFindFreeEvent(self, EVFILT_USER);

    if (event == NULL) {
        LogE(TAG, "Failed to find free space for user event %" PRIu16, id);
        return;
    }

    // Add the event to kqueue
    struct kevent userEvent;
    EV_SET(&userEvent, id, EVFILT_USER, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, event);

    int result = kevent(self->kqueueFD, &userEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add user event %" PRIu16 " to kqueue", id);
        return;
    }

    // Finalize the event
    event->id = id;
    event->type = EVFILT_USER;
    event->user.userEventFired = callback;

    event->isActive = true;
    self->userEventsCount += 1;
}

static void EventLoopHandleStopUserEvent(EventLoopRef self, EventID id, void *context) {
    self->keepRunning = false;
}

static void EventLoopHandleUserEvent(EventLoopRef self, Event *event) {
    // This event may have been dropped, so skip it
    if (!event->isActive || event->shouldDeactivate) {
        return;
    }

    // Call the callback
    if (event->user.userEventFired != NULL) {
        event->user.userEventFired(self, event->id, self->callbackContext);
    }

    // Clear the trigger
    struct kevent userEvent;
    EV_SET(&userEvent, event->id, EVFILT_USER, EV_CLEAR, 0, 0, NULL);

    int result = kevent(self->kqueueFD, &userEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to clear triggered user event %" PRIu16 " to kqueue", event->id);
        return;
    }
}

bool EventLoopHasUserEvent(EventLoopRef self, EventID id) {
    bool result = EventLoopHasEvent(self, id, EVFILT_USER);
    return result;
}

void EventLoopRemoveUserEvent(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EVFILT_USER);

    if (event == NULL) {
        LogE(TAG, "Cannot remove user event %" PRIu16 ", which does not exist", id);
        return;
    }

    struct kevent userEvent;
    EV_SET(&userEvent, id, EVFILT_USER, EV_DELETE, 0, 0, NULL);

    int result = kevent(self->kqueueFD, &userEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to remove user event %" PRIu16 " from kqueue", id);
    }

    event->shouldDeactivate = true;
}

void EventLoopTriggerUserEvent(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EVFILT_USER);

    if (event == NULL) {
        LogE(TAG, "Failed to find the user event for %" PRIu16);
        return;
    }

    struct kevent userEvent;
    EV_SET(&userEvent, id, EVFILT_USER, 0, NOTE_TRIGGER, 0, event);

    int result = kevent(self->kqueueFD, &userEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to trigger user event %" PRIu16 " to kqueue", id);
        return;
    }
}


// MARK: - Callbacks

void EventLoopSetCallbackContext(EventLoopRef self, void *context) {
    self->callbackContext = context;
}
