#include <fenv.h>
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#include "xrt.h"
#include "include/float_env_native.h"

/* TinyCC 的 Windows CRT 只有 fenv.h 声明；同步维护 x87 与 SSE 状态。 */
#if defined(__TINYC__) && defined(_WIN64) && defined(__x86_64__)
static unsigned xfenv_win_mxcsr(void)
{
    unsigned value;
    __asm__ __volatile__("stmxcsr %0" : "=m"(value));
    return value;
}

static void xfenv_win_set_mxcsr(unsigned value)
{
    __asm__ __volatile__("ldmxcsr %0" : : "m"(value));
}

static void xfenv_win_capture(fenv_t* saved)
{
    __asm__ __volatile__("fnstenv %0" : "=m"(*saved));
    __asm__ __volatile__("fldenv %0" : : "m"(*saved));
    saved->__mxcsr = xfenv_win_mxcsr();
}

static void xfenv_win_restore(const fenv_t* saved)
{
    __asm__ __volatile__("fldenv %0" : : "m"(*saved));
    xfenv_win_set_mxcsr(saved->__mxcsr);
}

static int xfenv_win_get_round(void)
{
    unsigned short control;
    __asm__ __volatile__("fnstcw %0" : "=m"(control));
    return (int)(control & 0x0c00u);
}

static int xfenv_win_set_round(int native)
{
    unsigned short control;
    unsigned mxcsr;
    if (native != FE_TONEAREST && native != FE_DOWNWARD &&
        native != FE_UPWARD && native != FE_TOWARDZERO) return -1;
    __asm__ __volatile__("fnstcw %0" : "=m"(control));
    control = (unsigned short)((control & ~0x0c00u) | (unsigned)native);
    __asm__ __volatile__("fldcw %0" : : "m"(control));
    mxcsr = xfenv_win_mxcsr();
    mxcsr = (mxcsr & ~0x6000u) | ((unsigned)native << 3);
    xfenv_win_set_mxcsr(mxcsr);
    return 0;
}

static int xfenv_win_test_flags(int native)
{
    unsigned short status;
    __asm__ __volatile__("fnstsw %0" : "=m"(status));
    return (int)((status | xfenv_win_mxcsr()) & (unsigned)native);
}

static int xfenv_win_edit_flags(int native, int raise)
{
    fenv_t saved;
    xfenv_win_capture(&saved);
    if (raise) {
        saved.__status_word = (unsigned short)(saved.__status_word | native);
        saved.__mxcsr |= (unsigned)native;
    } else {
        saved.__status_word = (unsigned short)(saved.__status_word & ~native);
        saved.__mxcsr &= ~(unsigned)native;
    }
    xfenv_win_restore(&saved);
    return 0;
}

static int xfenv_win_hold(fenv_t* saved)
{
    fenv_t quiet;
    xfenv_win_capture(saved);
    quiet = *saved;
    quiet.__control_word |= 0x003fu;
    quiet.__status_word &= (unsigned short)~0x003fu;
    quiet.__mxcsr = (quiet.__mxcsr | 0x1f80u) & ~0x003fu;
    xfenv_win_restore(&quiet);
    return 0;
}

#define XFENV_GETROUND() xfenv_win_get_round()
#define XFENV_SETROUND(mode) xfenv_win_set_round(mode)
#define XFENV_TESTEXCEPT(flags) xfenv_win_test_flags(flags)
#define XFENV_CLEAREXCEPT(flags) xfenv_win_edit_flags(flags, 0)
#define XFENV_RAISEEXCEPT(flags) xfenv_win_edit_flags(flags, 1)
#define XFENV_GETENV(saved) (xfenv_win_capture(saved), 0)
#define XFENV_SETENV(saved) (xfenv_win_restore(saved), 0)
#define XFENV_HOLDEXCEPT(saved) xfenv_win_hold(saved)
#else
#define XFENV_GETROUND() fegetround()
#define XFENV_SETROUND(mode) fesetround(mode)
#define XFENV_TESTEXCEPT(flags) fetestexcept(flags)
#define XFENV_CLEAREXCEPT(flags) feclearexcept(flags)
#define XFENV_RAISEEXCEPT(flags) feraiseexcept(flags)
#define XFENV_GETENV(saved) fegetenv(saved)
#define XFENV_SETENV(saved) fesetenv(saved)
#define XFENV_HOLDEXCEPT(saved) feholdexcept(saved)
#endif

/* 线程身份由宿主和扩展包共用的 XRT 提供。 */
typedef struct xfenv_snapshot {
    fenv_t value;
    uint64 owner_thread;
} xfenv_snapshot;

static int xfenv_round_to_native(int mode)
{
    switch (mode) {
    case 0: return FE_TONEAREST;
    case 1: return FE_DOWNWARD;
    case 2: return FE_UPWARD;
    case 3: return FE_TOWARDZERO;
    default: return -1;
    }
}

int xfenv_get_round(void)
{
    switch (XFENV_GETROUND()) {
    case FE_TONEAREST: return 0;
    case FE_DOWNWARD: return 1;
    case FE_UPWARD: return 2;
    case FE_TOWARDZERO: return 3;
    default: return -1;
    }
}

int xfenv_set_round(int mode)
{
    int native = xfenv_round_to_native(mode);
    return native >= 0 && XFENV_SETROUND(native) == 0;
}

static int xfenv_flags_to_native(int flags)
{
    int native = 0;
    if ((flags & ~31) != 0) return -1;
    if ((flags & 1) != 0) native |= FE_INVALID;
    if ((flags & 2) != 0) native |= FE_DIVBYZERO;
    if ((flags & 4) != 0) native |= FE_OVERFLOW;
    if ((flags & 8) != 0) native |= FE_UNDERFLOW;
    if ((flags & 16) != 0) native |= FE_INEXACT;
    return native;
}

int xfenv_get_flags(void)
{
    int native = XFENV_TESTEXCEPT(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW |
                              FE_UNDERFLOW | FE_INEXACT);
    int flags = 0;
    if (native < 0) return -1;
    if ((native & FE_INVALID) != 0) flags |= 1;
    if ((native & FE_DIVBYZERO) != 0) flags |= 2;
    if ((native & FE_OVERFLOW) != 0) flags |= 4;
    if ((native & FE_UNDERFLOW) != 0) flags |= 8;
    if ((native & FE_INEXACT) != 0) flags |= 16;
    return flags;
}

int xfenv_clear_flags(int flags)
{
    int native = xfenv_flags_to_native(flags);
    return native >= 0 && XFENV_CLEAREXCEPT(native) == 0;
}

int xfenv_raise_flags(int flags)
{
    int native = xfenv_flags_to_native(flags);
    return native >= 0 && XFENV_RAISEEXCEPT(native) == 0;
}

static xfenv_snapshot* xfenv_snapshot_new(int hold)
{
    xfenv_snapshot* snapshot = (xfenv_snapshot*)xrtMalloc(sizeof(*snapshot));
    if (snapshot == NULL) return NULL;
    snapshot->owner_thread = xrtThreadCurrentId();
    if ((hold ? XFENV_HOLDEXCEPT(&snapshot->value) : XFENV_GETENV(&snapshot->value)) != 0) {
        xrtFree(snapshot);
        return NULL;
    }
    return snapshot;
}

void* xfenv_capture(void) { return xfenv_snapshot_new(0); }
void* xfenv_hold(void) { return xfenv_snapshot_new(1); }
void* xfenv_snapshot_adopt(void* snapshot) { return snapshot; }
void xfenv_snapshot_destroy(void* snapshot) { xrtFree(snapshot); }

/* 已持有快照后才改变当前线程环境，避免语言对象构造失败时留下 hold 状态。 */
int xfenv_snapshot_hold(void* raw)
{
    xfenv_snapshot* snapshot = (xfenv_snapshot*)raw;
    fenv_t previous;
    if (snapshot == NULL) return 0;
    if (snapshot->owner_thread != xrtThreadCurrentId()) return -1;
    if (XFENV_GETENV(&previous) != 0) return 0;
    if (XFENV_HOLDEXCEPT(&snapshot->value) == 0) return 1;
    snapshot->value = previous;
    (void)XFENV_SETENV(&previous);
    return 0;
}

/* -1 表示跨线程，0 表示 CRT 拒绝恢复。 */
int xfenv_snapshot_restore(void* raw)
{
    const xfenv_snapshot* snapshot = (const xfenv_snapshot*)raw;
    if (snapshot == NULL) return 0;
    if (snapshot->owner_thread != xrtThreadCurrentId()) return -1;
    return XFENV_SETENV(&snapshot->value) == 0 ? 1 : 0;
}
