#include <math.h>
#include <stdio.h>
#include <string.h>
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "dinov3.h"
#include "fake_ort.h"

static int require(int condition, const char* label)
{
    if (condition) return 0;
    fprintf(stderr, "dinov3: %s: %s\n", label, dinov3_last_error_native());
    return 1;
}

static int embedding_matches(const char* json)
{
    xvalue* values;
    double value;
    int ok;
    if (json == NULL) return 0;
    values = xrtJsonParse(xrtStrView(json));
    ok = xrtValueType(values) == XVALUE_ARRAY && xrtValueCount(values) == 3u &&
        xrtValueGetFloat(xrtValueArrayGet(values, 0u), &value) && value == 1.0 &&
        xrtValueGetFloat(xrtValueArrayGet(values, 1u), &value) && value == 2.0 &&
        xrtValueGetFloat(xrtValueArrayGet(values, 2u), &value) && value == 3.0;
    xrtValueRelease(values);
    return ok;
}

static int owned_result_budgets(DinoFakeResult* result)
{
    int operation;
    /* 预热独立错误槽，预算只覆盖当前格式化调用。 */
    (void)dinov3_embedding_json_native(NULL);
    for (operation = 0; operation < 2; ++operation) {
        int budget;
        int needed = operation == 0 ? 10 : 14;
        int success = 0;
        for (budget = 0; budget <= needed; ++budget) {
            xmemdebugsnapshot before, after;
            char* text;
            bool triggered;
            bool memory;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            if (!xrtMemDebugFailAfter((uint64)budget)) return 1;
            text = operation == 0 ? dinov3_embedding_json_native(result) :
                dinov3_output_json_native(result, "owned source");
            triggered = xrtMemDebugFailTriggered();
            memory = xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            xrtMemDebugFailClear();
            if (require(triggered ? text == NULL && memory : text != NULL,
                        "JSON failure must preserve memory error")) {
                fprintf(stderr, "operation=%d budget=%d triggered=%d memory=%d\n",
                    operation, budget, (int)triggered, (int)memory);
                xrtFree(text);
                return 1;
            }
            if (!triggered && require(xrtJsonValid(xrtStrView(text)), "budget success JSON")) {
                xrtFree(text);
                return 1;
            }
            xrtFree(text);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            if (require(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes,
                        "JSON failure/success allocation balance")) return 1;
            if (!triggered) {
                if (require(budget == needed, "fixed C allocation budget changed")) return 1;
                printf("dinov3 C owned operation %d: %d allocations\n", operation, budget);
                success = 1;
                break;
            }
        }
        if (require(success, "JSON fault scan must include a success endpoint")) return 1;
    }
    return 0;
}

int main(void)
{
    float left_values[3] = {1.0f, 2.0f, 3.0f};
    float right_values[3] = {0.0f, 1.0f, 0.0f};
    DinoFakeResult left = {"[1,3]", 3, left_values};
    DinoFakeResult right = {"[3]", 3, right_values};
    char source_path[5001];
    char* embedding;
    char* output;
    char* second;

    if (require(dinov3_available_native(), "fake ONNX bridge load")) return 1;
    if (owned_result_budgets(&left)) return 1;
    embedding = dinov3_embedding_json_native(&left);
    if (require(embedding_matches(embedding), "embedding JSON")) return 1;
    memset(source_path, 'x', sizeof(source_path) - 1);
    source_path[0] = '"';
    source_path[1] = '\n';
    source_path[sizeof(source_path) - 1] = '\0';
    output = dinov3_output_json_native(&left, source_path);
    second = dinov3_output_json_native(&right, "second");
    if (require(output != NULL && second != NULL && strlen(output) > 5000 &&
                xrtJsonValid(xrtStrView(output)) && xrtJsonValid(xrtStrView(second)) &&
                strstr(output, "\\n") != NULL && strstr(output, "\"shape\":[1,3]") != NULL,
                "owned escaped output JSON")) return 1;
    if (require(fabs(dinov3_l2_norm_native(&left) - sqrt(14.0)) < 1e-10 &&
                fabs(dinov3_cosine_native(&left, &right) - 2.0 / sqrt(14.0)) < 1e-10,
                "norm and cosine")) return 1;
    xrtFree(second);
    xrtFree(output);
    xrtFree(embedding);

    if (require(dinov3_embedding_json_native(NULL) == NULL &&
                strstr(dinov3_last_error_native(), "unavailable") != NULL,
                "missing embedding must fail")) return 1;
    if (require(dinov3_output_json_native(NULL, "missing") == NULL &&
                strstr(dinov3_last_error_native(), "unavailable") != NULL,
                "missing output must fail")) return 1;
    embedding = dinov3_embedding_json_native(&left);
    if (require(embedding_matches(embedding) &&
                dinov3_last_error_native()[0] == '\0',
                "embedding after missing output")) return 1;
    xrtFree(embedding);

    left.shape = "[2,3]";
    if (require(dinov3_embedding_json_native(&left) == NULL,
                "shape/count mismatch")) return 1;
    left.shape = "broken";
    if (require(dinov3_embedding_json_native(&left) == NULL,
                "malformed shape")) return 1;
    left.shape = "[100001]";
    if (require(dinov3_embedding_json_native(&left) == NULL,
                "shape limit")) return 1;
    left.shape = "[3]";
    left.count = -1;
    xrtSetErrorKind(XERR_MEMORY);
    if (require(dinov3_embedding_json_native(&left) == NULL && xrtGetError() == NULL &&
                strcmp(dinov3_last_error_native(), "invalid DinoV3 embedding shape or count") == 0,
                "negative count must not inherit a stale memory error")) return 1;
    left.count = 3;
    right.count = 2;
    right.shape = "[2]";
    if (require(dinov3_cosine_native(&left, &right) == 0.0 &&
                strstr(dinov3_last_error_native(), "different lengths") != NULL,
                "cosine length mismatch")) return 1;
    right.count = 3;
    right.shape = "[3]";
    left.shape = "[1,3]";
    left_values[0] = NAN;
    if (require(dinov3_embedding_json_native(&left) == NULL &&
                dinov3_l2_norm_native(&left) == 0.0 &&
                strstr(dinov3_last_error_native(), "non-finite") != NULL,
                "non-finite embedding")) return 1;
    left_values[0] = 1.0f;
    left.shape = "[0]";
    left.count = 0;
    embedding = dinov3_embedding_json_native(&left);
    if (require(embedding != NULL && strcmp(embedding, "[]") == 0,
                "zero-length embedding")) return 1;
    xrtFree(embedding);
    puts("dinov3 C result processing: PASS");
    return 0;
}
