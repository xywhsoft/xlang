#ifndef RAPIDOCR_XLANG_C_API_H
#define RAPIDOCR_XLANG_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* RapidOCRHandle;
typedef void* RapidOCROutput;

int rapidocr_xl_available(void);
const char* rapidocr_xl_last_error(void);
RapidOCRHandle rapidocr_xl_create(const char* config_path);
void rapidocr_xl_release(RapidOCRHandle handle);
RapidOCROutput rapidocr_xl_call(
	RapidOCRHandle handle,
	const char* img,
	int use_det,
	int use_cls,
	int use_rec,
	int return_word_box
);
void rapidocr_xl_output_release(RapidOCROutput output);
int rapidocr_xl_output_ok(RapidOCROutput output);
const char* rapidocr_xl_output_json(RapidOCROutput output);
const char* rapidocr_xl_output_text(RapidOCROutput output);
const char* rapidocr_xl_output_boxes(RapidOCROutput output);
const char* rapidocr_xl_output_txts(RapidOCROutput output);
const char* rapidocr_xl_output_scores(RapidOCROutput output);
const char* rapidocr_xl_output_word_results(RapidOCROutput output);
const char* rapidocr_xl_output_elapse_list(RapidOCROutput output);
double rapidocr_xl_output_elapse(RapidOCROutput output);
int rapidocr_xl_output_vis(RapidOCROutput output, const char* save_path);

#ifdef __cplusplus
}
#endif

#endif
