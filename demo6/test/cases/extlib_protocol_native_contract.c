#include "xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 实际生成的两个 ABI 头由门禁注入，同时与各 C 实现共同验证。 */

static void* create_object(int kind)
{
    switch (kind) {
    case 0: return xhttp_protocol_headers_parse("X-Test: one\r\n\r\n", 15);
    case 1: return xhttp_protocol_cookies_parse("sid=one; theme=dark", 19);
    case 2: return xhttp_protocol_set_cookie_parse("sid=one; Secure", 15);
    default: return xhttp_protocol_media_parse("text/plain; charset=utf-8", 25);
    }
}

static void destroy_object(int kind, void* handle)
{
    switch (kind) {
    case 0: xhttp_protocol_headers_destroy(handle); break;
    case 1: xhttp_protocol_cookies_destroy(handle); break;
    case 2: xhttp_protocol_set_cookie_destroy(handle); break;
    default: xhttp_protocol_media_destroy(handle); break;
    }
}

static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount);
    assert(after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
}

/* 只有非法内容返回 false；C 参数错误不能被谓词的结果合同吞掉。 */
static void predicate(void)
{
    const char* invalid[] = {"", "missing", "sid=x; SameSite=None", "sid=x\r\nBad: y"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(!xhttp_protocol_set_cookie_validate(invalid[i], strlen(invalid[i])));
        assert(xrtGetError() == NULL);
        assert(xhttp_protocol_set_cookie_validate("sid=x; Secure", 13));
        assert(xrtGetError() == NULL);
    }
    assert(!xhttp_protocol_set_cookie_validate(NULL, 1));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
}

/* 构造中途分配失败也必须清理部分对象；析构不能覆盖调用者正在处理的错误。 */
static void allocations(void)
{
    for (int kind = 0; kind < 4; ++kind) {
        bool reached_failure = false;
        bool reached_success = false;
        for (int fail = 0; fail < 24; ++fail) {
            xmemdebugsnapshot before;
            xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(fail));
            void* object = create_object(kind);
            bool triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                reached_failure = true;
                assert(object == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            } else {
                reached_success = true;
                assert(object != NULL && xrtGetError() == NULL);
            }
            xrtSetErrorKind(XERR_TIMEOUT);
            destroy_object(kind, object);
            assert(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT);
            balanced(before);
        }
        assert(reached_failure && reached_success);
    }
}

/* Chunk 的租约独立于 Body 和 Reader；二进制输入不经零结尾文本。 */
static void body_lifetime(void)
{
    unsigned char data[] = {0, 255, 42};
    void* body = xhttp_body_create_copy(data, sizeof(data));
    assert(body != NULL);
    void* reader = xhttp_body_open(body);
    assert(reader != NULL);
    void* chunk = xhttp_body_reader_next(reader, sizeof(data));
    assert(chunk != NULL);
    xhttp_body_destroy(body);
    xhttp_body_reader_destroy(reader);
    assert(xhttp_body_chunk_size(chunk) == sizeof(data));
    assert(memcmp(xhttp_body_chunk_data(chunk), data, sizeof(data)) == 0);
    xhttp_body_chunk_destroy(chunk);
    xhttp_body_destroy(NULL);
    xhttp_body_reader_destroy(NULL);
    xhttp_body_chunk_destroy(NULL);
}

int main(void)
{
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    predicate();
    allocations();
    body_lifetime();
    balanced(before);
    puts("HTTP C native contract: predicate, 96 allocation attempts and independent chunk lifetime PASS");
    return 0;
}
