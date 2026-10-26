#ifndef XLANG_BLOOM_H
#define XLANG_BLOOM_H

#ifdef __cplusplus
extern "C" {
#endif

/* 创建、恢复和 clone 返回拥有型句柄；destroy(NULL) 安全。对象由调用方串行访问。 */
void *xbloom_create(long long capacity, double fpRate);
void xbloom_destroy(void *filter);
void xbloom_add(void *filter, const char *data, long long length);
int xbloom_contains(void *filter, const char *data, long long length);
void xbloom_clear(void *filter);
long long xbloom_count(void *filter);
long long xbloom_bits(void *filter);
int xbloom_hashes(void *filter);
long long xbloom_capacity(void *filter);
double xbloom_estimated_fpp(void *filter);
long long xbloom_estimated_count(void *filter);
int xbloom_union(void *filter, void *other);
const char *xbloom_serialize(void *filter);
char *xbloom_serialize_owned(void *filter);
void *xbloom_deserialize(const char *data);
void *xbloom_clone(void *filter);
void *xbloom_adopt(void *filter);

void *xcounting_bloom_create(long long capacity, double fpRate);
void xcounting_bloom_destroy(void *filter);
void xcounting_bloom_add(void *filter, const char *data, long long length);
void xcounting_bloom_remove(void *filter, const char *data, long long length);
int xcounting_bloom_contains(void *filter, const char *data, long long length);
void xcounting_bloom_clear(void *filter);
long long xcounting_bloom_count(void *filter);
long long xcounting_bloom_slots(void *filter);
int xcounting_bloom_hashes(void *filter);
long long xcounting_bloom_capacity(void *filter);
double xcounting_bloom_estimated_fpp(void *filter);
long long xcounting_bloom_estimated_count(void *filter);
int xcounting_bloom_union(void *filter, void *other);
const char *xcounting_bloom_serialize(void *filter);
char *xcounting_bloom_serialize_owned(void *filter);
void *xcounting_bloom_deserialize(const char *data);
void *xcounting_bloom_clone(void *filter);
void *xcounting_bloom_adopt(void *filter);

/* length=-1 按 C 字符串取长度；非负值允许包含 NUL 的二进制输入。
   serialize 字符串借用 XRT 临时区；serialize_owned 返回独立内存，须用 xrtFree 释放。
   合并不转移对象所有权。 */
#ifdef __cplusplus
}
#endif
#endif
