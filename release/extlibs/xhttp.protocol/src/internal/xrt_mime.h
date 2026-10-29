#ifndef XHTTP_INTERNAL_MIME_H
#define XHTTP_INTERNAL_MIME_H

#include "xhttp_internal.h"

#include "../../include/xrt/mime.h"



#if defined(XHTTP_FEATURE_MIME)

/* MIME 参数访问器在严格参数校验通过后接收借用条目。 */
typedef bool (*xrt_mime_param_visitor)(
	const xhttpparam* pParam,
	void* pContext
);



/* 验证并扫描严格参数列表。 */
bool __xrtMimeParametersInspect(
	xstrview Parameters,
	xrt_mime_param_visitor Visitor,
	void* pContext
);



/* 安全累加 MIME 写出长度。 */
bool __xrtMimeSizeAdd(size_t* pSize, size_t iAdd);



/* 严格验证非空 MIME 参数列表，并拒绝重复参数名称。 */
bool __xrtMimeParametersValid(xstrview Parameters);



/* 按 MIME Sniff 规则静默解析媒体类型 essence，并忽略参数尾。 */
bool __xrtMimeSniffTypeParse(
	xstrview Text,
	xmediatype* pType
);



/* 判断输出范围是否覆盖任意借用视图。 */
bool __xrtMimeOutputOverlap(
	const xstrview* pViews,
	size_t iCount,
	const void* pOutput,
	size_t iSize
);



/* 为同构 Writer 分配零结尾结果。 */
str __xrtMimeBuild(
	const void* pValue,
	bool (*Write)(const void*, void*, size_t, size_t*),
	size_t* pSize
);



/* 验证已解析媒体类型结构与严格参数列表。 */
bool __xrtHttpMediaTypeValid(const xmediatype* pType);

#endif



#endif
