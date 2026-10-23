#include "../include/aho_corasick_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 原生桥也必须满足同一错误类别、所有权和流状态合同。 */
static void error_is(xerrkind kind)
{
    assert(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == kind);
    assert(xac_last_error()[0] != 0);
    xrtClearError();
}

static void bytes_are(void* raw, const char* expected, size_t size)
{
    xbuffer* bytes = raw;
    assert(bytes != NULL && bytes->Size == size);
    assert(size == 0 || memcmp(bytes->Data, expected, size) == 0);
    xrtBufferDestroy(bytes);
}

static void* automaton(uint64 matches, uint64 input, uint64 output)
{
    void* ac = xac_create(0, 0, 0, 0, 0, matches, input, output);
    assert(ac != NULL);
    assert(xac_add_pattern(ac, "a", 1) && xac_add_pattern(ac, "ab", 2));
    assert(xac_build(ac) && xac_build(ac));
    return ac;
}

static void results(void)
{
    void* ac = automaton(0, 0, 0);
    void* result = xac_search(ac, "zab", 3);
    void* first = xac_find(ac, "zab", 3);
    void* missing = xac_find(ac, "zzz", 3);
    assert(xac_result_count(result) == 2 && xac_result_count(first) == 1);
    assert(xac_result_count(missing) == 0 && xac_result_count(NULL) == 0);
    assert(xac_pattern_count(ac) == 2 && xac_is_built(ac));
    bytes_are(xac_pattern(ac, 1), "ab", 2);
    assert(xac_pattern(ac, -1) == NULL); error_is(XERR_RANGE);
    assert(xac_result_start(result, -1) == -1); error_is(XERR_RANGE);
    assert(xac_result_end(result, INT64_MAX) == -1); error_is(XERR_RANGE);
    assert(xac_result_pattern(result, -1) == NULL); error_is(XERR_RANGE);
    assert(xac_result_pattern(NULL, 0) == NULL); error_is(XERR_ARGUMENT);
    assert(xac_result_pattern(missing, 0) == NULL); error_is(XERR_RANGE);
    xac_destroy(ac);
    bytes_are(xac_result_pattern(result, 1), "ab", 2);
    bytes_are(xac_result_pattern(first, 0), "a", 1);
    assert(xac_result_start(first, 0) == 1 && xac_result_end(first, 0) == 2);
    xac_result_destroy(missing);
    xac_result_destroy(first);
    xac_result_destroy(result);
    xac_result_destroy(NULL);

    /* C 视图是明确长度的二进制，模式中的零字节不能被截断。 */
    ac = xac_create(0, 0, 0, 0, 0, 0, 0, 0);
    assert(ac && xac_add_pattern(ac, "a\0b", 3) && xac_build(ac));
    result = xac_search(ac, "xa\0by", 5);
    assert(result && xac_result_count(result) == 1);
    bytes_are(xac_result_pattern(result, 0), "a\0b", 3);
    assert(xac_result_start(result, 0) == 1 && xac_result_end(result, 0) == 4);
    xac_result_destroy(result);
    xac_destroy(ac);
}

static void streams(void)
{
    void* ac = automaton(0, 5, 0);
    void* stream = xac_stream_create(ac);
    xac_destroy(ac);
    assert(stream && xac_stream_offset(stream) == 0 && !xac_stream_pending(stream));
    assert(xac_stream_start(stream) == -1); error_is(XERR_STATE);
    assert(xac_stream_feed(stream, "a", 1) && xac_stream_next(stream) == 1);
    bytes_are(xac_stream_pattern(stream), "a", 1);
    assert(xac_stream_feed(stream, "b", 1) == 0); error_is(XERR_STATE);
    assert(xac_stream_next(stream) == 0 && !xac_stream_pending(stream));
    assert(xac_stream_pattern(stream) == NULL); error_is(XERR_STATE);
    assert(xac_stream_feed(stream, "b", 1) && xac_stream_next(stream) == 1);
    assert(xac_stream_start(stream) == 0 && xac_stream_end_pos(stream) == 2);
    assert(xac_stream_next(stream) == 0 && xac_stream_matches(stream) == 2);
    /* 超预算分块必须在分配/复制前拒绝；已有状态保持不变。 */
    assert(!xac_stream_feed(stream, "xxxx", 4)); error_is(XERR_RANGE);
    assert(xac_stream_offset(stream) == 2 && !xac_stream_pending(stream));
    assert(xac_stream_feed(stream, "xxx", 3) && xac_stream_next(stream) == 0);
    assert(xac_stream_offset(stream) == 5);
    assert(xac_stream_enter(stream));
    assert(!xac_stream_enter(stream)); error_is(XERR_STATE);
    assert(!xac_stream_feed(stream, "", 0)); error_is(XERR_STATE);
    xac_stream_reset(stream); error_is(XERR_STATE);
    assert(xac_stream_offset(stream) == 5);
    xac_stream_leave(stream);
    xac_stream_reset(stream);
    assert(xac_stream_offset(stream) == 0 && xac_stream_matches(stream) == 0);
    assert(xac_stream_feed(stream, NULL, 0) && xac_stream_next(stream) == 0);
    xac_stream_destroy(stream);

    ac = automaton(1, 0, 0);
    stream = xac_stream_create(ac);
    assert(xac_stream_feed(stream, "aba", 3) && xac_stream_next(stream) == 1);
    assert(xac_stream_next(stream) == -1); error_is(XERR_RANGE);
    assert(!xac_stream_pending(stream) && xac_stream_offset(stream) == 2);
    assert(xac_stream_next(stream) == -1); error_is(XERR_STATE);
    assert(!xac_stream_feed(stream, "a", 1)); error_is(XERR_STATE);
    xac_stream_reset(stream);
    assert(xac_stream_feed(stream, "a", 1) && xac_stream_next(stream) == 1);
    assert(xac_stream_next(stream) == 0);
    xac_stream_destroy(stream);

    /* 核心 C 回调路径失败时也报告真实已消费字节，不能让调用方重放前缀。 */
    xrtahostate* state = xrtAhoStateCreate(ac);
    size_t consumed = 99;
    assert(xrtAhoStateFeed(state, XRT_STR_LITERAL("aba"), NULL, NULL, &consumed) == XRT_AHO_SCAN_ERROR);
    assert(consumed == 2 && xrtAhoStateOffset(state) == 2);
    error_is(XERR_RANGE);
    xrtAhoStateDestroy(state);
    xac_destroy(ac);
}

static void transforms(void)
{
    void* ac = automaton(0, 0, 4);
    bytes_are(xac_replace(ac, "zab", 3, "*", 1), "z*", 2);
    bytes_are(xac_mask(ac, "zab", 3, "#", 1), "z##", 3);
    xbuffer* plain = xac_replace(ac, "zab", 3, "*", 1);
    void* original = plain->Data;
    char* taken = xac_buffer_take_cstr(plain);
    assert(taken == original && strcmp(taken, "z*") == 0);
    assert(plain->Data == NULL && plain->Size == 0);
    xrtFree(taken);
    xrtBufferDestroy(plain);

    xbuffer* with_nul = xrtBufferCreate();
    xbytesview nul_text = { (const unsigned char*)"a\0b", 3 };
    assert(with_nul && xrtBufferAppend(with_nul, nul_text));
    assert(xac_buffer_take_cstr(with_nul) == NULL);
    assert(with_nul->Size == 3 && memcmp(with_nul->Data, "a\0b", 3) == 0);
    xrtBufferDestroy(with_nul);

    bytes exact_data = xrtMalloc(3);
    assert(exact_data != NULL);
    memcpy(exact_data, "abc", 3);
    xbuffer* exact = xrtBufferCreateTake(&exact_data, 3, 3);
    assert(exact && xac_buffer_take_cstr(exact) == NULL);
    assert(exact->Size == 3 && memcmp(exact->Data, "abc", 3) == 0);
    xrtBufferDestroy(exact);

    assert(xac_highlight(ac, "zab", 3, "<", 1, ">", 1) == NULL); error_is(XERR_RANGE);
    assert(xac_replace(ac, "xxxxx", 5, "", 0) == NULL); error_is(XERR_RANGE);
    xbuffer* output = xrtBufferCreate();
    assert(output && xac_output_append(ac, output, "012345", 6, 1, 4));
    assert(!xac_output_append(ac, output, "x", 1, 0, 1)); error_is(XERR_RANGE);
    assert(output->Size == 4 && memcmp(output->Data, "1234", 4) == 0);
    assert(!xac_output_append(ac, output, "x", 1, 2, 0)); error_is(XERR_RANGE);
    assert(xac_output_append(ac, output, NULL, 0, 0, 0));
    xrtBufferDestroy(output);
    xac_destroy(ac);

    ac = automaton(0, 0, 0);
    void* scan = xac_scan_begin(ac, "aab", 3);
    assert(scan && xac_scan_pattern(scan) == NULL); error_is(XERR_STATE);
    assert(xac_scan_next(scan) == 1 && xac_scan_start(scan) == 0 && xac_scan_end_pos(scan) == 1);
    bytes_are(xac_scan_pattern(scan), "a", 1);
    assert(xac_scan_next(scan) == 1 && xac_scan_next(scan) == 1);
    assert(xac_scan_next(scan) == 0 && xac_scan_next(scan) == 0);
    assert(xac_scan_end_pos(scan) == -1); error_is(XERR_STATE);
    xac_scan_end(scan);
    void* result = xac_search_non_overlapping(ac, "ab", 2);
    assert(result && xac_result_count(result) == 1);
    bytes_are(xac_result_pattern(result, 0), "ab", 2);
    xac_result_destroy(result);
    assert(xac_contains(ac, NULL, 0) == 0 && xac_count(ac, "aab", 3) == 3);
    assert(xac_contains(ac, NULL, 1) == -1); error_is(XERR_ARGUMENT);
    xac_destroy(ac);
    xac_scan_end(NULL);
    xac_stream_destroy(NULL);
    puts("aho native: ownership, result states, limits and stream recovery passed");
}

int main(void)
{
    results();
    streams();
    transforms();
    return 0;
}
