//
//  EventLoop.h
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-12-07.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "config.h"

#include "EventLoop.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Log.h"

#if TARGET_PLATFORM_APPLE
#include <sys/event.h>
#else
// TODO: Epoll etc al includes
#endif


// MARK: - Constants & Globals

#define EVENTS_STEP 5
#define EVENTS_TO_PROCESS 5
#define INTERNAL_EVENT_ID UINT16_MAX
#define RECEIVE_BUFFER_SIZE 1024
#define SEND_BUFFER_SIZE 1024
#define TAG "EventLoop"

typedef struct _Event * EventRef;

typedef enum _EventType {
    EventTypeUnknown = 0,
    EventTypeServer,
    EventTypeServerPeer,
    EventTypeTimer,
    EventTypeUser,
} EventType;

typedef struct _Event {
    EventType type;
    EventID id;
    bool isActive;

    union {
        struct {
            int fd;

            EventLoopServerDidAcceptCallback didAccept;
            EventLoopServerShouldAcceptCallback shouldAccept;
            EventLoopServerDidReceiveDataCallback  didReceiveData;
            EventLoopServerPeerDidDisconnectCallback peerDidDisconnect;
        } server;

        struct {
            int fd;
            EventID serverID;
            uint8_t *receiveBuffer;
            size_t receiveBufferSize;
            uint8_t *sendBuffer;
            size_t sendBufferSize;
            EventLoopServerDidReceiveDataCallback  didReceiveData;
            EventLoopServerPeerDidDisconnectCallback peerDidDisconnect;
        } serverPeer;

        struct {
            EventLoopTimerFiredCallback timerFired;
        } timer;

        struct {
            EventLoopUserEventFiredCallback userEventFired;
        } user;
    };
} Event;

typedef struct _EventLoop {
#if TARGET_PLATFORM_APPLE
    int kqueueFD;
#elif TARGET_PLATFORM_LINUX
    #warning Implement Epoll
#else
    #error Unhandled target platform
#endif

    bool keepRunning;
    EventID nextID;

    EventRef *serverEvents;
    size_t serverEventsCount;
    size_t serverEventsSize;

    EventRef *serverPeerEvents;
    size_t serverPeerEventsCount;
    size_t serverPeerEventsSize;

    EventRef *timerEvents;
    size_t timerEventsCount;
    size_t timerEventsSize;

    EventRef *userEvents;
    size_t userEventsCount;
    size_t userEventsSize;

    EventRef *deactivatedEvents;
    size_t deactivatedEventsCount;
    size_t deactivatedEventsSize;

    void *callbackContext;
} EventLoop;


// MARK: - Prototypes

// Lifecycle Methods
static void EventDestroy(Event * NONNULL event);

// Event Loop Controls
static void EventLoopHandleServerEvent(EventLoopRef NONNULL eventLoop, Event * NONNULL event);
static void EventLoopHandleServerPeerDisconnect(EventLoopRef NONNULL eventLoop, Event * NONNULL event);
static void EventLoopHandleServerPeerReadEvent(EventLoopRef NONNULL eventLoop, Event * NONNULL event);
static void EventLoopHandleServerPeerWriteEvent(EventLoopRef NONNULL eventLoop, Event * NONNULL event);
static void EventLoopHandleTimerEvent(EventLoopRef NONNULL eventLoop, Event * NONNULL event);
static void EventLoopHandleUserEvent(EventLoopRef NONNULL eventLoop, Event * NONNULL event);

// Callbacks
static void EventLoopHandleStopUserEvent(EventLoopRef NONNULL eventLoop, EventID id, void * NULLABLE context);

// Event Management
static void EventLoopDeactivateEvent(EventLoopRef NONNULL eventLoop, EventRef NONNULL event);
static void EventLoopDeactivateEvents(EventLoopRef NONNULL eventLoop);
static void EventLoopExpandEvents(EventLoopRef NONNULL eventLoop, EventRef NONNULL * NONNULL * NONNULL events, size_t * NONNULL eventsSize);
static Event * EventLoopFindExistingEvent(EventLoopRef NONNULL eventLoop, EventID id, EventType type);
static bool EventLoopHasEvent(EventLoopRef NONNULL eventLoop, EventID id, EventType type);


// MARK: - Lifecycle Methods

static void EventDestroy(Event *event) {
    if (event->type == EventTypeServer) {
        int tempFD = event->server.fd;
        event->server.fd = -1;

        if (tempFD != -1) {
            close(tempFD);
        }
    } else if (event->type == EventTypeServerPeer) {
        int tempFD = event->server.fd;
        event->serverPeer.fd = -1;

        if (tempFD != -1) {
            close(tempFD);
        }

        event->serverPeer.sendBufferSize = 0;
        SAFE_DESTROY(event->serverPeer.sendBuffer, free);

        event->serverPeer.receiveBufferSize = 0;
        SAFE_DESTROY(event->serverPeer.receiveBuffer, free);
    } else if (event->type == EventTypeTimer) {
#if TARGET_PLATFORM_LINUX
        #warning Close timer fd
#endif
    } else if (event->type == EventTypeUser) {
#if TARGET_PLATFORM_LINUX
        #warning Close event fd
#endif
    }
}

EventLoopRef EventLoopCreate() {
    // Build the object and initialize it
    EventLoopRef self = (EventLoopRef)calloc(1, sizeof(EventLoop));

#if TARGET_PLATFORM_APPLE
    self->kqueueFD = -1;
#elif TARGET_PLATFORM_LINUX
    #warning Implement epoll
#else
    #error Unhandled target platform
#endif

    // Set up kqueue
#if TARGET_PLATFORM_APPLE
    self->kqueueFD = kqueue();
#elif TARGET_PLATFORM_LINUX
    #warning Implement epoll
#else
    #error Unhandled target platform
#endif

    // Add the stop event
    EventLoopAddUserEvent(self, INTERNAL_EVENT_ID, EventLoopHandleStopUserEvent);

    return self;
}

void EventLoopDestroy(EventLoopRef self) {
    if (self->serverEvents != NULL) {
        EventRef *temp = self->serverEvents;
        self->serverEvents = NULL;

        size_t size = self->serverEventsSize;
        self->serverEventsSize = 0;
        self->serverEventsCount = 0;

        for (size_t idx = 0; idx < size; idx++) {
            SAFE_DESTROY(temp[idx], EventDestroy);
        }

        free(temp);
    }

    if (self->timerEvents != NULL) {
        EventRef *temp = self->timerEvents;
        self->timerEvents = NULL;

        size_t size = self->timerEventsSize;
        self->timerEventsSize = 0;
        self->timerEventsCount = 0;

        for (size_t idx = 0; idx < size; idx++) {
            SAFE_DESTROY(temp[idx], EventDestroy);
        }

        free(temp);
    }

    if (self->userEvents != NULL) {
        EventRef *temp = self->userEvents;
        self->userEvents = NULL;

        size_t size = self->userEventsSize;
        self->userEventsSize = 0;
        self->userEventsCount = 0;

        for (size_t idx = 0; idx < size; idx++) {
            SAFE_DESTROY(temp[idx], EventDestroy);
        }

        free(temp);
    }
    
#if TARGET_PLATFORM_APPLE
    if (self->kqueueFD != -1) {
        int temp = self->kqueueFD;
        self->kqueueFD = -1;

        close(temp);
    }
#elif TARGET_PLATFORM_LINUX
    #warning Implement epoll
#else
    #error Unhandled target platform
#endif

    free(self);
}


// MARK: - Event Loop Control

void EventLoopRun(EventLoopRef self) {
    self->keepRunning = true;

    while (self->keepRunning) {
        EventLoopRunOnce(self, -1);
    }
}

#if TARGET_PLATFORM_APPLE
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
        struct kevent *kqueueEvent = events + idx;
        Event *event = (Event *)kqueueEvent->udata;

        switch (kqueueEvent->filter) {
            case EVFILT_READ:
                if (event->type == EventTypeServer) {
                    EventLoopHandleServerEvent(self, event);
                } else if (event->type == EventTypeServerPeer) {
                    if ((kqueueEvent->flags & EV_EOF) == EV_EOF) {
                        EventLoopHandleServerPeerDisconnect(self, event);
                    } else {
                        EventLoopHandleServerPeerReadEvent(self, event);
                    }
                } else {
                    LogE(TAG, "Unhandled read event for event %" PRIu16 ", %i", event->id, event->type);
                }

                break;
            case EVFILT_WRITE:
                if (event->type == EventTypeServerPeer) {
                    if ((kqueueEvent->flags & EV_EOF) == EV_EOF) {
                        EventLoopHandleServerPeerDisconnect(self, event);
                    } else {
                        EventLoopHandleServerPeerWriteEvent(self, event);
                    }
                } else {
                    LogE(TAG, "Unhandled write event for event %" PRIu16 ", %i", event->id, event->type);
                }

                break;
            case EVFILT_TIMER:
                EventLoopHandleTimerEvent(self, event);
                break;
            case EVFILT_USER:
                EventLoopHandleUserEvent(self, event);
                break;
            default:
                LogE(TAG, "Unhandled event filter: %i", kqueueEvent->filter);
                break;
        }
    }

    EventLoopDeactivateEvents(self);
}
#elif TARGET_PLATFORM_LINUX
#warning Implement epoll run once
#else
#error Unhandled target platform
#endif

void EventLoopStop(EventLoopRef self) {
    EventLoopTriggerUserEvent(self, INTERNAL_EVENT_ID);
}


// MARK: - Event Management

static void EventLoopDeactivateEvent(EventLoopRef self, EventRef event) {
    if (self->deactivatedEventsCount >= self->deactivatedEventsSize) {
        EventLoopExpandEvents(self, &self->deactivatedEvents, &self->deactivatedEventsSize);
    }

    event->isActive = false;

    self->deactivatedEvents[self->deactivatedEventsCount] = event;
    self->deactivatedEventsCount += 1;
}

static void EventLoopDeactivateEvents(EventLoopRef self) {
    for (size_t idx = 0; idx < self->deactivatedEventsCount; idx++) {
        SAFE_DESTROY(self->deactivatedEvents[idx], EventDestroy);
    }

    if (self->deactivatedEvents != NULL) {
        memset(self->deactivatedEvents, 0, sizeof(EventRef) * self->deactivatedEventsSize);
    }

    self->deactivatedEventsCount = 0;
}

static void EventLoopExpandEvents(EventLoopRef self, EventRef **events, size_t *eventsSize) {
    EventRef *currentEvents = *events;
    size_t currentSize = *eventsSize;

    currentEvents = (EventRef *)realloc(currentEvents, sizeof(EventRef) * (currentSize + EVENTS_STEP));
    memset(currentEvents + currentSize, 0, sizeof(EventRef) * EVENTS_STEP);

    *events = currentEvents;
    *eventsSize += EVENTS_STEP;
}

static Event * EventLoopFindExistingEvent(EventLoopRef self, EventID id, EventType type) {
    EventRef *events;
    size_t size;

    switch (type) {
        case EventTypeServer:
            events = self->serverEvents;
            size = self->serverEventsSize;
            break;
        case EventTypeServerPeer:
            events = self->serverPeerEvents;
            size = self->serverPeerEventsSize;
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
        EventRef event = events[idx];

        if (event != NULL && event->id == id) {
            return event;
        }
    }

    return NULL;
}

static bool EventLoopHasEvent(EventLoopRef self, EventID id, EventType type) {
    EventRef *events;
    size_t size;

    switch (type) {
        case EventTypeServer:
            events = self->serverEvents;
            size = self->serverEventsSize;
            break;
        case EventTypeServerPeer:
            events = self->serverPeerEvents;
            size = self->serverPeerEventsSize;
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
        EventRef event = events[idx];

        if (event != NULL && event->id == id) {
            return true;
        }
    }

    return false;
}


// MARK: - Servers

void EventLoopAddServer(EventLoopRef self, const EventLoopServerDescriptor *descriptor) {
    int fd = -1;
    int flags = 0;
    struct sockaddr_storage address;
    struct sockaddr_in *ipv4Address = NULL;
    int result = 0;
    EventRef event = NULL;

    // Do nothing if the server already exists
    if (EventLoopHasEvent(self, descriptor->id, EventTypeServer)) {
        LogE(TAG, "Server %" PRIu16 " already exists", descriptor->id);
        goto add_server_error_cleanup;
    }

    // Create the server socket
    fd = socket(PF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        LogErrno(TAG, errno, "Failed to create a socket for %" PRIu16, descriptor->id);
        goto add_server_error_cleanup;
    }

    flags = fcntl(fd, F_GETFL, 0);
    result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to make socket non-blocking for %" PRIu16, descriptor->id);
        goto add_server_error_cleanup;
    }

    memset(&address, 0, sizeof(address));
    address.ss_family = AF_INET;

    ipv4Address = (struct sockaddr_in *)&address;
    ipv4Address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ipv4Address->sin_port = htons(descriptor->port);

    result = bind(fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in));

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to bind socket for %" PRIu16, descriptor->id);
        goto add_server_error_cleanup;
    }

    result = listen(fd, SOMAXCONN);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to listen on socket %" PRIu16, descriptor->id);
        goto add_server_error_cleanup;;
    }

    // Build the event
    event = (EventRef)calloc(1, sizeof(Event));
    event->type = EventTypeServer;
    event->id = descriptor->id;
    event->isActive = true;
    event->server.fd = fd;
    event->server.didAccept = descriptor->didAccept;
    event->server.shouldAccept = descriptor->shouldAccept;
    event->server.didReceiveData = descriptor->didReceiveData;
    event->server.peerDidDisconnect = descriptor->peerDidDisconnect;

    // Add the event to kqueue
    struct kevent serverEvent;
    EV_SET(&serverEvent, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, event);

    result = kevent(self->kqueueFD, &serverEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add server %" PRIu16 " to kqueue", descriptor->id);
        goto add_server_error_cleanup;
    }

    // Expand the servers if needed
    if (self->serverEventsCount >= self->serverEventsSize) {
        EventLoopExpandEvents(self, &self->serverEvents, &self->serverEventsSize);
    }

    // Inject the event in to the list
    size_t previousCount = self->serverEventsCount;

    for (size_t idx = 0; idx < self->serverEventsSize; idx++) {
        if (self->serverEvents[idx] == NULL) {
            self->serverEvents[idx] = event;
            self->serverEventsCount += 1;
            break;
        }
    }

    if (previousCount == self->serverEventsCount) {
        LogE(TAG, "Failed to add server event %" PRIu16 " to the list", event->id);
        goto add_server_error_cleanup;
    }

    return;

add_server_error_cleanup:

    SAFE_DESTROY(event, EventDestroy);

    if (fd != -1) {
        close(fd);
    }
}

static void EventLoopHandleServerEvent(EventLoopRef self, Event *event) {
    struct sockaddr_storage remoteAddress;
    socklen_t remoteAddressSize = sizeof(remoteAddress);
    int clientFD = -1;
    int flags = 0;
    bool shouldAccept = true;
    EventID nextID = self->nextID;
    EventRef peerEvent = NULL;
    int result = 0;

    // Accept the connection
    clientFD = accept(event->server.fd, (struct sockaddr *)&remoteAddress, &remoteAddressSize);

    if (clientFD == -1) {
        LogErrno(TAG, errno, "Failed to accept client on server %" PRIu16, event->id);
        goto handle_server_event_error;
    }

    if (event->server.shouldAccept != NULL) {
        shouldAccept = event->server.shouldAccept(self, event->id, (struct sockaddr *)&remoteAddress, self->callbackContext);
    }

    if (!shouldAccept) {
        LogD(TAG, "New client not accepted");
        goto handle_server_event_error;
    }

    flags = fcntl(clientFD, F_GETFL, 0);
    result = fcntl(clientFD, F_SETFL, flags | O_NONBLOCK);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to make peer socket non-blocking for %" PRIu16, event->id);
        goto handle_server_event_error;
    }

    LogD(TAG, "New client on server %" PRIu16, event->id);

    // Determine the next ID to use
    bool idTaken = false;

    while (true) {
        for (size_t idx = 0; idx < self->serverPeerEventsSize; idx++) {
            EventRef current = self->serverPeerEvents[idx];

            if (current != NULL && current->id == nextID) {
                idTaken = true;
                break;
            }
        }

        if (idTaken) {
            nextID += 1;
        } else {
            break;
        }
    }

    // Build the peer
    peerEvent = (EventRef)calloc(1, sizeof(Event));
    peerEvent->type = EventTypeServerPeer;
    peerEvent->id = nextID;
    peerEvent->isActive = true;
    peerEvent->serverPeer.fd = clientFD;
    peerEvent->serverPeer.serverID = event->id;
    peerEvent->serverPeer.didReceiveData = event->server.didReceiveData;
    peerEvent->serverPeer.peerDidDisconnect = event->server.peerDidDisconnect;

    // Add the event to kqueue
    struct kevent serverPeerEvent;
    EV_SET(&serverPeerEvent, clientFD, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, peerEvent);

    result = kevent(self->kqueueFD, &serverPeerEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add server peer %" PRIu16 " of %" PRIu16 " to kqueue", peerEvent->id, event->id);
        goto handle_server_event_error;
    }

    // Expand the servers if needed
    if (self->serverPeerEventsCount >= self->serverPeerEventsSize) {
        EventLoopExpandEvents(self, &self->serverPeerEvents, &self->serverPeerEventsSize);
    }

    // Inject the event in to the list
    size_t previousCount = self->serverPeerEventsCount;

    for (size_t idx = 0; idx < self->serverPeerEventsSize; idx++) {
        if (self->serverPeerEvents[idx] == NULL) {
            self->serverPeerEvents[idx] = peerEvent;
            self->serverPeerEventsCount += 1;
            break;
        }
    }

    if (previousCount == self->serverPeerEventsCount) {
        LogE(TAG, "Failed to add server peer event %" PRIu16 " of %" PRIu16 " to the list", peerEvent->id, event->id);
        goto handle_server_event_error;
    }

    if (event->server.didAccept != NULL) {
        event->server.didAccept(self, event->id, peerEvent->id, (struct sockaddr *)&remoteAddress, self->callbackContext);
    }

    self->nextID = nextID + 1;

    return;

handle_server_event_error:

    SAFE_DESTROY(event, EventDestroy);

    if (clientFD != -1) {
        shutdown(clientFD, SHUT_RDWR);
        close(clientFD);
    }
}

static void EventLoopHandleServerPeerDisconnect(EventLoopRef self, Event *event) {
    LogI(TAG, "Server peer %" PRIu16 "/%" PRIu16 " disconnected", event->id, event->serverPeer.serverID);

    EventRef existingEvent = NULL;

    for (size_t idx = 0; idx < self->serverPeerEventsSize; idx++) {
        if (self->serverPeerEvents[idx] == event) {
            existingEvent = event;

            self->serverPeerEvents[idx] = NULL;
            self->serverPeerEventsCount -= 1;

            break;
        }
    }

    if (existingEvent == NULL ) {
        LogE(TAG, "Failed to find server peer %" PRIu16 " for %" PRIu16 " to disconnect", event->id, event->serverPeer.serverID);
    }

    if (event->serverPeer.peerDidDisconnect != NULL) {
        event->serverPeer.peerDidDisconnect(self, event->serverPeer.serverID, event->id, self->callbackContext);
    }

    EventLoopDeactivateEvent(self, event);
}

static void EventLoopHandleServerPeerReadEvent(EventLoopRef self, Event *event) {
    // Make the receive buffer if it doesn't exist
    if (event->serverPeer.receiveBuffer == NULL) {
        event->serverPeer.receiveBuffer = malloc(RECEIVE_BUFFER_SIZE);
        event->serverPeer.receiveBufferSize = RECEIVE_BUFFER_SIZE;
    }

    // Read the data
    ssize_t bytesRead = read(event->serverPeer.fd, event->serverPeer.receiveBuffer, event->serverPeer.receiveBufferSize);

    if (bytesRead == -1) {
        LogErrno(TAG, errno, "Failed to read from server peer %" PRIu16 " from %" PRIu16, event->id, event->serverPeer.serverID);
    } else if (bytesRead > 0) {
        if (event->serverPeer.didReceiveData != NULL) {
            event->serverPeer.didReceiveData(self, event->serverPeer.serverID, event->id, event->serverPeer.receiveBuffer, (size_t)bytesRead, self->callbackContext);
        }
    } else {
        LogW(TAG, "Read zero bytes from server peer %" PRIu16 "from %" PRIu16, event->id, event->serverPeer.serverID);
    }
}

static void EventLoopHandleServerPeerWriteEvent(EventLoopRef self, Event *event) {

}

bool EventLoopHasServer(EventLoopRef self, EventID id) {
    bool result = EventLoopHasEvent(self, id, EventTypeServer);
    return result;
}

void EventLoopRemoveServer(EventLoopRef self, EventID id) {
    // Remove the server from the list
    EventRef event = NULL;

    for (size_t idx = 0; idx < self->serverEventsSize; idx++) {
        EventRef current = self->serverEvents[idx];

        if (current != NULL && current->id == id) {
            event = current;

            self->serverEvents[idx] = NULL;
            self->serverEventsCount -= 1;

            break;
        }
    }

    if (event == NULL) {
        LogE(TAG, "Cannot remove server %" PRIu16 ", which does not exist", id);
        return;
    }

    // Drop existing clients
    for (size_t idx = 0; idx < self->serverPeerEventsSize; idx++) {
        EventRef current = self->serverPeerEvents[idx];

        if (current != NULL && current->serverPeer.serverID == event->id) {
            shutdown(current->serverPeer.fd, SHUT_RDWR);
            close(current->serverPeer.fd);
        }
    }

    // Remove the server from kqueue
    struct kevent serverEvent;
    EV_SET(&serverEvent, id, EVFILT_READ, EV_DELETE, 0, 0, NULL);

    int result = kevent(self->kqueueFD, &serverEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to remove user event %" PRIu16 " from kqueue", id);
    }

    // Append the server to the list of deactivations
    EventLoopDeactivateEvent(self, event);
}


// MARK: - Timers

void EventLoopAddTimer(EventLoopRef self, EventID id, uint32_t timeout, EventLoopTimerFiredCallback callback) {
    EventRef event = NULL;

    // Do nothing if the timer exists
    if (EventLoopHasEvent(self, id, EventTypeTimer)) {
        LogE(TAG, "Timer %" PRIu16 " already exists", id);
        goto add_timer_error_cleanup;
    }

    // Build the event
    event = (EventRef)calloc(1, sizeof(Event));
    event->type = EventTypeTimer;
    event->id = id;
    event->isActive = true;
    event->timer.timerFired = callback;

    // Add the event to kqueue
    struct kevent timerEvent;
    EV_SET(&timerEvent, id, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_CRITICAL, timeout, event);

    int result = kevent(self->kqueueFD, &timerEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add timer event %" PRIu16 " to kqueue", id);
        goto add_timer_error_cleanup;
    }

    // Expand the events if needed
    if (self->timerEventsCount >= self->timerEventsSize) {
        EventLoopExpandEvents(self, &self->timerEvents, &self->timerEventsSize);
    }

    // Inject the event in to the list
    size_t previousCount = self->timerEventsCount;

    for (size_t idx = 0; idx < self->timerEventsSize; idx++) {
        if (self->timerEvents[idx] == NULL) {
            self->timerEvents[idx] = event;
            self->timerEventsCount += 1;
            break;
        }
    }

    if (previousCount == self->timerEventsCount) {
        LogE(TAG, "Failed to add timer event %" PRIu16 " to the list", event->id);
        goto add_timer_error_cleanup;
    }

    return;

add_timer_error_cleanup:

    SAFE_DESTROY(event, EventDestroy);
}

static void EventLoopHandleTimerEvent(EventLoopRef self, Event *event) {
    // This event may have been dropped, so skip it
    if (!event->isActive) {
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
    // Remove the server from the list
    EventRef event = NULL;

    for (size_t idx = 0; idx < self->timerEventsSize; idx++) {
        EventRef current = self->timerEvents[idx];

        if (current != NULL && current->id == id) {
            event = current;

            self->timerEvents[idx] = NULL;
            self->timerEventsCount -= 1;

            break;
        }
    }

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

    EventLoopDeactivateEvent(self, event);
}


// MARK: - User Events

void EventLoopAddUserEvent(EventLoopRef self, EventID id, EventLoopUserEventFiredCallback callback) {
    EventRef event = NULL;

    // Do nothing if the user event exists
    if (EventLoopHasEvent(self, id, EventTypeUser)) {
        LogE(TAG, "User event %" PRIu16 " already exists", id);
        goto add_user_error_cleanup;
    }

    // Build the event
    event = (EventRef)calloc(1, sizeof(Event));
    event->type = EventTypeUser;
    event->id = id;
    event->isActive = true;
    event->user.userEventFired = callback;

    // Add the event to kqueue
    struct kevent userEvent;
    EV_SET(&userEvent, id, EVFILT_USER, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, event);

    int result = kevent(self->kqueueFD, &userEvent, 1, NULL, 0, NULL);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to add user event %" PRIu16 " to kqueue", id);
        return;
    }

    // Expand the events if needed
    if (self->userEventsCount >= self->userEventsSize) {
        EventLoopExpandEvents(self, &self->userEvents, &self->userEventsSize);
    }

    // Inject the event in to the list
    size_t previousCount = self->userEventsCount;

    for (size_t idx = 0; idx < self->userEventsSize; idx++) {
        if (self->userEvents[idx] == NULL) {
            self->userEvents[idx] = event;
            self->userEventsCount += 1;
            break;
        }
    }

    if (previousCount == self->userEventsCount) {
        LogE(TAG, "Failed to add user event %" PRIu16 " to the list", event->id);
        goto add_user_error_cleanup;
    }

    return;

add_user_error_cleanup:

    SAFE_DESTROY(event, EventDestroy);
}

static void EventLoopHandleStopUserEvent(EventLoopRef self, EventID id, void *context) {
    self->keepRunning = false;
}

static void EventLoopHandleUserEvent(EventLoopRef self, Event *event) {
    // This event may have been dropped, so skip it
    if (!event->isActive) {
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
    // Remove the event from the list
    EventRef event = NULL;

    for (size_t idx = 0; idx < self->userEventsSize; idx++) {
        EventRef current = self->userEvents[idx];

        if (current != NULL && current->id == id) {
            event = current;

            self->userEvents[idx] = NULL;
            self->userEventsCount -= 1;

            break;
        }
    }

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

    EventLoopDeactivateEvent(self, event);
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
