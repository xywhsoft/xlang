


/*
	Hash32 - nmhash32x [Ver2.0, Update : 2024/10/18 from https://github.com/rurban/smhasher]
		修改：
			删除函数：NMHASH32_0to8（仅NMHASH32依赖，只保留NMHASH32X）
			删除函数：NMHASH32_9to255（仅NMHASH32依赖，只保留NMHASH32X）
			删除定义：NMHASH32_9to32（仅NMHASH32依赖，只保留NMHASH32X）
			删除定义：NMHASH32_33to255（仅NMHASH32依赖，只保留NMHASH32X）
			删除函数：NMHASH32_avalanche32（仅NMHASH32依赖，只保留NMHASH32X）
			删除函数：NMHASH32（只保留NMHASH32X）
			添加部分条件编译分支：判断 __TINYC__ 以区分是否使用 TCC 编译
			删除头文件引入：<stdint.h> 和 <string.h>（最上面已经引入）
		使用协议注意事项：
			BSD 2-Clause 协议
			允许个人使用、商业使用
			复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/



/*
	BSD 2-Clause License

	Copyright (c) 2021, James Z.M. Gao 
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	   list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define NMH_VERSION 2

#ifdef _MSC_VER
#  pragma warning(push, 3)
#endif

#if defined(__cplusplus) && __cplusplus < 201103L
#  define __STDC_CONSTANT_MACROS 1
#endif

#if defined(__GNUC__)
#  if defined(__AVX2__)
#    include <immintrin.h>
#  elif defined(__SSE2__)
#    include <emmintrin.h>
#  endif
#elif defined(_MSC_VER)
#  include <intrin.h>
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#if (defined(__GNUC__) && (__GNUC__ >= 3))  \
  || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) \
  || defined(__clang__)
#    define NMH_likely(x) __builtin_expect(x, 1)
#else
#    define NMH_likely(x) (x)
#endif

#if defined(__has_builtin)
#  if __has_builtin(__builtin_rotateleft32)
#    define NMH_rotl32 __builtin_rotateleft32 /* clang */
#  endif
#endif
#if !defined(NMH_rotl32)
#  if defined(_MSC_VER)
	 /* Note: although _rotl exists for minGW (GCC under windows), performance seems poor */
#    define NMH_rotl32(x,r) _rotl(x,r)
#  else
#    define NMH_rotl32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))
#  endif
#endif

#ifdef __TINYC__
#  define NMH_RESTRICT   restrict
#  define NMH_VECTOR NMH_SCALAR
#elif ((defined(sun) || defined(__sun)) && __cplusplus) /* Solaris includes __STDC_VERSION__ with C++. Tested with GCC 5.5 */
#  define NMH_RESTRICT   /* disable */
#elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* >= C99 */
#  define NMH_RESTRICT   restrict
#elif defined(__cplusplus) && (defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER))
#  define NMH_RESTRICT __restrict__
#elif defined(__cplusplus) && defined(_MSC_VER)
#  define NMH_RESTRICT __restrict
#else
#  define NMH_RESTRICT   /* disable */
#endif

/* endian macros */
#ifndef NMHASH_LITTLE_ENDIAN
#  if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || defined(__x86_64__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || defined(__SDCC)
#    define NMHASH_LITTLE_ENDIAN 1
#  elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define NMHASH_LITTLE_ENDIAN 0
#  else
#    warning could not determine endianness! Falling back to little endian.
#    define NMHASH_LITTLE_ENDIAN 1
#  endif
#endif

/* vector macros */
#define NMH_SCALAR 0
#define NMH_SSE2   1
#define NMH_AVX2   2
#define NMH_AVX512 3

#ifndef NMH_VECTOR    /* can be defined on command line */
#  if defined(__AVX512BW__)
#    define NMH_VECTOR NMH_AVX512 /* _mm512_mullo_epi16 requires AVX512BW */
#  elif defined(__AVX2__)
#    define NMH_VECTOR NMH_AVX2  /* add '-mno-avx256-split-unaligned-load' and '-mn-oavx256-split-unaligned-store' for gcc */
#  elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP == 2))
#    define NMH_VECTOR NMH_SSE2
#  else
#    define NMH_VECTOR NMH_SCALAR
#  endif
#endif

/* align macros */
#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)   /* C11+ */
#  include <stdalign.h>
#  define NMH_ALIGN(n)      alignas(n)
#elif defined(__GNUC__)
#  define NMH_ALIGN(n)      __attribute__ ((aligned(n)))
#elif defined(_MSC_VER)
#  define NMH_ALIGN(n)      __declspec(align(n))
#else
#  define NMH_ALIGN(n)   /* disabled */
#endif

#if NMH_VECTOR > 0
#  define NMH_ACC_ALIGN 64
#elif defined(__BIGGEST_ALIGNMENT__)
#  define NMH_ACC_ALIGN __BIGGEST_ALIGNMENT__
#elif defined(__SDCC)
#  define NMH_ACC_ALIGN 1
#else
#  define NMH_ACC_ALIGN 16
#endif

/* constants */

/* primes from xxh */
#define NMH_PRIME32_1  UINT32_C(0x9E3779B1)
#define NMH_PRIME32_2  UINT32_C(0x85EBCA77)
#define NMH_PRIME32_3  UINT32_C(0xC2B2AE3D)
#define NMH_PRIME32_4  UINT32_C(0x27D4EB2F)

/*! Pseudorandom secret taken directly from FARSH. */
NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t NMH_ACC_INIT[32] = {
	UINT32_C(0xB8FE6C39), UINT32_C(0x23A44BBE), UINT32_C(0x7C01812C), UINT32_C(0xF721AD1C),
	UINT32_C(0xDED46DE9), UINT32_C(0x839097DB), UINT32_C(0x7240A4A4), UINT32_C(0xB7B3671F),
	UINT32_C(0xCB79E64E), UINT32_C(0xCCC0E578), UINT32_C(0x825AD07D), UINT32_C(0xCCFF7221),
	UINT32_C(0xB8084674), UINT32_C(0xF743248E), UINT32_C(0xE03590E6), UINT32_C(0x813A264C),

	UINT32_C(0x3C2852BB), UINT32_C(0x91C300CB), UINT32_C(0x88D0658B), UINT32_C(0x1B532EA3),
	UINT32_C(0x71644897), UINT32_C(0xA20DF94E), UINT32_C(0x3819EF46), UINT32_C(0xA9DEACD8),
	UINT32_C(0xA8FA763F), UINT32_C(0xE39C343F), UINT32_C(0xF9DCBBC7), UINT32_C(0xC70B4F1D),
	UINT32_C(0x8A51E04B), UINT32_C(0xCDB45931), UINT32_C(0xC89F7EC9), UINT32_C(0xD9787364),
};

#if defined(_MSC_VER) && _MSC_VER >= 1914
#  pragma warning(push)
#  pragma warning(disable: 5045)
#endif
#ifdef __SDCC
#  define const
#  pragma save
#  pragma disable_warning 110
#  pragma disable_warning 126
#endif

/* read functions */
static inline
uint32_t
NMH_readLE32(const void *const p)
{
	uint32_t v;
	memcpy(&v, p, 4);
#	if (NMHASH_LITTLE_ENDIAN)
	return v;
#	elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
	return __builtin_bswap32(v);
#	elif defined(_MSC_VER)
	return _byteswap_ulong(v);
#	else
	return ((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000);
#	endif
}

static inline
uint16_t
NMH_readLE16(const void *const p)
{
	uint16_t v;
	memcpy(&v, p, 2);
#	if (NMHASH_LITTLE_ENDIAN)
	return v;
#	else
	return (uint16_t)((v << 8) | (v >> 8));
#	endif
}

#define __NMH_M1 UINT32_C(0xF0D9649B)
#define __NMH_M2 UINT32_C(0x29A7935D)
#define __NMH_M3 UINT32_C(0x55D35831)

NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t __NMH_M1_V[32] = {
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
	__NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1, __NMH_M1,
};
NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t __NMH_M2_V[32] = {
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
	__NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2, __NMH_M2,
};
NMH_ALIGN(NMH_ACC_ALIGN) static const uint32_t __NMH_M3_V[32] = {
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
	__NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3, __NMH_M3,
};

#undef __NMH_M1
#undef __NMH_M2
#undef __NMH_M3

#if NMH_VECTOR == NMH_SCALAR
#define NMHASH32_long_round NMHASH32_long_round_scalar
static inline
void
NMHASH32_long_round_scalar(uint32_t *const NMH_RESTRICT accX, uint32_t *const NMH_RESTRICT accY, const uint8_t* const NMH_RESTRICT p)
{
	/* breadth first calculation will hint some compiler to auto vectorize the code
	 * on gcc, the performance becomes 10x than the depth first, and about 80% of the manually vectorized code
	 */
	const size_t nbGroups = sizeof(NMH_ACC_INIT) / sizeof(*NMH_ACC_INIT);
	size_t i;
	
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= NMH_readLE32(p + i * 4);
	}
	for (i = 0; i < nbGroups; ++i) {
		accY[i] ^= NMH_readLE32(p + i * 4 + sizeof(NMH_ACC_INIT));
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] += accY[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accY[i] ^= accX[i] >> 1;
	}
	for (i = 0; i < nbGroups * 2; ++i) {
		((uint16_t*)accX)[i] *= ((uint16_t*)__NMH_M1_V)[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accX[i] << 5 ^ accX[i] >> 13;
	}
	for (i = 0; i < nbGroups * 2; ++i) {
		((uint16_t*)accX)[i] *= ((uint16_t*)__NMH_M2_V)[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accY[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accX[i] << 11 ^ accX[i] >> 9;
	}
	for (i = 0; i < nbGroups * 2; ++i) {
		((uint16_t*)accX)[i] *= ((uint16_t*)__NMH_M3_V)[i];
	}
	for (i = 0; i < nbGroups; ++i) {
		accX[i] ^= accX[i] >> 10 ^ accX[i] >> 20;
	}
}
#endif

#if NMH_VECTOR == NMH_SSE2
#  define _NMH_MM_(F) _mm_ ## F
#  define _NMH_MMW_(F) _mm_ ## F ## 128
#  define _NMH_MM_T __m128i
#elif NMH_VECTOR == NMH_AVX2
#  define _NMH_MM_(F) _mm256_ ## F
#  define _NMH_MMW_(F) _mm256_ ## F ## 256
#  define _NMH_MM_T __m256i
#elif NMH_VECTOR == NMH_AVX512
#  define _NMH_MM_(F) _mm512_ ## F
#  define _NMH_MMW_(F) _mm512_ ## F ## 512
#  define _NMH_MM_T __m512i
#endif

#if NMH_VECTOR == NMH_SSE2 || NMH_VECTOR == NMH_AVX2 || NMH_VECTOR == NMH_AVX512
#  define NMHASH32_long_round NMHASH32_long_round_sse
#  define NMH_VECTOR_NB_GROUP (sizeof(NMH_ACC_INIT) / sizeof(*NMH_ACC_INIT) / (sizeof(_NMH_MM_T) / sizeof(*NMH_ACC_INIT)))
static inline
void
NMHASH32_long_round_sse(uint32_t* const NMH_RESTRICT accX, uint32_t* const NMH_RESTRICT accY, const uint8_t* const NMH_RESTRICT p)
{
	const _NMH_MM_T *const NMH_RESTRICT m1    = (const _NMH_MM_T * NMH_RESTRICT)__NMH_M1_V;
	const _NMH_MM_T *const NMH_RESTRICT m2    = (const _NMH_MM_T * NMH_RESTRICT)__NMH_M2_V;
	const _NMH_MM_T *const NMH_RESTRICT m3    = (const _NMH_MM_T * NMH_RESTRICT)__NMH_M3_V;
		  _NMH_MM_T *const              xaccX = (      _NMH_MM_T *             )accX;
		  _NMH_MM_T *const              xaccY = (      _NMH_MM_T *             )accY;
		  _NMH_MM_T *const              xp    = (      _NMH_MM_T *             )p;
	size_t i;
	
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(xaccX[i], _NMH_MMW_(loadu_si)(xp + i));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccY[i] = _NMH_MMW_(xor_si)(xaccY[i], _NMH_MMW_(loadu_si)(xp + i + NMH_VECTOR_NB_GROUP));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(add_epi32)(xaccX[i], xaccY[i]);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccY[i] = _NMH_MMW_(xor_si)(xaccY[i], _NMH_MM_(srli_epi32)(xaccX[i], 1));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(mullo_epi16)(xaccX[i], *m1);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(_NMH_MMW_(xor_si)(xaccX[i], _NMH_MM_(slli_epi32)(xaccX[i], 5)), _NMH_MM_(srli_epi32)(xaccX[i], 13));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(mullo_epi16)(xaccX[i], *m2);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(xaccX[i], xaccY[i]);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(_NMH_MMW_(xor_si)(xaccX[i], _NMH_MM_(slli_epi32)(xaccX[i], 11)), _NMH_MM_(srli_epi32)(xaccX[i], 9));
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MM_(mullo_epi16)(xaccX[i], *m3);
	}
	for (i = 0; i < NMH_VECTOR_NB_GROUP; ++i) {
		xaccX[i] = _NMH_MMW_(xor_si)(_NMH_MMW_(xor_si)(xaccX[i], _NMH_MM_(srli_epi32)(xaccX[i], 10)), _NMH_MM_(srli_epi32)(xaccX[i], 20));
	}
}
#  undef _NMH_MM_
#  undef _NMH_MMW_
#  undef _NMH_MM_T
#  undef NMH_VECTOR_NB_GROUP
#endif

static
uint32_t
NMHASH32_long(const uint8_t* const NMH_RESTRICT p, size_t const len, uint32_t const seed)
{
	NMH_ALIGN(NMH_ACC_ALIGN) uint32_t accX[sizeof(NMH_ACC_INIT)/sizeof(*NMH_ACC_INIT)];
	NMH_ALIGN(NMH_ACC_ALIGN) uint32_t accY[sizeof(accX)/sizeof(*accX)];
	size_t const nbRounds = (len - 1) / (sizeof(accX) + sizeof(accY));
	size_t i;
	uint32_t sum = 0;
	
	/* init */
	for (i = 0; i < sizeof(accX)/sizeof(*accX); ++i) accX[i] = NMH_ACC_INIT[i];
	for (i = 0; i < sizeof(accY)/sizeof(*accY); ++i) accY[i] = seed;
	
	for (i = 0; i < nbRounds; ++i) {
		NMHASH32_long_round(accX, accY, p + i * (sizeof(accX) + sizeof(accY)));
	}
	NMHASH32_long_round(accX, accY, p + len - (sizeof(accX) + sizeof(accY)));
	
	/* merge acc */
	for (i = 0; i < sizeof(accX)/sizeof(*accX); ++i) accX[i] ^= NMH_ACC_INIT[i];
	for (i = 0; i < sizeof(accX)/sizeof(*accX); ++i) sum += accX[i];
	
#	if SIZE_MAX > UINT32_C(-1)
	sum += (uint32_t)(len >> 32);
#	endif
	return sum ^ (uint32_t)len;
}

static inline
uint32_t
NMHASH32X_0to4(uint32_t x, uint32_t const seed)
{
	/* [bdab1ea9 18 a7896a1b 12 83796a2d 16] = 0.092922873297662509 */
	x ^= seed;
	x *= UINT32_C(0xBDAB1EA9);
	x += NMH_rotl32(seed, 31);
	x ^= x >> 18;
	x *= UINT32_C(0xA7896A1B);
	x ^= x >> 12;
	x *= UINT32_C(0x83796A2D);
	x ^= x >> 16;
	return x;
}

static inline
uint32_t
NMHASH32X_5to8(const uint8_t* const NMH_RESTRICT p, size_t const len, uint32_t const seed)
{
	/* - 5 to 9 bytes
	 * - mixer: [11049a7d 23 bcccdc7b 12 065e9dad 12] = 0.16577596555667246 */
	
	uint32_t       x = NMH_readLE32(p) ^ NMH_PRIME32_3;
	uint32_t const y = NMH_readLE32(p + len - 4) ^ seed;
	x += y;
	x ^= x >> len;
	x *= UINT32_C(0x11049A7D);
	x ^= x >> 23;
	x *= UINT32_C(0xBCCCDC7B);
	x ^= NMH_rotl32(y, 3);
	x ^= x >> 12;
	x *= UINT32_C(0x065E9DAD);
	x ^= x >> 12;
	return x;
}

static inline
uint32_t
NMHASH32X_9to255(const uint8_t* const NMH_RESTRICT p, size_t const len, uint32_t const seed)
{
	/* - at least 9 bytes
	 * - base mixer: [11049a7d 23 bcccdc7b 12 065e9dad 12] = 0.16577596555667246
	 * - tail mixer: [16 a52fb2cd 15 551e4d49 16] = 0.17162579707098322
	 */
	
	uint32_t x = NMH_PRIME32_3;
	uint32_t y = seed;
	uint32_t a = NMH_PRIME32_4;
	uint32_t b = seed;
	size_t i, r = (len - 1) / 16;
	
	for (i = 0; i < r; ++i) {
		x ^= NMH_readLE32(p + i * 16 + 0);
		y ^= NMH_readLE32(p + i * 16 + 4);
		x ^= y;
		x *= UINT32_C(0x11049A7D);
		x ^= x >> 23;
		x *= UINT32_C(0xBCCCDC7B);
		y  = NMH_rotl32(y, 4);
		x ^= y;
		x ^= x >> 12;
		x *= UINT32_C(0x065E9DAD);
		x ^= x >> 12;

		a ^= NMH_readLE32(p + i * 16 + 8);
		b ^= NMH_readLE32(p + i * 16 + 12);
		a ^= b;
		a *= UINT32_C(0x11049A7D);
		a ^= a >> 23;
		a *= UINT32_C(0xBCCCDC7B);
		b  = NMH_rotl32(b, 3);
		a ^= b;
		a ^= a >> 12;
		a *= UINT32_C(0x065E9DAD);
		a ^= a >> 12;
	}
	
	if (NMH_likely(((uint8_t)len-1) & 8)) {
		if (NMH_likely(((uint8_t)len-1) & 4)) {
			a ^= NMH_readLE32(p + r * 16 + 0);
			b ^= NMH_readLE32(p + r * 16 + 4);
			a ^= b;
			a *= UINT32_C(0x11049A7D);
			a ^= a >> 23;
			a *= UINT32_C(0xBCCCDC7B);
			a ^= NMH_rotl32(b, 4);
			a ^= a >> 12;
			a *= UINT32_C(0x065E9DAD);
		} else {
			a ^= NMH_readLE32(p + r * 16) + b;
			a ^= a >> 16;
			a *= UINT32_C(0xA52FB2CD);
			a ^= a >> 15;
			a *= UINT32_C(0x551E4D49);
		}
		
		x ^= NMH_readLE32(p + len - 8);
		y ^= NMH_readLE32(p + len - 4);
		x ^= y;
		x *= UINT32_C(0x11049A7D);
		x ^= x >> 23;
		x *= UINT32_C(0xBCCCDC7B);
		x ^= NMH_rotl32(y, 3);
		x ^= x >> 12;
		x *= UINT32_C(0x065E9DAD);
	} else {
		if (NMH_likely(((uint8_t)len-1) & 4)) {
			a ^= NMH_readLE32(p + r * 16) + b;
			a ^= a >> 16;
			a *= UINT32_C(0xA52FB2CD);
			a ^= a >> 15;
			a *= UINT32_C(0x551E4D49);
		}
		x ^= NMH_readLE32(p + len - 4) + y;
		x ^= x >> 16;
		x *= UINT32_C(0xA52FB2CD);
		x ^= x >> 15;
		x *= UINT32_C(0x551E4D49);
	}
	
	x ^= (uint32_t)len;
	x ^= NMH_rotl32(a, 27); /* rotate one lane to pass Diff test */
	x ^= x >> 14;
	x *= UINT32_C(0x141CC535);
	
	return x;
}

static inline
uint32_t
NMHASH32X_avalanche32(uint32_t x)
{
	/* mixer with 2 mul from skeeto/hash-prospector:
	 * [15 d168aaad 15 af723597 15] = 0.15983776156606694
	 */
	x ^= x >> 15;
	x *= UINT32_C(0xD168AAAD);
	x ^= x >> 15;
	x *= UINT32_C(0xAF723597);
	x ^= x >> 15;
	return x;
}

/* use 32*32->32 multiplication for short hash */
static inline
uint32_t
NMHASH32X(const void* const NMH_RESTRICT input, size_t const len, uint32_t seed)
{
	const uint8_t *const p = (const uint8_t *)input;
	if (NMH_likely(len <= 8)) {
		if (NMH_likely(len > 4)) {
			return NMHASH32X_5to8(p, len, seed);
		} else {
			/* 0-4 bytes */
			union { uint32_t u32; uint16_t u16[2]; uint8_t u8[4]; } data;
			switch (len) {
				case 0: seed += NMH_PRIME32_2;
					data.u32 = 0;
					break;
				case 1: seed += NMH_PRIME32_2 + (UINT32_C(1) << 24) + (1 << 1);
					data.u32 = p[0];
					break;
				case 2: seed += NMH_PRIME32_2 + (UINT32_C(2) << 24) + (2 << 1);
					data.u32 = NMH_readLE16(p);
					break;
				case 3: seed += NMH_PRIME32_2 + (UINT32_C(3) << 24) + (3 << 1);
					data.u16[1] = p[2];
					data.u16[0] = NMH_readLE16(p);
					break;
				case 4: seed += NMH_PRIME32_1;
					data.u32 = NMH_readLE32(p);
					break;
				default: return 0;
			}
			return NMHASH32X_0to4(data.u32, seed);
		}
	}
	if (NMH_likely(len < 256)) {
		return NMHASH32X_9to255(p, len, seed);
	}
	return NMHASH32X_avalanche32(NMHASH32_long(p, len, seed));
}

#if defined(_MSC_VER) && _MSC_VER >= 1914
#  pragma warning(pop)
#endif
#ifdef __SDCC
#  pragma restore
#  undef const
#endif



// 计算 32 位哈希值
XXAPI uint32 xrtHash32_WithSeed(ptr key, size_t len, uint32 seed)
{
	return NMHASH32X(key, len, seed);
}
XXAPI uint32 xrtHash32(ptr key, size_t len)
{
	return xrtHash32_WithSeed(key, len, HASH32_SEED);
}









/*
Hash64 - rapidhash [Ver3.0, Update : 2025/08/14 from https://github.com/Nicoshev/rapidhash]
	修改：
		删除头文件引入：<stdint.h> 和 <string.h>（最上面已经引入）
	使用协议注意事项：
		BSD 2-Clause 协议
		允许个人使用、商业使用
		复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/



/*
 * rapidhash - Very fast, high quality, platform-independent hashing algorithm.
 * Copyright (C) 2024 Nicolas De Carli
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */

/*
 *  Includes.
 */
 #if defined(_MSC_VER)
 # include <intrin.h>
 # if defined(_M_X64) && !defined(_M_ARM64EC)
 #   pragma intrinsic(_umul128)
 # endif
 #endif
 
 /*
  *  C/C++ macros.
  */
 
 #ifdef _MSC_VER
 # define RAPIDHASH_ALWAYS_INLINE __forceinline
 #elif defined(__GNUC__)
 # define RAPIDHASH_ALWAYS_INLINE inline __attribute__((__always_inline__))
 #else
 # define RAPIDHASH_ALWAYS_INLINE inline
 #endif
 
 #ifdef __cplusplus
 # define RAPIDHASH_NOEXCEPT noexcept
 # define RAPIDHASH_CONSTEXPR constexpr
 # ifndef RAPIDHASH_INLINE
 #   define RAPIDHASH_INLINE RAPIDHASH_ALWAYS_INLINE
 # endif
 # if __cplusplus >= 201402L && !defined(_MSC_VER)
 #   define RAPIDHASH_INLINE_CONSTEXPR RAPIDHASH_ALWAYS_INLINE constexpr
 # else
 #   define RAPIDHASH_INLINE_CONSTEXPR RAPIDHASH_ALWAYS_INLINE
 # endif
 #else
 # define RAPIDHASH_NOEXCEPT
 # define RAPIDHASH_CONSTEXPR static const
 # ifndef RAPIDHASH_INLINE
 #   define RAPIDHASH_INLINE static RAPIDHASH_ALWAYS_INLINE
 # endif
 # define RAPIDHASH_INLINE_CONSTEXPR RAPIDHASH_INLINE
 #endif

 /*
  *  Unrolled macro.
  *  Improves large input speed, but increases code size and worsens small input speed.
  *
  *  RAPIDHASH_COMPACT: Normal behavior.
  *  RAPIDHASH_UNROLLED: 
  *
  */
  #ifndef RAPIDHASH_UNROLLED
  # define RAPIDHASH_COMPACT
  #elif defined(RAPIDHASH_COMPACT)
  # error "cannot define RAPIDHASH_COMPACT and RAPIDHASH_UNROLLED simultaneously."
  #endif
 
 /*
  *  Protection macro, alters behaviour of rapid_mum multiplication function.
  *
  *  RAPIDHASH_FAST: Normal behavior, max speed.
  *  RAPIDHASH_PROTECTED: Extra protection against entropy loss.
  */
 #ifndef RAPIDHASH_PROTECTED
 # define RAPIDHASH_FAST
 #elif defined(RAPIDHASH_FAST)
 # error "cannot define RAPIDHASH_PROTECTED and RAPIDHASH_FAST simultaneously."
 #endif
 
 /*
  *  Likely and unlikely macros.
  */
 #if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
 # define _likely_(x)  __builtin_expect(x,1)
 # define _unlikely_(x)  __builtin_expect(x,0)
 #else
 # define _likely_(x) (x)
 # define _unlikely_(x) (x)
 #endif
 
 /*
  *  Endianness macros.
  */
 #ifndef RAPIDHASH_LITTLE_ENDIAN
 # if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
 #   define RAPIDHASH_LITTLE_ENDIAN
 # elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
 #   define RAPIDHASH_BIG_ENDIAN
 # else
 #   warning "could not determine endianness! Falling back to little endian."
 #   define RAPIDHASH_LITTLE_ENDIAN
 # endif
 #endif
 
 /*
  *  Default secret parameters.
  */
   RAPIDHASH_CONSTEXPR uint64_t rapid_secret[8] = {
     0x2d358dccaa6c78a5ull,
     0x8bb84b93962eacc9ull,
     0x4b33a62ed433d4a3ull,
     0x4d5a2da51de1aa47ull,
     0xa0761d6478bd642full,
     0xe7037ed1a0b428dbull,
     0x90ed1765281c388cull,
     0xaaaaaaaaaaaaaaaaull};
 
 /*
  *  64*64 -> 128bit multiply function.
  *
  *  @param A  Address of 64-bit number.
  *  @param B  Address of 64-bit number.
  *
  *  Calculates 128-bit C = *A * *B.
  *
  *  When RAPIDHASH_FAST is defined:
  *  Overwrites A contents with C's low 64 bits.
  *  Overwrites B contents with C's high 64 bits.
  *
  *  When RAPIDHASH_PROTECTED is defined:
  *  Xors and overwrites A contents with C's low 64 bits.
  *  Xors and overwrites B contents with C's high 64 bits.
  */
 RAPIDHASH_INLINE_CONSTEXPR void rapid_mum(uint64_t *A, uint64_t *B) RAPIDHASH_NOEXCEPT {
 #if defined(__SIZEOF_INT128__)
   __uint128_t r=*A; r*=*B;
   #ifdef RAPIDHASH_PROTECTED
   *A^=(uint64_t)r; *B^=(uint64_t)(r>>64);
   #else
   *A=(uint64_t)r; *B=(uint64_t)(r>>64);
   #endif
 #elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
   #if defined(_M_X64)
     #ifdef RAPIDHASH_PROTECTED
     uint64_t a, b;
     a=_umul128(*A,*B,&b);
     *A^=a;  *B^=b;
     #else
     *A=_umul128(*A,*B,B);
     #endif
   #else
     #ifdef RAPIDHASH_PROTECTED
     uint64_t a, b;
     b = __umulh(*A, *B);
     a = *A * *B;
     *A^=a;  *B^=b;
     #else
     uint64_t c = __umulh(*A, *B);
     *A = *A * *B;
     *B = c;
     #endif
   #endif
 #else
   uint64_t ha=*A>>32, hb=*B>>32, la=(uint32_t)*A, lb=(uint32_t)*B;
   uint64_t rh=ha*hb, rm0=ha*lb, rm1=hb*la, rl=la*lb, t=rl+(rm0<<32), c=t<rl;
   uint64_t lo=t+(rm1<<32); 
   c+=lo<t; 
   uint64_t hi=rh+(rm0>>32)+(rm1>>32)+c;
   #ifdef RAPIDHASH_PROTECTED
   *A^=lo;  *B^=hi;
   #else
   *A=lo;  *B=hi;
   #endif
 #endif
 }
 
 /*
  *  Multiply and xor mix function.
  *
  *  @param A  64-bit number.
  *  @param B  64-bit number.
  *
  *  Calculates 128-bit C = A * B.
  *  Returns 64-bit xor between high and low 64 bits of C.
  */
  RAPIDHASH_INLINE_CONSTEXPR uint64_t rapid_mix(uint64_t A, uint64_t B) RAPIDHASH_NOEXCEPT { rapid_mum(&A,&B); return A^B; }
 
 /*
  *  Read functions.
  */
 #ifdef RAPIDHASH_LITTLE_ENDIAN
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint64_t v; memcpy(&v, p, sizeof(uint64_t)); return v;}
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint32_t v; memcpy(&v, p, sizeof(uint32_t)); return v;}
 #elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint64_t v; memcpy(&v, p, sizeof(uint64_t)); return __builtin_bswap64(v);}
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint32_t v; memcpy(&v, p, sizeof(uint32_t)); return __builtin_bswap32(v);}
 #elif defined(_MSC_VER)
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint64_t v; memcpy(&v, p, sizeof(uint64_t)); return _byteswap_uint64(v);}
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT { uint32_t v; memcpy(&v, p, sizeof(uint32_t)); return _byteswap_ulong(v);}
 #else
 RAPIDHASH_INLINE uint64_t rapid_read64(const uint8_t *p) RAPIDHASH_NOEXCEPT {
   uint64_t v; memcpy(&v, p, 8);
   return (((v >> 56) & 0xff)| ((v >> 40) & 0xff00)| ((v >> 24) & 0xff0000)| ((v >>  8) & 0xff000000)| ((v <<  8) & 0xff00000000)| ((v << 24) & 0xff0000000000)| ((v << 40) & 0xff000000000000)| ((v << 56) & 0xff00000000000000));
 }
 RAPIDHASH_INLINE uint64_t rapid_read32(const uint8_t *p) RAPIDHASH_NOEXCEPT {
   uint32_t v; memcpy(&v, p, 4);
   return (((v >> 24) & 0xff)| ((v >>  8) & 0xff00)| ((v <<  8) & 0xff0000)| ((v << 24) & 0xff000000));
 }
 #endif
 
 /*
  *  rapidhash main function.
  *
  *  @param key     Buffer to be hashed.
  *  @param len     @key length, in bytes.
  *  @param seed    64-bit seed used to alter the hash result predictably.
  *  @param secret  Triplet of 64-bit secrets used to alter hash result predictably.
  *
  *  Returns a 64-bit hash.
  */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhash_internal(const void *key, size_t len, uint64_t seed, const uint64_t* secret) RAPIDHASH_NOEXCEPT {
  const uint8_t *p=(const uint8_t *)key;
  seed ^= rapid_mix(seed ^ secret[2], secret[1]);
  uint64_t a=0, b=0;
  size_t i = len;
  if (_likely_(len <= 16)) {
    if (len >= 4) {
      seed ^= len;
      if (len >= 8) {
        const uint8_t* plast = p + len - 8;
        a = rapid_read64(p);
        b = rapid_read64(plast);
      } else {
        const uint8_t* plast = p + len - 4;
        a = rapid_read32(p);
        b = rapid_read32(plast);
      }
    } else if (len > 0) {
      a = (((uint64_t)p[0])<<45)|p[len-1];
      b = p[len>>1];
    } else
      a = b = 0;
  } else {
    uint64_t see1 = seed, see2 = seed;
    uint64_t see3 = seed, see4 = seed;
    uint64_t see5 = seed, see6 = seed;
#ifdef RAPIDHASH_COMPACT
    if (i > 112) {
      do {
        seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
        see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
        see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
        see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
        see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
        see5 = rapid_mix(rapid_read64(p + 80) ^ secret[5], rapid_read64(p + 88) ^ see5);
        see6 = rapid_mix(rapid_read64(p + 96) ^ secret[6], rapid_read64(p + 104) ^ see6);
        p += 112;
        i -= 112;
      } while(i > 112);
      seed ^= see1;
      see2 ^= see3;
      see4 ^= see5;
      seed ^= see6;
      see2 ^= see4;
      seed ^= see2;
    }
#else 
    if (i > 224) {
      do {
          seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
          see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
          see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
          see5 = rapid_mix(rapid_read64(p + 80) ^ secret[5], rapid_read64(p + 88) ^ see5);
          see6 = rapid_mix(rapid_read64(p + 96) ^ secret[6], rapid_read64(p + 104) ^ see6);
          seed = rapid_mix(rapid_read64(p + 112) ^ secret[0], rapid_read64(p + 120) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 128) ^ secret[1], rapid_read64(p + 136) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 144) ^ secret[2], rapid_read64(p + 152) ^ see2);
          see3 = rapid_mix(rapid_read64(p + 160) ^ secret[3], rapid_read64(p + 168) ^ see3);
          see4 = rapid_mix(rapid_read64(p + 176) ^ secret[4], rapid_read64(p + 184) ^ see4);
          see5 = rapid_mix(rapid_read64(p + 192) ^ secret[5], rapid_read64(p + 200) ^ see5);
          see6 = rapid_mix(rapid_read64(p + 208) ^ secret[6], rapid_read64(p + 216) ^ see6);
          p += 224;
          i -= 224;
      } while (i > 224);
    }
    if (i > 112) {
      seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
      see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
      see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
      see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
      see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
      see5 = rapid_mix(rapid_read64(p + 80) ^ secret[5], rapid_read64(p + 88) ^ see5);
      see6 = rapid_mix(rapid_read64(p + 96) ^ secret[6], rapid_read64(p + 104) ^ see6);
      p += 112;
      i -= 112;
    }
    seed ^= see1;
    see2 ^= see3;
    see4 ^= see5;
    seed ^= see6;
    see2 ^= see4;
    seed ^= see2;
#endif
    if (i > 16) {
      seed = rapid_mix(rapid_read64(p) ^ secret[2], rapid_read64(p + 8) ^ seed);
      if (i > 32) {
          seed = rapid_mix(rapid_read64(p + 16) ^ secret[2], rapid_read64(p + 24) ^ seed);
          if (i > 48) {
              seed = rapid_mix(rapid_read64(p + 32) ^ secret[1], rapid_read64(p + 40) ^ seed);
              if (i > 64) {
                  seed = rapid_mix(rapid_read64(p + 48) ^ secret[1], rapid_read64(p + 56) ^ seed);
                  if (i > 80) {
                      seed = rapid_mix(rapid_read64(p + 64) ^ secret[2], rapid_read64(p + 72) ^ seed);
                      if (i > 96) {
                          seed = rapid_mix(rapid_read64(p + 80) ^ secret[1], rapid_read64(p + 88) ^ seed);
                      }
                  }
              }
          }
      }
    }
    a=rapid_read64(p+i-16) ^ i;  b=rapid_read64(p+i-8);
  }
  a ^= secret[1];
  b ^= seed;
  rapid_mum(&a, &b);
  return rapid_mix(a ^ secret[7], b ^ secret[1] ^ i);
}

 /*
  *  rapidhashMicro main function.
  *
  *  @param key     Buffer to be hashed.
  *  @param len     @key length, in bytes.
  *  @param seed    64-bit seed used to alter the hash result predictably.
  *  @param secret  Triplet of 64-bit secrets used to alter hash result predictably.
  *
  *  Returns a 64-bit hash.
  */
  RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashMicro_internal(const void *key, size_t len, uint64_t seed, const uint64_t* secret) RAPIDHASH_NOEXCEPT {
    const uint8_t *p=(const uint8_t *)key;
    seed ^= rapid_mix(seed ^ secret[2], secret[1]);
    uint64_t a=0, b=0;
    size_t i = len;
    if (_likely_(len <= 16)) {
      if (len >= 4) {
        seed ^= len;
        if (len >= 8) {
          const uint8_t* plast = p + len - 8;
          a = rapid_read64(p);
          b = rapid_read64(plast);
        } else {
          const uint8_t* plast = p + len - 4;
          a = rapid_read32(p);
          b = rapid_read32(plast);
        }
      } else if (len > 0) {
        a = (((uint64_t)p[0])<<45)|p[len-1];
        b = p[len>>1];
      } else
        a = b = 0;
    } else {
      if (i > 80) {
        uint64_t see1 = seed, see2 = seed;
        uint64_t see3 = seed, see4 = seed;
        do {
          seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
          see3 = rapid_mix(rapid_read64(p + 48) ^ secret[3], rapid_read64(p + 56) ^ see3);
          see4 = rapid_mix(rapid_read64(p + 64) ^ secret[4], rapid_read64(p + 72) ^ see4);
          p += 80;
          i -= 80;
        } while(i > 80);
        seed ^= see1;
        see2 ^= see3;
        seed ^= see4;
        seed ^= see2;
      }
      if (i > 16) {
        seed = rapid_mix(rapid_read64(p) ^ secret[2], rapid_read64(p + 8) ^ seed);
        if (i > 32) {
            seed = rapid_mix(rapid_read64(p + 16) ^ secret[2], rapid_read64(p + 24) ^ seed);
            if (i > 48) {
                seed = rapid_mix(rapid_read64(p + 32) ^ secret[1], rapid_read64(p + 40) ^ seed);
                if (i > 64) {
                    seed = rapid_mix(rapid_read64(p + 48) ^ secret[1], rapid_read64(p + 56) ^ seed);
                }
            }
        }
      }
      a=rapid_read64(p+i-16) ^ i;  b=rapid_read64(p+i-8);
    }
    a ^= secret[1];
    b ^= seed;
    rapid_mum(&a, &b);
    return rapid_mix(a ^ secret[7], b ^ secret[1] ^ i);
  }

  /*
  *  rapidhashNano main function.
  *
  *  @param key     Buffer to be hashed.
  *  @param len     @key length, in bytes.
  *  @param seed    64-bit seed used to alter the hash result predictably.
  *  @param secret  Triplet of 64-bit secrets used to alter hash result predictably.
  *
  *  Returns a 64-bit hash.
  */
  RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashNano_internal(const void *key, size_t len, uint64_t seed, const uint64_t* secret) RAPIDHASH_NOEXCEPT {
    const uint8_t *p=(const uint8_t *)key;
    seed ^= rapid_mix(seed ^ secret[2], secret[1]);
    uint64_t a=0, b=0;
    size_t i = len;
    if (_likely_(len <= 16)) {
      if (len >= 4) {
        seed ^= len;
        if (len >= 8) {
          const uint8_t* plast = p + len - 8;
          a = rapid_read64(p);
          b = rapid_read64(plast);
        } else {
          const uint8_t* plast = p + len - 4;
          a = rapid_read32(p);
          b = rapid_read32(plast);
        }
      } else if (len > 0) {
        a = (((uint64_t)p[0])<<45)|p[len-1];
        b = p[len>>1];
      } else
        a = b = 0;
    } else {
      if (i > 48) {
        uint64_t see1 = seed, see2 = seed;
        do {
          seed = rapid_mix(rapid_read64(p) ^ secret[0], rapid_read64(p + 8) ^ seed);
          see1 = rapid_mix(rapid_read64(p + 16) ^ secret[1], rapid_read64(p + 24) ^ see1);
          see2 = rapid_mix(rapid_read64(p + 32) ^ secret[2], rapid_read64(p + 40) ^ see2);
          p += 48;
          i -= 48;
        } while(i > 48);
        seed ^= see1;
        seed ^= see2;
      }
      if (i > 16) {
        seed = rapid_mix(rapid_read64(p) ^ secret[2], rapid_read64(p + 8) ^ seed);
        if (i > 32) {
            seed = rapid_mix(rapid_read64(p + 16) ^ secret[2], rapid_read64(p + 24) ^ seed);
        }
      }
      a=rapid_read64(p+i-16) ^ i;  b=rapid_read64(p+i-8);
    }
    a ^= secret[1];
    b ^= seed;
    rapid_mum(&a, &b);
    return rapid_mix(a ^ secret[7], b ^ secret[1] ^ i);
  }
 
/*
 *  rapidhash seeded hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhash_withSeed(const void *key, size_t len, uint64_t seed) RAPIDHASH_NOEXCEPT {
  return rapidhash_internal(key, len, seed, rapid_secret);
}
 
/*
 *  rapidhash general purpose hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *
 *  Calls rapidhash_withSeed using provided parameters and the default seed.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhash(const void *key, size_t len) RAPIDHASH_NOEXCEPT {
  return rapidhash_withSeed(key, len, 0);
}

/*
 *  rapidhashMicro seeded hash function.
 *
 *  Designed for HPC and server applications, where cache misses make a noticeable performance detriment.
 *  Clang-18+ compiles it to ~140 instructions without stack usage, both on x86-64 and aarch64.
 *  Faster for sizes up to 512 bytes, just 15%-20% slower for inputs above 1kb.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
 RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashMicro_withSeed(const void *key, size_t len, uint64_t seed) RAPIDHASH_NOEXCEPT {
  return rapidhashMicro_internal(key, len, seed, rapid_secret);
}
 
/*
 *  rapidhashMicro hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *
 *  Calls rapidhash_withSeed using provided parameters and the default seed.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashMicro(const void *key, size_t len) RAPIDHASH_NOEXCEPT {
  return rapidhashMicro_withSeed(key, len, 0);
}

/*
 *  rapidhashNano seeded hash function.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *  @param seed    64-bit seed used to alter the hash result predictably.
 *
 *  Calls rapidhash_internal using provided parameters and default secrets.
 *
 *  Returns a 64-bit hash.
 */
 RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashNano_withSeed(const void *key, size_t len, uint64_t seed) RAPIDHASH_NOEXCEPT {
  return rapidhashNano_internal(key, len, seed, rapid_secret);
}
 
/*
 *  rapidhashNano hash function.
 *
 *  Designed for Mobile and embedded applications, where keeping a small code size is a top priority.
 *  Clang-18+ compiles it to less than 100 instructions without stack usage, both on x86-64 and aarch64.
 *  The fastest for sizes up to 48 bytes, but may be considerably slower for larger inputs.
 *
 *  @param key     Buffer to be hashed.
 *  @param len     @key length, in bytes.
 *
 *  Calls rapidhash_withSeed using provided parameters and the default seed.
 *
 *  Returns a 64-bit hash.
 */
RAPIDHASH_INLINE_CONSTEXPR uint64_t rapidhashNano(const void *key, size_t len) RAPIDHASH_NOEXCEPT {
  return rapidhashNano_withSeed(key, len, 0);
}



// 计算 64 位哈希值
XXAPI uint64 xrtHash64_WithSeed(ptr key, size_t len, uint64 seed)
{
	return rapidhash_internal(key, len, seed, rapid_secret);
}
XXAPI uint64 xrtHash64(ptr key, size_t len)
{
	return xrtHash64_WithSeed(key, len, 0);
}

XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed)
{
	return rapidhashMicro_internal(key, len, seed, rapid_secret);
}
XXAPI uint64 xrtHash64_Micro(ptr key, size_t len)
{
	return xrtHash64_Micro_WithSeed(key, len, 0);
}

XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed)
{
	return rapidhashNano_internal(key, len, seed, rapid_secret);
}
XXAPI uint64 xrtHash64_Nano(ptr key, size_t len)
{
	return xrtHash64_Nano_WithSeed(key, len, 0);
}


