#ifndef XL_FLOAT_ENV_NATIVE_H
#define XL_FLOAT_ENV_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 舍入模式：0 最近、1 向下、2 向上、3 向零。 */
int xfenv_get_round(void);
int xfenv_set_round(int mode);

/* 异常位：invalid=1、divideByZero=2、overflow=4、underflow=8、inexact=16。 */
int xfenv_get_flags(void);
int xfenv_clear_flags(int flags);
int xfenv_raise_flags(int flags);

/* 快照由调用方持有；只能在创建它的线程恢复。 */
void* xfenv_capture(void);
void* xfenv_hold(void);
void* xfenv_snapshot_adopt(void* snapshot);
/* 1 成功、0 失败、-1 跨线程；调用方仍拥有快照。 */
int xfenv_snapshot_hold(void* snapshot);
int xfenv_snapshot_restore(void* snapshot);
void xfenv_snapshot_destroy(void* snapshot);

#ifdef __cplusplus
}
#endif

#endif
