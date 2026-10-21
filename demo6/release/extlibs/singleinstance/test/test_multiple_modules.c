/* 互斥操作写在断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

/* 两套独立静态状态模拟同一进程中的两个 JIT 包实例。 */
int xsi_a_acquire(const char* name);
int xsi_a_release(void);
int xsi_b_acquire(const char* name);
int xsi_b_release(void);

static int child_acquire(const char* name, int expected)
{
#ifdef _WIN32
    /* Windows 同进程的独立句柄同样必须互斥。 */
    return xsi_b_acquire(name) == expected;
#else
    pid_t child = fork();
    int status;
    if (child < 0) return 0;
    if (child == 0) _exit(xsi_b_acquire(name) == expected ? 0 : 1);
    return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

int main(void)
{
    char name[96];
#ifdef _WIN32
    snprintf(name, sizeof(name), "xlang_si_modules_%lu", (unsigned long)GetCurrentProcessId());
#else
    snprintf(name, sizeof(name), "xlang_si_modules_%ld", (long)getpid());
#endif
    assert(xsi_a_acquire(name) == 1);
    assert(xsi_b_acquire(name) == 0);
    assert(child_acquire(name, 0));
    assert(xsi_b_release());
    assert(child_acquire(name, 0));
    assert(xsi_a_release());
    assert(child_acquire(name, 1));
#ifdef _WIN32
    assert(xsi_a_acquire(name) == 0);
    assert(xsi_b_release());
    assert(xsi_a_acquire(name) == 1);
    assert(xsi_a_release());
#endif
    puts("singleinstance independent module state: PASS");
    return 0;
}
