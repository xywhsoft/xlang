#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_BUILD)

#define __XMAIL_BUILD_STACK 1024u



/* 验证 Builder 对象和公开状态没有被调用方破坏。 */
static bool __xrtMailBuilderValid(const xmailbuilder* pBuilder)
{
	if ( !xrtMemRangeValid(pBuilder, sizeof(*pBuilder)) ||
		 (pBuilder->Write == NULL) ||
		 (pBuilder->State < XMAIL_BUILDER_HEADERS) ||
		 (pBuilder->State > XMAIL_BUILDER_FAILED) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 要求 Builder 位于指定阶段且当前未从输出回调重入。 */
static bool __xrtMailBuilderState(
	const xmailbuilder* pBuilder,
	xmailbuilderstate State
)
{
	if ( !__xrtMailBuilderValid(pBuilder) ) {
		return false;
	}
	if ( pBuilder->Busy ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_CALLBACK,
			"mail builder cannot be reentered from its output callback"
		);
		return false;
	}
	if ( pBuilder->State != State ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_PROTOCOL,
			"mail builder operation is invalid in the current state"
		);
		return false;
	}
	return true;
}



/* 同步提交一段借用输出，并把回调失败固化为终止状态。 */
static bool __xrtMailBuilderEmit(
	xmailbuilder* pBuilder,
	const void* pData,
	size_t iSize
)
{
	xbytesview Data;
	bool bResult;

	if ( !xrtMemRangeValid(pData, iSize) ||
		 xrtMemRangesOverlap(pBuilder, sizeof(*pBuilder), pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( iSize > (SIZE_MAX - pBuilder->Written) ) {
		pBuilder->State = XMAIL_BUILDER_FAILED;
		__xrtMailSetSizeOverflow();
		return false;
	}
	Data.Data = (cbytes)pData;
	Data.Size = iSize;
	xrtClearError();
	pBuilder->Busy = true;
	bResult = pBuilder->Write(Data, pBuilder->UserData);
	pBuilder->Busy = false;
	if ( !bResult ) {
		pBuilder->State = XMAIL_BUILDER_FAILED;
		if ( xrtGetError() == NULL ) {
			__xrtMailError(
				XERR_IO,
				XMAIL_ERROR_CALLBACK,
				"mail builder output callback failed"
			);
		}
		return false;
	}
	if ( iSize >= 2u ) {
		pBuilder->Tail[0] = Data.Data[iSize - 2u];
		pBuilder->Tail[1] = Data.Data[iSize - 1u];
		pBuilder->TailSize = 2u;
	} else if ( pBuilder->TailSize == 0 ) {
		pBuilder->Tail[0] = Data.Data[0];
		pBuilder->TailSize = 1u;
	} else {
		pBuilder->Tail[0] = pBuilder->Tail[pBuilder->TailSize - 1u];
		pBuilder->Tail[1] = Data.Data[0];
		pBuilder->TailSize = 2u;
	}
	pBuilder->Written += iSize;
	return true;
}



/* 通过查询、栈缓冲和按需堆缓冲调用文本写入原语。 */
static bool __xrtMailBuilderHeaderValue(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	size_t iLineSize
)
{
	char arrStack[__XMAIL_BUILD_STACK];
	char* sOutput = arrStack;
	size_t iRequired;
	bool bResult;

	if ( !xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
		NULL,
		0,
		&iRequired
	) ) {
		return false;
	}
	if ( iRequired >= sizeof(arrStack) ) {
		if ( iRequired == SIZE_MAX ) {
			__xrtMailSetSizeOverflow();
			return false;
		}
		sOutput = (char*)xrtMalloc(iRequired + 1u);
		if ( sOutput == NULL ) {
			return false;
		}
	}
	bResult = xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
		sOutput,
		iRequired + 1u,
		&iRequired
	) && __xrtMailBuilderEmit(pBuilder, sOutput, iRequired);
	if ( sOutput != arrStack ) {
		xrtFree(sOutput);
	}
	return bResult;
}



/* 初始化流式邮件 Builder。 */
XRT_API bool xrtMailBuilderInit(
	xmailbuilder* pBuilder,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	if ( !xrtMemRangeValid(pBuilder, sizeof(*pBuilder)) ||
		 (pWrite == NULL) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pBuilder->Write = pWrite;
	pBuilder->UserData = pUserData;
	pBuilder->Written = 0;
	pBuilder->State = XMAIL_BUILDER_HEADERS;
	pBuilder->Tail[0] = 0;
	pBuilder->Tail[1] = 0;
	pBuilder->TailSize = 0;
	pBuilder->Busy = false;
	return true;
}



/* 验证、折叠并写出一个字段。 */
XRT_API bool xrtMailBuilderHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	size_t iLineSize
)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	return __xrtMailBuilderHeaderValue(
		pBuilder,
		Name,
		Value,
		iLineSize
	);
}



/* 编码 UTF-8 字段值后写出字段。 */
XRT_API bool xrtMailBuilderWordHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	xmailwordencoding Encoding,
	size_t iLineSize
)
{
	str sValue;
	size_t iValueSize;
	bool bResult;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	sValue = xrtMailWordEncode(Value, Encoding, &iValueSize);
	if ( sValue == NULL ) {
		return false;
	}
	bResult = __xrtMailBuilderHeaderValue(
		pBuilder,
		Name,
		(xstrview){ sValue, iValueSize },
		iLineSize
	);
	xrtFree(sValue);
	return bResult;
}



/* 格式化地址列表后写出字段。 */
XRT_API bool xrtMailBuilderAddressHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t iLineSize
)
{
	str sValue;
	size_t iValueSize;
	bool bResult;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	sValue = xrtMailAddressList(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		&iValueSize
	);
	if ( sValue == NULL ) {
		return false;
	}
	bResult = __xrtMailBuilderHeaderValue(
		pBuilder,
		Name,
		(xstrview){ sValue, iValueSize },
		iLineSize
	);
	xrtFree(sValue);
	return bResult;
}



/* 零复制写出已经完整验证的字段块。 */
XRT_API bool xrtMailBuilderHeaderBlock(
	xmailbuilder* pBuilder,
	xstrview Block
)
{
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;
	size_t iCount = 0;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	if ( !__xrtMailViewValid(Block) || (Block.Size < 2u) ||
		 (Block.Data[Block.Size - 2u] != '\r') ||
		 (Block.Data[Block.Size - 1u] != '\n') ||
		 xrtMemRangesOverlap(pBuilder, sizeof(*pBuilder), Block.Data, Block.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; (i + 3u) < Block.Size; i++ ) {
		if ( (Block.Data[i] == '\r') && (Block.Data[i + 1u] == '\n') &&
			 (Block.Data[i + 2u] == '\r') &&
			 (Block.Data[i + 3u] == '\n') ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
	}
	if ( !xrtMailHeaderCursorInit(&Cursor, Block) ) {
		return false;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
		iCount++;
	}
	if ( (Next != XMAIL_NEXT_END) || (iCount == 0) ) {
		return false;
	}
	return __xrtMailBuilderEmit(pBuilder, Block.Data, Block.Size);
}



/* 结束字段并进入正文阶段。 */
XRT_API bool xrtMailBuilderHeadersEnd(xmailbuilder* pBuilder)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	if ( !__xrtMailBuilderEmit(pBuilder, "\r\n", 2u) ) {
		return false;
	}
	pBuilder->State = XMAIL_BUILDER_BODY;
	return true;
}



/* 零复制写出正文或已编码 MIME 片段。 */
XRT_API bool xrtMailBuilderBody(
	xmailbuilder* pBuilder,
	const void* pData,
	size_t iSize
)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_BODY) ) {
		return false;
	}
	return __xrtMailBuilderEmit(pBuilder, pData, iSize);
}



/* 写出 multipart 分隔片段。 */
XRT_API bool xrtMailBuilderMultipart(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
)
{
	char arrOutput[XMAIL_BOUNDARY_MAX + 9u];
	char* sOutput = arrOutput;
	size_t iSize;
	bool bAtLine;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_BODY) ) {
		return false;
	}
	bAtLine = (pBuilder->TailSize == 2u) &&
		(pBuilder->Tail[0] == (unsigned char)'\r') &&
		(pBuilder->Tail[1] == (unsigned char)'\n');
	if ( !xrtMailMultipartMarkWrite(
		Boundary,
		((Mark == XMAIL_MULTIPART_NEXT) && bAtLine) ?
			XMAIL_MULTIPART_FIRST : Mark,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) ) {
		return false;
	}
	if ( (Mark == XMAIL_MULTIPART_CLOSE) && bAtLine ) {
		sOutput += 2u;
		iSize -= 2u;
	}
	return __xrtMailBuilderEmit(pBuilder, sOutput, iSize);
}



/* 开始 multipart 的一个新 part，并重新进入字段阶段。 */
XRT_API bool xrtMailBuilderPartBegin(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
)
{
	if ( (Mark != XMAIL_MULTIPART_FIRST) &&
		 (Mark != XMAIL_MULTIPART_NEXT) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailBuilderMultipart(pBuilder, Boundary, Mark) ) {
		return false;
	}
	pBuilder->State = XMAIL_BUILDER_HEADERS;
	return true;
}



/* 关闭 Builder，不猜测调用方的正文或 multipart 结构。 */
XRT_API bool xrtMailBuilderFinish(xmailbuilder* pBuilder)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_BODY) ) {
		return false;
	}
	pBuilder->State = XMAIL_BUILDER_CLOSED;
	return true;
}

#endif
