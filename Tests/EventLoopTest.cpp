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
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <EventLoop.h>
#include <Log.h>


class EventLoopTest : public ::testing::Test {

    protected:

    static void LogMessage(LogLevel level, const char *tag, const char *message) {
        std::cerr << "[          ] [" << tag << "/" << message << std::endl;
    }

    static void ServerDidAccept(EventLoopRef self, EventID serverID, EventID peerID, struct sockaddr *address, void *context) {
        EventLoopTest *thiz = reinterpret_cast<EventLoopTest *>(context);

        memcpy(&thiz->lastAcceptAddress, address, address->sa_len);
        thiz->lastAcceptID = serverID;
        thiz->lastAcceptPeerID = peerID;
    }

    static void ServerDidReceiveData(EventLoopRef eventLoop, EventID serverID, EventID peerID, const uint8_t *data, size_t size, void *context) {
        EventLoopTest *thiz = reinterpret_cast<EventLoopTest *>(context);

        thiz->lastReceivedData = reinterpret_cast<uint8_t *>(realloc(thiz->lastReceivedData, size));
        memcpy(thiz->lastReceivedData, data, size);

        thiz->lastReceivedDataSize = size;
    }

    static void ServerPeerDidDisconnect(EventLoopRef eventLoop, EventID serverID, EventID peerID, void *context) {
        EventLoopTest *thiz = reinterpret_cast<EventLoopTest *>(context);
        thiz->lastDisconnectID = serverID;
        thiz->lastDisconnectPeerID = peerID;
    }

    static bool ServerShouldAccept(EventLoopRef self, EventID id, struct sockaddr *address, void *context) {
        EventLoopTest *thiz = reinterpret_cast<EventLoopTest *>(context);

        memcpy(&thiz->lastAcceptAddress, address, address->sa_len);
        thiz->lastAcceptID = id;

        return thiz->serverShouldAccept;
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

    static void UserFired(EventLoopRef eventLoop, EventID id, void *context) {
        EventLoopTest *thiz = reinterpret_cast<EventLoopTest *>(context);
        thiz->userCounter += 1;
    }

    void SetUp() override {
        eventLoop = EventLoopCreate();
        EventLoopSetCallbackContext(eventLoop, reinterpret_cast<void *>(this));

        LogEnableCallbackOutput(true, LogMessage);
        LogEnableConsoleOutput(false);
        LogEnableSystemOutput(false);

        lastReceivedData = nullptr;
    }

    void TearDown() override {
        SAFE_DESTROY(eventLoop, EventLoopDestroy);
        SAFE_DESTROY(lastReceivedData, free);
    }

    protected:

    EventLoopRef eventLoop;
    bool serverShouldAccept;
    uint32_t timerCounter;
    uint32_t userCounter;

    struct sockaddr_storage lastAcceptAddress;
    EventID lastAcceptID;
    EventID lastAcceptPeerID;

    EventID lastDisconnectID;
    EventID lastDisconnectPeerID;

    uint8_t *lastReceivedData;
    size_t lastReceivedDataSize;
};

TEST_F(EventLoopTest, TimesOut) {
    auto start = std::chrono::high_resolution_clock::now();
    EventLoopRunOnce(eventLoop, 250);
    auto end = std::chrono::high_resolution_clock::now();

    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    auto diffMs = diff.count();

    ASSERT_GE(diffMs, 250);
}

TEST_F(EventLoopTest, RegistersServers) {
    bool result = EventLoopHasServer(eventLoop, 1);
    ASSERT_FALSE(result);

    EventLoopServerDescriptor descriptor = {};
    descriptor.id = 1;
    descriptor.port = 5353;

    EventLoopAddServer(eventLoop, &descriptor);
    result = EventLoopHasServer(eventLoop, 1);
    ASSERT_TRUE(result);

    EventLoopRemoveServer(eventLoop, 1);

    // Server's need to run their loop once to process removals
    EventLoopRunOnce(eventLoop, 250);

    result = EventLoopHasServer(eventLoop, 1);
    ASSERT_FALSE(result);
}

TEST_F(EventLoopTest, ServersAskForAcceptance) {
    serverShouldAccept = true;
    lastAcceptID = 0;

    EventLoopServerDescriptor descriptor = {};
    descriptor.id = 1;
    descriptor.port = 5354;
    descriptor.shouldAccept = ServerShouldAccept;

    EventLoopAddServer(eventLoop, &descriptor);

    std::thread thread([&]() {
        EventLoopRunOnce(eventLoop, 1000);
    });

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr.s_addr);
    address.sin_family = AF_INET;
    address.sin_port = htons(5354);

    int result = connect(fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    ASSERT_NE(result, -1);

    close(fd);

    thread.join();

    ASSERT_EQ(lastAcceptID, 1);
}

TEST_F(EventLoopTest, ServersDidAccept) {
    lastAcceptID = 0;
    lastAcceptPeerID = UINT16_MAX;

    EventLoopServerDescriptor descriptor = {};
    descriptor.id = 1;
    descriptor.port = 5355;
    descriptor.didAccept = ServerDidAccept;

    EventLoopAddServer(eventLoop, &descriptor);

    std::thread thread([&]() {
        EventLoopRunOnce(eventLoop, 1000);
    });

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr.s_addr);
    address.sin_family = AF_INET;
    address.sin_port = htons(5355);

    int result = connect(fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    ASSERT_NE(result, -1);

    close(fd);

    thread.join();

    ASSERT_EQ(lastAcceptID, 1);
    ASSERT_NE(lastAcceptPeerID, UINT16_MAX);
}

TEST_F(EventLoopTest, ServerReceivesData) {
    lastReceivedData = NULL;
    lastReceivedDataSize  = 0;

    EventLoopServerDescriptor descriptor = {};
    descriptor.id = 1;
    descriptor.port = 5356;
    descriptor.didReceiveData = ServerDidReceiveData;

    EventLoopAddServer(eventLoop, &descriptor);

    std::thread thread([&]() {
        // Must run twice for accept and data
        EventLoopRunOnce(eventLoop, 1000);
        EventLoopRunOnce(eventLoop, 1000);
    });

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr.s_addr);
    address.sin_family = AF_INET;
    address.sin_port = htons(5356);

    int result = connect(fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    ASSERT_NE(result, -1);

    uint8_t data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    size_t dataSize = sizeof(data);

    ssize_t bytesWritten = write(fd, data, dataSize);
    ASSERT_EQ(bytesWritten, 10);

    thread.join();

    ASSERT_EQ(lastReceivedDataSize, 10);

    for (size_t idx = 0; idx < 10; idx++) {
        ASSERT_EQ(lastReceivedData[idx], static_cast<uint8_t>(idx));
    }

    close(fd);
}

TEST_F(EventLoopTest, ServerReceivesDisconnectPeer) {
    lastDisconnectID = UINT16_MAX;
    lastDisconnectPeerID = UINT16_MAX;

    EventLoopServerDescriptor descriptor = {};
    descriptor.id = 1;
    descriptor.port = 5357;
    descriptor.peerDidDisconnect = ServerPeerDidDisconnect;

    EventLoopAddServer(eventLoop, &descriptor);

    std::thread thread([&]() {
        // Must run twice for accept and data
        EventLoopRunOnce(eventLoop, 1000);
        EventLoopRunOnce(eventLoop, 1000);
    });

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr.s_addr);
    address.sin_family = AF_INET;
    address.sin_port = htons(5357);

    int result = connect(fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    ASSERT_NE(result, -1);

    shutdown(fd, SHUT_RDWR);
    close(fd);

    thread.join();

    ASSERT_EQ(lastDisconnectID, 1);
    ASSERT_NE(lastDisconnectPeerID, UINT16_MAX);
}

TEST_F(EventLoopTest, RegistersTimers) {
    bool result = EventLoopHasTimer(eventLoop, 1);
    ASSERT_FALSE(result);

    EventLoopAddTimer(eventLoop, 1, 250, NULL);
    result = EventLoopHasTimer(eventLoop, 1);
    ASSERT_TRUE(result);

    EventLoopRemoveTimer(eventLoop, 1);

    // Timer's need to run their loop once to process removals
    EventLoopRunOnce(eventLoop, 250);

    result = EventLoopHasTimer(eventLoop, 1);
    ASSERT_FALSE(result);
}

TEST_F(EventLoopTest, TimersFireOnce) {
    timerCounter = 0;

    EventLoopAddTimer(eventLoop, 1, 100, TimerFired);

    EventLoopRunOnce(eventLoop, 200);

    ASSERT_EQ(timerCounter, 1);
}

TEST_F(EventLoopTest, TimersFireRepeatedly) {
    timerCounter = 0;

    EventLoopAddTimer(eventLoop, 1, 100, TimerFiredRepeatedly);

    EventLoopRun(eventLoop);

    ASSERT_EQ(timerCounter, 5);
}

TEST_F(EventLoopTest, RegistersUserEvents) {
    bool result = EventLoopHasUserEvent(eventLoop, 2);
    ASSERT_FALSE(result);

    EventLoopAddUserEvent(eventLoop, 2, NULL);
    result = EventLoopHasUserEvent(eventLoop, 2);
    ASSERT_TRUE(result);

    EventLoopRemoveUserEvent(eventLoop, 2);

    // User events need to run their loop once to process removals
    EventLoopRunOnce(eventLoop, 250);

    result = EventLoopHasUserEvent(eventLoop, 2);
    ASSERT_FALSE(result);
}

TEST_F(EventLoopTest, UserFires) {
    userCounter = 0;

    EventLoopAddUserEvent(eventLoop, 1, UserFired);

    EventLoopTriggerUserEvent(eventLoop, 1);

    EventLoopRunOnce(eventLoop, 200);

    ASSERT_EQ(userCounter, 1);
}