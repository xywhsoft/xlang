#ifndef XRT_FORM_H
#define XRT_FORM_H

#include <xrt.h>



#if defined(XHTTP_FEATURE_FORM_URLENCODED) && \
	(!defined(XRT_FEATURE_CODEC_PERCENT) || !defined(XHTTP_FEATURE_QUERY))
	#error "XRT form-urlencoded requires codec_percent and query"
#endif



#if defined(XHTTP_FEATURE_FORM_URLENCODED)

/* 已解码表单字段借用可变输入或调用方提供的原始字节。 */
typedef struct xformfield {
	xbytesview Name;
	xbytesview Value;
} xformfield;



/* 表单限额中的零表示不限，不影响无配置的底层 codec。 */
typedef struct xformlimits {
	size_t MaxFields;
	size_t MaxName;
	size_t MaxValue;
	size_t MaxBytes;
} xformlimits;



/* 单字段查找结果明确区分找到、正常结束和输入或调用错误。 */
typedef enum xformfind {
	XFORM_FIND_ERROR = -1,
	XFORM_FIND_END = 0,
	XFORM_FIND_FOUND = 1
} xformfind;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_FORM_URLENCODED)

/* 按 application/x-www-form-urlencoded 规则编码字节并写出零结尾文本。 */
XRT_API bool xrtFormEncode(
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 严格解码 form-urlencoded 字节；加号转换为空格，支持同址收缩。 */
XRT_API bool xrtFormDecode(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 分配并返回由 xrtFree 释放的零结尾 form-urlencoded 文本。 */
XRT_API str xrtFormEncodeNew(
	const void* pData,
	size_t iSize,
	size_t* pOutputSize
);



/* 分配解码字节；末尾哨兵零不计入必须返回的长度。 */
XRT_API bytes xrtFormDecodeNew(
	xstrview Text,
	size_t* pOutputSize
);



/*
	严格预检并原地解码表单字段；空 Fields 可查询数量且不修改 Data。
	字段数组容量不足时 Count 返回所需数量，输入保持不变。
*/
XRT_API bool xrtFormParse(
	void* pData,
	size_t iSize,
	xformfield* pFields,
	size_t iCapacity,
	size_t* pCount,
	const xformlimits* pLimits
);



/*
	严格验证整个表单，并从 Offset 开始查找下一个解码后名称相同的字段。
	空 Value 可查询精确长度；容量不足时 Size 返回所需长度且 Offset 和 Value 不变。
*/
XRT_API xformfind xrtFormFind(
	xstrview Text,
	xbytesview Name,
	size_t* pOffset,
	void* pValue,
	size_t iCapacity,
	size_t* pSize
);



/* 写出不带零结尾的 form-urlencoded 字段列表；每个字段始终包含等号。 */
XRT_API bool xrtFormWrite(
	const xformfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并构建零结尾 form-urlencoded 字段列表；结果由 xrtFree 释放。 */
XRT_API str xrtFormBuild(
	const xformfield* pFields,
	size_t iCount,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
