//
//  Configuration.c
//  Woodpeckers
//
//  Created by Stephen H. Gerstacker on 2020-11-26
//  Copyright © 2020 Stephen H. Gerstacker. All rights reserved.
//

#include "Configuration.h"

#include <errno.h>
#include <stdbool.h>

#include <yaml.h>

#include "Log.h"


// MARK: - Constants & Globals

#define TAG "Configuration"

static bool DumpParseEvents = false;

typedef struct _ConfigurationBird {
    char *name;

    char **statics;
    size_t totalStatics;

    char **backs;
    size_t totalBacks;

    char **forwards;
    size_t totalForwards;
} ConfigurationBird;

typedef struct _ConfigurationOutput {
    char *name;
    ConfigurationOutputType type;

    union {
        struct {
            char *path;
        } file;

        struct {
            int pin;
        } gpio;
    };
} ConfigurationOutput;

typedef struct _Configuration {
    uint32_t minWait;
    uint32_t maxWait;
    uint32_t minPecks;
    uint32_t maxPecks;
    uint32_t peckWait;

    ConfigurationOutput *outputs;
    size_t totalOutputs;

    ConfigurationBird *birds;
    size_t totalBirds;
} Configuration;

typedef enum _ScalarKey {
    ScalarKeyNone = 0,
    ScalarKeyMinWait,
    ScalarKeyMaxWait,
    ScalarKeyMinPecks,
    ScalarKeyMaxPecks,
    ScalarKeyPeckWait,
    ScalarKeyType,
    ScalarKeyPath,
    ScalarKeyPin,
    ScalarKeyStatic,
    ScalarKeyBack,
    ScalarKeyForward,
} ScalarKey;

typedef enum _Section {
    SectionNone = 0,
    SectionSettings,
    SectionOutputs,
    SectionBirds
} Section;

typedef struct _ParsingContext {
    Section section;
    ScalarKey scalarKey;

    ConfigurationOutput output;
    bool isInOutput;

    ConfigurationBird bird;
    bool isInBird;
} ParsingContext;


// MARK: - Prototypes

static NONNULL ConfigurationRef ConfigurationCreateDefaults(void);
static bool ConfigurationParse(ConfigurationRef self, yaml_parser_t * NONNULL parser);
static bool ConfigurationParseFromFile(ConfigurationRef self, const char * NONNULL path);
static bool ConfigurationParseFromString(ConfigurationRef self, const char * NONNULL value);

static bool ConfigurationParseBirds(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseBirdsMappingEnd(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseBirdsScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseBirdsSequenceEnd(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);

static bool ConfigurationParseNoSection(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseNoSectionScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);

static bool ConfigurationParseOutput(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseOutputMappingEnd(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseOutputMappingStart(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseOutputScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);

static bool ConfigurationParseSettings(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseSettingsMappingEnd(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseSettingsScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);

static void ConfigurationBirdDestroy(ConfigurationBird * NONNULL bird);
static void ConfigurationBirdReset(ConfigurationBird * NONNULL bird);
static void ConfigurationOutputDestroy(ConfigurationOutput * NONNULL output);
static void ConfigurationOutputReset(ConfigurationOutput * NONNULL output);


// MARK: - Lifecycle Methods

ConfigurationRef NULLABLE ConfigurationCreate(void) {
    ConfigurationRef self = ConfigurationCreateDefaults();

    return self;
}

static ConfigurationRef ConfigurationCreateDefaults() {
    ConfigurationRef self = (ConfigurationRef)calloc(1, sizeof(Configuration));

    self->minWait = 1000;
    self->maxWait = 4000;
    self->minPecks = 1;
    self->maxPecks = 3;
    self->peckWait = 500;

    return self;
}

ConfigurationRef ConfigurationCreateFromFile(const char *path) {
    ConfigurationRef self = ConfigurationCreateDefaults();
    
    bool success = ConfigurationParseFromFile(self, path);

    if (!success) {
        SAFE_DESTROY(self, ConfigurationDestroy);
    }

    return self;
}

ConfigurationRef ConfigurationCreateFromString(const char *value) {
    ConfigurationRef self = ConfigurationCreateDefaults();

    bool success = ConfigurationParseFromString(self, value);

    if (!success) {
        SAFE_DESTROY(self, ConfigurationDestroy);
    }

    return self;
}

void ConfigurationDestroy(ConfigurationRef self) {
    ConfigurationOutput *tempOutputs = self->outputs;
    size_t tempTotalOutputs = self->totalOutputs;

    self->outputs = NULL;
    self->totalOutputs = 0;

    for (size_t idx = 0; idx < tempTotalOutputs; idx++) {
        ConfigurationOutputDestroy(&tempOutputs[idx]);
    }

    SAFE_DESTROY(tempOutputs, free);

    ConfigurationBird *tempBirds = self->birds;
    size_t tempTotalBirds = self->totalBirds;

    self->birds = NULL;
    self->totalBirds = 0;

    for (size_t idx = 0; idx < tempTotalBirds; idx++) {
        ConfigurationBirdDestroy(&tempBirds[idx]);
    }

    SAFE_DESTROY(tempBirds, free);

    free(self);
}


// MARK: - Parsing

static bool ConfigurationParse(ConfigurationRef self, yaml_parser_t *parser) {
    bool isDone = false;
    bool success = false;

    yaml_event_t event;

    ParsingContext context;
    memset(&context, 0, sizeof(ParsingContext));

    while (!isDone) {
        int result = yaml_parser_parse(parser, &event);

        if (result == 0) {
            break;
        }

        if (DumpParseEvents) {
            switch (event.type) {
                case YAML_NO_EVENT:
                    LogD(TAG, "Event: None");
                    break;
                case YAML_STREAM_START_EVENT:
                    LogD(TAG, "Event: Stream Start");
                    break;
                case YAML_STREAM_END_EVENT:
                    LogD(TAG, "Event: Stream End");
                    break;
                case YAML_DOCUMENT_START_EVENT:
                    LogD(TAG, "Event: Document Start");
                    break;
                case YAML_DOCUMENT_END_EVENT:
                    LogD(TAG, "Event: Document End");
                    break;
                case YAML_ALIAS_EVENT:
                    LogD(TAG, "Event: Alias");
                    break;
                case YAML_SCALAR_EVENT:
                    LogD(TAG, "Event: Scalar: %s, %s, %s", event.data.scalar.anchor, event.data.scalar.tag, event.data.scalar.value);
                    break;
                case YAML_SEQUENCE_START_EVENT:
                    LogD(TAG, "Event: Sequence Start");
                    break;
                case YAML_SEQUENCE_END_EVENT:
                    LogD(TAG, "Event: Sequence End");
                    break;
                case YAML_MAPPING_START_EVENT:
                    LogD(TAG, "Event: Mapping Start");
                    break;
                case YAML_MAPPING_END_EVENT:
                    LogD(TAG, "Event: Mapping End");
                    break;
            }
        }

        switch (context.section) {
            case SectionNone:
                isDone = !ConfigurationParseNoSection(self, &event, &context);
                break;
            case SectionSettings:
                isDone = !ConfigurationParseSettings(self, &event, &context);
                break;
            case SectionOutputs:
                isDone = !ConfigurationParseOutput(self, &event, &context);
                break;
            case SectionBirds:
                isDone = !ConfigurationParseBirds(self, &event, &context);
                break;
        }

        if (event.type == YAML_STREAM_END_EVENT) {
            isDone = true;
            success = true;
        }

        yaml_event_delete(&event);
    }

    ConfigurationOutputDestroy(&context.output);
    ConfigurationBirdDestroy(&context.bird);

    return success;
} 

static bool ConfigurationParseFromFile(ConfigurationRef self, const char *path) {
    bool success = false;

    FILE *file = fopen(path, "r");

    if (file == NULL) {
        LogErrno(TAG, errno, "Failed to open configuration file for reading");
        goto parse_from_file_cleanup;
    }

    yaml_parser_t parser;
    yaml_parser_initialize(&parser);

    yaml_parser_set_input_file(&parser, file);

    success = ConfigurationParse(self, &parser);
    
parse_from_file_cleanup:

    SAFE_DESTROY(file, fclose);
    yaml_parser_delete(&parser);

    return success;
}

static bool ConfigurationParseFromString(ConfigurationRef self, const char *value) {
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);

    yaml_parser_set_input_string(&parser, (const unsigned char *)value, strlen(value));

    bool success = ConfigurationParse(self, &parser);

    yaml_parser_delete(&parser);

    return success;
}

static bool ConfigurationParseBirds(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    switch (event->type) {
        case YAML_MAPPING_END_EVENT:
            return ConfigurationParseBirdsMappingEnd(self, event, context);
            break;
        case YAML_SCALAR_EVENT:
            return ConfigurationParseBirdsScalar(self, event, context);
            break;
        case YAML_SEQUENCE_END_EVENT:
            return ConfigurationParseBirdsSequenceEnd(self, event, context);
            break;
        case YAML_MAPPING_START_EVENT:
        case YAML_SEQUENCE_START_EVENT:
            // NOTE: Nothing to do with these events
            return true;
            break;
        default:
            LogE(TAG, "Invalid event %i in Bird section", event->type);
            return false;
            break;
    }
}

static bool ConfigurationParseBirdsMappingEnd(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    // Ignore if we are not in a bird, we're at the end of the section
    if (!context->isInBird) {
        context->section = SectionNone;
        return true;
    }

    // Validate the bird
    if (context->bird.name == NULL) {
        LogE(TAG, "Bird is missing a name");
        return false;
    }

    // Copy the bird in to place
    self->birds = (ConfigurationBird *)realloc(self->birds, sizeof(ConfigurationBird) * (self->totalBirds + 1));
    memcpy(self->birds + self->totalBirds, &context->bird, sizeof(ConfigurationBird));
    self->totalBirds += 1;

    // Clean up
    ConfigurationBirdReset(&context->bird);
    context->isInBird = false;

    return true;
}

static bool ConfigurationParseBirdsScalar(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    bool success = false;

    const char *value = (const char *)event->data.scalar.value;
    size_t valueSize = event->data.scalar.length;

    if (context->bird.name == NULL) { // If we have no scalar key, we're in the name portion
        context->bird.name = strndup(value, valueSize);
        context->isInBird = true;
        success = true;
    } else if (valueSize == 0) { // Empty scalars come after the name
        success = true;
    } else if (context->scalarKey == ScalarKeyNone) {
        if (strcmp(value, "Static") == 0) {
            context->scalarKey = ScalarKeyStatic;
            success = true;
        } else if (strcmp(value, "Back") == 0) {
            context->scalarKey = ScalarKeyBack;
            success = true;
        } else if (strcmp(value, "Forward") == 0) {
            context->scalarKey = ScalarKeyForward;
            success = true;
        } else {
            LogE(TAG, "Invalid Bird section: %s", value);
        } 
    } else {
        switch (context->scalarKey) {
            case ScalarKeyStatic:
                context->bird.statics = (char **)realloc(context->bird.statics, sizeof(char *) * (context->bird.totalStatics + 1));
                context->bird.statics[context->bird.totalStatics] = strndup(value, valueSize);
                context->bird.totalStatics += 1;
                success = true;
                break;
            case ScalarKeyBack:
                context->bird.backs = (char **)realloc(context->bird.backs, sizeof(char *) * (context->bird.totalBacks + 1));
                context->bird.backs[context->bird.totalBacks] = strndup(value, valueSize);
                context->bird.totalBacks += 1;
                success = true;
                break;
            case ScalarKeyForward:
                context->bird.forwards = (char **)realloc(context->bird.forwards, sizeof(char *) * (context->bird.totalForwards + 1));
                context->bird.forwards[context->bird.totalForwards] = strndup(value, valueSize);
                context->bird.totalForwards += 1;
                success = true;
                break;
            default:
                LogE(TAG, "Invalid scalar key in Bird: %i", context->scalarKey);
                break;
        }
    }

    return success;
}

static bool ConfigurationParseBirdsSequenceEnd(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    if (context->scalarKey != ScalarKeyNone) { // If we were in a scalar key, break out
        context->scalarKey = ScalarKeyNone;
    }

    return true;
}

static bool ConfigurationParseNoSection(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    switch (event->type) {
        case YAML_SCALAR_EVENT:
            return ConfigurationParseNoSectionScalar(self, event, context);
            break;
        case YAML_MAPPING_END_EVENT:
        case YAML_MAPPING_START_EVENT:
        case YAML_STREAM_END_EVENT:
        case YAML_STREAM_START_EVENT:
        case YAML_DOCUMENT_END_EVENT:
        case YAML_DOCUMENT_START_EVENT:
            // NOTE: Nothing to do with these events
            return true;
            break;
        default:
            LogE(TAG, "Invalid event %i without a section", event->type);
            return false;
            break;
    }
}

static bool ConfigurationParseNoSectionScalar(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    bool success;

    const char *value = (const char *)event->data.scalar.value;
    size_t valueSize = event->data.scalar.length;

    if (valueSize == 0) {
        // Empty scalars are possible. Do nothing.
        success = true;
    } else  if (strcmp(value, "Settings") == 0) {
        context->section = SectionSettings;
        success = true;
    } else if  (strcmp(value, "Outputs") == 0) {
        context->section = SectionOutputs;
        success = true;
    } else if (strcmp(value, "Birds") == 0) {
        context->section = SectionBirds;
        success = true;
    } else {
        LogE(TAG, "Invalid section name: %s", value);
        success = false;
    }

    return success;
}

static bool ConfigurationParseOutput(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    switch (event->type) {
        case YAML_SCALAR_EVENT:
            return ConfigurationParseOutputScalar(self, event, context);
            break;
        case YAML_MAPPING_END_EVENT:
            return ConfigurationParseOutputMappingEnd(self, event, context);
            break;
        case YAML_MAPPING_START_EVENT:
            return ConfigurationParseOutputMappingStart(self, event, context);
            break;
        case YAML_SEQUENCE_END_EVENT:
        case YAML_SEQUENCE_START_EVENT:
            // NOTE: Nothing to do with these events
            return true;
            break;
        default:
            LogE(TAG, "Invalid event %i in Output section", event->type);
            return false;
            break;
    }
}

static bool ConfigurationParseOutputMappingEnd(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context) {
    // If we ended outside of an output, we end the section
    if (!context->isInOutput) {
        context->section = SectionNone;
        return true;
    }

    // Validate the data
    ConfigurationOutput *output = &context->output;
    if (output->name == NULL) {
        LogE(TAG, "Output processed without a name");
        return false;
    } else if (output->type == ConfigurationOutputTypeUnknown) {
        LogE(TAG, "Output processed without a type");
        return false;
    } else if (output->type == ConfigurationOutputTypeMemory) {
        // Nothing to validate here
    } else if (output->type == ConfigurationOutputTypeFile) {
        if (output->file.path == NULL) {
            LogE(TAG, "File output processed without a path");
            return false;
        }
    } else if (output->type == ConfigurationOutputTypeGPIO) {
        if (output->gpio.pin == -1) {
            LogE(TAG, "GPIO output processed without a pin");
            return false;
        }
    }

    // Add the output to the list
    self->outputs = (ConfigurationOutput *)realloc(self->outputs, sizeof(ConfigurationOutput) * (self->totalOutputs + 1));
    memcpy(self->outputs + self->totalOutputs, output, sizeof(ConfigurationOutput));
    self->totalOutputs += 1;

    // Clean up
    ConfigurationOutputReset(output);
    context->isInOutput = false;

    return true;
}

static bool ConfigurationParseOutputMappingStart(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context) {
    // Reset the scratch output for more parsing
    ConfigurationOutputReset(&context->output);

    // Reset the parsing state
    context->scalarKey = ScalarKeyNone;

    // Flag that we are in an output mapping
    context->isInOutput = true;

    return true;
}

static bool ConfigurationParseOutputScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context) {
    bool success = false;

    const char *value = (const char *)event->data.scalar.value;
    size_t valueSize = event->data.scalar.length;

    if (context->output.name == NULL) { // The first scalar should be the name
        context->output.name = strndup(value, valueSize);
        success = true;
    } else if (context->scalarKey == ScalarKeyNone && valueSize == 0 ) { // A blank scalar comes after the name
        success = true;
    } else if (context->scalarKey == ScalarKeyNone) {
        if (strcmp(value, "Type") == 0) {
            context->scalarKey = ScalarKeyType;
            success = true;
        } else if (strcmp(value, "Path") == 0) {
            context->scalarKey = ScalarKeyPath;
            success = true;
        } else if (strcmp(value, "Pin") == 0) {
            context->scalarKey = ScalarKeyPin;
            success = true;
        } else {
            LogE(TAG, "Unhandled output scalar key: %s", value);
        }
    } else {
        switch (context->scalarKey) {
            case ScalarKeyType:
                if (valueSize == 0) {
                    LogE(TAG, "Empty output scalar value");
                } else if (strcmp(value, "Memory") == 0) {
                    context->output.type = ConfigurationOutputTypeMemory;
                    success = true;
                } else if (strcmp(value, "File") == 0) {
                    context->output.type = ConfigurationOutputTypeFile;
                    context->output.file.path = NULL;
                    success = true;
                } else if (strcmp(value, "GPIO") == 0) {
                    context->output.type = ConfigurationOutputTypeGPIO;
                    context->output.gpio.pin = -1;
                    success = true;
                } else {
                    LogE(TAG, "Unhandled output type: %s", value);
                }

                break;
            case ScalarKeyPath:
                context->output.file.path = strndup(value, valueSize);
                success = true;
                break;
            case ScalarKeyPin:
                context->output.gpio.pin = strtol(value, NULL, 10);
                success = true;
                break;
            default:
                LogE(TAG, "Unhandled output scalar key for value %s", value);
                break;
        }

        context->scalarKey = ScalarKeyNone;
    }

    return success;
}

static bool ConfigurationParseSettings(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    switch (event->type) {
        case YAML_MAPPING_END_EVENT:
            return ConfigurationParseSettingsMappingEnd(self, event, context);
            break;
        case YAML_SCALAR_EVENT:
            return ConfigurationParseSettingsScalar(self, event, context);
            break;
        case YAML_MAPPING_START_EVENT:
            // NOTE: Nothing to do with these events
            return true;
            break;
        default:
            LogE(TAG, "Invalid event %i in Settings section", event->type);
            return false;
            break;
    }
}

static bool ConfigurationParseSettingsMappingEnd(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    // If we ended without a full value, we've failed
    if (context->scalarKey != ScalarKeyNone) {
        LogE(TAG, "Failed to find value for a Settings key");
        return false;
    }

    // Transition back out of the section
    context->section = SectionNone;

    return true;
}

static bool ConfigurationParseSettingsScalar(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    bool success = false;

    const char *value = (const char *)event->data.scalar.value;
    // size_t valueSize = event->data.scalar.length;

    if (context->scalarKey == ScalarKeyNone) {
        

        if (strcmp(value, "MinWait") == 0) {
            context->scalarKey = ScalarKeyMinWait;
            success = true;
        } else if (strcmp(value, "MaxWait") == 0) {
            context->scalarKey = ScalarKeyMaxWait;
            success = true;
        } else if (strcmp(value, "MinPecks") == 0) {
            context->scalarKey = ScalarKeyMinPecks;
            success = true;
        } else if (strcmp(value, "MaxPecks") == 0) {
            context->scalarKey = ScalarKeyMaxPecks;
            success = true;
        } else if (strcmp(value, "PeckWait") == 0) {
            context->scalarKey = ScalarKeyPeckWait;
            success = true;
        } else {
            LogE(TAG, "Unhandled Settings key: %s", value);
        }
    } else {
        switch (context->scalarKey) {
            case ScalarKeyMinWait:
                self->minWait = (uint32_t)strtol(value, NULL, 10);
                success = true;
                break;
            case ScalarKeyMaxWait:
                self->maxWait = (uint32_t)strtol(value, NULL, 10);
                success = true;
                break;
            case ScalarKeyMinPecks:
                self->minPecks = (uint32_t)strtol(value, NULL, 10);
                success = true;
                break;
            case ScalarKeyMaxPecks:
                self->maxPecks = (uint32_t)strtol(value, NULL, 10);
                success = true;
                break;
            case ScalarKeyPeckWait:
                self->peckWait = (uint32_t)strtol(value, NULL, 10);
                success = true;
                break;
            default:
                LogE(TAG, "Unhandled Settings value");
                break;
        }

        context->scalarKey = ScalarKeyNone;
    }

    return success;
}


// MARK: - Settings

uint32_t ConfigurationGetMinWait(const ConfigurationRef self) {
    return self->minWait;
}

uint32_t ConfigurationGetMaxWait(const ConfigurationRef self) {
    return self->maxWait;
}

uint32_t ConfigurationGetMinPecks(const ConfigurationRef self) {
    return self->minPecks;
}

uint32_t ConfigurationGetMaxPecks(const ConfigurationRef self) {
    return self->maxPecks;
}

uint32_t ConfigurationGetPeckWait(const ConfigurationRef self) {
    return self->peckWait;
}


// MARK: - Outputs

const char * ConfigurationGetOutputName(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalOutputs) {
        return NULL;
    }

    return self->outputs[idx].name;
}

const char * ConfigurationGetOutputPath(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalOutputs) {
        return NULL;
    }

    if (self->outputs[idx].type != ConfigurationOutputTypeFile) {
        return NULL;
    }

    return self->outputs[idx].file.path;
}

int ConfigurationGetOutputPin(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalOutputs) {
        return -1;
    }

    if (self->outputs[idx].type != ConfigurationOutputTypeGPIO) {
        return -1;
    }

    return self->outputs[idx].gpio.pin;
}

ConfigurationOutputType ConfigurationGetOutputType(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalOutputs) {
        return ConfigurationOutputTypeUnknown;
    }

    return self->outputs[idx].type;
}

size_t ConfigurationGetTotalOutputs(const ConfigurationRef self) {
    return self->totalOutputs;
}


// MARK: - Birds

const char * ConfigurationGetBirdBack(const ConfigurationRef self, size_t birdIdx, size_t idx) {
    if (birdIdx >= self->totalBirds) {
        return NULL;
    }

    const ConfigurationBird *bird = self->birds + birdIdx;

    if (idx >= bird->totalBacks) {
        return NULL;
    }

    return bird->backs[idx];
}

size_t ConfigurationGetBirdTotalBacks(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalBirds) {
        return 0;
    }

    return self->birds[idx].totalBacks;
}

const char * ConfigurationGetBirdForward(const ConfigurationRef self, size_t birdIdx, size_t idx) {
    if (birdIdx >= self->totalBirds) {
        return NULL;
    }

    const ConfigurationBird *bird = self->birds + birdIdx;

    if (idx >= bird->totalForwards) {
        return NULL;
    }

    return bird->forwards[idx];
}

size_t ConfigurationGetBirdTotalForwards(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalBirds) {
        return 0;
    }

    return self->birds[idx].totalForwards;
}

const char * ConfigurationGetBirdName(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalBirds) {
        return NULL;
    }

    return self->birds[idx].name;
}

const char * ConfigurationGetBirdStatic(const ConfigurationRef self, size_t birdIdx, size_t idx) {
    if (birdIdx >= self->totalBirds) {
        return NULL;
    }

    const ConfigurationBird *bird = self->birds + birdIdx;

    if (idx >= bird->totalStatics) {
        return NULL;
    }

    return bird->statics[idx];
}

size_t ConfigurationGetBirdTotalStatics(const ConfigurationRef self, size_t idx) {
    if (idx >= self->totalBirds) {
        return 0;
    }

    return self->birds[idx].totalStatics;
}

size_t ConfigurationGetTotalBirds(const ConfigurationRef self) {
    return self->totalBirds;
}


// MARK: - Utilities

static void ConfigurationBirdDestroy(ConfigurationBird *bird) {
    SAFE_DESTROY(bird->name, free);

    for (size_t idx = 0; idx < bird->totalStatics; idx++) {
        SAFE_DESTROY(bird->statics[idx], free);
    }

    SAFE_DESTROY(bird->statics, free);

    for (size_t idx = 0; idx < bird->totalBacks; idx++) {
        SAFE_DESTROY(bird->backs[idx], free);
    }

    SAFE_DESTROY(bird->backs, free);

    for (size_t idx = 0; idx < bird->totalForwards; idx++) {
        SAFE_DESTROY(bird->forwards[idx], free);
    }

    SAFE_DESTROY(bird->forwards, free);
}

static void ConfigurationBirdReset(ConfigurationBird * NONNULL bird) {
    memset(bird, 0, sizeof(ConfigurationBird));
}

static void ConfigurationOutputDestroy(ConfigurationOutput *output) {
    SAFE_DESTROY(output->name, free);

    if (output->type == ConfigurationOutputTypeFile) {
        SAFE_DESTROY(output->file.path, free);
    }

    ConfigurationOutputReset(output);
}

static void ConfigurationOutputReset(ConfigurationOutput *output) {
    memset(output, 0, sizeof(ConfigurationOutput));
}


// MARK: - Debug

void ConfigurationSetDumpParseEvents(bool dump) {
    DumpParseEvents = dump;
}
