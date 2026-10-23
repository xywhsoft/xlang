#define XRT_MODULE_MEMORY_DEBUG
#include "../xregex_native.c"
#include <assert.h>
#include <stdio.h>

static void unchanged(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.InvalidFreeCount == before->InvalidFreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

static void capture(void* matcher, int64 index, const char* text, size_t size)
{
    xbuffer* result = xregex_matcher_capture_text_native(matcher, index);
    assert(result && result->Size == size);
    assert(size == 0 || memcmp(result->Data, text, size) == 0);
    xrtBufferDestroy(result);
}

/* 成功时必须交出原指针；无法以 C 字符串表达时不能改变缓冲所有权。 */
static void text_transfer(void)
{
    static const unsigned char binary[] = {'a', 0, 'b'};
    xbytesview bytesView = {binary, sizeof(binary)};
    xbuffer* result = xregex_escape_native("a+b?", 4);
    bytes owned;
    void* original;
    void* text;
    assert(result && result->Data && result->Capacity > result->Size);
    original = result->Data;
    text = xregex_buffer_take_cstr_native(result);
    assert(text == original && strcmp((const char*)text, "a\\+b\\?") == 0);
    assert(result->Data == NULL);
    xrtBufferDestroy(result);
    xrtFree(text);

    result = xrtBufferFrom(bytesView);
    assert(result && result->Size == sizeof(binary));
    original = result->Data;
    assert(xregex_buffer_take_cstr_native(result) == NULL);
    assert(result->Data == original && memcmp(result->Data, binary, sizeof(binary)) == 0);
    xrtBufferDestroy(result);

    owned = xrtMalloc(3);
    assert(owned);
    memcpy(owned, "abc", 3);
    result = xrtBufferCreateTake(&owned, 3, 3);
    assert(result && owned == NULL && result->Capacity == result->Size);
    original = result->Data;
    assert(xregex_buffer_take_cstr_native(result) == NULL && result->Data == original);
    xrtBufferDestroy(result);
}

/* 调用结束后立即覆盖并释放输入，之后的捕获和 next 仍能读取完整内容。 */
static void ownership(void)
{
    xregex* regex = xrtRegexCompile(xrtStrView("([a-z]+)-([0-9]+)"));
    void* matcher = xregex_matcher_create_native(regex);
    xregexmatcher* raw = xregex_matcher_native_handle(matcher);
    str input = xrtStrDup("ab-1 cd-22");
    xregexcapture found;
    xbuffer* saved;
    assert(regex && matcher && raw && raw != matcher && input);
    assert(xregex_matcher_capture_count_native(matcher) == 3);
    xrtRegexRelease(regex);
    assert(xregex_matcher_capture_count_native(matcher) == 3);
    assert(xregex_matcher_find_native(matcher, input, strlen(input), 0) == XREGEX_MATCH);
    memset(input, '!', strlen(input));
    xrtFree(input);
    capture(matcher, 0, "ab-1", 4);
    capture(matcher, 1, "ab", 2);
    assert(xrtRegexMatcherCapture(raw, 2, &found));
    assert(found.Text.Size == 1 && found.Text.Data[0] == '1');
    saved = xregex_matcher_capture_text_native(matcher, 0);
    assert(saved);
    assert(xregex_matcher_next_native(matcher) == XREGEX_MATCH);
    capture(matcher, 0, "cd-22", 5);
    assert(xregex_matcher_capture_begin_native(matcher, 0) == 5);
    assert(xregex_matcher_capture_end_native(matcher, 0) == 10);

    /* 参数拒绝和复制失败均保留之前的有效借用，不能提前释放旧文本。 */
    assert(xregex_matcher_find_native(matcher, "x", 1, 2) == XREGEX_ERROR);
    xrtClearError();
    capture(matcher, 0, "cd-22", 5);
    assert(xrtMemDebugFailAfter(0));
    assert(xregex_matcher_full_native(matcher, "xy-7", 4) == XREGEX_ERROR);
    assert(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xrtClearError();
    capture(matcher, 0, "cd-22", 5);
    assert(xregex_matcher_at_native(matcher, "xy-7", 4, 0) == XREGEX_MATCH);
    capture(matcher, 0, "xy-7", 4);
    assert(xregex_matcher_full_native(matcher, "none", 4) == XREGEX_NONE);
    assert(!xregex_matcher_matched_native(matcher));
    assert(xregex_matcher_full_native(matcher, "z-3", 3) == XREGEX_MATCH);
    xregex_matcher_destroy_native(matcher);
    assert(saved->Size == 4 && memcmp(saved->Data, "ab-1", 4) == 0);
    xrtBufferDestroy(saved);
    xregex_matcher_destroy_native(NULL);
    assert(xregex_matcher_native_handle(NULL) == NULL);
}

/* 明确长度 C 输入允许零字节，不能被 strlen 截断。 */
static void binary_and_empty(void)
{
    static const char binary[] = {'a', 0, 'b'};
    xregex* regex = xrtRegexCompile(xrtStrView("a\\x00b"));
    void* matcher = xregex_matcher_create_native(regex);
    assert(regex && matcher);
    xrtRegexRelease(regex);
    assert(xregex_matcher_full_native(matcher, binary, sizeof(binary)) == XREGEX_MATCH);
    capture(matcher, 0, binary, sizeof(binary));
    xregex_matcher_destroy_native(matcher);
    regex = xrtRegexCompile(xrtStrView(""));
    matcher = xregex_matcher_create_native(regex);
    assert(regex && matcher);
    xrtRegexRelease(regex);
    assert(xregex_matcher_full_native(matcher, NULL, 0) == XREGEX_MATCH);
    capture(matcher, 0, "", 0);
    assert(xregex_matcher_next_native(matcher) == XREGEX_NONE);
    xregex_matcher_destroy_native(matcher);
}

static void splitter_handle(void)
{
    xregex* regex = xrtRegexCompile(xrtStrView(","));
    str input = xrtStrDup("left,right");
    void* splitter = xregex_splitter_create_native(regex, input, 10, -1, 0);
    xregexsplitter* raw = xregex_splitter_native_handle(splitter);
    xregexsplitpart part;
    xbuffer* text;
    assert(regex && input && splitter && raw && raw != splitter);
    xrtRegexRelease(regex);
    memset(input, '!', 10);
    xrtFree(input);
    assert(xrtRegexSplitterNext(raw, &part) == XREGEX_MATCH);
    assert(part.Text.Size == 4 && memcmp(part.Text.Data, "left", 4) == 0);
    assert(xregex_splitter_next_native(splitter) == XREGEX_MATCH);
    text = xregex_splitter_text_native(splitter);
    assert(text && text->Size == 5 && memcmp(text->Data, "right", 5) == 0);
    xrtBufferDestroy(text);
    assert(xregex_splitter_next_native(splitter) == XREGEX_NONE);
    xregex_splitter_destroy_native(splitter);
    xregex_splitter_destroy_native(NULL);
    assert(xregex_splitter_native_handle(NULL) == NULL);
}

/* 覆盖构造、输入副本、执行缓存与捕获交付中的每个分配点。 */
static unsigned int allocation_failures(void)
{
    unsigned int i, injected = 0, success = 0;
    xregex* regex = xrtRegexCompile(xrtStrView("([a-z]+)-([0-9]+)"));
    xmemdebugsnapshot before;
    assert(regex);
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < 256 && success < 2; ++i) {
        void* matcher;
        xbuffer* text = NULL;
        bool failed;
        assert(xrtMemDebugFailAfter(i));
        matcher = xregex_matcher_create_native(regex);
        if (matcher && xregex_matcher_find_native(matcher, "ab-1 cd-22", 10, 0) == XREGEX_MATCH)
            text = xregex_matcher_capture_text_native(matcher, 0);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (failed) {
            assert(text == NULL);
            ++injected;
        } else {
            assert(matcher && text && text->Size == 4);
            ++success;
        }
        xrtBufferDestroy(text);
        xregex_matcher_destroy_native(matcher);
        unchanged(&before);
    }
    assert(injected > 0 && success == 2);
    xrtRegexRelease(regex);
    return injected;
}

/* set 编译必须借用公开 xvalue 合同，释放原始数组和 set 后 matcher 仍有效。 */
static void set_ownership(void)
{
    xvalue* patterns = xrtValueArray();
    void* set;
    void* matcher;
    xregex* saved;
    str input = xrtStrDup("error 42");
    assert(patterns && input);
    assert(xrtValueArrayAppendNew(patterns, xrtValueString(xrtStrView("error"))));
    assert(xrtValueArrayAppendNew(patterns, xrtValueString(xrtStrView("[0-9]+"))));
    set = xregex_set_compile_native(patterns, 0, XREGEX_PATTERN_DEFAULT, XREGEX_CAPTURES_DEFAULT);
    assert(set && xregex_set_native_handle(set) == set);
    saved = xregex_set_regex_native(set, 1);
    matcher = xregex_set_matcher_create_native(set);
    assert(saved && matcher);
    xrtValueRelease(patterns);
    xregex_set_release_native(set);
    assert(xregex_set_matcher_match_native(matcher, input, strlen(input), 0) == XREGEX_MATCH);
    memset(input, '!', strlen(input));
    xrtFree(input);
    assert(xregex_set_matcher_count_native(matcher) == 2);
    assert(xregex_set_matcher_first_native(matcher) == 0);
    assert(xregex_set_matcher_index_native(matcher, 1) == 1);
    assert(xrtRegexSetMatcherMatched(matcher, 1));
    assert(xrtRegexFullTest(saved, xrtStrView("123")) == XREGEX_MATCH);
    xregex_set_matcher_destroy_native(matcher);
    xrtRegexRelease(saved);

    /* 不能把含非字符串值的动态数组按字符串指针数组解释。 */
    patterns = xrtValueArray();
    assert(patterns && xrtValueArrayAppendNew(patterns, xrtValueInt(7)));
    assert(!xregex_set_compile_native(patterns, 0, XREGEX_PATTERN_DEFAULT, XREGEX_CAPTURES_DEFAULT));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    xrtValueRelease(patterns);
}

/* 拆分、转义、替换和集合编译同样逐个注入分配失败，不能只验证 matcher。 */
static unsigned int other_allocation_failures(void)
{
    xregex* regex = xrtRegexCompile(xrtStrView("([0-9]+)"));
    xvalue* patterns = xrtValueArray();
    xmemdebugsnapshot before;
    unsigned int kind, injected = 0;
    assert(regex && patterns);
    assert(xrtValueArrayAppendNew(patterns, xrtValueString(xrtStrView("[0-9]+"))));
    assert(xrtValueArrayAppendNew(patterns, xrtValueString(xrtStrView("[a-z]+"))));
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 4; ++kind) {
        unsigned int i, successes = 0;
        for (i = 0; i < 1024 && successes < 2; ++i) {
            void* owner = NULL;
            xbuffer* text = NULL;
            bool ok = false, failed;
            assert(xrtMemDebugFailAfter(i));
            if (kind == 0) {
                owner = xregex_splitter_create_native(regex, "a1b2", 4, -1, XREGEX_SPLIT_CAPTURES);
                if (owner && xregex_splitter_next_native(owner) == XREGEX_MATCH)
                    text = xregex_splitter_text_native(owner);
                ok = text != NULL;
            } else if (kind == 1) {
                text = xregex_escape_native("a+b?", 4);
                ok = text != NULL;
            } else if (kind == 2) {
                text = xregex_replace_native(regex, "a1b2", 4, "<$1>", 4, -1);
                ok = text != NULL;
            } else {
                owner = xregex_set_compile_native(patterns, 0, XREGEX_PATTERN_DEFAULT, XREGEX_CAPTURES_DEFAULT);
                ok = owner != NULL;
            }
            failed = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (failed) { assert(!ok); ++injected; }
            else { assert(ok); ++successes; }
            xrtBufferDestroy(text);
            if (kind == 0) xregex_splitter_destroy_native(owner);
            if (kind == 3) xregex_set_release_native(owner);
            unchanged(&before);
        }
        assert(successes == 2);
    }
    xrtValueRelease(patterns);
    xrtRegexRelease(regex);
    return injected;
}

/* 错误位置查询借用显式对象；不读取或覆盖当前线程诊断。 */
static void error_details(void)
{
    xerror* saved;
    assert(!xregex_compile_native("(", 1, 0, XREGEX_PATTERN_DEFAULT, XREGEX_CAPTURES_DEFAULT));
    saved = xrtTakeError();
    assert(saved && xregex_error_offset_native(saved) >= 0);
    assert(xregex_error_pattern_index_native(saved) == -1);
    assert(xregex_valid_native("ok", 2));
    assert(xregex_error_offset_native(saved) >= 0);
    assert(!xrtGetError());
    xrtErrorFree(saved);
    assert(xregex_error_offset_native(NULL) == -1);
    assert(xregex_error_pattern_index_native(NULL) == -1);
}

/* valid 只抑制语法拒绝，不能把参数、限额和分配故障伪装成非法模式。 */
static unsigned int validation_errors(void)
{
    xmemdebugsnapshot before;
    unsigned int i, rejected = 0, success = 0;
    str large = xrtMalloc(XREGEX_PATTERN_DEFAULT + 1u);
    assert(large);
    memset(large, 'a', XREGEX_PATTERN_DEFAULT + 1u);
    assert(!xregex_valid_native(large, XREGEX_PATTERN_DEFAULT + 1u));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    xrtFree(large);
    assert(!xregex_valid_native(NULL, 1));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    assert(!xregex_valid_native("(", 1));
    assert(xrtGetError() == NULL);
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < 256 && success < 2; ++i) {
        bool valid, failed;
        assert(xrtMemDebugFailAfter(i));
        valid = xregex_valid_native("(a|b)+", 6);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (failed) {
            assert(!valid);
            assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else {
            assert(valid && xrtGetError() == NULL);
            ++success;
        }
        unchanged(&before);
    }
    assert(rejected > 0 && success == 2);
    return rejected;
}

int main(void)
{
    xmemdebugsnapshot before;
    unsigned int rejected;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    text_transfer();
    ownership();
    binary_and_empty();
    splitter_handle();
    set_ownership();
    error_details();
    rejected = allocation_failures();
    rejected += other_allocation_failures();
    rejected += validation_errors();
    unchanged(&before);
    printf("xregex native ownership: %u allocation failures checked\n", rejected);
    return 0;
}
