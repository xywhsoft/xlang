#ifndef XRT_RANDOM_H
#define XRT_RANDOM_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_RANDOM_DEFAULT) && !defined(XRT_FEATURE_RANDOM)
	#error "XRT default random support requires XRT_FEATURE_RANDOM"
#endif

#if defined(XRT_FEATURE_RANDOM_TEXT) && !defined(XRT_FEATURE_RANDOM)
	#error "XRT random text support requires XRT_FEATURE_RANDOM"
#endif

#if defined(XRT_FEATURE_RANDOM_TEXT_DEFAULT) && \
	(!defined(XRT_FEATURE_RANDOM_TEXT) || !defined(XRT_FEATURE_RANDOM_DEFAULT))
	#error "XRT default random text requires random text and default random support"
#endif

#if defined(XRT_FEATURE_RANDOM_SECURE_TEXT) && \
	!defined(XRT_FEATURE_RANDOM_SECURE)
	#error "XRT secure random text requires secure random support"
#endif



#if defined(XRT_FEATURE_RANDOM_SECURE)

/* 操作系统安全随机源稳定错误代码。 */
typedef enum xrandomerror {
	XRANDOM_ERROR_SYSTEM = 1
} xrandomerror;

#endif



#if defined(XRT_FEATURE_RANDOM)

/* PCG32 状态由调用方持有；字段公开仅用于无分配存储，不应直接修改。 */
typedef struct xrng {
	uint64 State;
	uint64 Increment;
	uint32 Guard;
	uint32 Reserved;
} xrng;



/* 静态初始化得到一条固定、可复现的默认序列。 */
#define XRT_RNG_INITIALIZER \
	{ UINT64_C(0x853C49E6748FEA9B), UINT64_C(0xDA3E39CB94B95BDB), \
		UINT32_C(0x524E4731), 0u }

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_RANDOM_SECURE)

/* 使用操作系统密码安全随机源填满缓冲；失败时清零整个输出。 */
XRT_API bool xrtSecureRandom(ptr pData, size_t iSize);

#endif



#if defined(XRT_FEATURE_RANDOM_SECURE_TEXT)

/* 使用操作系统安全随机源和自定义字母表写入随机文本并补零。 */
XRT_API bool xrtSecureText(xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength);



/* 使用自定义字母表创建由 xrtFree 释放的密码安全随机字符串。 */
XRT_API str xrtSecureStringFrom(xstrview Alphabet, size_t iLength);



/* 使用 URL-safe 64 字符字母表创建密码安全随机字符串。 */
XRT_API str xrtSecureString(size_t iLength);

#endif



#if defined(XRT_FEATURE_RANDOM)

/* 用 seed 和 stream 初始化或重置一个显式随机数状态。 */
XRT_API void xrtRngSeed(xrng* pRng, uint64 iSeed, uint64 iStream);



/* 判断显式随机数状态是否已经初始化且内部约束自洽。 */
XRT_API bool xrtRngReady(const xrng* pRng);



/* 从显式状态生成一个 32 位伪随机数。 */
XRT_API uint32 xrtRng32(xrng* pRng);



/* 从同一个显式状态连续生成并组合一个 64 位伪随机数。 */
XRT_API uint64 xrtRng64(xrng* pRng);



/* 按稳定的小端字节顺序填充缓冲区；同一状态在所有平台产生相同结果。 */
XRT_API bool xrtRngBytes(xrng* pRng, ptr pData, size_t iSize);



/* 无偏生成 [0, iBound) 内的 32 位整数；iBound 必须非零。 */
XRT_API uint32 xrtRngBelow32(xrng* pRng, uint32 iBound);



/* 无偏生成 [0, iBound) 内的 64 位整数；iBound 必须非零。 */
XRT_API uint64 xrtRngBelow64(xrng* pRng, uint64 iBound);



/* 无偏生成半开区间 [iMin, iMax) 内的整数。 */
XRT_API int64 xrtRngRange(xrng* pRng, int64 iMin, int64 iMax);



/* 无偏生成闭区间 [iMin, iMax] 内的整数，包括完整 int64 域。 */
XRT_API int64 xrtRngRangeClosed(xrng* pRng, int64 iMin, int64 iMax);



/* 生成半开区间 [0.0, 1.0) 内具有 53 位精度的双精度数。 */
XRT_API double xrtRngReal(xrng* pRng);



/* 使用 Fisher-Yates 算法原地打乱定长元素数组，不执行内存分配。 */
XRT_API bool xrtRngShuffle(xrng* pRng,
	ptr pData, size_t iCount, size_t iItemSize);

#endif



#if defined(XRT_FEATURE_RANDOM_DEFAULT)

/* 重置当前线程的便捷随机数状态。 */
XRT_API void xrtRandSeed(uint64 iSeed, uint64 iStream);



/* 从当前线程状态生成一个 32 位伪随机数。 */
XRT_API uint32 xrtRand32(void);



/* 从当前线程状态生成一个 64 位伪随机数。 */
XRT_API uint64 xrtRand64(void);



/* 使用当前线程随机状态按稳定的小端顺序填充字节。 */
XRT_API bool xrtRandBytes(ptr pData, size_t iSize);



/* 从当前线程状态无偏生成 [0, iBound) 内的整数。 */
XRT_API uint64 xrtRandBelow(uint64 iBound);



/* 从当前线程状态无偏生成半开区间 [iMin, iMax) 内的整数。 */
XRT_API int64 xrtRandRange(int64 iMin, int64 iMax);



/* 从当前线程状态无偏生成闭区间 [iMin, iMax] 内的整数。 */
XRT_API int64 xrtRandRangeClosed(int64 iMin, int64 iMax);



/* 从当前线程状态生成 [0.0, 1.0) 内的双精度数。 */
XRT_API double xrtRandReal(void);



/* 使用当前线程随机状态原地打乱定长元素数组。 */
XRT_API bool xrtRandShuffle(ptr pData, size_t iCount, size_t iItemSize);

#endif



#if defined(XRT_FEATURE_RANDOM_TEXT)

/* 把可复现随机文本写入调用方缓冲区并补零。 */
XRT_API bool xrtRngText(xrng* pRng, xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength);



/* 使用自定义字母表创建由 xrtFree 释放的可复现随机字符串。 */
XRT_API str xrtRngStringFrom(xrng* pRng, xstrview Alphabet, size_t iLength);



/* 使用 URL-safe 64 字符字母表创建可复现随机字符串。 */
XRT_API str xrtRngString(xrng* pRng, size_t iLength);

#endif



#if defined(XRT_FEATURE_RANDOM_TEXT_DEFAULT)

/* 使用当前线程随机状态把文本写入调用方缓冲区并补零。 */
XRT_API bool xrtRandText(xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength);



/* 使用当前线程随机状态和自定义字母表创建随机字符串。 */
XRT_API str xrtRandStringFrom(xstrview Alphabet, size_t iLength);



/* 使用当前线程随机状态和默认字母表创建随机字符串。 */
XRT_API str xrtRandString(size_t iLength);

#endif



XRT_EXTERN_C_END

#endif
