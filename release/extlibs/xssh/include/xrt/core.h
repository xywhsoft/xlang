#ifndef XRT_CORE_H
#define XRT_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



/* XRT 版本信息。 */
#define XRT_VERSION_MAJOR	2
#define XRT_VERSION_MINOR	0
#define XRT_VERSION_PATCH	0
#define XRT_VERSION_TEXT		"2.0.0-dev"



/* 所有基于 size_t 的查找接口共用的未找到标记。 */
#define XRT_NPOS SIZE_MAX



/* 动态库符号导出规则。 */
#if defined(_WIN32) || defined(_WIN64)
	#if defined(XRT_BUILD_SHARED)
		#define XRT_API __declspec(dllexport)
	#elif defined(XRT_USE_SHARED)
		#define XRT_API __declspec(dllimport)
	#else
		#define XRT_API
	#endif
#elif defined(__GNUC__) || defined(__clang__)
	#if defined(XRT_BUILD_SHARED)
		#define XRT_API __attribute__((visibility("default")))
	#else
		#define XRT_API
	#endif
#else
	#define XRT_API
#endif



/* C 与 C++ 共用的链接规则。 */
#if defined(__cplusplus)
	#define XRT_EXTERN_C_BEGIN extern "C" {
	#define XRT_EXTERN_C_END }
#else
	#define XRT_EXTERN_C_BEGIN
	#define XRT_EXTERN_C_END
#endif



/* XRT 公共基础类型。 */
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef void* ptr;
typedef char* str;
typedef const char* cstr;
typedef unsigned char* bytes;
typedef const unsigned char* cbytes;



/* 通用 IO 与文件游标共享的移动基准。 */
typedef enum xseek {
	XSEEK_START = 0,
	XSEEK_CURRENT,
	XSEEK_END
} xseek;



/* 绝对时间使用 Unix Epoch 微秒；该标量也是 xlang time 类型的底层表示。 */
typedef int64 xtime;



/* 解析器、压缩器与归档器共用的资源边界。零值表示不限制对应项目。 */
#define XRT_RESOURCE_LIMITS_VERSION 1u
#define XRT_RESOURCE_ALLOW_SYMLINKS 0x00000001u
#define XRT_RESOURCE_ALLOW_HARDLINKS 0x00000002u
#define XRT_RESOURCE_ALLOW_DEVICE_FILES 0x00000004u
#define XRT_RESOURCE_ALLOW_EXTERNAL_ENTITIES 0x00000008u

typedef struct xrtresourcelimits {
	uint32 iSize;
	uint32 iVersion;
	uint64 iMaxInputBytes;
	uint64 iMaxOutputBytes;
	uint64 iMaxItemBytes;
	uint64 iMaxEntries;
	uint64 iMaxNodes;
	uint32 iMaxDepth;
	uint32 iMaxCompressionRatio;
	uint32 iFlags;
	uint32 iReserved;
} xrtresourcelimits;



/* 长耗时流操作共用的进度事件。回调仅在发起操作的线程内同步调用。 */
#define XRT_PROGRESS_VERSION 1u

typedef enum xrtprogressflag {
	XRT_PROGRESS_TOTAL_KNOWN = 1u << 0,
	XRT_PROGRESS_FINAL = 1u << 1
} xrtprogressflag;

typedef struct xrtprogress {
	uint32 iSize;
	uint32 iVersion;
	uint32 iFlags;
	uint32 iReserved;
	uint64 iInputBytes;
	uint64 iTotalInputBytes;
	uint64 iOutputBytes;
} xrtprogress;

/* 返回 false 请求取消。实现不得在回调返回后继续保存 pProgress 或 pUserData。 */
typedef bool (*xrtprogressproc)(const xrtprogress* pProgress, ptr pUserData);

static inline bool xrtProgressReport(xrtprogressproc pProc, ptr pUserData,
	uint64 iInputBytes, uint64 iTotalInputBytes, uint64 iOutputBytes, uint32 iFlags)
{
	xrtprogress tProgress;
	if ( pProc == NULL ) { return true; }
	tProgress.iSize = (uint32)sizeof(tProgress);
	tProgress.iVersion = XRT_PROGRESS_VERSION;
	tProgress.iFlags = iFlags;
	tProgress.iReserved = 0u;
	tProgress.iInputBytes = iInputBytes;
	tProgress.iTotalInputBytes = iTotalInputBytes;
	tProgress.iOutputBytes = iOutputBytes;
	return pProc(&tProgress, pUserData);
}



/* 字节视图只借用内存，不拥有数据，也不要求末尾补零。 */
typedef struct xbytesview {
	cbytes Data;
	size_t Size;
} xbytesview;



/* 字符串视图只借用字节，不拥有数据，也不要求末尾补零。 */
typedef struct xstrview {
	cstr Data;
	size_t Size;
} xstrview;



/* INIT 用于聚合初始化器；LITERAL 用于赋值和函数实参表达式。 */
#define XRT_BYTES_INIT(sData) \
	{ (const unsigned char*)(sData), sizeof(sData) - 1u }
#define XRT_STR_INIT(sText) { (sText), sizeof(sText) - 1u }



#if defined(__cplusplus)
	#define XRT_BYTES_LITERAL(sData) xbytesview{ (const unsigned char*)(sData), sizeof(sData) - 1u }
	#define XRT_STR_LITERAL(sText) xstrview{ (sText), sizeof(sText) - 1u }
#else
	#define XRT_BYTES_LITERAL(sData) ((xbytesview){ (const unsigned char*)(sData), sizeof(sData) - 1u })
	#define XRT_STR_LITERAL(sText) ((xstrview){ (sText), sizeof(sText) - 1u })
#endif



XRT_EXTERN_C_BEGIN



/* 返回当前 XRT 版本字符串。 */
XRT_API cstr xrtVersion(void);



/* 初始化一组适合处理不受信任输入的通用资源边界。 */
XRT_API void xrtResourceLimitsInit(xrtresourcelimits* pLimits);



/* 原子增加有效引用计数，失败时返回 -1。 */
XRT_API int32 xrtRefRetain(volatile int32* pCount);



/* 原子减少有效引用计数，失败时返回 -1。 */
XRT_API int32 xrtRefRelease(volatile int32* pCount);



XRT_EXTERN_C_END

#endif
