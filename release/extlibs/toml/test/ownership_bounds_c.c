/* 日期格式边界与正文/关闭双故障分别验证，不能只核对私有错误码。 */
#define XRT_MODULE_ALL
#include "../include/xrt_toml.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
typedef struct { unsigned body, close, closed; size_t offset; xerror *first, *last; } probe;

static bool close_probe(ptr context)
{
    probe* p = context;
    ++p->closed;
    if (p->close != 0) xrtSetError(p->last);
    else xrtClearError();
    return p->close == 0;
}

static bool read_probe(ptr context, ptr data, size_t size, size_t* read)
{
    probe* p = context;
    if (p->first != NULL) { *read = 0; xrtSetError(p->first); return false; }
    cstr text = p->body == 3 ? "value =\n" : "value = 1\n";
    size_t remaining = strlen(text) - p->offset;
    *read = remaining < size ? remaining : size;
    memcpy(data, text + p->offset, *read); p->offset += *read;
    return true;
}

static bool write_probe(ptr context, const void* data, size_t size, size_t* written)
{
    probe* p = context; (void)data;
    if (p->body == 1 || p->body == 2) { *written = 0; xrtSetError(p->first); return false; }
    *written = size;
    return true;
}

static bool flush_probe(ptr context)
{
    probe* p = context;
    if (p->body == 3 || p->body == 4) { xrtSetError(p->first); return false; }
    return true;
}

static bool balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount;
}

static xvalue* table(xvalue* value, cstr key)
{
    xvalue* root = xrtValueObject();
    if (root == NULL || value == NULL || !xrtValueObjectSetTake(root, xrtStrView(key), &value)) {
        xrtValueRelease(root); xrtValueRelease(value); return NULL;
    }
    return root;
}

static bool io_case(bool writer, unsigned body, unsigned close, bool owned, bool with_error)
{
    static const int parse_codes[] = {0,4,2,3,5,4};
    static const int write_codes[] = {0,4,2,4,2,6,5,3};
    static const xerrkind parse_kinds[] = {0,XERR_IO,XERR_MEMORY,0,XERR_RANGE,XERR_STATE};
    static const xerrkind write_kinds[] = {0,XERR_IO,XERR_MEMORY,XERR_IO,XERR_MEMORY,0,0,0};
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    probe p = {body,close,0,0,NULL,NULL};
    xerrkind kind = writer ? write_kinds[body] : parse_kinds[body];
    if (kind != 0) p.first = xrtErrorCreate(kind, "test.toml.body", 711, "body first");
    p.last = xrtErrorCreate(close == 2 ? XERR_MEMORY : XERR_IO, "test.toml.close", 922, "close second");
    xrttomlerror error = {0};
    xrttomloptions options;
    xrtTomlOptionsInit(&options);
    xvalue* root = NULL;
    xvalue* result = NULL;
    xreader* input = NULL;
    xwriter* output = NULL;
    bool success = false, ok = p.last != NULL && (kind == 0 || p.first != NULL);
    int expected = writer ? write_codes[body] : parse_codes[body];
    const xerror* cause = p.first;
    if (body == 0 && owned && close != 0) { expected = close == 2 ? 2 : 4; cause = p.last; }
    if (ok && writer) {
        root = body == 5 ? xrtValueNull() : table(body == 7 ? xrtValueTime(INT64_MAX) : xrtValueInt(1), "value");
        xwriterops ops = {0}; ops.Write = write_probe; ops.Flush = flush_probe; ops.Close = close_probe;
        output = xrtWriterCreate(&ops, &p);
        if (body == 6) options.tLimits.iMaxOutputBytes = 1;
        xrtClearError();
        ok = output != NULL && root != NULL;
        if (ok) { success = xrtTomlWrite(root, output, owned, &options, with_error ? &error : NULL); if (owned) output = NULL; }
    } else if (ok) {
        xreaderops ops = {0}; ops.Read = read_probe; ops.Close = close_probe;
        input = xrtReaderCreate(&ops, &p);
        xrtClearError(); ok = input != NULL;
        if (ok) { result = xrtTomlParse(input, owned, &options, with_error ? &error : NULL); success = result != NULL; if (owned) input = NULL; }
    }
    ok = ok && success == (expected == 0) && (!with_error || error.iCode == expected) &&
        p.closed == (unsigned)owned && xrtGetError() == cause;
    if (!ok && failures < 8) fprintf(stderr, "TOML io writer=%d body=%u close=%u owned=%d errorOut=%d expected=%d actual=%d cause=%d\n",
        (int)writer,body,close,(int)owned,(int)with_error,expected,with_error ? error.iCode : -1,xrtErrorKind(xrtGetError()));
    xrtValueRelease(root); xrtValueRelease(result);
    if (input != NULL) (void)xrtReaderDestroy(input);
    if (output != NULL) (void)xrtWriterDestroy(output);
    xrtErrorFree(p.first); xrtErrorFree(p.last);
    return balanced(before) && ok;
}

static xvalue* time_table(xtime time, unsigned shape)
{
    xvalue* value = xrtValueTime(time);
    if (shape == 0) return table(value, "value");
    if (shape == 1) return table(table(value, "value"), "nested");
    if (shape == 3) value = table(value, "value");
    xvalue* array = xrtValueArray();
    if (array == NULL || value == NULL || !xrtValueArrayAppendTake(array, &value) ||
        (shape == 3 && !xrtValueArrayAppend(array, xrtValueBool(true)))) {
        xrtValueRelease(value); xrtValueRelease(array); return NULL;
    }
    return table(array, "items");
}

static bool time_ok(xvalue* parsed, unsigned shape, xtime time)
{
    xvalue* value;
    if (shape == 0) value = xrtValueObjectGet(parsed, XRT_STR_LITERAL("value"));
    else if (shape == 1) value = xrtValueObjectGet(xrtValueObjectGet(parsed, XRT_STR_LITERAL("nested")), XRT_STR_LITERAL("value"));
    else {
        value = xrtValueArrayGet(xrtValueObjectGet(parsed, XRT_STR_LITERAL("items")), 0);
        if (shape == 3) value = xrtValueObjectGet(value, XRT_STR_LITERAL("value"));
    }
    xrttomldatetime parts;
    xdatetime utc;
    return xrtTomlGetDateTime(value, &parts) && xrtTimeSplit(time, &utc) &&
        parts.iKind == XRT_TOML_OFFSET_DATETIME && parts.iYear == utc.Year &&
        parts.iMonth == utc.Month && parts.iDay == utc.Day && parts.iHour == utc.Hour &&
        parts.iMinute == utc.Minute && parts.iSecond == utc.Second &&
        parts.iMicrosecond == utc.Millisecond * 1000 && parts.iTimezoneMinutes == 0;
}

/* 带参数的桥接声明与源定义在同一编译单元核对。 */
void* xtoml_stringify_text(xvalue*, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, uint32);
int64 xtoml_error_code(void);
xvalue* xtoml_parse(const char*, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long, unsigned long long, uint32);

/* 每个预算由新进程执行，才能真正覆盖尚未初始化的 Once/线程局部键。 */
static int cold(unsigned point)
{
    static const char source[] = "value = 1\n";
    xrtSetErrorInfo(XERR_STATE,"test.toml",0,"warm XRT slot"); xrtClearError();
    if (!xrtMemDebugFailAfter(point)) return 2;
    xvalue* first = xtoml_parse(source,sizeof(source)-1,0,0,0,0,0,0);
    bool triggered = xrtMemDebugFailTriggered();
    bool ok = triggered && xrtValueType(first) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
    xrtMemDebugFailClear(); xrtValueRelease(first);
    xvalue* retry = xtoml_parse(source,sizeof(source)-1,0,0,0,0,0,0);
    int64 number;
    ok = ok && retry != NULL && xrtGetError() == NULL && xtoml_error_code() == XRT_TOML_OK &&
        xrtValueGetInt(xrtValueObjectGet(retry,XRT_STR_LITERAL("value")),&number) && number == 1;
    xrtValueRelease(retry); xrtClearError();
    if (ok) puts("toml cold startup: PASS");
    return ok ? 0 : 1;
}

static bool time_case(xtime time, unsigned shape, bool bridge, bool valid, bool oom)
{
    xvalue* root = time_table(time, shape);
    if (root == NULL) return false;
    bool ok = true, completed = false, failed = false;
    for (unsigned point = 0; point != (oom ? 256u : 1u); ++point) {
        xrttomlerror error;
        xmemdebugsnapshot before;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        if (oom && !xrtMemDebugFailAfter(point)) { ok = false; break; }
        xbuffer* buffer = bridge ? NULL : xrtTomlStringify(root, NULL, &error);
        cstr text = bridge ? xtoml_stringify_text(root,0,0,0,0,0) : (buffer != NULL ? (cstr)buffer->Data : NULL);
        bool triggered = oom && xrtMemDebugFailTriggered();
        if (oom) xrtMemDebugFailClear();
        int code = bridge ? (int)xtoml_error_code() : error.iCode;
        if (triggered) { failed = true; ok = text == NULL && code == XRT_TOML_ERROR_MEMORY; }
        else if (valid) {
            size_t size = bridge ? (text != NULL ? strlen(text) : 0) : (buffer != NULL ? buffer->Size : 0);
            xvalue* parsed = text != NULL ? xrtTomlParseMemory(text, size, NULL, &error) : NULL;
            ok = text != NULL && code == XRT_TOML_OK && parsed != NULL && time_ok(parsed,shape,time);
            xrtValueRelease(parsed);
        } else ok = text == NULL && code == XRT_TOML_ERROR_FORMAT;
        if (!ok && failures < 8) fprintf(stderr,"TOML time=%lld shape=%u bridge=%d point=%u valid=%d code=%d result=%d\n",
            (long long)time,shape,(int)bridge,point,(int)valid,code,text != NULL);
        if (bridge) xrtFree((ptr)text);
        xrtBufferDestroy(buffer);
        ok = balanced(before) && ok;
        if (!ok) break;
        if (!triggered) { completed = true; break; }
    }
    xrtValueRelease(root);
    return ok && completed && (!oom || failed);
}

/* 独立 Python datetime/tomllib 参照只使用其可表示的 0001..9999 年。 */
static int records(void)
{
    static const xtime fixed[] = {0,1,XRT_TIME_UNIX_EPOCH-1001,XRT_TIME_UNIX_EPOCH-1,
        XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+1,INT64_C(315537897599998),INT64_C(315537897599999)};
    uint64 seed = UINT64_C(0x9e3779b97f4a7c15);
    for (unsigned i = 0; i != 520; ++i) {
        xtime time;
        if (i < 8) time = fixed[i];
        else { seed = seed*UINT64_C(6364136223846793005)+1;
            time = (int64)(seed % UINT64_C(315537897600000)); }
        xvalue* root = time_table(time,0);
        xrttomlerror error;
        xbuffer* output = root != NULL ? xrtTomlStringify(root,NULL,&error) : NULL;
        if (output == NULL) { xrtValueRelease(root); return 2; }
        printf("%u\t%lld\t",i,(long long)time);
        for (size_t n = 0; n != output->Size; ++n) printf("%02x",output->Data[n]);
        putchar('\n');
        xrtBufferDestroy(output); xrtValueRelease(root);
    }
    return ferror(stdout) ? 2 : 0;
}

int main(int argc, char** argv)
{
    /* XRT 为公元毫秒；TOML 自身的 year=0/微秒字段由独立日期类型测试覆盖。 */
    static const xtime valid[] = {0,1,XRT_TIME_UNIX_EPOCH-1,XRT_TIME_UNIX_EPOCH,
        XRT_TIME_UNIX_EPOCH+1,INT64_C(315537897599997),INT64_C(315537897599998),INT64_C(315537897599999)};
    static const xtime invalid[] = {INT64_MIN,-1,-2,INT64_C(315537897600000),INT64_C(315537897600001),INT64_MAX};
    if (!xrtMemDebugEnabled()) { fputs("TOML bounds require XRT memory debugging\n",stderr); return 2; }
    if (argc == 2 && strcmp(argv[1],"--records") == 0) return records();
    if (argc == 3 && strcmp(argv[1],"--cold") == 0 && strlen(argv[2]) == 1 && argv[2][0] >= '0' && argv[2][0] <= '3')
        return cold((unsigned)(argv[2][0]-'0'));
    if (argc != 1) { fputs("usage: bounds [--records | --cold 0..3]\n",stderr); return 2; }
    if (xtoml_error_code() != 0) { fputs("TOML error-slot initialization failed\n",stderr); return 2; }
    xrtSetErrorInfo(XERR_STATE,"test.toml",0,"warm slot"); xrtClearError();
    for (unsigned writer = 0; writer != 2; ++writer)
        for (unsigned body = 0; body != (writer ? 8u : 6u); ++body)
            for (unsigned close = 0; close != 3; ++close)
                for (unsigned owned = 0; owned != 2; ++owned)
                    for (unsigned output = 0; output != 2; ++output) {
                        ++checks; if (!io_case(writer != 0,body,close,owned != 0,output != 0)) ++failures;
                    }
    for (unsigned shape = 0; shape != 4; ++shape) for (unsigned bridge = 0; bridge != 2; ++bridge) {
        for (size_t i = 0; i != sizeof(valid)/sizeof(valid[0]); ++i) {
            ++checks; if (!time_case(valid[i],shape,bridge != 0,true,false)) ++failures;
        }
        for (size_t i = 0; i != sizeof(invalid)/sizeof(invalid[0]); ++i) {
            ++checks; if (!time_case(invalid[i],shape,bridge != 0,false,false)) ++failures;
        }
        ++checks; if (!time_case(1,shape,bridge != 0,true,true)) ++failures;
    }
    printf("toml ownership/calendar: %u checks, %u failures\n",checks,failures);
    return failures != 0;
}

#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
#include "../toml_native.c"
