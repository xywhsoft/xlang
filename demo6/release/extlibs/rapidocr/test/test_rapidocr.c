/* RapidOCR C smoke: runtime discovery and missing-image lifecycle path. */
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "xrt.h"
#include "rapidocr.h"
#include "rapidocr_json_private.h"

static int fail(const char* message)
{
    fprintf(stderr, "rapidocr C smoke failed: %s\n", message);
    return 1;
}

static char* test_path(const char* relative)
{
    char* app_dir = xrtPathAppDir();
    char* path = app_dir != NULL ? xrtPathJoin(app_dir, relative) : NULL;
    xrtFree(app_dir);
    return path;
}

static int check_assembly(void)
{
    char* first = rapidocr_assemble_result_native("[]",
        "{\"text\":\"a\\\"b\\\\c\",\"score\":0.5}", 0.0);
    char* second = rapidocr_assemble_result2_native("[]",
        "{\"text\":\"second\",\"score\":1}",
        "{\"text\":\"third\",\"score\":2}", 0.25);
    xvalue* parsed = first != NULL ? xrtJsonParse(xrtStrView(first)) : NULL;
    xvalue* texts = parsed != NULL ? xrtValueObjectGet(parsed, xrtStrView("txts")) : NULL;
    xvalue* text = texts != NULL ? xrtValueArrayGet(texts, 0u) : NULL;
    xstrview view;
    int ok = first != NULL && second != NULL && parsed != NULL &&
        xrtValueGetString(text, &view) && view.Size == 5u &&
        memcmp(view.Data, "a\"b\\c", view.Size) == 0 &&
        strstr(second, "second") != NULL && strstr(first, "second") == NULL;
    xrtValueRelease(parsed);
    xrtFree(second);
    xrtFree(first);
    if (!ok) return fail("escaped JSON or independent result ownership");

    /* 旧实现的 1 KiB 文本/64 KiB 总结果上限必须真正消失。 */
    {
        char* long_text = (char*)malloc(2049u);
        xvalue* recs = xrtValueArray();
        char* recs_json;
        char* result;
        xvalue* value;
        xvalue* result_texts;
        size_t i;
        if (long_text == NULL || recs == NULL) {
            free(long_text);
            xrtValueRelease(recs);
            return fail("long result setup");
        }
        memset(long_text, 'X', 2048u);
        long_text[2048] = '\0';
        for (i = 0u; i < 40u; ++i) {
            xvalue* rec = xrtValueObject();
            if (rec == NULL ||
                !xrtValueObjectSetNew(rec, xrtStrView("text"), xrtValueString(xrtStrView(long_text))) ||
                !xrtValueObjectSetNew(rec, xrtStrView("score"), xrtValueFloat(0.75))) {
                xrtValueRelease(rec);
                free(long_text);
                xrtValueRelease(recs);
                return fail("long result construction");
            }
            if (!xrtValueArrayAppendNew(recs, rec)) {
                free(long_text);
                xrtValueRelease(recs);
                return fail("long result append");
            }
        }
        recs_json = xrtJsonStringify(recs, false, NULL);
        result = recs_json != NULL ? rapidocr_assemble_result_list_native("[]", recs_json, 0.0) : NULL;
        value = result != NULL ? xrtJsonParse(xrtStrView(result)) : NULL;
        result_texts = value != NULL ? xrtValueObjectGet(value, xrtStrView("txts")) : NULL;
        ok = result != NULL && strlen(result) > 65536u &&
            xrtValueCount(result_texts) == 40u &&
            xrtValueGetString(xrtValueArrayGet(result_texts, 39u), &view) && view.Size == 2048u &&
            memcmp(view.Data, long_text, view.Size) == 0;
        xrtValueRelease(value);
        xrtFree(result);
        xrtFree(recs_json);
        xrtValueRelease(recs);
        free(long_text);
        if (!ok) return fail("long result was truncated");
    }

    if (rapidocr_assemble_result_native("not json", "{}", 0.0) != NULL ||
        rapidocr_assemble_result_list_native("[]", "{}", 0.0) != NULL ||
        rapidocr_assemble_result_native("[]", "{}", NAN) != NULL) {
        return fail("invalid result accepted");
    }
    if (!rapidocr_cls_should_rotate_native("{\"label\":\"180\",\"score\":0.95}", 0.9) ||
        rapidocr_cls_should_rotate_native("{\"label\":\"0\",\"nested\":{\"label\":\"180\"},\"score\":1}", 0.9) ||
        rapidocr_cls_should_rotate_native("{bad}", 0.9)) {
        return fail("classifier JSON field selection");
    }
    return 0;
}

static int check_decode(void)
{
    const char* keys = "release/extlibs/rapidocr/test/charset_special.txt";
    void* info = rapidocr_charset_info_native(keys);
    xvalue* details = info != NULL ? xrtJsonParse(xrtStrView(info)) : NULL;
    xvalue* count = details != NULL ? xrtValueObjectGet(details, xrtStrView("count")) : NULL;
    int64 lines = 0;
    int ok = xrtValueGetInt(count, &lines) && lines == 3;
    xrtValueRelease(details);
    xrtFree(info);
    if (!ok) return fail("charset metadata JSON");
    {
        void* charset = rapidocr_charset_open_native(keys);
        int invalid_result = charset != NULL &&
            rapidocr_rec_decode_result_at_charset_native(NULL, charset, 0) == NULL &&
            strstr(rapidocr_last_error_native(), "invalid recognizer") != NULL;
        if (charset == NULL || !invalid_result || !rapidocr_charset_release_native(charset) ||
            rapidocr_charset_release_native(NULL) != 0 ||
            rapidocr_charset_open_native("__missing_rapidocr_charset__.txt") != NULL ||
            strstr(rapidocr_last_error_native(), "charset") == NULL) {
            return fail("charset handle lifecycle");
        }
    }
    {
        char* crlf_path = test_path("charset_crlf.txt");
        const unsigned char crlf[] = "A\r\n\r\nZ";
        void* metadata;
        void* decoded;
        xvalue* parsed;
        xvalue* count_value;
        xvalue* decoded_value;
        xvalue* decoded_text;
        xstrview decoded_view;
        int64 line_count = 0;
        if (crlf_path == NULL ||
            !xrtFileWriteAll(crlf_path, (xbytesview){ crlf, sizeof(crlf) - 1u })) {
            xrtFree(crlf_path);
            return fail("CRLF charset fixture creation");
        }
        metadata = rapidocr_charset_info_native(crlf_path);
        parsed = metadata != NULL ? xrtJsonParse(xrtStrView(metadata)) : NULL;
        count_value = parsed != NULL ? xrtValueObjectGet(parsed, xrtStrView("count")) : NULL;
        ok = xrtValueGetInt(count_value, &line_count) && line_count == 3;
        xrtValueRelease(parsed);
        xrtFree(metadata);
        decoded = rapidocr_rec_decode_native(
            "{\"shape\":[1,4],\"data\":[0,0,0,1]}", crlf_path);
        decoded_value = decoded != NULL ? xrtJsonParse(xrtStrView(decoded)) : NULL;
        decoded_text = decoded_value != NULL ? xrtValueObjectGet(decoded_value, xrtStrView("text")) : NULL;
        ok = ok && xrtValueGetString(decoded_text, &decoded_view) &&
            decoded_view.Size == 1u && decoded_view.Data[0] == 'Z';
        xrtValueRelease(decoded_value);
        xrtFree(decoded);
        int removed = xrtFileDelete(crlf_path);
        xrtFree(crlf_path);
        if (!removed || !ok) return fail("CRLF charset parsing");
    }
    void* result = rapidocr_rec_decode_native(
        "{\"shape\":[3,4],\"data\":[0,1,0,0,0,0,1,0,0,0,0,1]}", keys);
    xvalue* value = result != NULL ? xrtJsonParse(xrtStrView(result)) : NULL;
    xvalue* text = value != NULL ? xrtValueObjectGet(value, xrtStrView("text")) : NULL;
    xstrview view;
    ok = xrtValueGetString(text, &view) && view.Size == 3u &&
        memcmp(view.Data, "\"\\A", 3u) == 0;
    xrtValueRelease(value);
    xrtFree(result);
    if (!ok) return fail("recognizer JSON escapes");

    {
        const int frames = 5000;
        size_t capacity = (size_t)frames * 16u + 80u;
        char* source = (char*)malloc(capacity);
        size_t used;
        int i;
        if (source == NULL) return fail("long recognizer setup");
        used = (size_t)snprintf(source, capacity, "{\"shape\":[%d,4],\"data\":[", frames);
        for (i = 0; i < frames; ++i) {
            const char* scores = i % 2 == 0 ? "0,1,0,0" : "0,0,1,0";
            int written = snprintf(source + used, capacity - used, "%s%s", i == 0 ? "" : ",", scores);
            if (written < 0 || (size_t)written >= capacity - used) {
                free(source);
                return fail("long recognizer input capacity");
            }
            used += (size_t)written;
        }
        if (snprintf(source + used, capacity - used, "]}") < 0) {
            free(source);
            return fail("long recognizer input closure");
        }
        result = rapidocr_rec_decode_native(source, keys);
        free(source);
        value = result != NULL ? xrtJsonParse(xrtStrView(result)) : NULL;
        text = value != NULL ? xrtValueObjectGet(value, xrtStrView("text")) : NULL;
        ok = xrtValueGetString(text, &view) && view.Size == (size_t)frames &&
            view.Data[0] == '"' && view.Data[1] == '\\';
        xrtValueRelease(value);
        xrtFree(result);
        if (!ok) return fail("long recognizer result was truncated");
    }

    if (rapidocr_rec_decode_native("{\"shape\":[3,4],\"data\":[0,1]}", keys) != NULL ||
        strstr(rapidocr_last_error_native(), "incomplete") == NULL) {
        return fail("incomplete recognizer output accepted");
    }
    {
        const char* malformed[] = {
            "{\"meta\":{\"shape\":[1,3],\"data\":[0,1,0]},\"shape\":[1,3],\"data\":[]}",
            "{\"shape\":[2147483647,2147483647],\"data\":[]}",
            "{\"shape\":[1,3],\"data\":[0,1,\"bad\"]}",
            "{\"shape\":[1,3],\"data\":[0,1,0,42]}"
        };
        size_t i;
        for (i = 0u; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
            result = rapidocr_rec_decode_native(malformed[i], keys);
            if (result != NULL) {
                xrtFree(result);
                return fail("malformed recognizer tensor accepted");
            }
        }
        if (rapidocr_det_boxes_from_output_native(malformed[0], 32, 32, 0.5) != NULL ||
            strstr(rapidocr_last_error_native(), "invalid detector tensor") == NULL) {
            return fail("nested detector tensor fields accepted");
        }
    }
    {
        char* corrupt_path = test_path("charset_nul.txt");
        const unsigned char corrupt[] = { 'A', '\n', 'B', '\0', 'C', '\n' };
        void* decoded;
        if (corrupt_path == NULL ||
            !xrtFileWriteAll(corrupt_path, (xbytesview){ corrupt, sizeof(corrupt) })) {
            xrtFree(corrupt_path);
            return fail("NUL charset fixture creation");
        }
        decoded = rapidocr_rec_decode_native("{\"shape\":[1,3],\"data\":[0,1,0]}", corrupt_path);
        ok = decoded == NULL && strstr(rapidocr_last_error_native(), "NUL") != NULL;
        xrtFree(decoded);
        int removed = xrtFileDelete(corrupt_path);
        xrtFree(corrupt_path);
        if (!removed || !ok) return fail("NUL charset accepted");
    }
    return 0;
}

static int check_classifier(void)
{
    int dims[8] = { 0 };
    void* first = rapidocr_cls_json_encode(1, 0.975, 2);
    void* second = rapidocr_cls_json_encode(0, 0.875, 2);
    xvalue* root = first != NULL ? xrtJsonParse(xrtStrView(first)) : NULL;
    xvalue* label = root != NULL ? xrtValueObjectGet(root, xrtStrView("label")) : NULL;
    xstrview name;
    int ok = first != NULL && second != NULL &&
        rapidocr_json_shape_dims("[1,6,2]", dims, 8) == 3 &&
        dims[0] == 1 && dims[1] == 6 && dims[2] == 2 &&
        rapidocr_json_shape_dims("[1,2.0]", dims, 8) == 0 &&
        rapidocr_json_shape_dims("[1,2147483648]", dims, 8) == 0 &&
        xrtValueGetString(label, &name) && name.Size == 3u &&
        memcmp(name.Data, "180", 3u) == 0 &&
        strstr(second, "\"label\":\"0\"") != NULL &&
        strstr(first, "\"label\":\"180\"") != NULL &&
        rapidocr_cls_decode_result_native(NULL) == NULL &&
        rapidocr_cls_decode_result_at_native(NULL, 1) == NULL;
    xrtValueRelease(root);
    xrtFree(second);
    xrtFree(first);
    if (!ok) return fail("classifier result ownership or shape bounds");
    return 0;
}

static int check_ocr_items(void)
{
    const char* source = "{\"boxes\":[{\"box\":[[1,2],[11,2],[11,8],[1,8]]}],"
        "\"txts\":[\"a\\\"b\\\\c\"],\"scores\":[0.75]}";
    void* first = rapidocr_to_ocr_items_json2_native(source, 0.5);
    void* second = rapidocr_to_ocr_items_json2_native(source, 0.8);
    xvalue* parsed = first != NULL ? xrtJsonParse(xrtStrView(first)) : NULL;
    xvalue* item = parsed != NULL ? xrtValueArrayGet(parsed, 0u) : NULL;
    xvalue* text = item != NULL ? xrtValueObjectGet(item, xrtStrView("Text")) : NULL;
    xstrview view;
    int ok = first != NULL && second != NULL &&
        xrtValueCount(parsed) == 1u &&
        xrtValueGetString(text, &view) && view.Size == 5u &&
        memcmp(view.Data, "a\"b\\c", 5u) == 0 &&
        strcmp(second, "[]") == 0 && strstr(first, "Ratio") != NULL;
    xrtValueRelease(parsed);
    xrtFree(second);
    xrtFree(first);
    if (!ok) return fail("OCR item escaping, filtering, or ownership");

    {
        char long_text[2049];
        char source_text[2300];
        int written;
        void* output;
        xvalue* value;
        xvalue* long_item;
        xvalue* long_word;
        memset(long_text, 'X', 2048u);
        long_text[2048] = '\0';
        written = snprintf(source_text, sizeof(source_text),
            "{\"boxes\":[{\"box\":[[1,2],[11,2],[11,8],[1,8]]}],\"txts\":[\"%s\"],\"scores\":[1]}",
            long_text);
        if (written < 0 || (size_t)written >= sizeof(source_text)) return fail("long OCR source setup");
        output = rapidocr_to_ocr_items_json_native(source_text);
        value = output != NULL ? xrtJsonParse(xrtStrView(output)) : NULL;
        long_item = value != NULL ? xrtValueArrayGet(value, 0u) : NULL;
        long_word = long_item != NULL ? xrtValueObjectGet(long_item, xrtStrView("Text")) : NULL;
        ok = xrtValueGetString(long_word, &view) && view.Size == 2048u &&
            memcmp(view.Data, long_text, 2048u) == 0;
        xrtValueRelease(value);
        xrtFree(output);
        if (!ok) return fail("long OCR item was truncated");
    }
    if (rapidocr_to_ocr_items_json2_native("{\"boxes\":[{}],\"txts\":[\"a\"],\"scores\":[1]}", 0.5) != NULL ||
        rapidocr_to_ocr_items_json2_native(source, NAN) != NULL ||
        strstr(rapidocr_last_error_native(), "invalid OCR") == NULL) {
        return fail("invalid OCR items accepted");
    }
    return 0;
}

static int check_detector_boxes(void)
{
    const char* source = "{\"shape\":[4,4],\"data\":[1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0]}";
    void* first = rapidocr_det_boxes_from_output_native(source, 80, 80, 0.5);
    void* second = rapidocr_det_boxes_from_output_native(source, 80, 80, 0.5);
    xvalue* parsed = first != NULL ? xrtJsonParse(xrtStrView(first)) : NULL;
    int ok = first != NULL && second != NULL &&
        xrtValueCount(parsed) == 1u &&
        rapidocr_box_count_native(first) == 1 &&
        strcmp(first, second) == 0;
    xrtValueRelease(parsed);
    xrtFree(second);
    xrtFree(first);
    if (!ok) return fail("detector box result ownership");

    {
        RapidJsonBox boxes[256];
        void* output;
        xvalue* value;
        size_t i;
        for (i = 0u; i < 256u; ++i) {
            int x = 1000000000 + (int)i * 20;
            int y = 1000000000 + (int)i * 20;
            int points[8] = { x, y, x + 10, y, x + 10, y + 10, x, y + 10 };
            memcpy(boxes[i].points, points, sizeof(points));
            boxes[i].score = 0.12345678901234568;
            boxes[i].pixels = INT_MAX;
        }
        output = rapidocr_boxes_json_encode(boxes, 256u);
        value = output != NULL ? xrtJsonParse(xrtStrView(output)) : NULL;
        {
            void* geometry = output != NULL ? rapidocr_boxes_open_native(output) : NULL;
            ok = output != NULL && strlen(output) > 32768u &&
                xrtValueCount(value) == 256u && geometry != NULL &&
                rapidocr_boxes_count_handle_native(geometry) == 256 &&
                rapidocr_boxes_coord_native(geometry, 255, 0) == 1000005100.0;
            rapidocr_boxes_release_native(geometry);
        }
        xrtValueRelease(value);
        xrtFree(output);
        if (!ok) return fail("large detector result was truncated");
    }
    if (rapidocr_det_boxes_from_output_native("{}", 80, 80, 0.5) != NULL ||
        strstr(rapidocr_last_error_native(), "invalid detector") == NULL ||
        rapidocr_det_boxes_from_result_native(NULL, 80, 80, 0.5) != NULL ||
        rapidocr_det_boxes_from_result_db_native(NULL, 80, 80, 0.5, 0.5, 1.6, 0, 3, 0) != NULL) {
        return fail("invalid detector input accepted");
    }
    return 0;
}

static int check_box_geometry(void)
{
    const char* boxes = "[{\"note\":\"\\\"box\\\":[[999,999]]\",\"box\":[[2,3],[12,3],[12,9],[2,9]]},"
        "{\"box\":[[20.5,30],[40.5,30],[40.5,50],[20.5,50]]}]";
    const char* malformed = "[{\"box\":[[1,2],[3,4],[5,6]]}]";
    const char* overflow = "[{\"box\":[[-2147483648,0],[2147483647,0],[2147483647,1],[-2147483648,1]]}]";
    const char* huge = "[{\"box\":[[0,0],[1e308,0],[1e308,1],[0,1]]}]";
    if (rapidocr_box_count_native(boxes) != 2 ||
        rapidocr_first_box_x_native(boxes) != 2 ||
        rapidocr_first_box_y_native(boxes) != 3 ||
        rapidocr_first_box_width_native(boxes) != 10 ||
        rapidocr_first_box_height_native(boxes) != 6 ||
        rapidocr_box_x_native(boxes, 1) != 20 ||
        rapidocr_box_width_native(boxes, 1) != 20 ||
        rapidocr_box_point_x_native(boxes, 1, 0) != 20.5 ||
        rapidocr_box_point_y_native(boxes, 1, 2) != 50.0 ||
        rapidocr_box_count_native("{\"box\":[[1,2]]}") != 0 ||
        rapidocr_box_count_native(malformed) != 0 ||
        rapidocr_box_x_native(malformed, 0) != 0 ||
        rapidocr_box_point_x_native(malformed, 0, 0) != 0.0 ||
        rapidocr_box_width_native(overflow, 0) != 0 ||
        rapidocr_box_rec_target_width_native(huge, 0, 320, 48) != 320) {
        return fail("structured detector geometry");
    }
    {
        char mutable_boxes[] = "[{\"box\":[[2,3],[12,3],[12,9],[2,9]]}]";
        void* geometry = rapidocr_boxes_open_native(mutable_boxes);
        if (geometry == NULL) return fail("geometry snapshot creation");
        mutable_boxes[0] = '!';
        if (rapidocr_boxes_count_handle_native(geometry) != 1 ||
            rapidocr_boxes_coord_native(geometry, 0, 0) != 2.0 ||
            rapidocr_boxes_coord_native(geometry, 0, 5) != 9.0 ||
            rapidocr_boxes_rec_target_width_native(geometry, 0, 320, 48) !=
                rapidocr_box_rec_target_width_native(boxes, 0, 320, 48) ||
            rapidocr_boxes_coord_native(geometry, 1, 0) != 0.0 ||
            rapidocr_boxes_coord_native(geometry, 0, 8) != 0.0 ||
            !rapidocr_boxes_release_native(geometry) ||
            rapidocr_boxes_release_native(NULL) != 0) {
            return fail("geometry snapshot ownership or indexing");
        }
        geometry = rapidocr_boxes_open_native(malformed);
        if (geometry != NULL || strstr(rapidocr_last_error_native(), "detector boxes") == NULL) {
            rapidocr_boxes_release_native(geometry);
            return fail("malformed geometry snapshot accepted");
        }
    }
    return 0;
}

static int check_resource_path(void)
{
    char* parent = test_path("extlibs");
    char* base = test_path("extlibs/rapidocr");
    char* fixture = test_path("extlibs/rapidocr/中文路径.txt");
    const unsigned char data[] = { 'o', 'k' };
    char* first;
    char* second;
    int ok;
    if (parent == NULL || base == NULL || fixture == NULL ||
        !xrtDirCreateAll(base) ||
        !xrtFileWriteAll(fixture, (xbytesview){ data, sizeof(data) })) {
        xrtFree(fixture);
        xrtFree(base);
        xrtFree(parent);
        return fail("UTF-8 resource fixture creation");
    }
    first = rapidocr_extlib_path_native("中文路径.txt");
    second = rapidocr_extlib_path_native("中文路径.txt");
    ok = first != NULL && second != NULL && first != second &&
        strcmp(first, second) == 0 && xrtPathExists(first);
    xrtFree(second);
    xrtFree(first);
    int removed = xrtFileDelete(fixture);
    (void)xrtDirRemove(base);
    (void)xrtDirRemove(parent);
    xrtFree(fixture);
    xrtFree(base);
    xrtFree(parent);
    if (!removed || !ok) return fail("UTF-8 resource path ownership");
    return 0;
}

typedef struct RapidThreadCase {
    atomic_int* start;
    int kind;
    int failed;
} RapidThreadCase;

static void rapid_thread_body(RapidThreadCase* test)
{
    int i;
    const char* expected = test->kind ? "empty charset path" :
        "invalid OCR result JSON or out of memory";
    while (atomic_load(test->start) == 0) { }
    (void)rapidocr_available_native();
    for (i = 0; i < 100; ++i) {
        if (test->kind) {
            if (rapidocr_charset_info_native("") != NULL) test->failed = 1;
        } else {
            if (rapidocr_to_ocr_items_json2_native("not-json", 0.5) != NULL) test->failed = 1;
        }
        if (strcmp(rapidocr_last_error_native(), expected) != 0) test->failed = 1;
    }
    {
        char* path = rapidocr_extlib_path_native("models/missing.txt");
        if (path == NULL || rapidocr_last_error_native()[0] != '\0') test->failed = 1;
        xrtFree(path);
    }
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI rapid_thread_entry(LPVOID data)
{
    rapid_thread_body((RapidThreadCase*)data);
    return 0;
}
#else
static void* rapid_thread_entry(void* data)
{
    rapid_thread_body((RapidThreadCase*)data);
    return NULL;
}
#endif

static int check_parallel_load_and_errors(void)
{
    enum { THREADS = 8 };
    RapidThreadCase cases[THREADS];
    atomic_int start = ATOMIC_VAR_INIT(0);
    int i;
    int created = 0;
    int failed = 0;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE threads[THREADS];
#else
    pthread_t threads[THREADS];
#endif
    (void)rapidocr_charset_info_native("");
    for (i = 0; i < THREADS; ++i) {
        cases[i].start = &start;
        cases[i].kind = i & 1;
        cases[i].failed = 0;
#if defined(_WIN32) || defined(_WIN64)
        threads[i] = CreateThread(NULL, 0, rapid_thread_entry, &cases[i], 0, NULL);
        if (threads[i] == NULL) break;
#else
        if (pthread_create(&threads[i], NULL, rapid_thread_entry, &cases[i]) != 0) break;
#endif
        ++created;
    }
    atomic_store(&start, 1);
    for (i = 0; i < created; ++i) {
#if defined(_WIN32) || defined(_WIN64)
        if (WaitForSingleObject(threads[i], INFINITE) != WAIT_OBJECT_0) failed = 1;
        CloseHandle(threads[i]);
#else
        if (pthread_join(threads[i], NULL) != 0) failed = 1;
#endif
        if (cases[i].failed) failed = 1;
    }
    if (created != THREADS || failed ||
        strcmp(rapidocr_last_error_native(), "empty charset path") != 0) {
        return fail("parallel backend load or thread-local errors");
    }
    return 0;
}

int main(void)
{
    void* engine;
    void* output;

    if (check_parallel_load_and_errors() != 0) return 1;
    if (check_resource_path() != 0) return 1;
    if (check_assembly() != 0) return 1;
    if (check_decode() != 0) return 1;
    if (check_classifier() != 0) return 1;
    if (check_ocr_items() != 0) return 1;
    if (check_detector_boxes() != 0) return 1;
    if (check_box_geometry() != 0) return 1;
    /* 无原生桥接时 C 推理应明确失败；XLang 另走包内模型链路。 */
    if (!rapidocr_available_native()) {
        if (rapidocr_create_native(NULL) != NULL ||
            rapidocr_call_native(NULL, "__xlang_missing_image__.png", 1, 1, 1, 0) != NULL ||
            rapidocr_output_json_native(NULL) != NULL ||
            rapidocr_last_error_native()[0] == 0) {
            return fail("missing native shim must fail closed");
        }
    } else {
        engine = rapidocr_create_native(NULL);
        if (engine == NULL) return fail("engine creation");
        output = rapidocr_call_native(engine, "__xlang_missing_image__.png", 1, 1, 1, 0);
        if (output != NULL) {
            if (rapidocr_output_ok_native(output) != 0 ||
                !rapidocr_output_release_native(output)) {
                rapidocr_release_native(engine);
                return fail("missing image error path");
            }
        } else if (rapidocr_last_error_native()[0] == 0) {
            rapidocr_release_native(engine);
            return fail("missing image diagnostic");
        }
        if (!rapidocr_release_native(engine)) return fail("release path");
    }
    puts("rapidocr C smoke: PASS");
    return 0;
}
