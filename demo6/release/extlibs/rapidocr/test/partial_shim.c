#if defined(_WIN32) || defined(_WIN64)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

static int placeholder;

EXPORT void* rapidocr_xl_create(const char* config_path)
{
    (void)config_path;
    return &placeholder;
}

EXPORT void* rapidocr_xl_call(void* engine, const char* image_path,
    int use_det, int use_cls, int use_rec, int return_word_box)
{
    (void)engine;
    (void)image_path;
    (void)use_det;
    (void)use_cls;
    (void)use_rec;
    (void)return_word_box;
    return &placeholder;
}
