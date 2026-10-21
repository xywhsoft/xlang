#include "rapidocr_xlang_c_api.h"

#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

static int engine_marker;
static int output_marker;
static const char* last_error = "";

EXPORT int rapidocr_xl_available(void) { return 1; }
EXPORT const char* rapidocr_xl_last_error(void) { return last_error; }

EXPORT RapidOCRHandle rapidocr_xl_create(const char* config_path)
{
    (void)config_path;
    last_error = "";
    return &engine_marker;
}

EXPORT void rapidocr_xl_release(RapidOCRHandle handle) { (void)handle; }

EXPORT RapidOCROutput rapidocr_xl_call(RapidOCRHandle handle, const char* image_path,
    int use_det, int use_cls, int use_rec, int return_word_box)
{
    (void)use_det;
    (void)use_cls;
    (void)use_rec;
    (void)return_word_box;
    if (handle != &engine_marker || image_path == NULL ||
        strcmp(image_path, "missing.png") == 0) {
        last_error = "mock missing image";
        return NULL;
    }
    last_error = "";
    return &output_marker;
}

EXPORT void rapidocr_xl_output_release(RapidOCROutput output) { (void)output; }
EXPORT int rapidocr_xl_output_ok(RapidOCROutput output) { return output == &output_marker; }
EXPORT const char* rapidocr_xl_output_json(RapidOCROutput output)
{
    return output == &output_marker ? "{\"boxes\":[],\"txts\":[\"mock text\"],\"scores\":[1]}" : NULL;
}
EXPORT const char* rapidocr_xl_output_text(RapidOCROutput output) { return output == &output_marker ? "mock text" : NULL; }
EXPORT const char* rapidocr_xl_output_boxes(RapidOCROutput output) { return output == &output_marker ? "[]" : NULL; }
EXPORT const char* rapidocr_xl_output_txts(RapidOCROutput output) { return output == &output_marker ? "[\"mock text\"]" : NULL; }
EXPORT const char* rapidocr_xl_output_scores(RapidOCROutput output) { return output == &output_marker ? "[1]" : NULL; }
EXPORT const char* rapidocr_xl_output_word_results(RapidOCROutput output) { return output == &output_marker ? "[]" : NULL; }
EXPORT const char* rapidocr_xl_output_elapse_list(RapidOCROutput output) { return output == &output_marker ? "[]" : NULL; }
EXPORT double rapidocr_xl_output_elapse(RapidOCROutput output) { return output == &output_marker ? 0.25 : 0.0; }
EXPORT int rapidocr_xl_output_vis(RapidOCROutput output, const char* save_path)
{
    return output == &output_marker && save_path != NULL;
}
