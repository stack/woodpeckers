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

// MARK: - Constants & Globals

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
} ParsingContext;


// MARK: - Prototypes

static NONNULL ConfigurationRef ConfigurationCreateDefaults(void);
static bool ConfigurationParse(ConfigurationRef self, yaml_parser_t * NONNULL parser);
static bool ConfigurationParseFromFile(ConfigurationRef self, const char * NONNULL path);
static bool ConfigurationParseFromString(ConfigurationRef self, const char * NONNULL value);

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
    free(self);
}


// MARK: - Parsing

// TODO: Move these to the top

static bool ConfigurationParseNoSection(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseNoSectionScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);

static bool ConfigurationParseOutput(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseOutputMappingEnd(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseOutputMappingStart(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseOutputScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);

static bool ConfigurationParseSettings(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseSettingsMappingEnd(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);
static bool ConfigurationParseSettingsScalar(ConfigurationRef NONNULL self, const yaml_event_t * NONNULL event, ParsingContext * NONNULL context);

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

        switch (event.type) {
            case YAML_NO_EVENT:
                printf("- No Event\n");
                break;
            case YAML_STREAM_START_EVENT:
                printf("- Stream Start Event\n");
                break;
            case YAML_STREAM_END_EVENT:
                printf("- Stream End Event\n");
                break;
            case YAML_DOCUMENT_START_EVENT:
                printf("- Document Start Event\n");
                break;
            case YAML_DOCUMENT_END_EVENT:
                printf("- Document End Event\n");
                break;
            case YAML_ALIAS_EVENT:
                printf("- Alias Event\n");
                break;
            case YAML_SCALAR_EVENT:
                printf("- Scalar Event: %s, %s, %s\n", event.data.scalar.anchor, event.data.scalar.tag, event.data.scalar.value);
                break;
            case YAML_SEQUENCE_START_EVENT:
                printf("- Sequence Start Event\n");
                break;
            case YAML_SEQUENCE_END_EVENT:
                printf("- Sequence End Event\n");
                break;
            case YAML_MAPPING_START_EVENT:
                printf("- Mapping Start Event\n");
                break;
            case YAML_MAPPING_END_EVENT:
                printf("- Mapping End Event\n");
                break;
        }

        if (context.section == SectionNone) {
            isDone = !ConfigurationParseNoSection(self, &event, &context);
        } else if (context.section == SectionSettings) {
            isDone = !ConfigurationParseSettings(self, &event, &context);
        } else if (context.section == SectionOutputs) {
            isDone = !ConfigurationParseOutput(self, &event, &context);
        } else {
            printf("Unhandled event for section %i\n", context.section);
        }

        if (event.type == YAML_STREAM_END_EVENT) {
            isDone = true;
            success = true;
        }

        yaml_event_delete(&event);
    }

    return success;
} 

static bool ConfigurationParseFromFile(ConfigurationRef self, const char *path) {
    bool success = false;

    FILE *file = fopen(path, "r");

    if (file == NULL) {
        printf("Failed to open configuration file for reading: %i", errno);
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
            printf("Invalid event %i without a section\n", event->type);
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
        printf("Invalid section name: %s\n", value);
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
            printf("Invalid event %i in Output section\n", event->type);
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
        printf("Output processed without a name\n");
        return false;
    } else if (output->type == ConfigurationOutputTypeUnknown) {
        printf("Output processed without a type\n");
        return false;
    } else if (output->type == ConfigurationOutputTypeMemory) {
        // Nothing to validate here
    } else if (output->type == ConfigurationOutputTypeFile) {
        if (output->file.path == NULL) {
            printf("File output processed without a path\n");
            return false;
        }
    } else if (output->type == ConfigurationOutputTypeGPIO) {
        if (output->gpio.pin == -1) {
            printf("GPIO output processed without a pin\n");
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
            printf("Unhandled output scalar key: %s\n", value);
        }
    } else {
        switch (context->scalarKey) {
            case ScalarKeyType:
                if (valueSize == 0) {
                    printf("Empty output scalar value\n");
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
                    printf("Unhandled output type: %s\n", value);
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
                printf("Unhandled output scalar key for value %s\n", value);
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
            printf("Invalid event %i in Settings section\n", event->type);
            return false;
            break;
    }
}

static bool ConfigurationParseSettingsMappingEnd(ConfigurationRef self, const yaml_event_t *event, ParsingContext *context) {
    // If we ended without a full value, we've failed
    if (context->scalarKey != ScalarKeyNone) {
        printf("Failed to find value for a Settings key\n");
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
            printf("Unhandled Settings key: %s\n", value);
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
                printf("Unhandled Settings value\n");
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


// MARK: - Utilities

static void ConfigurationOutputReset(ConfigurationOutput *output) {
    memset(output, 0, sizeof(ConfigurationOutput));
}
