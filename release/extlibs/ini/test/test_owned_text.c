/* 直接验收声明层拥有型出口，不把缺失名称改成非空结果。 */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
#include "../ini_native.c"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "INI owned text failed at %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static char* output(void* config, unsigned operation)
{
    switch (operation) {
        case 0: return xini_get(config, "s", "key");
        case 1: return xini_get(config, "", "global");
        case 2: return xini_get(config, "s", "empty");
        case 3: return xini_stringify_text(config);
        case 4: return xini_error_text(config);
        case 5: return xini_section_name(config, 0);
        case 6: return xini_section_name(config, 1);
        case 7: return xini_key_name(config, "s", 0);
        default: return xini_key_name(config, "s", 1);
    }
}

int main(void)
{
    const char source[] = "global=root\n[s]\nkey=value\nempty=\n";
    const char* expected[] = {"value", "root", "", NULL, "INI operation failed", "", "s", "key", "empty"};
    void* config = xini_parse(source, sizeof(source) - 1u, true, true, false, 0, 0, 0, 0, 0);
    unsigned operation;
    CHECK(config != NULL && xini_error_code(config) == XRT_INI_OK);
    for (operation = 0u; operation < 9u; ++operation) {
        char* held = output(config, operation);
        unsigned attempt, needed = operation == 3u ? 2u : 1u;
        CHECK(held != NULL);
        if (operation == 3u) CHECK(strstr(held, "[s]") != NULL);
        else CHECK(strcmp(held, expected[operation]) == 0);
        for (attempt = 0u; attempt < 2u; ++attempt) {
            char* result;
            bool triggered, memory_error;
            unsigned budget = attempt == 0u ? 0u : needed;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(budget));
            result = output(config, operation);
            triggered = xrtMemDebugFailTriggered();
            memory_error = xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            xrtMemDebugFailClear();
            if (budget == 0u) {
                CHECK(triggered && memory_error && result == NULL);
            } else {
                CHECK(!triggered && !memory_error && result != NULL && result != held);
                CHECK(strcmp(result, held) == 0);
            }
            xrtFree(result);
        }
        xrtFree(held);
    }
    xrtClearError();
    CHECK(xini_section_name(config, -1) == NULL && xrtGetError() == NULL);
    CHECK(xini_section_name(config, 2) == NULL && xrtGetError() == NULL);
    CHECK(xini_key_name(config, "s", -1) == NULL && xrtGetError() == NULL);
    CHECK(xini_key_name(config, "s", 2) == NULL && xrtGetError() == NULL);
    CHECK(xini_key_name(config, "absent", 0) == NULL && xrtGetError() == NULL);
    CHECK(xini_get(config, "s", "absent") == NULL && xrtGetError() == NULL);
    CHECK(xini_error_code(config) == XRT_INI_ERROR_NOT_FOUND);
    char* diagnostic = xini_error_text(config);
    char* value = xini_get(config, "s", "key");
    CHECK(diagnostic != NULL && strstr(diagnostic, "does not exist") != NULL);
    CHECK(value != NULL && strcmp(value, "value") == 0 && xini_error_code(config) == XRT_INI_OK);
    CHECK(xini_set(config, "s", "key", "changed"));
    CHECK(strcmp(value, "value") == 0 && strstr(diagnostic, "does not exist") != NULL);
    xrtFree(value);
    xrtFree(diagnostic);
    xini_destroy(config);
    puts("ini C owned text budgets: PASS");
    return 0;
}
