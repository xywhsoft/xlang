#ifndef XRT_XML_H
#define XRT_XML_H

/* XML SAX 公共契约直接依赖通用 Reader。 */
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif

#include "xrt.h"

/* 调用方先选择 XRT_MODULE_*；实现文件通过内部头引入完整运行时。 */

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_XML_OPTIONS_VERSION 1u
#define XRT_XML_NEXT_ERROR (-1)
#define XRT_XML_NEXT_END 0
#define XRT_XML_NEXT_EVENT 1

typedef enum {
	XRT_XML_OK = 0,
	XRT_XML_ERROR_ARGUMENT = 1,
	XRT_XML_ERROR_MEMORY = 2,
	XRT_XML_ERROR_FORMAT = 3,
	XRT_XML_ERROR_IO = 4,
	XRT_XML_ERROR_LIMIT = 5,
	XRT_XML_ERROR_DTD = 6,
	XRT_XML_ERROR_EXTERNAL_ENTITY = 7,
	XRT_XML_ERROR_CALLBACK = 8
} xrtxmlstatus;

typedef enum {
	XRT_XML_EVENT_START = 1,
	XRT_XML_EVENT_END = 2,
	XRT_XML_EVENT_TEXT = 3,
	XRT_XML_EVENT_COMMENT = 4,
	XRT_XML_EVENT_PROCESSING_INSTRUCTION = 5,
	XRT_XML_EVENT_START_NAMESPACE = 6,
	XRT_XML_EVENT_END_NAMESPACE = 7
} xrtxmleventtype;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
	bool bNamespaceAware;
	bool bPreserveComments;
	bool bAllowDtd;
	bool bAllowExternalEntities;
} xrtxmloptions;

typedef struct {
	int iCode;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrtxmlerror;

typedef struct {
	str sName;
	str sLocalName;
	str sNamespaceUri;
	str sPrefix;
	str sValue;
} xrtxmlattribute;

typedef struct {
	int iType;
	str sName;
	str sLocalName;
	str sNamespaceUri;
	str sPrefix;
	str sText;
	str sTarget;
	xrtxmlattribute* pAttributes;
	uint32 iAttributeCount;
	uint32 iDepth;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
} xrtxmlevent;

typedef struct xrtxmlsax_struct xrtxmlsax;
typedef bool (*xrtxml_event_proc)(ptr pContext, const xrtxmlevent* pEvent);

XRT_API void xrtXmlOptionsInit(xrtxmloptions* pOptions);
XRT_API void xrtXmlErrorInit(xrtxmlerror* pError);
XRT_API void xrtXmlEventClear(xrtxmlevent* pEvent);

/* pull SAX：事件中的字符串在下一次 Next 或 Destroy 前有效。 */
XRT_API xrtxmlsax* xrtXmlSaxOpen(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XRT_API xrtxmlsax* xrtXmlSaxOpenMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XRT_API xrtxmlsax* xrtXmlSaxOpenFile(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError);
XRT_API int xrtXmlSaxNext(xrtxmlsax* pParser, const xrtxmlevent** ppEvent,
	xrtxmlerror* pError);
XRT_API xrtxmlsax* xrtXmlSaxRetain(xrtxmlsax* pParser);
XRT_API void xrtXmlSaxDestroy(xrtxmlsax* pParser);

/* push SAX：回调返回 false 会立即终止解析。 */
XRT_API bool xrtXmlSaxParse(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxml_event_proc pHandler, ptr pContext,
	xrtxmlerror* pError);
XRT_API bool xrtXmlSaxParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxml_event_proc pHandler, ptr pContext,
	xrtxmlerror* pError);
XRT_API bool xrtXmlSaxParseFile(str sPath, const xrtxmloptions* pOptions,
	xrtxml_event_proc pHandler, ptr pContext, xrtxmlerror* pError);

#ifdef __cplusplus
}
#endif

#endif
