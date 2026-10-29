#ifndef SINGLEINSTANCE_NATIVE_H
#define SINGLEINSTANCE_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* acquire: 1=取得、0=其他包实例占用、-1=错误。 */
int xsi_acquire(const char* name);
int xsi_release(void);
int xsi_is_acquired(void);

#ifdef __cplusplus
}
#endif
#endif
