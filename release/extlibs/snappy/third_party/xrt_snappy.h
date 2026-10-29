/*
 * xrt_snappy.h
 *
 * 纯 C 实现的 Snappy 压缩格式（格式兼容 Google Snappy / RFC）。
 * 为 xlang 扩展库提供 TCC 友好的压缩/解压一次性 API，不依赖任何
 * 编译器内建函数或 C++ 运行时。
 *
 * 格式参考：https://github.com/google/snappy/blob/main/format_description.txt
 */
#ifndef XRT_SNAPPY_CODEC_H
#define XRT_SNAPPY_CODEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 压缩结果状态码。 */
#define XS_CODEC_OK 0
#define XS_CODEC_ERROR_BUFFER_TOO_SMALL 1
#define XS_CODEC_ERROR_BAD_INPUT 2

/* 返回压缩 n 字节输入所需的输出缓冲上限（含 varint 长度头）。 */
size_t xsCodecMaxCompressedLength(size_t n);

/* 将 input[0..n) 压缩写入 output。
 * 成功返回 XS_CODEC_OK 并通过 *out_len 写出压缩长度；
 * 输出缓冲不足时返回 XS_CODEC_ERROR_BUFFER_TOO_SMALL。 */
int xsCodecCompress(const unsigned char* input, size_t n,
    unsigned char* output, size_t out_capacity, size_t* out_len);

/* 校验压缩流并返回其声明的原始长度。
 * 流损坏或截断时返回非 XS_CODEC_OK。 */
int xsCodecUncompressedLength(const unsigned char* compressed, size_t n,
    size_t* out_len);

/* 将 compressed[0..n) 解压写入 output。
 * output 容量须不小于声明的原始长度；成功返回 XRT_SNAPPY_OK 并通过
 * *out_len 写出原始长度，流损坏时返回 XS_CODEC_ERROR_BAD_INPUT。 */
int xsCodecDecompress(const unsigned char* compressed, size_t n,
    unsigned char* output, size_t out_capacity, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* XRT_SNAPPY_CODEC_H */
