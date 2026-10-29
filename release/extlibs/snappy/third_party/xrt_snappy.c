/*
 * xrt_snappy.c
 *
 * 纯 C 实现的 Snappy 压缩格式编解码器（格式兼容 Google Snappy）。
 * 仅使用标准 C（无编译器内建函数、无 C++），可在 TCC 下编译。
 *
 * 压缩器采用哈希表贪心匹配（4 字节匹配），产出标准 snappy 元素流；
 * 解压器逐元素解析 literal / copy，并对越界与重叠复制做安全校验。
 */
#include "xrt_snappy.h"

#include <string.h>
#include <stdlib.h>



/* ----------------------------- varint 长度头 ----------------------------- */

static size_t xs_write_varint(unsigned char* out, size_t v)
{
	size_t i = 0u;
	while ( v >= 0x80u ) {
		out[i++] = (unsigned char)(v | 0x80u);
		v >>= 7;
	}
	out[i++] = (unsigned char)v;
	return i;
}

/* 从 in[0..n) 读取 varint；成功返回消费字节数，截断/溢出返回 0。 */
static size_t xs_read_varint(const unsigned char* in, size_t n, size_t* out)
{
	size_t result = 0u;
	size_t shift = 0u;
	size_t i = 0u;
	while ( i < n ) {
		unsigned char b = in[i];
		if ( shift >= (sizeof(size_t) * 8u) ) { return 0u; }
		result |= (size_t)(b & 0x7fu) << shift;
		++i;
		if ( (b & 0x80u) == 0u ) { *out = result; return i; }
		shift += 7u;
	}
	return 0u;
}



/* ----------------------------- 输出缓冲管理 ----------------------------- */

typedef struct {
	unsigned char* buf;
	size_t cap;
	size_t len;
	int overflow;
} xs_writer;

static void xs_put_byte(xs_writer* w, unsigned char b)
{
	if ( w->len < w->cap ) { w->buf[w->len] = b; } else { w->overflow = 1; }
	++w->len;
}

static void xs_put_bytes(xs_writer* w, const unsigned char* p, size_t n)
{
	size_t i;
	for ( i = 0u; i < n; ++i ) { xs_put_byte(w, p[i]); }
}



/* ----------------------------- 元素发射 ----------------------------- */

/* 发射 literal 元素：tag 低 2 位 = 00，高 6 位编码长度。 */
static void xs_emit_literal(xs_writer* w, const unsigned char* lit, size_t len)
{
	size_t n = len - 1u;
	if ( n < 60u ) {
		xs_put_byte(w, (unsigned char)((n << 2) | 0u));
	} else if ( n < 0x100u ) {
		xs_put_byte(w, (unsigned char)((60u << 2) | 0u));
		xs_put_byte(w, (unsigned char)n);
	} else if ( n < 0x10000u ) {
		xs_put_byte(w, (unsigned char)((61u << 2) | 0u));
		xs_put_byte(w, (unsigned char)(n & 0xffu));
		xs_put_byte(w, (unsigned char)((n >> 8) & 0xffu));
	} else if ( n < 0x1000000u ) {
		xs_put_byte(w, (unsigned char)((62u << 2) | 0u));
		xs_put_byte(w, (unsigned char)(n & 0xffu));
		xs_put_byte(w, (unsigned char)((n >> 8) & 0xffu));
		xs_put_byte(w, (unsigned char)((n >> 16) & 0xffu));
	} else {
		xs_put_byte(w, (unsigned char)((63u << 2) | 0u));
		xs_put_byte(w, (unsigned char)(n & 0xffu));
		xs_put_byte(w, (unsigned char)((n >> 8) & 0xffu));
		xs_put_byte(w, (unsigned char)((n >> 16) & 0xffu));
		xs_put_byte(w, (unsigned char)((n >> 24) & 0xffu));
	}
	xs_put_bytes(w, lit, len);
}

/* 发射 copy 元素。offset <= 2047 且 len 在 4..11 时用 copy1，否则 copy2。 */
static void xs_emit_copy(xs_writer* w, size_t offset, size_t len)
{
	while ( len > 0u ) {
		if ( offset <= 2047u && len >= 4u && len <= 11u ) {
			xs_put_byte(w, (unsigned char)(1u | ((len - 4u) << 2) | ((offset >> 8) << 5)));
			xs_put_byte(w, (unsigned char)(offset & 0xffu));
			return;
		} else {
			size_t take = len > 64u ? 64u : len;
			if ( take < 1u ) { take = 1u; }
			xs_put_byte(w, (unsigned char)(2u | ((take - 1u) << 2)));
			xs_put_byte(w, (unsigned char)(offset & 0xffu));
			xs_put_byte(w, (unsigned char)((offset >> 8) & 0xffu));
			len -= take;
		}
	}
}



/* ----------------------------- 压缩器 ----------------------------- */

#define XS_HASH_BITS 14u
#define XS_HASH_SIZE (1u << XS_HASH_BITS)
#define XS_HASH_MASK (XS_HASH_SIZE - 1u)
#define XS_MAX_OFFSET 65535u

static size_t xs_hash4(const unsigned char* p)
{
	size_t v = (size_t)p[0] | ((size_t)p[1] << 8) |
		((size_t)p[2] << 16) | ((size_t)p[3] << 24);
	/* 乘法散列后取低 XS_HASH_BITS 位（用掩码代替右移，规避部分
	 * 编译器对非常量移位量的代码生成差异）。 */
	return (v * 0x1e35a7bdu) & XS_HASH_MASK;
}

size_t xsCodecMaxCompressedLength(size_t n)
{
	return 32u + n + n / 6u;
}

int xsCodecCompress(const unsigned char* input, size_t n,
	unsigned char* output, size_t out_capacity, size_t* out_len)
{
	xs_writer w;
	size_t* table;
	size_t lit_start = 0u;
	size_t ip = 0u;
	size_t i;

	if ( out_len == NULL ) { return XS_CODEC_ERROR_BAD_INPUT; }
	if ( output == NULL && out_capacity != 0u ) { return XS_CODEC_ERROR_BAD_INPUT; }
	if ( input == NULL && n != 0u ) { return XS_CODEC_ERROR_BAD_INPUT; }

	table = (size_t*)malloc(sizeof(size_t) * XS_HASH_SIZE);
	if ( table == NULL ) { return XS_CODEC_ERROR_BUFFER_TOO_SMALL; }

	w.buf = output;
	w.cap = out_capacity;
	w.len = 0u;
	w.overflow = 0;

	/* 长度头（varint）。 */
	{
		unsigned char tmp[16];
		size_t vn = xs_write_varint(tmp, n);
		xs_put_bytes(&w, tmp, vn);
	}

	if ( n == 0u ) {
		if ( w.overflow ) { free(table); return XS_CODEC_ERROR_BUFFER_TOO_SMALL; }
		*out_len = w.len;
		free(table);
		return XS_CODEC_OK;
	}

	for ( i = 0u; i < XS_HASH_SIZE; ++i ) { table[i] = (size_t)-1; }

	while ( ip + 4u <= n ) {
		size_t h = xs_hash4(input + ip);
		size_t cand = table[h];
		table[h] = ip;
		if ( cand != (size_t)-1 && ip - cand <= XS_MAX_OFFSET &&
			cand < ip &&
			input[cand] == input[ip] && input[cand + 1] == input[ip + 1] &&
			input[cand + 2] == input[ip + 2] && input[cand + 3] == input[ip + 3] ) {
			/* 命中匹配：先冲刷累计的 literal。 */
			size_t match_len = 4u;
			size_t offset = ip - cand;
			while ( ip + match_len < n && input[cand + match_len] == input[ip + match_len] ) {
				++match_len;
			}
			if ( ip > lit_start ) {
				xs_emit_literal(&w, input + lit_start, ip - lit_start);
			}
			xs_emit_copy(&w, offset, match_len);
			ip += match_len;
			lit_start = ip;
		} else {
			++ip;
		}
	}
	/* 冲刷尾部 literal（可能覆盖剩余不足 4 字节的尾巴）。 */
	if ( n > lit_start ) {
		xs_emit_literal(&w, input + lit_start, n - lit_start);
	}

	if ( w.overflow ) { free(table); return XS_CODEC_ERROR_BUFFER_TOO_SMALL; }
	*out_len = w.len;
	free(table);
	return XS_CODEC_OK;
}



/* ----------------------------- 解压器 ----------------------------- */

int xsCodecUncompressedLength(const unsigned char* compressed, size_t n,
	size_t* out_len)
{
	size_t len = 0u;
	if ( out_len == NULL ) { return XS_CODEC_ERROR_BAD_INPUT; }
	if ( compressed == NULL && n != 0u ) { return XS_CODEC_ERROR_BAD_INPUT; }
	if ( xs_read_varint(compressed, n, &len) == 0u ) {
		return XS_CODEC_ERROR_BAD_INPUT;
	}
	*out_len = len;
	return XS_CODEC_OK;
}

int xsCodecDecompress(const unsigned char* compressed, size_t n,
	unsigned char* output, size_t out_capacity, size_t* out_len)
{
	size_t declared = 0u;
	size_t hdr;
	size_t ip;
	size_t op = 0u;

	if ( out_len == NULL ) { return XS_CODEC_ERROR_BAD_INPUT; }
	if ( compressed == NULL && n != 0u ) { return XS_CODEC_ERROR_BAD_INPUT; }
	if ( output == NULL && out_capacity != 0u ) { return XS_CODEC_ERROR_BAD_INPUT; }

	hdr = xs_read_varint(compressed, n, &declared);
	if ( hdr == 0u ) { return XS_CODEC_ERROR_BAD_INPUT; }
	if ( declared > out_capacity ) { return XS_CODEC_ERROR_BUFFER_TOO_SMALL; }

	ip = hdr;
	while ( ip < n ) {
		unsigned char tag = compressed[ip++];
		unsigned int type = (unsigned int)(tag & 3u);
		if ( type == 0u ) {
			/* literal */
			size_t len = (size_t)(tag >> 2) + 1u;
			if ( len > 60u ) {
				size_t extra = len - 60u;   /* 1..4 字节长度 */
				size_t i;
				len = 0u;
				if ( ip + extra > n ) { return XS_CODEC_ERROR_BAD_INPUT; }
				for ( i = 0u; i < extra; ++i ) {
					len |= (size_t)compressed[ip + i] << (8u * i);
				}
				ip += extra;
				len += 1u;
			}
			if ( ip + len > n ) { return XS_CODEC_ERROR_BAD_INPUT; }
			if ( op + len > declared ) { return XS_CODEC_ERROR_BAD_INPUT; }
			memcpy(output + op, compressed + ip, len);
			ip += len;
			op += len;
		} else {
			/* copy */
			size_t len;
			size_t offset;
			if ( type == 1u ) {
				if ( ip + 1u > n ) { return XS_CODEC_ERROR_BAD_INPUT; }
				len = 4u + ((size_t)(tag >> 2) & 7u);
				offset = ((size_t)(tag >> 5) << 8) | (size_t)compressed[ip];
				ip += 1u;
			} else if ( type == 2u ) {
				if ( ip + 2u > n ) { return XS_CODEC_ERROR_BAD_INPUT; }
				len = ((size_t)(tag >> 2)) + 1u;
				offset = (size_t)compressed[ip] | ((size_t)compressed[ip + 1] << 8);
				ip += 2u;
			} else {
				if ( ip + 4u > n ) { return XS_CODEC_ERROR_BAD_INPUT; }
				len = ((size_t)(tag >> 2)) + 1u;
				offset = (size_t)compressed[ip] | ((size_t)compressed[ip + 1] << 8) |
					((size_t)compressed[ip + 2] << 16) | ((size_t)compressed[ip + 3] << 24);
				ip += 4u;
			}
			if ( offset == 0u || offset > op ) { return XS_CODEC_ERROR_BAD_INPUT; }
			if ( op + len > declared ) { return XS_CODEC_ERROR_BAD_INPUT; }
			{
				size_t k;
				for ( k = 0u; k < len; ++k ) {
					output[op] = output[op - offset];
					++op;
				}
			}
		}
	}

	if ( op != declared ) { return XS_CODEC_ERROR_BAD_INPUT; }
	*out_len = op;
	return XS_CODEC_OK;
}
