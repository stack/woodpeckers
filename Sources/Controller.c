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

#include "EventLoop.h"
#include "Log.h"


// MARK: - Constants & Globals

#define TAG "Controller"

#define DEFAULT_MIN_WAIT 1000
#define DEFAULT_MAX_WAIT 5000
#define DEFAULT_MIN_PECKS 2
#define DEFAULT_MAX_PECKS 4
#define DEFAULT_PECK_WAIT 500

typedef enum _ControllerState {
    ControllerStateInitial = 0,
    ControllerStateStartup,
    ControllerStateWaiting,
    ControllerStatePecking,
} ControllerState;

typedef struct _Controller {
    uint32_t minWait;
    uint32_t maxWait;
    uint32_t minPecks;
    uint32_t maxPecks;
    uint32_t peckWait;

    EventLoopRef eventLoop;
} Controller;


// MARK: - Lifecycle Methods

ControllerRef ControllerCreate() {
    ControllerRef self = (ControllerRef)calloc(1, sizeof(Controller));

    self->minWait = DEFAULT_MIN_WAIT;
    self->maxWait = DEFAULT_MAX_WAIT;
    self->minPecks = DEFAULT_MIN_PECKS;
    self->maxPecks = DEFAULT_MAX_PECKS;
    self->peckWait = DEFAULT_PECK_WAIT;

    self->eventLoop = EventLoopCreate();

    return self;
}

void ControllerDestroy(ControllerRef self) {
    SAFE_DESTROY(self->eventLoop, EventLoopDestroy);

    free(self);
}


// MARK: - Running

void ControllerRun(ControllerRef NONNULL self) {
    // TODO: Switch to start up

    EventLoopRun(self->eventLoop);
}


// MARK: - Properties

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
