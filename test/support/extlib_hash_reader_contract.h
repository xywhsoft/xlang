/* 三个普通哈希扩展共用的测试合同；不是生产依赖，也不替换 XRT Reader。 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef union { uint64 Word; uint8 Bytes[128]; } hash_output;
typedef struct { int Code, Backend; uint64 Offset; char Message[256]; } hash_detail;
static bool test_hash_file(unsigned variant, xreader* reader, hash_output* output, xrtprogressproc progress, ptr user, hash_detail* detail);
static bool test_hash_memory(unsigned variant, const void* data, size_t size, hash_output* output);

enum { FAIL_TELL, FAIL_SIZE, FAIL_READ_FIRST, FAIL_READ_LATE, FAIL_PROGRESS_FIRST, FAIL_PROGRESS_FINAL,
    CONTROL_TELL_UNSUPPORTED, CONTROL_SIZE_UNSUPPORTED, CONTROL_NO_TELL, CONTROL_NO_SIZE, CONTROL_NO_SEEK, CONTROL_PAST_END };
typedef struct {
    int Mode;
    size_t Position, Start, Size, FailureOffset;
    unsigned Reads, Tells, Sizes, Seeks, Progress, Closed;
    bool Recover, Known;
    xerror* Cause;
} hash_reader_probe;
static const uint8 hash_input[] = "abcdefghi";
static unsigned contract_cases, contract_checks, contract_failures, guarded_failures, real_oom_cases;

static void require_contract(bool ok, cstr message)
{
    ++contract_checks;
    if (!ok) { if (contract_failures < 12) fprintf(stderr, "%s reader contract: %s\n", HASH_TEST_NAME, message); ++contract_failures; }
}

static bool contract_memory(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

static bool contract_message(const char text[256], cstr original)
{
    size_t size = strlen(original);
    if (size > 255) size = 255;
    return memchr(text, 0, 256) == text + size && memcmp(text, original, size) == 0;
}

static bool publish_cause(hash_reader_probe* probe)
{
    xrtSetError(probe->Cause);
    probe->FailureOffset = probe->Position - probe->Start;
    require_contract(xrtMemDebugFailAfter(0), "arm no-allocation diagnostic guard");
    ++guarded_failures;
    return false;
}

static bool contract_read(ptr context, ptr data, size_t requested, size_t* count)
{
    hash_reader_probe* p = context;
    ++p->Reads; *count = 0;
    if (!p->Recover && (p->Mode == FAIL_READ_FIRST || (p->Mode == FAIL_READ_LATE && p->Reads > 1))) return publish_cause(p);
    size_t left = p->Position < p->Size ? p->Size - p->Position : 0;
    *count = left < requested ? left : requested;
    if (*count > 3) *count = 3;
    if (*count) memcpy(data, hash_input + p->Position, *count);
    p->Position += *count;
    return true;
}

static bool contract_tell(ptr context, uint64* position)
{
    hash_reader_probe* p = context; ++p->Tells;
    if (!p->Recover && (p->Mode == FAIL_TELL || p->Mode == CONTROL_TELL_UNSUPPORTED)) return publish_cause(p);
    *position = p->Position; return true;
}

static bool contract_size(ptr context, uint64* size)
{
    hash_reader_probe* p = context; ++p->Sizes;
    if (!p->Recover && (p->Mode == FAIL_SIZE || p->Mode == CONTROL_SIZE_UNSUPPORTED)) return publish_cause(p);
    *size = p->Size; return true;
}

static bool contract_seek(ptr context, int64 offset, xseek origin, uint64* position)
{
    hash_reader_probe* p = context; ++p->Seeks;
    if (origin != XSEEK_START || offset < 0) return false;
    p->Position = (size_t)offset;
    if (position) *position = p->Position;
    return true;
}

static bool contract_close(ptr context) { ++((hash_reader_probe*)context)->Closed; return true; }

static bool contract_progress(const xrtprogress* progress, ptr context)
{
    hash_reader_probe* p = context; ++p->Progress;
    require_contract(progress->iInputBytes == p->Position - p->Start && progress->iOutputBytes == 0, "progress relative byte counters");
    require_contract(((progress->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0) == p->Known &&
        progress->iTotalInputBytes == (p->Known ? p->Size - p->Start : 0), "known versus unavailable metadata flags");
    if (!p->Recover && (p->Mode == FAIL_PROGRESS_FIRST ||
        (p->Mode == FAIL_PROGRESS_FINAL && (progress->iFlags & XRT_PROGRESS_FINAL)))) return publish_cause(p);
    return true;
}

static void contract_case(unsigned variant, int mode, xerrkind kind, int backend, bool snapshot, unsigned start, bool long_message)
{
    hash_reader_probe p = {0};
    xreaderops ops = {contract_read, contract_seek, contract_tell, contract_size, contract_close};
    hash_output output, expected, sentinel;
    hash_detail detail;
    char message[301];
    bool failed = mode <= FAIL_PROGRESS_FINAL;
    p.Mode = mode; p.Size = sizeof(hash_input) - 1; p.Start = p.Position = start;
    p.Known = mode < CONTROL_TELL_UNSUPPORTED;
    if (mode == CONTROL_NO_TELL) ops.Tell = NULL;
    if (mode == CONTROL_NO_SIZE) ops.Size = NULL;
    if (mode == CONTROL_NO_SEEK) ops.Seek = NULL;
    if (mode == CONTROL_PAST_END) p.Start = p.Position = p.Size + 2;
    if (long_message) { memset(message, 'x', sizeof(message)-1); message[sizeof(message)-1] = 0; }
    else message[0] = 0;
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    xrtSetErrorInfo(kind, "test.hash.reader", backend, message);
    p.Cause = xrtTakeError();
    xreader* reader = xrtReaderCreate(&ops, &p);
    require_contract(reader != NULL && p.Cause != NULL, "create owned test Reader/cause");
    if (!reader || !p.Cause) { xrtReaderDestroy(reader); xrtErrorFree(p.Cause); return; }
    memset(&output, 0xa5, sizeof(output)); sentinel = output; expected = output;
    size_t remaining = p.Start < p.Size ? p.Size - p.Start : 0;
    const void* input = remaining ? hash_input + p.Start : NULL;
    require_contract(test_hash_memory(variant, input, remaining, &expected), "prepare normal digest reference");
    xrtClearError();
    bool ok = test_hash_file(variant, reader, &output, contract_progress, &p, snapshot ? &detail : NULL);
    bool allocated = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    require_contract(!allocated && p.Closed == 0 && p.Seeks == 0, "diagnostics do not allocate, close or seek borrowed Reader");
    if (failed) {
        require_contract(!ok && memcmp(&output, &sentinel, sizeof(output)) == 0, "failure leaves complete output/canary unchanged");
        require_contract(xrtGetError() == p.Cause && xrtErrorKind(xrtGetError()) == kind && xrtErrorCode(xrtGetError()) == backend &&
            strcmp(xrtErrorDomain(xrtGetError()), "test.hash.reader") == 0 && strcmp(xrtErrorMessage(xrtGetError()), message) == 0,
            "original error identity/domain/code/message preserved");
        if (mode == FAIL_TELL || mode == FAIL_SIZE)
            require_contract(p.Position == p.Start && p.Reads == 0 && p.Progress == 0 &&
                p.Tells == 1 && p.Sizes == (unsigned)(mode == FAIL_SIZE), "failed metadata stops before input/progress");
        if (snapshot) {
            int code = mode >= FAIL_PROGRESS_FIRST ? HASH_TEST_CANCELLED : kind == XERR_MEMORY ? HASH_TEST_MEMORY : HASH_TEST_IO;
            size_t length = long_message ? 255 : 0;
            const char* end = memchr(detail.Message, 0, sizeof(detail.Message));
            bool exact = detail.Code == code && detail.Backend == backend && detail.Offset == p.FailureOffset &&
                contract_message(detail.Message, message);
            if (!exact && contract_failures < 3) fprintf(stderr, "snapshot mode=%d kind=%d backend=%d actual=%d/%d/%llu NUL=%d expected=%u\n",
                mode, (int)kind, backend, detail.Code, detail.Backend, (unsigned long long)detail.Offset,
                end ? (int)(end-detail.Message) : -1, (unsigned)length);
            require_contract(exact, "C snapshot preserves category/backend/offset/bounded message");
        }
    } else {
        require_contract(ok && memcmp(&output, &expected, sizeof(output)) == 0 && xrtGetError() == NULL,
            "unsupported/missing metadata or past-end remains a valid stream");
        if (snapshot) require_contract(detail.Code == 0 && detail.Backend == 0 && detail.Message[0] == 0, "successful fallback clears detail");
    }
    /* 恢复是调用方的显式定位，不把失败或摘要读取变成隐式回滚。 */
    p.Recover = true; p.Known = mode != CONTROL_NO_TELL && mode != CONTROL_NO_SIZE && mode != CONTROL_NO_SEEK && mode != CONTROL_PAST_END;
    xrtClearError();
    if (ops.Seek) require_contract(xrtReaderSeek(reader, (int64)p.Start, XSEEK_START, NULL), "explicit rewind same Reader");
    else {
        p.Start = p.Position; expected = sentinel;
        require_contract(test_hash_memory(variant, NULL, 0, &expected), "non-seeking Reader stays at EOF");
    }
    output = sentinel;
    require_contract(test_hash_file(variant, reader, &output, contract_progress, &p, snapshot ? &detail : NULL) &&
        memcmp(&output, &expected, sizeof(output)) == 0 && p.Closed == 0, "same Reader operation recovers");
    require_contract(xrtReaderDestroy(reader) && p.Closed == 1, "caller closes borrowed Reader exactly once");
    xrtClearError(); xrtErrorFree(p.Cause);
    require_contract(contract_memory(&before), "case and diagnostic owner memory balanced");
    ++contract_cases;
}

/* 与预置错误不同，这里让 Tell/Size 内的真实 XRT 分配失败。 */
static bool metadata_allocation(ptr context, uint64* value)
{
    hash_reader_probe* p = context;
    if (p->Mode == FAIL_TELL) ++p->Tells; else ++p->Sizes;
    if (!p->Recover) {
        void* block = xrtMalloc(8);
        if (block == NULL) { p->Cause = xrtErrorRef(xrtGetError()); return false; }
        xrtFree(block);
    }
    *value = p->Mode == FAIL_TELL ? p->Position : p->Size;
    return true;
}

static void metadata_oom(unsigned variant, int mode, bool snapshot)
{
    hash_reader_probe p = {0};
    p.Mode = mode; p.Size = sizeof(hash_input)-1; p.Known = true;
    xreaderops ops = {contract_read, contract_seek, contract_tell, contract_size, contract_close};
    if (mode == FAIL_TELL) ops.Tell = metadata_allocation; else ops.Size = metadata_allocation;
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    xreader* reader = xrtReaderCreate(&ops, &p);
    require_contract(reader != NULL, "create actual metadata OOM Reader");
    if (reader == NULL) return;
    hash_output output, sentinel, expected;
    hash_detail detail;
    memset(&output, 0xa5, sizeof(output)); sentinel = output; expected = output;
    require_contract(test_hash_memory(variant, hash_input, p.Size, &expected), "prepare actual OOM reference");
    xrtClearError(); require_contract(xrtMemDebugFailAfter(0), "arm real metadata allocation failure");
    bool ok = test_hash_file(variant, reader, &output, contract_progress, &p, snapshot ? &detail : NULL);
    bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    require_contract(triggered && !ok && p.Cause != NULL && xrtGetError() == p.Cause && xrtErrorKind(p.Cause) == XERR_MEMORY,
        "actual metadata OOM must remain the original failure");
    require_contract(p.Reads == 0 && p.Progress == 0 && p.Position == 0 && p.Closed == 0 &&
        memcmp(&output, &sentinel, sizeof(output)) == 0, "actual metadata OOM cannot consume input or publish digest");
    if (snapshot) require_contract(detail.Code == HASH_TEST_MEMORY && detail.Backend == xrtErrorCode(p.Cause) && detail.Offset == 0 &&
        contract_message(detail.Message, xrtErrorMessage(p.Cause)), "actual metadata OOM C snapshot");
    p.Recover = true; xrtClearError();
    require_contract(test_hash_file(variant, reader, &output, contract_progress, &p, snapshot ? &detail : NULL) &&
        memcmp(&output, &expected, sizeof(output)) == 0 && p.Closed == 0, "retry failed metadata without rewind or reopening");
    require_contract(xrtReaderDestroy(reader) && p.Closed == 1, "actual OOM Reader caller cleanup");
    xrtClearError(); xrtErrorFree(p.Cause);
    require_contract(contract_memory(&before), "actual metadata OOM memory balanced");
    ++real_oom_cases; ++contract_cases;
}

int main(void)
{
    static const xerrkind kinds[] = {XERR_MEMORY, XERR_IO, XERR_VALUE};
    static const int backends[] = {INT_MIN, -17, INT_MAX};
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.hash.reader", 0, "warm diagnostic"); xrtClearError();
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned variant = 0; variant < HASH_TEST_VARIANTS; ++variant) {
        for (int mode = FAIL_TELL; mode <= FAIL_PROGRESS_FINAL; ++mode)
            for (unsigned k = 0; k < 3; ++k) for (unsigned b = 0; b < 3; ++b)
                for (unsigned snapshot = 0; snapshot < 2; ++snapshot) for (unsigned start = 0; start < 2; ++start)
                    for (unsigned long_message = 0; long_message < 2; ++long_message)
                        contract_case(variant, mode, kinds[k], backends[b], snapshot != 0, start * 2, long_message != 0);
        for (int mode = CONTROL_TELL_UNSUPPORTED; mode <= CONTROL_PAST_END; ++mode)
            for (unsigned snapshot = 0; snapshot < 2; ++snapshot)
                contract_case(variant, mode, XERR_UNSUPPORTED, 19, snapshot != 0, 0, false);
        for (int mode = FAIL_TELL; mode <= FAIL_SIZE; ++mode) for (unsigned snapshot = 0; snapshot < 2; ++snapshot)
            metadata_oom(variant, mode, snapshot != 0);
    }
    require_contract(contract_cases == HASH_TEST_VARIANTS * 448 && real_oom_cases == HASH_TEST_VARIANTS * 4, "all declared contract cases executed");
    require_contract(contract_memory(&before), "all diagnostic owners retired");
    printf("%s reader contract: %u variants, %u cases, %u checks, %u guarded failures, %u real OOM, %u failures\n",
        HASH_TEST_NAME, HASH_TEST_VARIANTS, contract_cases, contract_checks, guarded_failures, real_oom_cases, contract_failures);
    return contract_failures ? 1 : 0;
}
