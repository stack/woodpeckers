#include "EventLoop.h"

int main(int argc, char **argv) {
    EventLoopRef eventLoop = EventLoopCreate();

    EventLoopAddTimer(eventLoop, 1, 1000);

    EventLoopRun(eventLoop);

    EventLoopDestroy(eventLoop);
    
    return 0;
}
