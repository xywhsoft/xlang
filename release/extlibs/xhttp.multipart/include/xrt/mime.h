#ifndef XRT_MIME_H
#define XRT_MIME_H

#include <xrt.h>



#if defined(XHTTP_FEATURE_MIME) && !defined(XRT_FEATURE_HTTP_PARAM)
	#error "XRT MIME support requires XRT_FEATURE_HTTP_PARAM"
#endif

#if defined(XHTTP_FEATURE_MIME_TYPES)

XRT_EXTERN_C_BEGIN



/*
	按扩展名查找内置媒体类型；扩展名可以带前导点。
	比较仅对 ASCII 字母忽略大小写，未知扩展名返回空视图且不修改线程错误。
	输入文本范围必须完整且不回绕；返回文本借用进程期静态存储。
*/
XRT_API xstrview xrtMimeByExt(
	xstrview Extension
);



/*
	按路径最后一段的最后一个扩展名查找内置媒体类型。
	Path 不要求零结尾；隐藏文件名本身不作为扩展名。
	输入文本范围必须完整且不回绕；返回文本借用进程期静态存储。
*/
XRT_API xstrview xrtMimeByPath(
	xstrview Path
);



/*
	按零结尾路径返回内置媒体类型。
	路径为空、没有扩展名或扩展名未知时返回 application/octet-stream。
*/
XRT_API cstr xrtMime(
	cstr sPath
);



XRT_EXTERN_C_END

#endif



#if defined(XHTTP_FEATURE_MIME)

/* 媒体类型保留完整 subtype；Parameters 不含第一个分号。 */
typedef struct xmediatype {
	xstrview Type;
	xstrview Subtype;
	xstrview Parameters;
} xmediatype;



XRT_EXTERN_C_BEGIN



/* 严格解析媒体类型和全部参数，重复参数名称视为无效。 */
XRT_API bool xrtHttpMediaTypeParse(
	xstrview Text,
	xmediatype* pType
);



/* 按 ASCII 大小写不敏感规则比较媒体类型，不比较参数。 */
XRT_API bool xrtHttpMediaTypeEqual(
	const xmediatype* pType,
	xstrview Type,
	xstrview Subtype
);



/* 返回最后一个加号后的结构化语法后缀；不存在时返回空视图。 */
XRT_API xstrview xrtHttpMediaTypeSuffix(
	const xmediatype* pType
);



/*
	判断已解析媒体类型是否适合通用内容编码压缩。
	函数不会把 WOFF、图像、音视频和归档等已压缩格式误判为可压缩。
*/
XRT_API bool xrtHttpMediaTypeCompressible(
	const xmediatype* pType
);



/*
	解析 Content-Type 值并判断是否适合通用内容编码压缩。
	合法但不可压缩的媒体类型返回 false，且不修改线程原有错误。
*/
XRT_API bool xrtHttpContentTypeCompressible(
	xstrview Text
);



/* 严格查找媒体类型参数。 */
XRT_API xhttpnext xrtHttpMediaTypeParam(
	const xmediatype* pType,
	xstrview Name,
	xhttpparam* pParam
);



/* 规范写出媒体类型；结果不附加零字符。 */
XRT_API bool xrtHttpMediaTypeWrite(
	const xmediatype* pType,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾媒体类型，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpMediaTypeBuild(
	const xmediatype* pType,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif

