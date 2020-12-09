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

#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Log.h"


// MARK: - Constants & Globals

#define EVENTS_STEP 5
#define EVENTS_TO_PROCESS 5
#define INTERNAL_EVENT_ID UINT16_MAX
#define SEND_BUFFER_SIZE 1024
#define TAG "EventLoop"

typedef enum _EventType {
    EventTypeUnknown = 0,
    EventTypeServer,
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
            uint16_t port;
            struct sockaddr_storage localAddress;
            EventLoopServerShouldAcceptCallback shouldAccept;
            EventLoopServerDidAcceptCallback didAccept;
        } server;
        struct {
            uint32_t timeoutMs;
            EventLoopTimerFiredCallback timerFired;
        } timer;
        struct {
            EventLoopUserEventFiredCallback userEventFired;
        } user;
    };
} Event;

typedef struct _ServerEventClient {
    bool isActive;

    int idx;
    int fd;
    struct sockaddr_storage remoteAddress;
    Event *server;

    uint8_t sendBuffer[SEND_BUFFER_SIZE];
    size_t sendBufferSize;
} ServerEventClient;

typedef struct _EventLoop {
    int kqueueFD;
    bool keepRunning;

    Event *serverEvents;
    size_t serverEventsCount;
    size_t serverEventsSize;

    ServerEventClient *serverEventClients;
    size_t serverEventClientsCount;
    size_t serverEventClientsSize;

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
static void EventLoopHandleServerClientReadEvent(EventLoopRef NONNULL self, Event * NONNULL event);
static void EventLoopHandleServerEvent(EventLoopRef NONNULL self, Event * NONNULL event);
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
    self->kqueueFD = -1;

    // Set up kqueue
    self->kqueueFD = kqueue();

    // Add the stop event
    EventLoopAddUserEvent(self, INTERNAL_EVENT_ID, EventLoopHandleStopUserEvent);

    return self;
}

void EventLoopDestroy(EventLoopRef self) {
    // TODO: Clean up server clients

    if (self->serverEvents != NULL) {
        Event *temp = self->serverEvents;
        self->serverEvents = NULL;
        self->serverEventsCount = 0;
        self->serverEventsSize = 0;

        free(temp);
    }

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

        Event *loopEvent = (Event *)event->udata;

        switch (event->filter) {
            case EVFILT_READ:
                if (loopEvent->type == EventTypeServer) {
                    EventLoopHandleServerEvent(self, loopEvent);
                } else {
                    EventLoopHandleServerClientReadEvent(self, loopEvent);
                }

                break;
            case EVFILT_TIMER:
                EventLoopHandleTimerEvent(self, loopEvent);
                break;
            case EVFILT_USER:
                EventLoopHandleUserEvent(self, loopEvent);
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
    // TODO: Deactivate clients?

    for (size_t idx = 0; idx < self->serverEventsSize; idx++) {
        Event *event = self->serverEvents + idx;

        if (event->shouldDeactivate) {
            memset(event, 0, sizeof(Event));
            event->server.fd = -1;
        }
    }

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
        case EventTypeServer:
            events = self->serverEvents;
            size = self->serverEventsSize;
            break;
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
        case EventTypeServer:
            events = self->serverEvents;
            size = self->serverEventsSize;
            break;
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
        case EventTypeServer:
            events = self->serverEvents;
            size = self->serverEventsSize;
            break;
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


// MARK: - Servers

void EventLoopAddServer(EventLoopRef self, EventID id, uint16_t port, EventLoopServerShouldAcceptCallback shouldAccept, EventLoopServerDidAcceptCallback didAccept) {
    // DO nothing if the server already exists
    if (EventLoopHasEvent(self, id, EventTypeServer)) {
        LogE(TAG, "Server %" PRIu16 " already exists", id);
        return;
    }

    // Expand the servers if needed
    if (self->serverEventsCount >= self->serverEventsSize) {
        EventLoopExpandEvents(self, &self->serverEvents, &self->serverEventsSize);
    }

    // Create the server socket
    int fd = socket(PF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        LogErrno(TAG, errno, "Failed to create a socket for %" PRIu16, id);
        return;
    }

    struct sockaddr_storage address;
    memset(&address, 0, sizeof(address));

    address.ss_family = AF_INET;

    struct sockaddr_in *ipv4Address = (struct sockaddr_in *)&address;
    ipv4Address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ipv4Address->sin_port = htons(port);

    int result = bind(fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in));

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to bind socket for %" PRIu16, id);
        close(fd);
        return;
    }

    result = listen(fd, SOMAXCONN);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to listen on socket %" PRIu16, id);
        close(fd);
        return;
    }

    // Find the next available server
    Event *event = EventLoopFindFreeEvent(self, EventTypeServer);

    if (event == NULL) {
        LogE(TAG, "Failed to find free space for server %" PRIu16, id);
        close(fd);
        return;
    }

    // Add the event to kqueue
    struct kevent serverEvent;
    EV_SET(&serverEvent, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, event);

    result = kevent(self->kqueueFD, &serverEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add server %" PRIu16 " to kqueue", id);
        close(fd);
        return;
    }

    event->id = id;
    event->type = EventTypeServer;
    event->server.fd = fd;
    event->server.port = port;
    event->server.localAddress = address;
    event->server.shouldAccept = shouldAccept;
    event->server.didAccept = didAccept;

    event->isActive = true;
    self->serverEventsCount += 1;
}

static void EventLoopHandleServerClientReadEvent(EventLoopRef self, Event *event) {
}

static void EventLoopHandleServerEvent(EventLoopRef self, Event *event) {
    // Accept the connection
    struct sockaddr_storage remoteAddress;
    socklen_t remoteAddressSize = sizeof(remoteAddress);

    int clientFD = accept(event->server.fd, (struct sockaddr *)&remoteAddress, &remoteAddressSize);

    if (clientFD == -1) {
        LogErrno(TAG, errno, "Failed to accept client on server %" PRIu16, event->id);
        return;
    }

    bool shouldAccept = true;

    if (event->server.shouldAccept != NULL) {
        shouldAccept = event->server.shouldAccept(self, event->id, (struct sockaddr *)&remoteAddress, self->callbackContext);
    }

    if (!shouldAccept) {
        shutdown(clientFD, SHUT_RDWR);
        close(clientFD);

        return;
    }

    LogD(TAG, "New client on server %" PRIu16, event->id);

    if (self->serverEventClientsCount >= self->serverEventClientsSize) {
        self->serverEventClients = (ServerEventClient *)realloc(self->serverEventClients, sizeof(ServerEventClient) * (self->serverEventClientsSize + EVENTS_STEP));
        memset(self->serverEventClients + self->serverEventClientsSize, 0, sizeof(ServerEventClient) * EVENTS_STEP);
        self->serverEventClientsSize += EVENTS_STEP;
    }

    ServerEventClient *client = NULL;

    for (size_t idx = 0; idx < self->serverEventClientsSize; idx++) {
        ServerEventClient *current = self->serverEventClients + idx;

        if (!current->isActive) {
            client = current;
            client->idx = idx;
            break;
        }
    }

    if (client == NULL) {
        LogE(TAG, "Failed to find space for a new client on %" PRIu16, event->id);
        shutdown(clientFD, SHUT_RDWR);
        close(clientFD);
    }

    client->fd = clientFD;
    memcpy(&client->remoteAddress, &remoteAddress, sizeof(remoteAddress));
    client->server = event;

    memset(client->sendBuffer, 0, SEND_BUFFER_SIZE);
    client->sendBufferSize = 0;

    // Add the event to kqueue
    struct kevent clientEvent;
    EV_SET(&clientEvent, clientFD, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, event);

    int result = kevent(self->kqueueFD, &clientEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add client %i from server %" PRIu16 " to kqueue", client->idx, event->id);
        shutdown(clientFD, SHUT_RDWR);
        close(clientFD);

        return;
    }

    client->isActive = true;
    self->serverEventClientsCount += 1;

    if (event->server.didAccept != NULL) {
        event->server.didAccept(self, event->id, (struct sockaddr *)&remoteAddress, client->idx, self->callbackContext);
    }
}

bool EventLoopHasServer(EventLoopRef self, EventID id) {
    bool result = EventLoopHasEvent(self, id, EventTypeServer);
    return result;
}

void EventLoopRemoveServer(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EventTypeServer);

    if (event == NULL) {
        LogE(TAG, "Cannot remove server %" PRIu16 ", which does not exist", id);
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
    event->type = EventTypeTimer;
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
    bool result = EventLoopHasEvent(self, id, EventTypeTimer);
    return result;
}

void EventLoopRemoveTimer(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EventTypeTimer);

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
    event->type = EventTypeUser;
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
    bool result = EventLoopHasEvent(self, id, EventTypeUser);
    return result;
}

void EventLoopRemoveUserEvent(EventLoopRef self, EventID id) {
    Event *event = EventLoopFindExistingEvent(self, id, EventTypeUser);

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
    Event *event = EventLoopFindExistingEvent(self, id, EventTypeUser);

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
