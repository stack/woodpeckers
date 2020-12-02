//
//  Output.c
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-12-01.
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "config.h"

#include "Output.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Log.h"


// MARK: - Constants & Globals

#define TAG "Output"

typedef enum _OutputType {
    OutputTypeMemory,
    OutputTypeFile,
    OutputTypeGPIO,
} OutputType;

typedef struct _Output {
    OutputType type;
    char *name;

    union {
        struct {
            bool value;
        } memory;
        struct {
            char *path;
            FILE *file;
        } file;
        struct {
            int pin;
        } gpio;
    };
} Output;


// MARK: - Prototypes

static OutputRef NONNULL OutputCreate(const char * NONNULL name);
static void OutputDestroyFile(OutputRef NONNULL output);
static void OutputDestroyMemory(OutputRef NONNULL output);
static void OutputDestroyGPIO(OutputRef NONNULL output);

static bool OutputSetUpFile(OutputRef NONNULL output);
static bool OutputSetUpGPIO(OutputRef NONNULL output);
static bool OutputSetUpMemory(OutputRef NONNULL output);
static void OutputTearDownFile(OutputRef NONNULL output);
static void OutputTearDownGPIO(OutputRef NONNULL output);
static void OutputTearDownMemory(OutputRef NONNULL output);

static bool OutputGetValueFile(const OutputRef NONNULL self);
static bool OutputGetValueGPIO(const OutputRef NONNULL self);
static bool OutputGetValueMemory(const OutputRef NONNULL self);

static void OutputSetValueFile(OutputRef NONNULL output, bool value);
static void OutputSetValueGPIO(OutputRef NONNULL output, bool value);
static void OutputSetValueMemory(OutputRef NONNULL output, bool value);


// MARK: - Lifecycle Methods

static OutputRef OutputCreate(const char *name) {
    OutputRef self = (OutputRef)calloc(1, sizeof(Output));

    self->name = strdup(name);

    return self;
}

OutputRef OutputCreateFile(const char *name, const char *path) {
    OutputRef self = OutputCreate(name);

    self->type = OutputTypeFile;
    self->file.path = strdup(path);
    self->file.file = NULL;

    return self;
}

OutputRef OutputCreateGPIO(const char *name, int pin) {
    OutputRef self = OutputCreate(name);

    self->type = OutputTypeGPIO;
    self->gpio.pin = pin;

    return self;
}

OutputRef OutputCreateMemory(const char *name) {
    OutputRef self = OutputCreate(name);

    self->type = OutputTypeMemory;
    self->memory.value = false;

    return self;
}

void OutputDestroy(OutputRef self) {
    switch (self->type) {
        case OutputTypeFile:
            OutputDestroyFile(self);
            break;
        case OutputTypeGPIO:
            OutputDestroyGPIO(self);
            break;
        case OutputTypeMemory:
            OutputDestroyMemory(self);
            break;
    }

    SAFE_DESTROY(self->name, free);

    free(self);
}

static void OutputDestroyFile(OutputRef self) {
    SAFE_DESTROY(self->file.path, free);
    SAFE_DESTROY(self->file.file, fclose);
}

static void OutputDestroyMemory(OutputRef self) {
    // Nothing to do
}

static void OutputDestroyGPIO(OutputRef self) {
    // Nothing to do
}


// MARK: - Set Up & Tear Down

bool OutputSetUp(OutputRef self) {
    switch (self->type) {
        case OutputTypeFile:
            return OutputSetUpFile(self);
            break;
        case OutputTypeGPIO:
            return OutputSetUpGPIO(self);
            break;
        case OutputTypeMemory:
            return OutputSetUpMemory(self);
            break;
    }
}

static bool OutputSetUpFile(OutputRef self) {
    FILE *file = fopen(self->file.path, "w+");

    if (file == NULL) {
        LogErrno(TAG, errno, "Failed to open file output %s at %s", self->name, self->file.path);
        return false;
    }

    self->file.file = file;

    return true;
}

static bool OutputSetUpGPIO(OutputRef self) {
    // TODO: Implement
    return false;
}

static bool OutputSetUpMemory(OutputRef self) {
    self->memory.value = false;
    return true;
}

void OutputTearDown(OutputRef self) {
    switch (self->type) {
        case OutputTypeFile:
            OutputTearDownFile(self);
            break;
        case OutputTypeGPIO:
            OutputTearDownGPIO(self);
            break;
        case OutputTypeMemory:
            OutputTearDownMemory(self);
            break;
    }
}

static void OutputTearDownFile(OutputRef self) {
    SAFE_DESTROY(self->file.file, fclose);
}

static void OutputTearDownGPIO(OutputRef self) {
    // TODO: Implement
}

static void OutputTearDownMemory(OutputRef self) {
    // Nothing to do
}


// MARK: - Properties

const char * OutputGetName(const OutputRef self) {
    return self->name;
}

bool OutputGetValue(const OutputRef self) {
    switch (self->type) {
        case OutputTypeFile:
            return OutputGetValueFile(self);
            break;
        case OutputTypeGPIO:
            return OutputGetValueGPIO(self);
            break;
        case OutputTypeMemory:
            return OutputGetValueMemory(self);
            break;
    }
}

static bool OutputGetValueFile(const OutputRef self) {
    int result = fseek(self->file.file, 0, SEEK_SET);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to seek file output %s for reading", self->name);
        return false;
    }

    char buffer;
    size_t bytesRead = fread(&buffer, sizeof(char), 1, self->file.file);

    if (bytesRead != 1) {
        return false;
    } else {
        return buffer == '1';
    }
}

static bool OutputGetValueGPIO(const OutputRef self) {
    // TODO: Implement
    return false;
}

static bool OutputGetValueMemory(const OutputRef self) {
    return self->memory.value;
}

void OutputSetValue(OutputRef self, bool value) {
    switch (self->type) {
        case OutputTypeFile:
            OutputSetValueFile(self, value);
            break;
        case OutputTypeGPIO:
            OutputSetValueGPIO(self, value);
            break;
        case OutputTypeMemory:
            OutputSetValueMemory(self, value);
            break;
    }
}

static void OutputSetValueFile(OutputRef self, bool value) {
    int result = fseek(self->file.file, 0, SEEK_SET);

    if (result == -1) {
        LogErrno(TAG, errno, "Failed to seek file output %s for writing", self->name);
        return;
    }

    char buffer = value ? '1' : '0';
    ssize_t bytesWritten = fwrite(&buffer, sizeof(char), 1, self->file.file);

    if (bytesWritten != 1) {
        LogErrno(TAG, errno, "Failed to write value to file output %s", self->name);
    }
}

static void OutputSetValueGPIO(OutputRef self, bool value) {
    // TODO: Imeplement
}

static void OutputSetValueMemory(OutputRef self, bool value) {
    self->memory.value = value;
}
