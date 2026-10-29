#ifndef XRT_INTERNAL_URL_H
#define XRT_INTERNAL_URL_H

#include "xrt_internal.h"
#include <xrt.h>
#include "../../include/xrt/url.h"



#if defined(XHTTP_FEATURE_URL)

/* URI 组件允许的保留字符按组件类型组合。 */
#define XRT_URL_COMPONENT_COLON		UINT32_C(0x00000001)
#define XRT_URL_COMPONENT_AT		UINT32_C(0x00000002)
#define XRT_URL_COMPONENT_SLASH		UINT32_C(0x00000004)
#define XRT_URL_COMPONENT_QUESTION	UINT32_C(0x00000008)



/* URI 组件流式验证状态只需要记录未完成的 percent-encoded 位数。 */
typedef struct xrt_url_component_state {
	uint8 Percent;
} xrt_url_component_state;



/* 验证 scheme 指定位置的单个 ASCII 字节。 */
bool __xrtUrlSchemeByte(uint8 iByte, size_t iIndex);



/* 初始化 URI 组件流式验证状态。 */
void __xrtUrlComponentStateInit(
	xrt_url_component_state* pState
);



/* 向 URI 组件验证状态追加一个字节。 */
bool __xrtUrlComponentStateByte(
	xrt_url_component_state* pState,
	uint8 iByte,
	uint32 iAllowed
);



/* 判断 URI 组件是否不存在未完成的 percent-encoded。 */
bool __xrtUrlComponentStateValid(
	const xrt_url_component_state* pState
);

/* 无错误副作用地严格解析 URI-reference，结果借用输入文本。 */
bool __xrtUrlParseValue(
	xstrview Text,
	xurl* pUrl
);



/* 无错误副作用地验证 URL 结构的组件和存在位保持一致。 */
bool __xrtUrlValueValid(const xurl* pUrl);



/* 无错误副作用地取得显式端口或已知 scheme 的默认端口。 */
bool __xrtUrlPortValue(const xurl* pUrl, uint16* pPort);



#endif

#endif
