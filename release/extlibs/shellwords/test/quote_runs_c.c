/* 连续片段/密集引号/任意非 NUL 字节的编码、所有权与逐点 OOM。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_shellwords.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t lengths[] = {0,1,2,3,7,15,31,32,33,63,64,65,127,128,129,255,256,257,1023,4096,16384};
static const unsigned char pattern[] = "\xe4\xbd\xa0 hello \\ \"\n";

static void word(char* output, size_t size, unsigned mode)
{
    for (size_t i = 0; i != size; ++i) {
        switch (mode) {
            case 0: output[i] = ' '; break;
            case 1: output[i] = '\''; break;
            case 2: output[i] = (char)pattern[i % (sizeof(pattern) - 1)]; break;
            case 3: output[i] = i % 97 == 0 ? '\'' : 'a'; break;
            case 4: output[i] = (char)(i % 255 + 1); break;
            default: output[i] = i % 2 == 0 ? '\\' : '\''; break;
        }
    }
    if (size != 0) output[0] = ' ';
    output[size] = 0;
}

/* 固定语料总含不安全字节，因此参考结果始终是单引号包裹的直接字节编码。 */
static char* expected(const char* input, size_t size)
{
    char* result = malloc(size * 4 + 3);
    if (result == NULL) return NULL;
    size_t n = 0;
    result[n++] = '\'';
    for (size_t i = 0; i != size; ++i) {
        if (input[i] == '\'') {
            memcpy(result + n, "'\\''", 4);
            n += 4;
        } else result[n++] = input[i];
    }
    result[n++] = '\'';
    result[n] = 0;
    return result;
}

static xvalue* one_word(const char* input, size_t size)
{
    xvalue* values = xrtValueArray();
    xvalue* text = xrtValueString((xstrview){input, size});
    if (values == NULL || text == NULL || !xrtValueArrayAppendTake(values, &text)) {
        xrtValueRelease(text);
        xrtValueRelease(values);
        return NULL;
    }
    return values;
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount &&
        before->DoubleFreeCount == after.DoubleFreeCount;
}

static bool vector(const char* input, size_t size)
{
    xmemdebugsnapshot before;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    char* reference = expected(input, size);
    xvalue* values = one_word(input, size);
    str quoted = xrtShellwordsQuote(input);
    str joined = xrtShellwordsJoin(values);
    xvalue* parsed = quoted != NULL ? xrtShellwordsSplit(quoted) : NULL;
    xstrview text;
    bool ok = reference != NULL && quoted != NULL && joined != NULL && quoted != joined &&
        strcmp(quoted, reference) == 0 && strcmp(joined, reference) == 0 &&
        xrtValueCount(parsed) == 1 &&
        xrtValueGetString(xrtValueArrayGet(parsed, 0), &text) && text.Size == size &&
        memcmp(text.Data, input, size) == 0;
    free(reference);
    xrtValueRelease(parsed);
    xrtValueRelease(values);
    xrtFree(quoted);
    xrtFree(joined);
    return balanced(&before) && ok;
}

static bool oom(const char* input, size_t size, bool join)
{
    char* reference = expected(input, size);
    xvalue* values = one_word(input, size);
    if (reference == NULL || values == NULL) { free(reference); xrtValueRelease(values); return false; }
    bool completed = false, failed = false;
    for (uint64 budget = 0; budget != 128; ++budget) {
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        if (!xrtMemDebugFailAfter(budget)) break;
        str result = join ? xrtShellwordsJoin(values) : xrtShellwordsQuote(input);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        bool ok = triggered ? result == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
            result != NULL && strcmp(result, reference) == 0 && xrtGetError() == NULL;
        failed = failed || triggered;
        xrtFree(result);
        if (!balanced(&before) || !ok) break;
        if (!triggered) { completed = true; break; }
    }
    free(reference);
    xrtValueRelease(values);
    return completed && failed;
}

static bool hex(const unsigned char* data, size_t size)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i != size; ++i)
        if (putchar(digits[data[i] >> 4]) == EOF || putchar(digits[data[i] & 15]) == EOF) return false;
    return true;
}

static bool record(unsigned id, const char* input, size_t size)
{
    str quoted = xrtShellwordsQuote(input);
    bool ok = quoted != NULL && printf("%u\t", id) > 0 && hex((const unsigned char*)input, size) &&
        putchar('\t') != EOF && hex((const unsigned char*)quoted, strlen(quoted)) && putchar('\n') != EOF;
    xrtFree(quoted);
    return ok;
}

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--records") != 0)) return 2;
    bool records = argc == 2;
    if (!records && !xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.shellwords", 0, "warm error slot");
    xrtClearError();
    char input[16385];
    unsigned count = 0;
    for (unsigned mode = 0; mode != 6; ++mode) {
        for (size_t i = 0; i != sizeof(lengths) / sizeof(lengths[0]); ++i) {
            word(input, lengths[i], mode);
            if (!(records ? record(count, input, lengths[i]) : vector(input, lengths[i]))) {
                fprintf(stderr, "shellwords quote vector %u failed\n", count);
                return 1;
            }
            ++count;
        }
    }
    if (records) {
        for (unsigned byte = 1; byte != 256; ++byte) {
            input[0] = (char)byte;
            input[1] = 0;
            if (!record(count++, input, 1)) return 1;
        }
        static const char* fixed[] = {"", "plain", "if", "A=B", "A+=B", "label:", "raw\x80\xff",
            "a-b=e", "a:b", "two words", "$(exit 7)", "line\nnext"};
        for (unsigned i = 0; i != sizeof(fixed) / sizeof(fixed[0]); ++i)
            if (!record(count++, fixed[i], strlen(fixed[i]))) return 1;
        return ferror(stdout) ? 1 : 0;
    }
    for (unsigned mode = 0; mode != 4; ++mode) {
        word(input, 4096, mode);
        if (!oom(input, 4096, false) || !oom(input, 4096, true)) {
            fprintf(stderr, "shellwords quote OOM mode %u failed\n", mode);
            return 1;
        }
    }
    puts("shellwords quote runs: 126 vectors, 8 OOM paths, PASS");
    return 0;
}

#include "../shellwords_native.c"
