#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_TREE)

#define __XMAIL_TREE_BLOCK_SIZE 4096u
#define __XMAIL_TREE_FLAGS \
	(XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER | XMAIL_TREE_RELAXED_QP)



/* 为不提供 max_align_t 的 C11 编译器描述普通对象最大对齐。 */
typedef union __xmailtreealign {
	void* Pointer;
	void (*Function)(void);
	long double Float;
	uint64 Integer;
} __xmailtreealign;



/* arena 块保证 Data 具有通用对象所需的对齐。 */
typedef struct __xmailtreeblock {
	struct __xmailtreeblock* Next;
	size_t Used;
	size_t Capacity;
	__xmailtreealign Align;
	unsigned char Data[];
} __xmailtreeblock;



/* 解析上下文集中维护预算和统一释放的 arena。 */
typedef struct __xmailtreecontext {
	xmailtree Tree;
	xmailtreelimits Limits;
	__xmailtreeblock* Blocks;
} __xmailtreecontext;



/* 发布 MIME 树协议错误。 */
static bool __xrtMailTreeError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME, sMessage);
	return false;
}



/* 发布 MIME 树预算错误。 */
static bool __xrtMailTreeLimit(cstr sMessage)
{
	__xrtMailError(XERR_RANGE, XMAIL_ERROR_LIMIT, sMessage);
	return false;
}



/* 释放解析上下文的全部 arena 块。 */
static void __xrtMailTreeBlocksFree(__xmailtreeblock* pBlock)
{
	while ( pBlock != NULL ) {
		__xmailtreeblock* pNext = pBlock->Next;

		xrtFree(pBlock);
		pBlock = pNext;
	}
}



/* 从不移动的 arena 中分配通用对齐内存。 */
static void* __xrtMailTreeAlloc(
	__xmailtreecontext* pContext,
	size_t iSize
)
{
	const size_t iAlign = _Alignof(__xmailtreealign);
	__xmailtreeblock* pBlock = pContext->Blocks;
	size_t iPosition;
	size_t iCapacity;
	size_t iTotal;

	if ( iSize == 0 ) {
		return NULL;
	}
	if ( pBlock != NULL ) {
		iPosition = (pBlock->Used + iAlign - 1u) & ~(iAlign - 1u);
		if ( (iPosition <= pBlock->Capacity) &&
			(iSize <= (pBlock->Capacity - iPosition)) ) {
			void* pData = pBlock->Data + iPosition;

			pBlock->Used = iPosition + iSize;
			return pData;
		}
	}
	iCapacity = iSize > __XMAIL_TREE_BLOCK_SIZE ?
		iSize : __XMAIL_TREE_BLOCK_SIZE;
	if ( !__xrtMailSizeAdd(
		offsetof(__xmailtreeblock, Data),
		iCapacity,
		&iTotal
	) ) {
		return NULL;
	}
	pBlock = (__xmailtreeblock*)xrtMalloc(iTotal);
	if ( pBlock == NULL ) {
		return NULL;
	}
	pBlock->Next = pContext->Blocks;
	pBlock->Used = iSize;
	pBlock->Capacity = iCapacity;
	pContext->Blocks = pBlock;
	return pBlock->Data;
}



/* 把借用文本复制到 arena，并附加不计入视图的零字节。 */
static bool __xrtMailTreeCopy(
	__xmailtreecontext* pContext,
	xstrview Source,
	xstrview* pCopy
)
{
	char* sCopy;
	size_t iSize;

	if ( !__xrtMailSizeAdd(Source.Size, 1u, &iSize) ) {
		return false;
	}
	sCopy = (char*)__xrtMailTreeAlloc(pContext, iSize);
	if ( sCopy == NULL ) {
		return false;
	}
	if ( Source.Size != 0 ) {
		memcpy(sCopy, Source.Data, Source.Size);
	}
	sCopy[Source.Size] = 0;
	*pCopy = __xrtMailView(sCopy, Source.Size);
	return true;
}



/* 按名称取得唯一字段，不存在时返回 END。 */
static xmailnext __xrtMailTreeHeader(
	const xmailmessageview* pMessage,
	xstrview Name,
	xmailheaderview* pHeader
)
{
	xmailheaderview Duplicate;
	xmailnext Next = xrtMailMessageHeader(pMessage, Name, 0, pHeader);

	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	Next = xrtMailMessageHeader(pMessage, Name, 1u, &Duplicate);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Next == XMAIL_NEXT_ITEM ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_HEADER,
			"MIME entity has a duplicate singleton field"
		);
		return XMAIL_NEXT_ERROR;
	}
	return XMAIL_NEXT_ITEM;
}



/* 展开字段值并把结果存入 arena。 */
static bool __xrtMailTreeUnfold(
	__xmailtreecontext* pContext,
	xstrview Value,
	xstrview* pUnfolded
)
{
	char* sValue;
	size_t iSize;
	size_t iCapacity;

	if ( !xrtMailHeaderUnfoldWrite(Value, NULL, 0, &iSize) ||
		 !__xrtMailSizeAdd(iSize, 1u, &iCapacity) ) {
		return false;
	}
	sValue = (char*)__xrtMailTreeAlloc(pContext, iCapacity);
	if ( sValue == NULL ) {
		return false;
	}
	if ( !xrtMailHeaderUnfoldWrite(
		Value,
		sValue,
		iCapacity,
		&iSize
	) ) {
		return false;
	}
	*pUnfolded = __xrtMailView(sValue, iSize);
	return true;
}



/* 去掉字段值两端普通空白。 */
static xstrview __xrtMailTreeTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		((Text.Data[iEnd - 1u] == ' ') ||
		 (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 解析可选 Content-Type；缺失时使用调用方指定的标准默认值。 */
static bool __xrtMailTreeContentType(
	__xmailtreecontext* pContext,
	xmailpart* pPart,
	bool bDigestDefault
)
{
	xmailheaderview Header;
	xmailnext Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-Type"),
		&Header
	);

	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		if ( bDigestDefault ) {
			pPart->ContentType.Source = XRT_STR_LITERAL("message/rfc822");
			pPart->ContentType.Type = XRT_STR_LITERAL("message");
			pPart->ContentType.Subtype = XRT_STR_LITERAL("rfc822");
		} else {
			pPart->ContentType.Source = XRT_STR_LITERAL("text/plain");
			pPart->ContentType.Type = XRT_STR_LITERAL("text");
			pPart->ContentType.Subtype = XRT_STR_LITERAL("plain");
		}
		pPart->ContentType.Parameters = __xrtMailView(NULL, 0);
		return true;
	}
	if ( !__xrtMailTreeUnfold(
		pContext,
		Header.Value,
		&pPart->ContentType.Source
	) ) {
		return false;
	}
	return xrtMailMediaTypeParse(
		pPart->ContentType.Source,
		&pPart->ContentType
	);
}



/* 从参数块查找值并存入 arena。 */
static xmailnext __xrtMailTreeParam(
	__xmailtreecontext* pContext,
	xstrview Parameters,
	xstrview Name,
	xstrview* pValue
)
{
	xmailparaminfo Info;
	xmailnext Next;
	char* sValue;
	size_t iSize;
	size_t iCapacity;

	Next = xrtMailParamFindWrite(
		Parameters,
		Name,
		NULL,
		0,
		&iSize,
		&Info
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( !__xrtMailSizeAdd(iSize, 1u, &iCapacity) ) {
		return XMAIL_NEXT_ERROR;
	}
	sValue = (char*)__xrtMailTreeAlloc(pContext, iCapacity);
	if ( sValue == NULL ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( xrtMailParamFindWrite(
		Parameters,
		Name,
		sValue,
		iCapacity,
		&iSize,
		&Info
	) != XMAIL_NEXT_ITEM ) {
		return XMAIL_NEXT_ERROR;
	}
	*pValue = __xrtMailView(sValue, iSize);
	return XMAIL_NEXT_ITEM;
}



/* 解析可选 Content-Disposition、文件名和 Content-ID。 */
static bool __xrtMailTreeMetadata(
	__xmailtreecontext* pContext,
	xmailpart* pPart
)
{
	xmailheaderview Header;
	xmailnext Next;

	Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-Disposition"),
		&Header
	);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailTreeUnfold(
			pContext,
			Header.Value,
			&pPart->Disposition.Source
		) || !xrtMailDispositionParse(
			pPart->Disposition.Source,
			&pPart->Disposition
		) ) {
			return false;
		}
		Next = __xrtMailTreeParam(
			pContext,
			pPart->Disposition.Parameters,
			XRT_STR_LITERAL("filename"),
			&pPart->FileName
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		pPart->Inline = __xrtMailAsciiEqualI(
			pPart->Disposition.Type,
			XRT_STR_LITERAL("inline")
		);
		pPart->Attachment = __xrtMailAsciiEqualI(
			pPart->Disposition.Type,
			XRT_STR_LITERAL("attachment")
		);
	}
	if ( pPart->FileName.Size == 0 ) {
		Next = __xrtMailTreeParam(
			pContext,
			pPart->ContentType.Parameters,
			XRT_STR_LITERAL("name"),
			&pPart->FileName
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
	}
	pPart->Attachment = pPart->Attachment || (pPart->FileName.Size != 0);

	Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-ID"),
		&Header
	);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailTreeUnfold(
			pContext,
			Header.Value,
			&pPart->ContentId
		) ) {
			return false;
		}
		pPart->ContentId = __xrtMailTreeTrim(pPart->ContentId);
		if ( (pPart->ContentId.Size >= 2u) &&
			(pPart->ContentId.Data[0] == '<') &&
			(pPart->ContentId.Data[pPart->ContentId.Size - 1u] == '>') ) {
			pPart->ContentId = __xrtMailSlice(
				pPart->ContentId,
				1u,
				pPart->ContentId.Size - 2u
			);
		}
	}
	return true;
}



/* 解析唯一 Content-Transfer-Encoding 字段。 */
static bool __xrtMailTreeTransfer(
	__xmailtreecontext* pContext,
	xmailpart* pPart
)
{
	xmailheaderview Header;
	xmailnext Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		&Header
	);

	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		pPart->Transfer = XMAIL_TRANSFER_7BIT;
		return true;
	}
	pPart->Transfer = xrtMailTransferParse(Header.Value);
	if ( pPart->Transfer != XMAIL_TRANSFER_UNKNOWN ) {
		return true;
	}
	if ( (pContext->Limits.Flags &
		XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER) != 0 ) {
		return true;
	}
	return __xrtMailTreeError(
		"MIME entity uses an unsupported transfer encoding"
	);
}



/* 按传输编码返回正文，并将转换结果计入全树预算。 */
static bool __xrtMailTreeBody(
	__xmailtreecontext* pContext,
	xmailpart* pPart
)
{
	uint32 iFlags = 0;
	unsigned char* pData;
	size_t iSize;
	size_t iCapacity;
	size_t iTotal;

	if ( (pPart->Transfer == XMAIL_TRANSFER_7BIT) ||
		 (pPart->Transfer == XMAIL_TRANSFER_8BIT) ||
		 (pPart->Transfer == XMAIL_TRANSFER_BINARY) ||
		 (pPart->Transfer == XMAIL_TRANSFER_UNKNOWN) ) {
		pPart->Data.Data = (const unsigned char*)pPart->Message.Body.Data;
		pPart->Data.Size = pPart->Message.Body.Size;
		pPart->Decoded = pPart->Transfer != XMAIL_TRANSFER_UNKNOWN;
		return true;
	}
	if ( (pPart->Transfer == XMAIL_TRANSFER_QUOTED_PRINTABLE) &&
		 ((pContext->Limits.Flags & XMAIL_TREE_RELAXED_QP) != 0) ) {
		iFlags = XMAIL_QP_RELAXED_SOFT_BREAK;
	}
	if ( !xrtMailMessageBodyWrite(
		&pPart->Message,
		pPart->Transfer,
		iFlags,
		NULL,
		0,
		&iSize
	) || !__xrtMailSizeAdd(
		pContext->Tree.DecodedBytes,
		iSize,
		&iTotal
	) ) {
		return false;
	}
	if ( (pContext->Limits.MaxDecodedBytes != SIZE_MAX) &&
		 (iTotal > pContext->Limits.MaxDecodedBytes) ) {
		return __xrtMailTreeLimit(
			"MIME tree exceeds the decoded byte limit"
		);
	}
	if ( !__xrtMailSizeAdd(iSize, 1u, &iCapacity) ) {
		return false;
	}
	pData = (unsigned char*)__xrtMailTreeAlloc(pContext, iCapacity);
	if ( pData == NULL ) {
		return false;
	}
	if ( !xrtMailMessageBodyWrite(
		&pPart->Message,
		pPart->Transfer,
		iFlags,
		pData,
		iSize,
		&iSize
	) ) {
		return false;
	}
	pData[iSize] = 0;
	pPart->Data.Data = pData;
	pPart->Data.Size = iSize;
	pPart->Decoded = true;
	pContext->Tree.DecodedBytes = iTotal;
	return true;
}



static bool __xrtMailTreeEntity(
	__xmailtreecontext* pContext,
	xstrview Source,
	size_t iDepth,
	bool bDigestDefault,
	xmailpart* pPart
);



/* 解析 multipart 子项，先计数再一次性分配连续节点。 */
static bool __xrtMailTreeMultipart(
	__xmailtreecontext* pContext,
	xmailpart* pPart,
	size_t iDepth
)
{
	xstrview Boundary;
	xstrview Body = __xrtMailView(
		(const char*)pPart->Data.Data,
		pPart->Data.Size
	);
	xmailmultipartcursor Cursor;
	xmailmultipartview View;
	xmailnext Next;
	size_t iCount = 0;
	size_t iBytes;
	bool bDigest = __xrtMailAsciiEqualI(
		pPart->ContentType.Subtype,
		XRT_STR_LITERAL("digest")
	);

	Next = __xrtMailTreeParam(
		pContext,
		pPart->ContentType.Parameters,
		XRT_STR_LITERAL("boundary"),
		&Boundary
	);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		return __xrtMailTreeError("multipart entity has no boundary parameter");
	}
	if ( !xrtMailMultipartCursorInit(
		&Cursor,
		Body,
		Boundary,
		pContext->Limits.MaxParts
	) ) {
		return false;
	}
	while ( (Next = xrtMailMultipartNext(&Cursor, &View)) == XMAIL_NEXT_ITEM ) {
		iCount++;
		if ( (pContext->Limits.MaxParts != SIZE_MAX) &&
			(iCount > (pContext->Limits.MaxParts -
			 pContext->Tree.PartCount)) ) {
			return __xrtMailTreeLimit("MIME tree exceeds the part limit");
		}
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	pPart->Preamble = Cursor.Preamble;
	pPart->Epilogue = Cursor.Epilogue;
	if ( iCount == 0 ) {
		return true;
	}
	if ( (iCount > (SIZE_MAX / sizeof(xmailpart))) ||
		 !__xrtMailSizeAdd(0, iCount * sizeof(xmailpart), &iBytes) ) {
		return false;
	}
	pPart->Children = (xmailpart*)__xrtMailTreeAlloc(pContext, iBytes);
	if ( pPart->Children == NULL ) {
		return false;
	}
	memset(pPart->Children, 0, iBytes);
	pPart->ChildCount = iCount;
	if ( !xrtMailMultipartCursorInit(
		&Cursor,
		Body,
		Boundary,
		iCount
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (xrtMailMultipartNext(&Cursor, &View) != XMAIL_NEXT_ITEM) ||
			 !__xrtMailTreeEntity(
				pContext,
				View.Source,
				iDepth + 1u,
				bDigest,
				&pPart->Children[i]
			 ) ) {
			return false;
		}
	}
	return xrtMailMultipartNext(&Cursor, &View) == XMAIL_NEXT_END;
}



/* 把 message/rfc822 解码正文作为唯一子消息继续解析。 */
static bool __xrtMailTreeEmbedded(
	__xmailtreecontext* pContext,
	xmailpart* pPart,
	size_t iDepth
)
{
	xstrview Source = __xrtMailView(
		(const char*)pPart->Data.Data,
		pPart->Data.Size
	);

	pPart->Children = (xmailpart*)__xrtMailTreeAlloc(
		pContext,
		sizeof(xmailpart)
	);
	if ( pPart->Children == NULL ) {
		return false;
	}
	memset(pPart->Children, 0, sizeof(xmailpart));
	pPart->ChildCount = 1u;
	pPart->Embedded = true;
	return __xrtMailTreeEntity(
		pContext,
		Source,
		iDepth + 1u,
		false,
		pPart->Children
	);
}



/* 解析一个 MIME entity，并递归进入 multipart 或 message/rfc822。 */
static bool __xrtMailTreeEntity(
	__xmailtreecontext* pContext,
	xstrview Source,
	size_t iDepth,
	bool bDigestDefault,
	xmailpart* pPart
)
{
	if ( iDepth > pContext->Limits.MaxDepth ) {
		return __xrtMailTreeLimit("MIME tree exceeds the depth limit");
	}
	if ( (pContext->Limits.MaxParts != SIZE_MAX) &&
		 (pContext->Tree.PartCount >= pContext->Limits.MaxParts) ) {
		return __xrtMailTreeLimit("MIME tree exceeds the part limit");
	}
	pContext->Tree.PartCount++;
	if ( !xrtMailMessageParse(
		Source,
		pContext->Limits.MaxHeaderBytes,
		pContext->Limits.MaxHeaders,
		&pPart->Message
	) || !__xrtMailTreeContentType(
		pContext,
		pPart,
		bDigestDefault
	) || !__xrtMailTreeMetadata(
		pContext,
		pPart
	) || !__xrtMailTreeTransfer(
		pContext,
		pPart
	) || !__xrtMailTreeBody(
		pContext,
		pPart
	) ) {
		return false;
	}
	if ( __xrtMailAsciiEqualI(
		pPart->ContentType.Type,
		XRT_STR_LITERAL("multipart")
	) && pPart->Decoded ) {
		return __xrtMailTreeMultipart(pContext, pPart, iDepth);
	}
	if ( __xrtMailAsciiEqualI(
		pPart->ContentType.Type,
		XRT_STR_LITERAL("message")
	) && __xrtMailAsciiEqualI(
		pPart->ContentType.Subtype,
		XRT_STR_LITERAL("rfc822")
	) && pPart->Decoded ) {
		return __xrtMailTreeEmbedded(pContext, pPart, iDepth);
	}
	return true;
}



/* 使用默认预算初始化 MIME 树限制。 */
XRT_API void xrtMailTreeLimitsInit(xmailtreelimits* pLimits)
{
	if ( pLimits == NULL ) {
		return;
	}
	memset(pLimits, 0, sizeof(*pLimits));
	pLimits->MaxDepth = XMAIL_TREE_DEPTH_DEFAULT;
	pLimits->MaxParts = XMAIL_TREE_PARTS_DEFAULT;
	pLimits->MaxSourceBytes = XMAIL_TREE_SOURCE_BYTES_DEFAULT;
	pLimits->MaxDecodedBytes = XMAIL_TREE_DECODED_BYTES_DEFAULT;
	pLimits->MaxHeaderBytes = XMAIL_MESSAGE_HEADER_BYTES_DEFAULT;
	pLimits->MaxHeaders = XMAIL_MESSAGE_HEADERS_DEFAULT;
}



/* 验证并展开 MIME 树的零值默认预算。 */
static bool __xrtMailTreeLimits(
	const xmailtreelimits* pInput,
	xmailtreelimits* pLimits
)
{
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		*pLimits = *pInput;
	} else {
		xrtMailTreeLimitsInit(pLimits);
	}
	if ( pLimits->MaxDepth == 0 ) {
		pLimits->MaxDepth = XMAIL_TREE_DEPTH_DEFAULT;
	}
	if ( pLimits->MaxParts == 0 ) {
		pLimits->MaxParts = XMAIL_TREE_PARTS_DEFAULT;
	}
	if ( pLimits->MaxSourceBytes == 0 ) {
		pLimits->MaxSourceBytes = XMAIL_TREE_SOURCE_BYTES_DEFAULT;
	}
	if ( pLimits->MaxDecodedBytes == 0 ) {
		pLimits->MaxDecodedBytes = XMAIL_TREE_DECODED_BYTES_DEFAULT;
	}
	if ( pLimits->MaxHeaderBytes == 0 ) {
		pLimits->MaxHeaderBytes = XMAIL_MESSAGE_HEADER_BYTES_DEFAULT;
	}
	if ( pLimits->MaxHeaders == 0 ) {
		pLimits->MaxHeaders = XMAIL_MESSAGE_HEADERS_DEFAULT;
	}
	if ( (pLimits->MaxDepth > XMAIL_TREE_DEPTH_MAX) ||
		 ((pLimits->Flags & ~__XMAIL_TREE_FLAGS) != 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证 MIME 树限制。 */
XRT_API bool xrtMailTreeLimitsValid(const xmailtreelimits* pLimits)
{
	xmailtreelimits Normalized;

	if ( pLimits == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailTreeLimits(pLimits, &Normalized);
}



/* 复制并解析完整 RFC 消息。 */
XRT_API bool xrtMailTreeParse(
	xstrview Source,
	const xmailtreelimits* pLimits,
	xmailtree* pTree
)
{
	__xmailtreecontext Context;
	xmailtreelimits Limits;
	xstrview OwnedSource;

	if ( !__xrtMailViewValid(Source) ||
		 !xrtMemRangeValid(pTree, sizeof(*pTree)) ||
		 xrtMemRangesOverlap(pTree, sizeof(*pTree), Source.Data, Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailTreeLimits(pLimits, &Limits) ) {
		return false;
	}
	if ( (Limits.MaxSourceBytes != SIZE_MAX) &&
		 (Source.Size > Limits.MaxSourceBytes) ) {
		return __xrtMailTreeLimit("MIME tree exceeds the source byte limit");
	}
	memset(&Context, 0, sizeof(Context));
	Context.Limits = Limits;
	if ( !__xrtMailTreeCopy(&Context, Source, &OwnedSource) ) {
		goto fail;
	}
	Context.Tree.Source = OwnedSource;
	Context.Tree.Root = (xmailpart*)__xrtMailTreeAlloc(
		&Context,
		sizeof(xmailpart)
	);
	if ( Context.Tree.Root == NULL ) {
		goto fail;
	}
	memset(Context.Tree.Root, 0, sizeof(xmailpart));
	if ( !__xrtMailTreeEntity(
		&Context,
		OwnedSource,
		1u,
		false,
		Context.Tree.Root
	) ) {
		goto fail;
	}
	Context.Tree.Storage = Context.Blocks;
	*pTree = Context.Tree;
	return true;

fail:
	__xrtMailTreeBlocksFree(Context.Blocks);
	return false;
}



/* 释放整棵 MIME 树。 */
XRT_API void xrtMailTreeFree(xmailtree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	__xrtMailTreeBlocksFree((__xmailtreeblock*)pTree->Storage);
	memset(pTree, 0, sizeof(*pTree));
}

#endif
