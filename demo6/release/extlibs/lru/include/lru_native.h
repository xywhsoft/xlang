#ifndef XLRU_NATIVE_H
#define XLRU_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* mode: 0=LRU, 1=LFU。销毁前结束其它线程对句柄的使用。 */
void* xlru_create(int64_t capacity, int64_t mode);
void xlru_destroy(void* cache);
int64_t xlru_put(void* cache, const char* key, const char* value);
/* 返回独立文本，以 xrtFree 释放；命中空串与缺失由 contains 区分。 */
void* xlru_get(void* cache, const char* key, const char* fallback);
int64_t xlru_contains(void* cache, const char* key);
/* remove: 1=已删除，0=不存在，-1=错误。 */
int64_t xlru_remove(void* cache, const char* key);
int64_t xlru_clear(void* cache);
int64_t xlru_count(void* cache);
int64_t xlru_capacity(void* cache);
int64_t xlru_hits(void* cache);
int64_t xlru_misses(void* cache);

#ifdef __cplusplus
}
#endif
#endif
