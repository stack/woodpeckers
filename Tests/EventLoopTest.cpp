//
//  EventLoopTest.cpp
//  Woodpeckers Tests
//
//  Created by Stephen H. Gerstacker on 2020-11-30.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>

#include <EventLoop.h>
#include <Log.h>

class EventLoopTest : public ::testing::Test {

    protected:

    static void LogMessage(LogLevel level, const char *tag, const char *message) {
        std::cerr << "[          ] [" << tag << "/" << message << std::endl;
    }

    static void TimerFired(EventLoopRef eventLoop, EventID id, void *context) {
        EventLoopTest *thiz = reinterpret_cast<EventLoopTest *>(context);
        thiz->timerCounter += 1;
    }

    static void TimerFiredRepeatedly(EventLoopRef eventLoop, EventID, void *context) {
        EventLoopTest *thiz = reinterpret_cast<EventLoopTest *>(context);
        thiz->timerCounter += 1;

        if (thiz->timerCounter >= 5) {
            EventLoopStop(eventLoop);
        }
    }

    void SetUp() override {
        eventLoop = EventLoopCreate();

        LogEnableCallbackOutput(true, LogMessage);
        LogEnableConsoleOutput(false);
        LogEnableSystemOutput(false);
    }

    void TearDown() override {
        SAFE_DESTROY(eventLoop, EventLoopDestroy);
    }

    EventLoopRef eventLoop;
    uint32_t timerCounter;

};

TEST_F(EventLoopTest, TimesOut) {
    auto start = std::chrono::high_resolution_clock::now();
    EventLoopRunOnce(eventLoop, 250);
    auto end = std::chrono::high_resolution_clock::now();

    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    auto diffMs = diff.count();

    ASSERT_GE(diffMs, 250);
}

TEST_F(EventLoopTest, RegistersTimers) {
    bool result = EventLoopHasTimer(eventLoop, 1);
    ASSERT_FALSE(result);

    EventLoopAddTimer(eventLoop, 1, 250, NULL);
    result = EventLoopHasTimer(eventLoop, 1);
    ASSERT_TRUE(result);

    EventLoopRemoveTimer(eventLoop, 1);

    // Timer's need to run their loop once to process removals
    EventLoopRunOnce(eventLoop, 0);

    result = EventLoopHasTimer(eventLoop, 1);
    ASSERT_FALSE(result);
}

TEST_F(EventLoopTest, RegistersUserEvents) {
    bool result = EventLoopHasUserEvent(eventLoop, 2);
    ASSERT_FALSE(result);

    EventLoopAddUserEvent(eventLoop, 2, NULL);
    result = EventLoopHasUserEvent(eventLoop, 2);
    ASSERT_TRUE(result);

    EventLoopRemoveUserEvent(eventLoop, 2);

    // User events need to run their loop once to process removals
    EventLoopRunOnce(eventLoop, 0);

    result = EventLoopHasUserEvent(eventLoop, 2);
    ASSERT_FALSE(result);
}

TEST_F(EventLoopTest, TimersFireOnce) {
    timerCounter = 0;

    EventLoopSetCallbackContext(eventLoop, reinterpret_cast<void *>(this));
    EventLoopAddTimer(eventLoop, 1, 100, TimerFired);

    EventLoopRunOnce(eventLoop, 200);

    ASSERT_EQ(timerCounter, 1);
}

TEST_F(EventLoopTest, TimersFireRepeatedly) {
    timerCounter = 0;

    EventLoopSetCallbackContext(eventLoop, reinterpret_cast<void *>(this));
    EventLoopAddTimer(eventLoop, 1, 100, TimerFiredRepeatedly);

    EventLoopRun(eventLoop);

    ASSERT_EQ(timerCounter, 5);
}