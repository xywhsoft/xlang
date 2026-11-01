#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
/* 同一夹具可对照旧实现；只替换编译期输入，不增加产品接口。 */
#ifndef XHCL_TEST_NATIVE
#define XHCL_TEST_NATIVE "../hcl_native.c"
#endif
#include XHCL_TEST_NATIVE
#include <stdio.h>

typedef struct scalar_case {
    cstr Source;
    cstr Key;
    int64_t Integer;
    double Float;
    bool Boolean;
    xerrkind Errors[3];
} scalar_case;

static unsigned failures, checks, faults;

/* 标量查询的返回值和错误必须一起核对，不能把错误的零值当成成功。 */
static bool query(const scalar_case* test, unsigned operation, xerrkind error)
{
    bool same;
    if (operation == 0u) {
        int64_t value = xhcl_get_int(test->Source, test->Key);
        same = value == (error == XERR_NONE ? test->Integer : 0);
    } else if (operation == 1u) {
        double value = xhcl_get_float(test->Source, test->Key);
        same = value == (error == XERR_NONE ? test->Float : 0.0);
    } else {
        bool value = xhcl_get_bool(test->Source, test->Key);
        same = value == (error == XERR_NONE ? test->Boolean : false);
    }
    const xerror* current = xrtGetError();
    same = same && xrtErrorKind(current) == error;
    if (error != XERR_NONE) {
        same = same && current != NULL && strcmp(xrtErrorDomain(current), XHCL_DOMAIN) == 0;
    }
    if (!same) {
        fprintf(stderr, "HCL scalar mismatch: op=%u expected-error=%d actual-error=%d source=%s\n",
            operation, (int)error, (int)xrtErrorKind(current), test->Source ? test->Source : "(null)");
    }
    ++checks;
    return same;
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount &&
        before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount &&
        before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

static void check_case(const scalar_case* test)
{
    for (unsigned operation = 0u; operation < 3u; ++operation) {
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        bool ok = query(test, operation, test->Errors[operation]);
        ok = balanced(&before) && ok;
        if (!ok) {
            fprintf(stderr, "HCL scalar state/memory failure: op=%u source=%s\n",
                operation, test->Source ? test->Source : "(null)");
            ++failures;
        }
    }
}

/* 有效、空值和缺失三条路径逐分配点失败，且每次失败后立即原样重试。 */
static void allocation_sweep(const scalar_case* test)
{
    for (unsigned operation = 0u; operation < 3u; ++operation) {
        bool done = false;
        for (unsigned budget = 0u; budget < 32u; ++budget) {
            xmemdebugsnapshot before;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            if (!xrtMemDebugFailAfter(budget)) { ++failures; return; }
            /* 先调用，再根据实际故障状态检查；避免期望本身影响分配。 */
            int64_t integer = 0;
            double number = 0;
            bool boolean = false;
            if (operation == 0u) integer = xhcl_get_int(test->Source, test->Key);
            else if (operation == 1u) number = xhcl_get_float(test->Source, test->Key);
            else boolean = xhcl_get_bool(test->Source, test->Key);
            bool triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            xerrkind expected = triggered ? XERR_MEMORY : test->Errors[operation];
            xerrkind actual = xrtErrorKind(xrtGetError());
            bool ok = actual == expected;
            if (operation == 0u) ok = integer == (expected == XERR_NONE ? test->Integer : 0) && ok;
            else if (operation == 1u) ok = number == (expected == XERR_NONE ? test->Float : 0.0) && ok;
            else ok = boolean == (expected == XERR_NONE ? test->Boolean : false) && ok;
            if (triggered) ++faults;
            ++checks;
            ok = balanced(&before) && ok;
            ok = query(test, operation, test->Errors[operation]) && ok;
            ok = balanced(&before) && ok;
            if (!ok) {
                fprintf(stderr, "HCL scalar allocation failure: op=%u budget=%u triggered=%d expected=%d actual=%d source=%s\n",
                    operation, budget, triggered, (int)expected, (int)actual, test->Source);
                ++failures;
                break;
            }
            if (!triggered) { done = budget > 0u; break; }
        }
        if (!done) ++failures;
    }
}

int main(void)
{
    static const scalar_case cases[] = {
        {"", "value", 0, 0, false, {XERR_NONE, XERR_NONE, XERR_NONE}},
        {"other = 1", "value", 0, 0, false, {XERR_NONE, XERR_NONE, XERR_NONE}},
        {"scope { other = 1 }", "value", 0, 0, false, {XERR_NONE, XERR_NONE, XERR_NONE}},
        {"value = \"\"", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"scope { value = \"\" }", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"value = \"\" /* tail */\nother = 1", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"value = \"\" // tail\r\n", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"value = \" \"", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"value = \"\\n\\t\"", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"value = \"\\u0020\"", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"value = 0", "value", 0, 0, false, {XERR_NONE, XERR_NONE, XERR_VALUE}},
        {"value = -0", "value", 0, 0, false, {XERR_NONE, XERR_NONE, XERR_VALUE}},
        {"value = \"0\"", "value", 0, 0, false, {XERR_NONE, XERR_NONE, XERR_VALUE}},
        {"value = false", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_NONE}},
        {"value = true", "value", 0, 0, true, {XERR_VALUE, XERR_VALUE, XERR_NONE}},
        {"value = \"false\"", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_NONE}},
        {"value = \"42\"", "value", 42, 42, false, {XERR_NONE, XERR_NONE, XERR_VALUE}},
        {"value = 1.25", "value", 0, 1.25, false, {XERR_VALUE, XERR_NONE, XERR_VALUE}},
        {"value = \"\"\nbroken {", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}},
        {"value = 0", "", 0, 0, false, {XERR_ARGUMENT, XERR_ARGUMENT, XERR_ARGUMENT}},
        {NULL, "value", 0, 0, false, {XERR_ARGUMENT, XERR_ARGUMENT, XERR_ARGUMENT}},
        {"value = \"\xff\"", "value", 0, 0, false, {XERR_VALUE, XERR_VALUE, XERR_VALUE}}
    };
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 1;
    xrtMemDebugFailClear();
    xrtSetErrorInfo(XERR_VALUE, "test.hcl", 0, "warm error slot");
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) check_case(&cases[i]);
    allocation_sweep(&cases[0]);
    allocation_sweep(&cases[3]);
    allocation_sweep(&cases[7]);
    allocation_sweep(&cases[10]);
    allocation_sweep(&cases[13]);
    allocation_sweep(&cases[16]);
    allocation_sweep(&cases[17]);
    /* 合法文本查询仍能表达空字符串；不把“空值拒绝”扩散到 get/getRaw。 */
    char* empty = xhcl_get("value = \"\"", "value");
    bool ok = empty != NULL && empty[0] == 0 && xrtGetError() == NULL;
    xrtFree(empty);
    char* raw = xhcl_get_raw("value = \"\"", "value");
    ok = raw != NULL && strcmp(raw, "\"\"") == 0 && xrtGetError() == NULL && ok;
    xrtFree(raw);
    if (!ok) ++failures;
    printf("HCL scalar values: %u checks, %u allocation faults, %u failures\n", checks, faults, failures);
    return failures == 0u ? 0 : 1;
}
