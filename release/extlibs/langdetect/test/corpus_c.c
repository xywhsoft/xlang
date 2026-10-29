#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/langdetect_native.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static uint64_t digest = UINT64_C(14695981039346656037);
static unsigned checks;
static bool records;

static void digest_bytes(const void* data, size_t size)
{
    const unsigned char* p = data;
    for (size_t i = 0; i < size; ++i) digest = (digest^p[i])*UINT64_C(1099511628211);
}

static void digest_number(int64_t value)
{
    // 固定小端字节序；不让C整型宽度或宿主端序改变对照摘要。
    uint64_t bits = (uint64_t)value;
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char byte = (unsigned char)(bits >> (i*8));
        digest_bytes(&byte, 1);
    }
}

static void digest_text(const char* value)
{
    digest_bytes(value, strlen(value)+1);
}

static void digest_error(void)
{
    digest_number(xrtErrorKind(xrtGetError()));
    digest_text(xlangdetect_last_error());
}

static void record_text(const char* label, const char* value)
{
    digest_text(value); digest_error();
    if (records) printf("%u\t%s\t%s\t%d\t%s\n", checks, label, value,
        (int)xrtErrorKind(xrtGetError()), xlangdetect_last_error());
}

static void record_number(const char* label, int64_t value)
{
    digest_number(value); digest_error();
    if (records) printf("%u\t%s\t%" PRId64 "\t%d\t%s\n", checks, label, value,
        (int)xrtErrorKind(xrtGetError()), xlangdetect_last_error());
}

static void sample(const char* input, size_t size)
{
    record_text("detect", xlangdetect_detect(input, size));
    record_text("code", xlangdetect_code(input, size));
    record_text("script", xlangdetect_script(input, size));
    record_number("confidence", xlangdetect_confidence(input, size));
    record_number("utf8", xlangdetect_valid_utf8(input, size));
    ++checks;
}

int main(int argc, char** argv)
{
    if (argc > 1) {
        if (argc != 2 || strcmp(argv[1], "--records")) return 2;
        records = true;
    }
    static const char* chunks[] = {
        "the", "THE", "and", "is", "in", "that", "with", "for", "le", "la", "les", "des", "est", "et",
        "une", "dans", "pour", "avec", "der", "die", "das", "und", "ist", "ein", "eine", "nicht", "mit", "von",
        "el", "los", "las", "es", "en", "que", "de", "para", "con", "os", "as", "um", "uma", "com", "dos",
        "nao", "il", "gli", "che", "di", "del", "della", "una", "per", "het", "een", "van", "dat", "niet", "voor", "met",
        " ", "\t", "\n", "0", "_", "-", "\xc3\xa9", "\xc3\x97", "\xc3\xb7", "\xcc\x81",
        "\xce\xb1", "\xe4\xb8\xad", "\xd0\x9f", "\xd9\x85", "\xe3\x81\x82", "\xed\x95\x9c", "\xf0\x9f\x98\x80"
    };
    static const char* fixed[] = {
        "", "the", "THE AND IS", "thethe", "éthe", "theé", "the×", "×the", "the÷",
        "你好한글", "かな한글", "你好is", "你好한글어", "Привет", "مر٣٤", "カ・ナ", "the😀",
        "the\xff", "the\xe4\xb8", "\xed\xa0\x80", "\xf4\x90\x80\x80"
    };
    uint32_t seed = UINT32_C(0x37cd901b);
    char buffer[512];
    xmemdebugsnapshot before, after;
    xrtSetErrorKind(XERR_IO); xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (size_t i = 0; i < sizeof(fixed)/sizeof(*fixed); ++i) sample(fixed[i], strlen(fixed[i]));
    sample(NULL, 0);
    sample(NULL, SIZE_MAX);
    for (unsigned i = 0; i < 2048; ++i) {
        size_t used = 0;
        unsigned pieces = i%40;
        for (unsigned j = 0; j < pieces; ++j) {
            seed = seed*UINT32_C(1664525)+UINT32_C(1013904223);
            const char* word = chunks[seed%(sizeof(chunks)/sizeof(*chunks))];
            size_t size = strlen(word);
            memcpy(buffer+used, word, size);
            used += size;
            if (j%3 == 0) buffer[used++] = (char)(i%2 ? 0 : ' ');
        }
        sample(buffer, used);
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount ||
        before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) return 2;
    printf("langdetect corpus: %u cases, %016" PRIx64 "\n", checks, digest);
    return 0;
}

#ifdef __TINYC__
#include "../langdetect_native.c"
#endif
