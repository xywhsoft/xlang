#ifndef XRT_XML_INTERNAL_H
#define XRT_XML_INTERNAL_H

#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "include/xrt_xml.h"
#include "../_expat/vendor/expat/lib/expat_config.h"
#include "../_expat/vendor/expat/lib/expat.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define XRT_XML_READ_CHUNK 32768u
#define XRT_XML_NS_SEPARATOR '\x1f'

struct xrtxmlsax_struct {
	uint32 iReferences;
	XML_Parser pExpat;
	xreader* pReader;
	bool bTakeReader;
	bool bPull;
	bool bSuspended;
	bool bFinalSent;
	bool bDone;
	bool bFailed;
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmlevent tEvent;
	xrtxmlevent* pPendingEvents;
	uint32 iPendingCount;
	uint32 iPendingIndex;
	uint32 iPendingCapacity;
	xrtxml_event_proc pHandler;
	ptr pHandlerContext;
	uint64 iInputBytes;
	uint64 iOutputBytes;
	uint64 iNodes;
	uint64 iAttributes;
	uint32 iDepth;
	unsigned char aReadBuffer[XRT_XML_READ_CHUNK];
};

void __xrtXmlSetError(xrtxmlsax* pParser, int iCode, const char* sMessage);
void __xrtXmlSetExpatError(xrtxmlsax* pParser);
bool __xrtXmlPrepareEvent(xrtxmlsax* pParser, int iType);
bool __xrtXmlEmitEvent(xrtxmlsax* pParser);
bool __xrtXmlTakePendingEvent(xrtxmlsax* pParser);
bool __xrtXmlSetEventName(xrtxmlsax* pParser, const XML_Char* sName);
bool __xrtXmlSetEventText(xrtxmlsax* pParser, const XML_Char* sText,
	int iSize);
bool __xrtXmlSetAttributes(xrtxmlsax* pParser, const XML_Char** pAttributes);
int __xrtXmlPump(xrtxmlsax* pParser);
bool __xrtXmlMemoryPreflight(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, bool bSnapshot, xrtxmlerror* pError);

#endif
