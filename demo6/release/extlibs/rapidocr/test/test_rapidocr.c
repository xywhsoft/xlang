/* RapidOCR C smoke: runtime discovery and missing-image lifecycle path. */
#include <stdio.h>
#include <string.h>

int rapidocr_available_native(void);
const char* rapidocr_backend_native(void);
const char* rapidocr_extlib_path_native(const char* relative);
const char* rapidocr_det_boxes_from_output_native(const char* output_json,
    int image_width, int image_height, double threshold);
int rapidocr_box_count_native(const char* boxes_json);
const char* rapidocr_assemble_result_native(const char* boxes_json,
    const char* rec_json, double elapsed);
void* rapidocr_create_native(const char* config_path);
int rapidocr_release_native(void* handle);
void* rapidocr_call_native(void* handle, const char* image_path,
    int use_det, int use_cls, int use_rec, int return_word_box);
int rapidocr_output_release_native(void* handle);
int rapidocr_output_ok_native(void* handle);
const char* rapidocr_output_json_native(void* handle);
const char* rapidocr_output_text_native(void* handle);
const char* rapidocr_output_boxes_native(void* handle);

static int fail(const char* message)
{
    fprintf(stderr, "rapidocr C smoke failed: %s\n", message);
    return 1;
}

int main(void)
{
    void* engine;
    void* output;

    if (strstr(rapidocr_extlib_path_native("models\\ppocr_keys_v1.txt"), "ppocr_keys_v1.txt") == NULL ||
        rapidocr_box_count_native(rapidocr_det_boxes_from_output_native("{}", 320, 240, 0.5)) != 0 ||
        strstr(rapidocr_assemble_result_native("[]", "{\"text\":\"\",\"score\":0}", 0.0), "\"boxes\"") == NULL) {
        return fail("standalone postprocessing");
    }
    /* The optional native shim may be absent; the bridge still returns a safe fallback object. */
    engine = rapidocr_create_native(NULL);
    if (engine == NULL) return fail("engine creation");
    output = rapidocr_call_native(engine, "__xlang_missing_image__.png", 1, 1, 1, 0);
    if (output == NULL || rapidocr_output_ok_native(output) != 0 ||
        rapidocr_output_json_native(output)[0] == 0 || rapidocr_output_text_native(output)[0] != 0 ||
        strcmp(rapidocr_output_boxes_native(output), "[]") != 0) {
        rapidocr_output_release_native(output);
        rapidocr_release_native(engine);
        return fail("missing image error path");
    }
    if (!rapidocr_output_release_native(output) || !rapidocr_release_native(engine)) {
        return fail("release path");
    }
    puts("rapidocr C smoke: PASS");
    return 0;
}
