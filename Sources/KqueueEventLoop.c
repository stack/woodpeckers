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


// MARK: - Constants & Globals

#define EVENTS_STEP 5
#define EVENTS_TO_PROCESS 5

typedef struct _Event {
    bool isActive;

    EventID id;
    int16_t type;

    union {
        struct {
            uint32_t timeoutMs;
        } timer;
    };
} Event;

typedef struct _EventLoop {
    int kqueueFD;
    bool keepRunning;

    Event *timerEvents;
    size_t timerEventsCount;
    size_t timerEventsSize;
} EventLoop;


// MARK: - Prototypes

// Event Loop Controls
static void EventLoopHandleTimerEvent(EventLoopRef self, EventID id);

// Event Management
static void EventLoopExpandEvents(EventLoopRef self, Event **events, size_t *eventsSize);
static Event * EventLoopFindExistingEvent(EventLoopRef self, EventID id, int16_t type);
static Event * EventLoopFindFreeEvent(EventLoopRef self, int16_t type);
static bool EventLoopHasEvent(EventLoopRef self, EventID id, int16_t type);


// MARK: - Lifecycle Methods

EventLoopRef EventLoopCreate() {
    // Build the object and initialize it
    EventLoopRef self = (EventLoopRef)calloc(1, sizeof(EventLoop));
    self->kqueueFD = -1;

    // Set up kqueue
    self->kqueueFD = kqueue();

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

    if (self->kqueueFD != -1) {
        int temp = self->kqueueFD;
        self->kqueueFD = -1;

        close(temp);
    }

    free(self);
}


// MARK: - Event Loop Control

void EventLoopHandleTimerEvent(EventLoopRef self, EventID id) {
    printf("Timer %" PRIu32 " fired\n", id);
}

void EventLoopRun(EventLoopRef self) {
    self->keepRunning = true;
    
    while (self->keepRunning) {
        EventLoopRunOnce(self);
    }
}

void EventLoopRunOnce(EventLoopRef self) {
    struct kevent events[EVENTS_TO_PROCESS];

    int eventsAvailable = kevent(self->kqueueFD, NULL, 0, events, EVENTS_TO_PROCESS, NULL);

    if (eventsAvailable == -1) {
        printf("Failed to get the next events: %i\n", errno);
        return;
    }

    for (int idx = 0; idx < eventsAvailable; idx++) {
        struct kevent *event = &(events[idx]);

        switch (event->filter) {
            case EVFILT_TIMER:
                EventLoopHandleTimerEvent(self, event->ident);
                break;
            default:
                printf("Unhandled event filter: %i\n", event->filter);
                break;
        }
    }
}

void EventLoopStop(EventLoopRef self) {
    self->keepRunning = false;

    // TODO: Trigger the internal user event
}


// MARK: - Event Management

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

void EventLoopAddTimer(EventLoopRef self, EventID id, uint32_t timeout) {
    // Do nothing if the timer exists
    if (EventLoopHasEvent(self, id, EVFILT_TIMER)) {
        printf("Timer %" PRIu32 " already exists\n", id);
        return;
    }

    // Expand the events if needed
    if (self->timerEventsCount >= self->timerEventsSize) {
        EventLoopExpandEvents(self, &self->timerEvents, &self->timerEventsSize);
    }

    // Find the next available event
    Event *event = EventLoopFindFreeEvent(self, EVFILT_TIMER);

    if (event == NULL) {
        printf("Failed to find free space for timer event %" PRIu32 "\n", id);
        return;
    }

    // Add the event to kqueue
    struct kevent timerEvent;
    EV_SET(&timerEvent, id, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_CRITICAL, timeout, NULL);

    int result = kevent(self->kqueueFD, &timerEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        printf("Failed to add timer event %" PRIu32 " to kqueue: %i\n", id, errno);
        return;
    }

    // Finalize the event
    event->id = id;
    event->type = EVFILT_TIMER;
    event->timer.timeoutMs = timeout;

    event->isActive = true;
    self->timerEventsCount += 1;
}

bool EventLoopHasTimer(EventLoopRef self, EventID id) {
    bool result = EventLoopHasEvent(self, id, EVFILT_TIMER);
    return result;
}

void EventLoopRemoveTimer(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EVFILT_TIMER);

    if (event == NULL) {
        printf("Cannot remove timer %" PRIu32 ", which does not exist\n", id);
        return;
    }

    struct kevent timerEvent;
    EV_SET(&timerEvent, id, EVFILT_TIMER, EV_DISABLE | EV_DELETE, 0, 0, NULL);

    int result = kevent(self->kqueueFD, &timerEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        printf("Failed to remove timer event %" PRIu32 " from kqueue: %i\n", id, errno);
    }

    memset(event, 0, sizeof(Event));
} 
