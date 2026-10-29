#ifndef XRT_HTTP_CONTENT_DISPOSITION_H
#define XRT_HTTP_CONTENT_DISPOSITION_H

#include <xrt.h>



#if defined(XHTTP_FEATURE_HTTP_CONTENT_DISPOSITION) && \
	(!defined(XHTTP_FEATURE_MIME) || \
	 !defined(XHTTP_FEATURE_HTTP_EXT_VALUE) || \
	 !defined(XRT_FEATURE_UNICODE))
	#error "XRT Content-Disposition requires MIME, extended-value and Unicode support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CONTENT_DISPOSITION)

/* Content-Disposition 常用参数标志。 */
typedef enum xcontentdispositionflags {
	XCONTENT_DISPOSITION_NONE = 0,
	XCONTENT_DISPOSITION_NAME = 0x01,
	XCONTENT_DISPOSITION_FILENAME = 0x02,
	XCONTENT_DISPOSITION_FILENAME_EXT = 0x04
} xcontentdispositionflags;



/* Type 和 Parameters 是权威借用视图，其余字段是可重建的常用参数缓存。 */
typedef struct xcontentdisposition {
	xstrview Type;
	xstrview Parameters;
	xhttpparam Name;
	xhttpparam FileName;
	xhttpparam FileNameExt;
	uint32 Flags;
} xcontentdisposition;



XRT_EXTERN_C_BEGIN



/* 严格解析 Content-Disposition，拒绝重复参数名称。 */
XRT_API bool xrtHttpContentDispositionParse(
	xstrview Text,
	xcontentdisposition* pDisposition
);



/* 严格查找 Content-Disposition 参数。 */
XRT_API xhttpnext xrtHttpContentDispositionParam(
	const xcontentdisposition* pDisposition,
	xstrview Name,
	xhttpparam* pParam
);



/* 读取文件名字节；优先有效 UTF-8 filename*，否则回退原始 filename。 */
XRT_API bool xrtHttpContentDispositionFileNameWrite(
	const xcontentdisposition* pDisposition,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾文件名字节，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpContentDispositionFileNameBuild(
	const xcontentdisposition* pDisposition,
	size_t* pSize
);



/* 规范写出 Content-Disposition；结果不附加零字符。 */
XRT_API bool xrtHttpContentDispositionWrite(
	const xcontentdisposition* pDisposition,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 Content-Disposition，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpContentDispositionBuild(
	const xcontentdisposition* pDisposition,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif
