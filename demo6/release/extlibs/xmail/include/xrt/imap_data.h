#ifndef XRT_IMAP_DATA_H
#define XRT_IMAP_DATA_H

#include <xrt/imap.h>



#if defined(XMAIL_FEATURE_IMAP_DATA) && !defined(XMAIL_FEATURE_IMAP)
	#error "XMAIL_FEATURE_IMAP_DATA requires IMAP"
#endif



#if defined(XMAIL_FEATURE_IMAP_DATA)

/* 数据值只描述当前响应片段；literal 正文仍由 Client 流式交付。 */
typedef enum ximapdatakind {
	XIMAP_DATA_ATOM = 1,
	XIMAP_DATA_NUMBER,
	XIMAP_DATA_QUOTED,
	XIMAP_DATA_NIL,
	XIMAP_DATA_LIST,
	XIMAP_DATA_LITERAL
} ximapdatakind;



/* Source 保留线路表示，Value 为去除引号或括号但尚未反转义的借用内容。 */
typedef struct ximapdataview {
	xstrview Source;
	xstrview Value;
	ximapdatakind Kind;
	uint64 Number;
	size_t LiteralSize;
	bool LiteralNonSynchronizing;
	bool LiteralBinary;
} ximapdataview;



/* 通用数据游标逐项读取一层 IMAP 数据值，不递归分配对象。 */
typedef struct ximapdatacursor {
	xstrview Text;
	size_t Position;
	bool Done;
} ximapdatacursor;



/* LIST 视图公开属性列表、分隔符、邮箱名和可选扩展。 */
typedef struct ximaplistview {
	xstrview Source;
	xstrview Attributes;
	xstrview Extensions;
	ximapdataview Delimiter;
	ximapdataview Mailbox;
} ximaplistview;



/* FLAGS 游标适用于 LIST 属性和 FETCH FLAGS 数据。 */
typedef struct ximapflagcursor {
	ximapdatacursor Data;
} ximapflagcursor;



/* STATUS 视图保留邮箱名并把属性对交给游标。 */
typedef struct ximapmailboxstatusview {
	xstrview Source;
	ximapdataview Mailbox;
	xstrview Items;
} ximapmailboxstatusview;



typedef struct ximapstatuscursor {
	ximapdatacursor Data;
} ximapstatuscursor;



/* 未知 STATUS 扩展仍以名称和值公开，不占用固定结构。 */
typedef struct ximapstatusitem {
	xstrview Name;
	ximapdataview Value;
} ximapstatusitem;



typedef enum ximapsearchitemkind {
	XIMAP_SEARCH_ID = 1,
	XIMAP_SEARCH_MODSEQ
} ximapsearchitemkind;



/* SEARCH 游标逐项返回 ID，并保留可选 MODSEQ 终项。 */
typedef struct ximapsearchcursor {
	ximapdatacursor Data;
} ximapsearchcursor;



typedef struct ximapsearchitem {
	uint64 Number;
	ximapsearchitemkind Kind;
} ximapsearchitem;



/* ESEARCH 不展开 sequence-set；Correlator 和 Items 均借用输入。 */
typedef struct ximapesearchview {
	xstrview Source;
	xstrview Correlator;
	xstrview Items;
	bool Uid;
} ximapesearchview;



typedef struct ximapesearchcursor {
	ximapdatacursor Data;
} ximapesearchcursor;



typedef struct ximapesearchitem {
	xstrview Name;
	ximapdataview Value;
} ximapesearchitem;



/* FETCH 视图只提取消息序号，属性值由可续段游标逐项交付。 */
typedef struct ximapfetchview {
	xstrview Source;
	xstrview Items;
	uint64 Sequence;
} ximapfetchview;



typedef struct ximapfetchcursor {
	xstrview Text;
	size_t Position;
	bool NeedMore;
	bool Done;
} ximapfetchcursor;



typedef struct ximapfetchitem {
	xstrview Attribute;
	ximapdataview Value;
} ximapfetchitem;



XRT_EXTERN_C_BEGIN



/* 初始化一层 IMAP 数据值游标。 */
XRT_API bool xrtImapDataCursorInit(
	ximapdatacursor* pCursor,
	xstrview Text
);



/* 读取下一 atom、number、quoted、NIL、完整 list 或行尾 literal 标记。 */
XRT_API xmailnext xrtImapDataNext(
	ximapdatacursor* pCursor,
	ximapdataview* pValue
);



/* 解码 atom、quoted 或 NIL；查询模式返回精确字节数，容量包含末尾零字节。 */
XRT_API bool xrtImapStringWrite(
	const ximapdataview* pValue,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解析完整 `LIST attributes delimiter mailbox [extensions]` 响应。 */
XRT_API bool xrtImapListParse(
	xstrview Text,
	ximaplistview* pList
);



/* 初始化完整括号 flag 列表的借用游标。 */
XRT_API bool xrtImapFlagCursorInit(
	ximapflagcursor* pCursor,
	xstrview Flags
);



/* 返回下一系统 flag、关键字或 LIST 属性。 */
XRT_API xmailnext xrtImapFlagNext(
	ximapflagcursor* pCursor,
	xstrview* pFlag
);



/* 解析完整 `STATUS mailbox (name value ...)` 响应。 */
XRT_API bool xrtImapStatusParse(
	xstrview Text,
	ximapmailboxstatusview* pStatus
);



/* 初始化 STATUS 属性对游标。 */
XRT_API bool xrtImapStatusCursorInit(
	ximapstatuscursor* pCursor,
	const ximapmailboxstatusview* pStatus
);



/* 返回下一 STATUS 名称和值，未知扩展保持可见。 */
XRT_API xmailnext xrtImapStatusNext(
	ximapstatuscursor* pCursor,
	ximapstatusitem* pItem
);



/* 初始化完整 `SEARCH [id ...] [(MODSEQ value)]` 响应游标。 */
XRT_API bool xrtImapSearchCursorInit(
	ximapsearchcursor* pCursor,
	xstrview Text
);



/* 返回下一 SEARCH ID 或 MODSEQ，不展开和分配数组。 */
XRT_API xmailnext xrtImapSearchNext(
	ximapsearchcursor* pCursor,
	ximapsearchitem* pItem
);



/* 解析 ESEARCH correlator、UID 指示器和返回数据区。 */
XRT_API bool xrtImapESearchParse(
	xstrview Text,
	ximapesearchview* pSearch
);



/* 初始化 ESEARCH 返回数据对游标。 */
XRT_API bool xrtImapESearchCursorInit(
	ximapesearchcursor* pCursor,
	const ximapesearchview* pSearch
);



/* 返回下一 ESEARCH 名称和值；ALL 等集合保持原始 atom 视图。 */
XRT_API xmailnext xrtImapESearchNext(
	ximapesearchcursor* pCursor,
	ximapesearchitem* pItem
);



/* 解析完整 `sequence FETCH (...)` 非标记响应。 */
XRT_API bool xrtImapFetchParse(
	xstrview Text,
	ximapfetchview* pFetch
);



/* 初始化 FETCH 属性游标；literal 正文不会进入该游标。 */
XRT_API bool xrtImapFetchCursorInit(
	ximapfetchcursor* pCursor,
	const ximapfetchview* pFetch
);



/* 在读取 literal 正文后继续解析下一行 FETCH 片段。 */
XRT_API bool xrtImapFetchCursorContinue(
	ximapfetchcursor* pCursor,
	xstrview Text
);



/* 返回下一 FETCH 属性和值；literal 值会把 NeedMore 置真。 */
XRT_API xmailnext xrtImapFetchNext(
	ximapfetchcursor* pCursor,
	ximapfetchitem* pItem
);



XRT_EXTERN_C_END

#endif

#endif
