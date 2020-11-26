#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "EventLoop.h"

static int firedCount = 0;
static EventLoopRef eventLoop = NULL;

static void TimerFired(EventLoopRef eventLoop, EventID id, void *context) {
    printf("Timer %" PRIu32 " fired\n", id);

    firedCount += 1;

    if (firedCount == 5) {
        EventLoopRemoveTimer(eventLoop, 1);
    }
}

int main(int argc, char **argv) {
    eventLoop = EventLoopCreate();

    EventLoopAddTimer(eventLoop, 1, 1000, TimerFired);

    EventLoopRun(eventLoop);

    EventLoopDestroy(eventLoop);

    return 0;
}
