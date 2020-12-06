//
//  EpollEventLoop.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-12-06.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "EventLoop.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include "Log.h"


// MARK: - Constants & Globals

#define EVENTS_STEP 5
#define EVENTS_TO_PROCESS 5
#define INTERNAL_EVENT_ID UINT16_MAX
#define TAG "EVENTLOOP"

typedef enum _EventType {
    EventTypeUnknown = 0,
    EventTypeTimer,
    EventTypeUser,
} EventType;

typedef struct _Event {
    bool isActive;
    bool shouldDeactivate;

    EventID id;
    EventType type;

    union {
        struct {
            int fd;
            uint32_t timeoutMs;
            EventLoopTimerFiredCallback timerFired;
        } timer;
        struct {
            int fd;
            EventLoopUserEventFiredCallback userEventFired;
        } user;
    };
} Event;

typedef struct _EventLoop {
    int epollFD;
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
static Event * EventLoopFindExistingEvent(EventLoopRef NONNULL self, EventID id, EventType type);
static Event * EventLoopFindFreeEvent(EventLoopRef NONNULL self, EventType type);
static bool EventLoopHasEvent(EventLoopRef NONNULL self, EventID id, EventType type);


// MARK: - Lifecycle Methods

EventLoopRef EventLoopCreate() {
    // Build the object and initialize it
    EventLoopRef self = (EventLoopRef)calloc(1, sizeof(EventLoop));
    self->epollFD = -1;

    // Set up kqueue
    self->epollFD = epoll_create1(0);

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

    if (self->epollFD != -1) {
        int temp = self->epollFD;
        self->epollFD = -1;

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
    struct epoll_event events[EVENTS_TO_PROCESS];
    int eventsAvailable = epoll_wait(self->epollFD, events, EVENTS_TO_PROCESS, timeout);

    if (eventsAvailable == -1) {
        LogErrno(TAG, errno, "Failed to get the next events");
        return;
    }

    for (int idx = 0; idx < eventsAvailable; idx++) {
        struct epoll_event *epollEvent = &(events[idx]);
        Event *event = epollEvent->data.ptr;

        switch (event->type) {
            case EventTypeTimer:
                EventLoopHandleTimerEvent(self, event);
                break;
            case EventTypeUser:
                EventLoopHandleUserEvent(self, event);
                break;
            default:
                LogE(TAG, "Unhandled event filter: %i", event->type);
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

static Event * EventLoopFindExistingEvent(EventLoopRef self, EventID id, EventType type) {
    Event *events;
    size_t size;

    switch (type) {
        case EventTypeTimer:
            events = self->timerEvents;
            size = self->timerEventsSize;
            break;
        case EventTypeUser:
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

static Event *EventLoopFindFreeEvent(EventLoopRef self, EventType type) {
    Event *events;
    size_t size;

    switch (type) {
        case EventTypeTimer:
            events = self->timerEvents;
            size = self->timerEventsSize;
            break;
        case EventTypeUser:
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

static bool EventLoopHasEvent(EventLoopRef self, EventID id, EventType type) {
    Event *events;
    size_t size;

    switch (type) {
        case EventTypeTimer:
            events = self->timerEvents;
            size = self->timerEventsSize;
            break;
        case EventTypeUser:
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
    if (EventLoopHasEvent(self, id, EventTypeTimer)) {
        LogE(TAG, "Timer %" PRIu16 " already exists", id);
        return;
    }

    // Expand the events if needed
    if (self->timerEventsCount >= self->timerEventsSize) {
        EventLoopExpandEvents(self, &self->timerEvents, &self->timerEventsSize);
    }

    // Find the next available event
    Event *event = EventLoopFindFreeEvent(self, EventTypeTimer);

    if (event == NULL) {
        LogE(TAG, "Failed to find free space for timer event %" PRIu16, id);
        return;
    }

    // Create the timer
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    if (fd == -1) {
        LogErrno(TAG, errno, "Failed to create timer event %" PRIu16, id);
        return;
    }

    struct timespec timeoutSpec;
    timeoutSpec.tv_sec = timeout / 1000LL;
    timeoutSpec.tv_nsec = (timeout - (timeoutSpec.tv_sec * 1000LL)) * 1000000LL;

    struct itimerspec timeoutValue;
    timeoutValue.it_value = timeoutSpec;
    timeoutValue.it_interval = timeoutSpec;

    int result = timerfd_settime(fd, 0, &timeoutValue, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to set timer event %" PRIu16 " timeout", id);
        close(fd);
        return;
    }

    // Add the timer to epoll
    struct epoll_event timerEvent;
    timerEvent.events = EPOLLIN;
    timerEvent.data.ptr = event;

    result = epoll_ctl(self->epollFD, EPOLL_CTL_ADD, fd, &timerEvent);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add timer event %" PRIu16 " to epoll", id);
        close(fd);
        return;
    }

    // Finalize the event
    event->id = id;
    event->type = EventTypeTimer;
    event->timer.fd = fd;
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

    // Clear the timer
    uint64_t count = 0;
    ssize_t bytesRead = read(event->timer.fd, &count, sizeof(count));

    if (bytesRead != sizeof(count)) {
        LogErrno(TAG, errno, "Failed to clear timer %" PRIu16 ", %li", event->id, bytesRead);
    }

    // Call the callback
    if (event->timer.timerFired != NULL) {
        event->timer.timerFired(self, event->id, self->callbackContext);
    }
}

bool EventLoopHasTimer(EventLoopRef self, EventID id) {
    bool result = EventLoopHasEvent(self, id, EventTypeTimer);
    return result;
}

void EventLoopRemoveTimer(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EventTypeTimer);

    if (event == NULL) {
        LogE(TAG, "Cannot remove timer %" PRIu16 ", which does not exist", id);
        return;
    }

    int result = epoll_ctl(self->epollFD, EPOLL_CTL_DEL, event->timer.fd, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to remove timer event %" PRIu16 " from epoll", id);
    }

    close(event->timer.fd);

    event->shouldDeactivate = true;
}


// MARK: - User Events

void EventLoopAddUserEvent(EventLoopRef self, EventID id, EventLoopUserEventFiredCallback callback) {
        // Do nothing if the user event exists
    if (EventLoopHasEvent(self, id, EventTypeUser)) {
        LogE(TAG, "User event %" PRIu16 " already exists", id);
        return;
    }

    // Expand the events if needed
    if (self->userEventsCount >= self->userEventsSize) {
        EventLoopExpandEvents(self, &self->userEvents, &self->userEventsSize);
    }

    // Find the next available event
    Event *event = EventLoopFindFreeEvent(self, EventTypeUser);

    if (event == NULL) {
        LogE(TAG, "Failed to find free space for user event %" PRIu16, id);
        return;
    }

    // Build the eventfd
    int fd = eventfd(0, EFD_NONBLOCK);

    if (fd == -1) {
        LogErrno(TAG, errno, "Failed to create event fd for %" PRIu16, id);
        return;
    }

    // Add the event to epoll
    struct epoll_event userEvent;
    userEvent.events = EPOLLIN;
    userEvent.data.ptr = event;

    int result = epoll_ctl(self->epollFD, EPOLL_CTL_ADD, fd, &userEvent);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add user event %" PRIu16 " to epoll", id);
        close(fd);
        return;
    }

    // Finalize the event
    event->id = id;
    event->type = EventTypeUser;
    event->user.fd = fd;
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

    // Clear the trigger
    uint64_t count = 0;
    ssize_t bytesRead = read(event->user.fd, &count, sizeof(count));

    if (bytesRead != sizeof(count)) {
        LogErrno(TAG, errno, "Failed to clear user event %" PRIu16, event->id);
    }

    // Call the callback
    if (event->user.userEventFired != NULL) {
        event->user.userEventFired(self, event->id, self->callbackContext);
    }
}

bool EventLoopHasUserEvent(EventLoopRef self, EventID id) {
    bool result = EventLoopHasEvent(self, id, EventTypeUser);
    return result;
}

void EventLoopRemoveUserEvent(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EventTypeUser);

    if (event == NULL) {
        LogE(TAG, "Cannot remove user event %" PRIu16 ", which does not exist", id);
        return;
    }

    int result = epoll_ctl(self->epollFD, EPOLL_CTL_DEL, event->user.fd, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to remove user event %" PRIu16 " from epoll", id);
    }

    close(event->user.fd);

    event->shouldDeactivate = true;
}

void EventLoopTriggerUserEvent(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EventTypeUser);

    if (event == NULL) {
        LogE(TAG, "Failed to find the user event for %" PRIu16);
        return;
    }

    // Write to the event
    uint64_t value = 1;
    size_t bytesWritten = write(event->user.fd, &value, sizeof(value));

    if (bytesWritten != sizeof(value)) {
        LogErrno(TAG, errno, "Failed to write to user event %" PRIu16, event->id);
    }
}


// MARK: - Callbacks

void EventLoopSetCallbackContext(EventLoopRef self, void *context) {
    self->callbackContext = context;
}
