#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "Configuration.h"
#include "EventLoop.h"

int main(int argc, char **argv) {

    if (argc != 2) {
        return EXIT_FAILURE;
    }

    ConfigurationRef configuration = ConfigurationCreateFromFile(argv[1]);

    if (configuration == NULL) {
        printf("Failed to load configuration\n");
    } else {
        ConfigurationDestroy(configuration);
    }

    return EXIT_SUCCESS;
}
