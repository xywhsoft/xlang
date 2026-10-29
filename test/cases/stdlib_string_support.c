#include <assert.h>
#include <stdint.h>
#include <string.h>

/* 断言边界与标准库测试的真实 extern 原型保持一致。 */
void expect_i64(int64_t actual, int64_t expected)
{
    assert(actual == expected);
}

void expect_str(const char* actual, const char* expected)
{
    assert(actual != NULL && expected != NULL && strcmp(actual, expected) == 0);
}
