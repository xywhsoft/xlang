#ifndef XRT_IMAP_BODY_H
#define XRT_IMAP_BODY_H

#include <xrt/imap_data.h>



#if defined(XMAIL_FEATURE_IMAP_BODY) && !defined(XMAIL_FEATURE_IMAP_DATA)
	#error "XMAIL_FEATURE_IMAP_BODY requires IMAP_DATA"
#endif



#if defined(XMAIL_FEATURE_IMAP_BODY)

/* BODYSTRUCTURE 递归校验的固定深度上限。 */
#define XIMAP_BODY_DEPTH_MAX 64u



/* 单部分消息按协议字段分为普通、文本和嵌套消息，multipart 单独表示。 */
typedef enum ximapbodykind {
	XIMAP_BODY_BASIC = 1,
	XIMAP_BODY_TEXT,
	XIMAP_BODY_MESSAGE,
	XIMAP_BODY_MULTIPART
} ximapbodykind;



/*
	BODYSTRUCTURE 的零分配视图。
	所有字符串和值都借用 Source；未在线路中出现的可选字段 Kind 为零。
*/
typedef struct ximapbodyview {
	xstrview Source;
	xstrview Children;
	xstrview Extensions;
	ximapdataview Type;
	ximapdataview Subtype;
	ximapdataview Parameters;
	ximapdataview Id;
	ximapdataview Description;
	ximapdataview Encoding;
	ximapdataview Envelope;
	ximapdataview Body;
	ximapdataview Md5;
	ximapdataview Disposition;
	ximapdataview Language;
	ximapdataview Location;
	uint64 Octets;
	uint64 Lines;
	size_t ChildCount;
	ximapbodykind Kind;
} ximapbodyview;



/* multipart 子部分游标只借用父视图的 Children 区。 */
typedef struct ximapbodycursor {
	ximapdatacursor Data;
	size_t Remaining;
} ximapbodycursor;



/* 参数游标用于 body-fld-param 和 disposition 参数列表。 */
typedef struct ximapbodyparamcursor {
	ximapdatacursor Data;
} ximapbodyparamcursor;



/* 参数名和值保留 IMAP string 的线路表示，可由 xrtImapStringWrite 解码。 */
typedef struct ximapbodyparam {
	ximapdataview Name;
	ximapdataview Value;
} ximapbodyparam;



XRT_EXTERN_C_BEGIN



/* 解析并递归校验一个完整的 BODY 或 BODYSTRUCTURE 括号值。 */
XRT_API bool xrtImapBodyParse(
	xstrview Text,
	ximapbodyview* pBody
);



/* 初始化 multipart 直接子部分的零分配游标。 */
XRT_API bool xrtImapBodyChildCursorInit(
	ximapbodycursor* pCursor,
	const ximapbodyview* pBody
);



/* 返回下一个直接子部分；每个结果仍会完整校验自己的递归结构。 */
XRT_API xmailnext xrtImapBodyChildNext(
	ximapbodycursor* pCursor,
	ximapbodyview* pBody
);



/* 初始化 NIL 或括号参数列表的成对游标。 */
XRT_API bool xrtImapBodyParamCursorInit(
	ximapbodyparamcursor* pCursor,
	const ximapdataview* pParameters
);



/* 返回下一参数名和值。 */
XRT_API xmailnext xrtImapBodyParamNext(
	ximapbodyparamcursor* pCursor,
	ximapbodyparam* pParameter
);



XRT_EXTERN_C_END

#endif

#endif
