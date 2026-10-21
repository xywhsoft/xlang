/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#ifndef XLANG_CLANGD_NATIVE_INPUT_H
#define XLANG_CLANGD_NATIVE_INPUT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct xlang_native_stamp {
  uint64_t index, size, modified;
  uint32_t volume, attributes;
} xlang_native_stamp;
/* The returned handle is the source of both metadata and compiler bytes. */
int xlangNativeOpen(const char *path, int read, void **handle,
                    xlang_native_stamp *stamp, unsigned long *error);
int xlangNativeStatus(void *handle, xlang_native_stamp *stamp,
                     unsigned long *error);
int xlangNativeRead(void *handle, char **bytes, size_t *size,
                   xlang_native_stamp *stamp, unsigned long *error);
void xlangNativeClose(void *handle);
void xlangNativeFree(void *bytes);
#ifdef __cplusplus
}
#endif
#endif
