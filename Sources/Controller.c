//
//  Controller.c
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-12-01.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "Controller.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "EventLoop.h"
#include "Log.h"
#include "Output.h"


// MARK: - Constants & Globals

#define TAG "Controller"

#define DEFAULT_MIN_WAIT 1000
#define DEFAULT_MAX_WAIT 5000
#define DEFAULT_MIN_PECKS 2
#define DEFAULT_MAX_PECKS 4
#define DEFAULT_PECK_WAIT 500

#define STARTUP_WAIT 500

#define INITIAL_TIMER_ID 1
#define PECKING_TIMER_ID 4
#define STARTUP_TIMER_ID 2
#define WAITING_TIMER_ID 3

typedef enum _ControllerState {
    ControllerStateInitial = 0,
    ControllerStateStartup,
    ControllerStateWaiting,
    ControllerStatePecking,
} ControllerState;

typedef struct _Bird {
    char *name;

    // NOTE: These arrays do not out the outputs
    OutputRef *statics;
    size_t totalStatics;

    OutputRef *backs;
    size_t totalBacks;

    OutputRef *forwards;
    size_t totalForwards;
} Bird;

typedef struct _Controller {
    uint32_t minWait;
    uint32_t maxWait;
    uint32_t minPecks;
    uint32_t maxPecks;
    uint32_t peckWait;

    EventLoopRef eventLoop;

    ControllerState state;

    OutputRef *outputs;
    size_t totalOutputs;

    Bird *birds;
    size_t totalBirds;

    size_t startupIndex;
    bool startupValue;

    int32_t pecksRemaining;
    size_t peckingBirdIndex;
    bool peckValue;
} Controller;


// MARK: - Prototypes

static void ControllerChangeState(ControllerRef NONNULL controller, ControllerState newState);

static void ControllerAppendOutput(ControllerRef NONNULL controller, OutputRef NONNULL output);

static void ControllerStartInitialState(ControllerRef NONNULL controller);
static void ControllerStartPeckingState(ControllerRef NONNULL controller);
static void ControllerStartStartupState(ControllerRef NONNULL controller);
static void ControllerStartWaitingState(ControllerRef NONNULL controller);
static void ControllerStopInitialState(ControllerRef NONNULL controller);
static void ControllerStopPeckingState(ControllerRef NONNULL controller);
static void ControllerStopStartupState(ControllerRef NONNULL controller);
static void ControllerStopWaitingState(ControllerRef NONNULL controller);
static void ControllerTimerPeckingFired(EventLoopRef NONNULL eventLoop, EventID id, void * NULLABLE context);
static void ControllerTimerStartupFired(EventLoopRef NONNULL eventLoop, EventID id, void * NULLABLE context);
static void ControllerTimerWaitingFired(EventLoopRef NONNULL eventLoop, EventID id, void * NULLABLE context);

static void ControllerDidAcceptClient(EventLoopRef NONNULL eventLoop, EventID serverID, EventID peerID, struct sockaddr * NONNULL address, void * NULLABLE context);
static void ControllerDidReceiveData(EventLoopRef NONNULL eventLoop, EventID serverID, EventID peerID, const uint8_t *data, size_t dataSize, void * NULLABLE context);
static bool ControllerShouldAcceptClient(EventLoopRef NONNULL eventLoop, EventID id, struct sockaddr * NONNULL address, void * NULLABLE context);

static bool ControllerBirdExists(ControllerRef NONNULL controller, const char * NONNULL name);
static OutputRef NULLABLE ControllerFindOutput(ControllerRef NONNULL controller, const char * NONNULL name);
static bool ControllerOutputExists(ControllerRef NONNULL controller, const char * NONNULL name);
static const char * ControllerStateToString(ControllerState state);


// MARK: - Lifecycle Methods

ControllerRef ControllerCreate() {
    ControllerRef self = (ControllerRef)calloc(1, sizeof(Controller));

    self->minWait = DEFAULT_MIN_WAIT;
    self->maxWait = DEFAULT_MAX_WAIT;
    self->minPecks = DEFAULT_MIN_PECKS;
    self->maxPecks = DEFAULT_MAX_PECKS;
    self->peckWait = DEFAULT_PECK_WAIT;

    self->eventLoop = EventLoopCreate();
    EventLoopSetCallbackContext(self->eventLoop, self);

    return self;
}

void ControllerDestroy(ControllerRef self) {
    SAFE_DESTROY(self->eventLoop, EventLoopDestroy);

    for (size_t idx = 0; idx < self->totalBirds; idx++) {
        SAFE_DESTROY(self->birds[idx].statics, free);
        SAFE_DESTROY(self->birds[idx].backs, free);
        SAFE_DESTROY(self->birds[idx].forwards, free);
        SAFE_DESTROY(self->birds[idx].name, free);
    }

    SAFE_DESTROY(self->birds, free);

    for (size_t idx = 0; idx < self->totalOutputs; idx++) {
        SAFE_DESTROY(self->outputs[idx], OutputDestroy);
    }

    SAFE_DESTROY(self->outputs, free);

    free(self);
}


// MARK: - Running

static void ControllerChangeState(ControllerRef self, ControllerState newState) {
    LogI(TAG, "Changing state from %s to %s", ControllerStateToString(self->state), ControllerStateToString(newState));

    switch (self->state) {
        case ControllerStateInitial:
            ControllerStopInitialState(self);
            break;
        case ControllerStatePecking:
            ControllerStopPeckingState(self);
            break;
        case ControllerStateStartup:
            ControllerStopStartupState(self);
            break;
        case ControllerStateWaiting:
            ControllerStopWaitingState(self);
            break;
    }

    switch (newState) {
        case ControllerStateInitial:
            ControllerStartInitialState(self);
            break;
        case ControllerStatePecking:
            ControllerStartPeckingState(self);
            break;
        case ControllerStateStartup:
            ControllerStartStartupState(self);
            break;
        case ControllerStateWaiting:
            ControllerStartWaitingState(self);
            break;
    }

    self->state = newState;
}

void ControllerRun(ControllerRef self) {
    ControllerChangeState(self, ControllerStateStartup);

    EventLoopRun(self->eventLoop);
}

bool ControllerSetUp(ControllerRef self) {
    srand(time(NULL));

    for (size_t idx = 0; idx < self->totalOutputs; idx++) {
        OutputRef output = self->outputs[idx];

        LogI(TAG, "Setting up output %s", OutputGetName(output));

        bool result = OutputSetUp(output);

        if (!result) {
            return false;
        }
    }

    EventLoopServerDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));

    descriptor.id = 42;
    descriptor.port = 5353;
    descriptor.shouldAccept = ControllerShouldAcceptClient;
    descriptor.didAccept = ControllerDidAcceptClient;
    descriptor.didReceiveData = ControllerDidReceiveData;

    EventLoopAddServer(self->eventLoop, &descriptor);

    return true;
}

void ControllerTearDown(ControllerRef self) {
    for (size_t idx = 0; idx < self->totalOutputs; idx++) {
        OutputRef output = self->outputs[idx];

        LogI(TAG, "Tearing down output %s", OutputGetName(output));

        OutputTearDown(output);
    }
}


// MARK: - Properties Setup

void ControllerSetMinWait(ControllerRef self, uint32_t value) {
    self->minWait = value;
}

void ControllerSetMaxWait(ControllerRef self, uint32_t value) {
    self->maxWait = value;
}

void ControllerSetMinPecks(ControllerRef self, uint32_t value) {
    self->minPecks = value;
}

void ControllerSetMaxPecks(ControllerRef self, uint32_t value) {
    self->maxPecks = value;
}

void ControllerSetPeckWait(ControllerRef self, uint32_t value) {
    self->peckWait = value;
}


// MARK: - Outputs Setup

bool ControllerAddFileOutput(ControllerRef self, const char *name, const char *path) {
    if (ControllerOutputExists(self, name)) {
        LogE(TAG, "Cannot add file output \"%s\" as another output has that name", name);
        return false;
    }

    OutputRef output = OutputCreateFile(name, path);
    ControllerAppendOutput(self, output);

    return true;
}

bool ControllerAddGPIOOutput(ControllerRef self, const char *name, int pin) {
    if (ControllerOutputExists(self, name)) {
        LogE(TAG, "Cannot add GPIO output \"%s\" as another output has that name", name);
        return false;
    }

    OutputRef output = OutputCreateGPIO(name, pin);
    ControllerAppendOutput(self, output);

    return true;
}

bool ControllerAddMemoryOutput(ControllerRef self, const char *name) {
    if (ControllerOutputExists(self, name)) {
        LogE(TAG, "Cannot add Memory output \"%s\" as another output has that name", name);
        return false;
    }

    OutputRef output = OutputCreateMemory(name);
    ControllerAppendOutput(self, output);

    return true;
}

static void ControllerAppendOutput(ControllerRef self, OutputRef output) {
    self->outputs = (OutputRef *)realloc(self->outputs, sizeof(OutputRef) * (self->totalOutputs + 1));
    self->outputs[self->totalOutputs] = output;
    self->totalOutputs += 1;
}


// MARK: - Birds Setup

bool ControllerAddBird(ControllerRef self, const char *name, const char **statics, size_t totalStatics, const char **backs, size_t totalBacks, const char **forwards, size_t totalForwards) {
    if (ControllerBirdExists(self, name)) {
        LogE(TAG, "Cannot add Bird \"%s\" as another bird has that name", name);
        return false;
    }

    self->birds = (Bird *)realloc(self->birds, sizeof(Bird) * (self->totalBirds + 1));
    Bird *bird = self->birds + self->totalBirds;
    self->totalBirds += 1;

    memset(bird, 0, sizeof(Bird));

    bird->name = strdup(name);
    bird->statics = (OutputRef *)calloc(totalStatics, sizeof(OutputRef));
    bird->backs = (OutputRef *)calloc(totalBacks, sizeof(OutputRef));
    bird->forwards = (OutputRef *)calloc(totalForwards, sizeof(OutputRef));

    for (size_t idx = 0; idx < totalStatics; idx++) {
        OutputRef output = ControllerFindOutput(self, statics[idx]);

        if (output == NULL) {
            LogE(TAG, "Cannot add output \"%s\" to bird \"%s\" because it does not exist", statics[idx], name);
            return false;
        }

        bird->statics[bird->totalStatics] = output;
        bird->totalStatics += 1;
    }

    for (size_t idx = 0; idx < totalBacks; idx++) {
        OutputRef output = ControllerFindOutput(self, backs[idx]);

        if (output == NULL) {
            LogE(TAG, "Cannot add output \"%s\" to bird \"%s\" because it does not exist", backs[idx], name);
            return false;
        }

        bird->backs[bird->totalBacks] = output;
        bird->totalBacks += 1;
    }

    for (size_t idx = 0; idx < totalForwards; idx++) {
        OutputRef output = ControllerFindOutput(self, forwards[idx]);

        if (output == NULL) {
            LogE(TAG, "Cannot add output \"%s\" to bird \"%s\" because it does not exist", forwards[idx], name);
            return false;
        }

        bird->forwards[bird->totalForwards] = output;
        bird->totalForwards += 1;
    }

    return true;
}


// MARK: - Running Methods

static void ControllerStartInitialState(ControllerRef self) {
    // Turn off all outputs
    for (size_t idx = 0; idx < self->totalOutputs; idx++) {
        OutputRef output = self->outputs[idx];
        OutputSetValue(output, false);
    }
}

static void ControllerStartPeckingState(ControllerRef self) {
    int range = self->maxPecks - self->minPecks;
    self->pecksRemaining = (rand() % range) + self->minPecks;
    self->peckValue = false;

    EventLoopAddTimer(self->eventLoop, PECKING_TIMER_ID, self->peckWait, ControllerTimerPeckingFired);
}

static void ControllerStartStartupState(ControllerRef self) {
    self->startupIndex = 0;
    self->startupValue = false;

    EventLoopAddTimer(self->eventLoop, STARTUP_TIMER_ID, STARTUP_WAIT, ControllerTimerStartupFired);
}

static void ControllerStartWaitingState(ControllerRef self) {
    uint32_t range = self->maxWait - self->minWait;
    uint32_t waitTime = (rand() % range) + self->minWait;

    LogI(TAG, "Waiting for %" PRIu32 " milliseconds", waitTime);

    EventLoopAddTimer(self->eventLoop, WAITING_TIMER_ID, waitTime, ControllerTimerWaitingFired);
}

static void ControllerStopInitialState(ControllerRef self) {
    // Nothing to do
}

static void ControllerStopPeckingState(ControllerRef self) {
    EventLoopRemoveTimer(self->eventLoop, PECKING_TIMER_ID);
}

static void ControllerStopStartupState(ControllerRef self) {
    EventLoopRemoveTimer(self->eventLoop, STARTUP_TIMER_ID);
}

static void ControllerStopWaitingState(ControllerRef self) {
    EventLoopRemoveTimer(self->eventLoop, WAITING_TIMER_ID);
}

static void ControllerTimerPeckingFired(EventLoopRef eventLoop, EventID id, void *context) {
    ControllerRef self = (ControllerRef)context;

    self->peckValue = !self->peckValue;

    Bird *bird = self->birds + self->peckingBirdIndex;

    for (size_t idx = 0; idx < bird->totalBacks; idx++) {
        OutputRef output = bird->backs[idx];
        OutputSetValue(output, !self->peckValue);
    }

    for (size_t idx = 0; idx < bird->totalForwards; idx++) {
        OutputRef output = bird->forwards[idx];
        OutputSetValue(output, self->peckValue);
    }

    if (!self->peckValue) {
        self->pecksRemaining -= 1;
    }

    if (self->pecksRemaining <= 0) {
        self->peckingBirdIndex = (self->peckingBirdIndex + 1) % self->totalBirds;
        ControllerChangeState(self, ControllerStateWaiting);
    }
}

static void ControllerTimerStartupFired(EventLoopRef eventLoop, EventID id, void *context) {
    ControllerRef self = (ControllerRef)context;

    self->startupValue = !self->startupValue;

    OutputRef output = self->outputs[self->startupIndex];
    OutputSetValue(output, self->startupValue);

    if (!self->startupValue) {
        self->startupIndex += 1;
    }

    if (self->startupIndex >= self->totalOutputs) {
        for (size_t birdIdx = 0; birdIdx < self->totalBirds; birdIdx++) {
            Bird *bird = self->birds + birdIdx;

            for (size_t outputIdx = 0; outputIdx < bird->totalStatics; outputIdx++) {
                output = bird->statics[outputIdx];
                OutputSetValue(output, true);
            }

            for (size_t outputIdx = 0; outputIdx < bird->totalBacks; outputIdx++) {
                output = bird->backs[outputIdx];
                OutputSetValue(output, true);
            }

            for (size_t outputIdx = 0; outputIdx < bird->totalForwards; outputIdx++) {
                output = bird->forwards[outputIdx];
                OutputSetValue(output, false);
            }
        }

        ControllerChangeState(self, ControllerStateWaiting);
    }
}

static void ControllerTimerWaitingFired(EventLoopRef eventLoop, EventID id, void *context) {
    ControllerRef self = (ControllerRef)context;
    
    ControllerChangeState(self, ControllerStatePecking);
}


// MARK: - Remote Server

static void ControllerDidAcceptClient(EventLoopRef eventLoop, EventID serverID, EventID peerID, struct sockaddr *address, void *context) {
    LogI(TAG, "New client connection %" PRIu16 " on %" PRIu16, peerID, serverID);
}

static void ControllerDidReceiveData(EventLoopRef eventLoop, EventID serverID, EventID peerID, const uint8_t *data, size_t dataSize, void * NULLABLE context) {
    LogI(TAG, "Client %" PRIu16 "/%" PRIu16 " received %zu bytes", serverID, peerID, dataSize);
}

static bool ControllerShouldAcceptClient(EventLoopRef eventLoop, EventID id, struct sockaddr *address, void *context) {
    return true;
}


// MARK: - Utilities

static bool ControllerBirdExists(ControllerRef self, const char *name) {
    bool exists = false;

    for (size_t idx = 0; idx < self->totalBirds; idx++) {
        if (strcmp(self->birds[idx].name, name) == 0) {
            exists = true;
            break;
        }
    }

    return exists;
}

static OutputRef ControllerFindOutput(ControllerRef self, const char *name) {
    OutputRef output = NULL;

    for (size_t idx = 0; idx < self->totalOutputs; idx++) {
        const char *outputName = OutputGetName(self->outputs[idx]);

        if (strcmp(outputName, name) == 0) {
            output = self->outputs[idx];
            break;
        }
    }

    return output;
}

static bool ControllerOutputExists(ControllerRef self, const char *name) {
    bool exists = false;

    for (size_t idx = 0; idx < self->totalOutputs; idx++) {
        const char *outputName = OutputGetName(self->outputs[idx]);

        if (strcmp(outputName, name) == 0) {
            exists = true;
            break;
        }
    }

    return exists;
}

static const char * ControllerStateToString(ControllerState state) {
    switch (state) {
        case ControllerStateInitial:
            return "Initial";
            break;
        case ControllerStateStartup:
            return "Startup";
            break;
        case ControllerStateWaiting:
            return "Waiting";
            break;
        case ControllerStatePecking:
            return "Pecking";
            break;
    }

    return "ERROR";
}

