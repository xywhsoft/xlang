#ifndef XRT_CODEC_H
#define XRT_CODEC_H

#include <xrt/error.h>
#include <xrt/memory.h>



#if defined(XRT_FEATURE_CODEC_HEX)

/* HEX 编码可选大写字母，解码可选忽略 ASCII 空白。 */
typedef enum xhexflag {
	XHEX_UPPER = UINT32_C(0x00000001),
	XHEX_IGNORE_SPACE = UINT32_C(0x00000002)
} xhexflag;



#endif



#if defined(XRT_FEATURE_CODEC_BASE64)

/* Base64 配置标志；默认使用标准字母表、规范填充并严格拒绝空白。 */
typedef enum xbase64flag {
	XBASE64_URL = UINT32_C(0x00000001),
	XBASE64_NO_PADDING = UINT32_C(0x00000002),
	XBASE64_IGNORE_SPACE = UINT32_C(0x00000004),
	XBASE64_OPTIONAL_PADDING = UINT32_C(0x00000008)
} xbase64flag;



/* 自定义字母表必须是 64 个互不重复的可见 ASCII 字符；空指针表示使用内置字母表。 */
typedef struct xbase64config {
	cstr Alphabet;
	uint32 Flags;
} xbase64config;



#endif



#if defined(XRT_FEATURE_CODEC_HEX) || defined(XRT_FEATURE_CODEC_BASE64) || \
	defined(XRT_FEATURE_CODEC_PERCENT)

/* Codec 模块稳定错误码；各编码族使用独立编号区间。 */
typedef enum xcodecerror {
	#if defined(XRT_FEATURE_CODEC_HEX)
	XCODEC_ERROR_HEX_CONFIG = 901,
	XCODEC_ERROR_HEX_FORMAT = 902,
	#endif

	#if defined(XRT_FEATURE_CODEC_BASE64)
	XCODEC_ERROR_BASE64_CONFIG = 1001,
	XCODEC_ERROR_BASE64_FORMAT = 1002,
	#endif

	#if defined(XRT_FEATURE_CODEC_PERCENT)
	XCODEC_ERROR_PERCENT_CONFIG = 1101,
	XCODEC_ERROR_PERCENT_FORMAT = 1102,
	#endif
} xcodecerror;

#endif



#if defined(XRT_FEATURE_CODEC_PERCENT)

/* 逐字节 percent 解码明确区分非法转义、输入结束和一个有效字节。 */
typedef enum xpercentnext {
	XPERCENT_NEXT_ERROR = -1,
	XPERCENT_NEXT_END = 0,
	XPERCENT_NEXT_BYTE = 1
} xpercentnext;



/*
	预编译的 ASCII 安全字符集合。
	该结构可按值复制，供大量字段编码时复用，避免反复构建字符位图。
*/
typedef struct xpercentmap {
	uint64 Bits[2];
} xpercentmap;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_CODEC_HEX)

/*
	把任意字节编码为 HEX 文本。
	输出为空且容量为零时只查询文本长度；实际写入要求容量额外包含末尾零字节。
*/
XRT_API bool xrtHexEncode(
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
);



/*
	严格解码 HEX 文本；输出为空且容量为零时只验证并查询字节数。
	输出可以与输入从同一地址开始，从而原地解码。
*/
XRT_API bool xrtHexDecode(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	uint32 iFlags
);



/* 编码并返回由 xrtFree 释放的末尾补零文本。 */
XRT_API str xrtHexEncodeNew(
	const void* pData,
	size_t iSize,
	uint32 iFlags
);



/* 解码并返回由 xrtFree 释放的字节；额外末尾零字节不计入结果长度。 */
XRT_API bytes xrtHexDecodeNew(
	xstrview Text,
	size_t* pOutputSize,
	uint32 iFlags
);

#endif



#if defined(XRT_FEATURE_CODEC_BASE64)

/*
	把字节编码为 Base64 文本。
	输出为空且容量为零时只查询文本长度；实际写入要求容量额外包含末尾零字节。
*/
XRT_API bool xrtBase64Encode(
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	const xbase64config* pConfig
);



/*
	严格解码 Base64 文本；输出为空且容量为零时只验证并查询字节数。
	输出可以与输入从同一地址开始，从而原地解码。
*/
XRT_API bool xrtBase64Decode(
	cstr sText,
	size_t iTextSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	const xbase64config* pConfig
);



/* 编码并返回由 xrtFree 释放的末尾补零文本。 */
XRT_API str xrtBase64EncodeNew(
	const void* pData,
	size_t iSize,
	const xbase64config* pConfig
);



/* 解码并返回由 xrtFree 释放的字节；额外的末尾零字节不计入结果长度。 */
XRT_API bytes xrtBase64DecodeNew(
	cstr sText,
	size_t iTextSize,
	size_t* pOutputSize,
	const xbase64config* pConfig
);

#endif





#if defined(XRT_FEATURE_CODEC_PERCENT)

/*
	构建可复用的 ASCII 安全字符集合。
	IncludeUnreserved 为真时先加入 RFC 3986 unreserved 字符；Safe 可继续追加
	可见 ASCII 字符。函数原子更新 Map，并拒绝控制字符、非 ASCII 和范围别名。
*/
XRT_API bool xrtPercentMapInit(
	xpercentmap* pMap,
	xstrview Safe,
	bool bIncludeUnreserved
);



/* 计算指定字符集合和空格规则下的精确编码长度。 */
XRT_API bool xrtPercentMeasure(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	size_t* pOutputSize
);



/*
	把已经由 xrtPercentMeasure 预检的输入顺序写入不重叠输出。
	调用方必须提供不少于测量结果的空间；返回实际写出字节数，不写终止零。
*/
XRT_API size_t xrtPercentWriteMeasured(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	char* sOutput
);



/*
	把已经测量的输入编码到可同址扩张的输出，并可补写终止零。
	OutputSize 必须是 xrtPercentMeasure 返回的精确长度，输出容量由调用方保证。
*/
XRT_API void xrtPercentEncodeMeasured(
	const void* pData,
	size_t iSize,
	const xpercentmap* pMap,
	bool bSpaceAsPlus,
	char* sOutput,
	size_t iOutputSize,
	bool bTerminate
);



/* 严格验证全部 percent 转义并计算解码字节数。 */
XRT_API bool xrtPercentDecodeMeasure(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pOutputSize
);



/*
	把已经由 xrtPercentDecodeMeasure 预检的文本顺序解码到输出。
	输出可以与输入同址，调用方必须提供不少于测量结果的空间。
*/
XRT_API size_t xrtPercentDecodeMeasured(
	xstrview Text,
	bool bPlusAsSpace,
	void* pOutput
);

/*
	无分配读取一个原始或 percent 转义字节；Offset 初始为零。
	语法、范围或别名错误不推进 Offset、不修改 Value，也不修改线程错误。
	PlusAsSpace 用于表单语义；普通 URI 路径必须传 false。
*/
XRT_API xpercentnext xrtPercentNext(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pOffset,
	uint8* pValue
);



/*
	按 RFC 3986 对字节进行百分号编码。
	ExtraSafe 可额外保留 URI 保留字符；编码文本包含零结尾，返回长度不计零结尾。
*/
XRT_API bool xrtPercentEncode(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/*
	按 RFC 3986 写出不带零结尾的编码片段。
	查询长度、原地扩张、重叠检查和失败原子性与 PercentEncode 一致。
*/
XRT_API bool xrtPercentWrite(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/*
	严格解码百分号转义；加号保持不变，输出可以与输入从同一地址开始。
	空输出且容量为零时只验证格式并查询解码字节数。
*/
XRT_API bool xrtPercentDecode(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 编码并返回由 xrtFree 释放的零结尾文本；长度输出可以为空。 */
XRT_API str xrtPercentEncodeNew(
	const void* pData,
	size_t iSize,
	xstrview ExtraSafe,
	size_t* pOutputSize
);



/* 解码并返回由 xrtFree 释放的字节；末尾哨兵零不计入返回长度。 */
XRT_API bytes xrtPercentDecodeNew(
	xstrview Text,
	size_t* pOutputSize
);

#endif



XRT_EXTERN_C_END

#endif
