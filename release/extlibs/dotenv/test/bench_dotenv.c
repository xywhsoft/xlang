#include "../include/xrt_dotenv.h"
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
    enum { ITEMS = 20000 };
    char *input = (char *)malloc(ITEMS * 16u);
    xrtdotenv *config = xrtDotenvCreate();
    clock_t started;
    double parse_seconds, lookup_seconds;
    size_t size = 0;
    int i;
    if (!input || !config) return 1;
    for (i = 0; i < ITEMS; ++i) {
        int written = snprintf(input + size, ITEMS * 16u - size,
            "K%05d=V\n", i);
        if (written != 9) return 1;
        size += (size_t)written;
    }
    started = clock();
    if (!xrtDotenvParse(config, input, size, size)) return 1;
    parse_seconds = (double)(clock() - started) / CLOCKS_PER_SEC;
    started = clock();
    for (i = ITEMS - 1; i >= 0; --i) {
        char key[8];
        snprintf(key, sizeof(key), "K%05d", i);
        if (strcmp(xrtDotenvGet(config, key), "V") != 0) return 1;
    }
    lookup_seconds = (double)(clock() - started) / CLOCKS_PER_SEC;
    printf("dotenv %d keys: parse %.3fs, lookup %.3fs\n",
        ITEMS, parse_seconds, lookup_seconds);
    xrtDotenvDestroy(config);
    free(input);
    return 0;
}
