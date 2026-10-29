#ifndef XRT_MULTIPART_H
#define XRT_MULTIPART_H

#include <xrt.h>

#if defined(XHTTP_FEATURE_MULTIPART_RANDOM)
#endif



#if defined(XHTTP_FEATURE_MULTIPART) && \
	(!defined(XHTTP_FEATURE_MIME) || \
	 !defined(XHTTP_FEATURE_HTTP_CONTENT_DISPOSITION))
	#error "XRT Multipart support requires MIME and Content-Disposition support"
#endif

#if defined(XHTTP_FEATURE_MULTIPART_WRITE) && \
	!defined(XHTTP_FEATURE_MULTIPART)
	#error "XRT Multipart writer requires XHTTP_FEATURE_MULTIPART"
#endif

#if defined(XHTTP_FEATURE_MULTIPART_STREAM) && \
	!defined(XHTTP_FEATURE_MULTIPART)
	#error "XRT Multipart stream reader requires XHTTP_FEATURE_MULTIPART"
#endif

#if defined(XHTTP_FEATURE_MULTIPART_RANDOM) && \
	(!defined(XHTTP_FEATURE_MULTIPART) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT Multipart random boundary requires multipart and secure random support"
#endif



#if defined(XHTTP_FEATURE_MULTIPART)

#define XMULTIPART_BOUNDARY_MAX 70u



/* 边界值由结构体持有，Data 始终附带零结尾，但 Size 不包含零字符。 */
typedef struct xmultipartboundary {
	size_t Size;
	char Data[XMULTIPART_BOUNDARY_MAX + 1u];
} xmultipartboundary;



/* Part 标志只表示实际出现的字段，不为派生属性重复保存状态。 */
typedef enum xmultipartpartflags {
	XMULTIPART_PART_NONE = 0,
	XMULTIPART_PART_DISPOSITION = 0x01,
	XMULTIPART_PART_CONTENT_TYPE = 0x02,
	XMULTIPART_PART_TRANSFER_ENCODING = 0x04
} xmultipartpartflags;



/* 完整正文校验标志；默认要求至少存在一个 Part。 */
typedef enum xmultipartvalidateflags {
	XMULTIPART_VALIDATE_NONE = 0,
	XMULTIPART_VALIDATE_ALLOW_EMPTY = 0x01
} xmultipartvalidateflags;



/*
	Part 的 Headers、Body 和字段值都借用完整 multipart 正文。
	Headers 不包含分隔正文的空行，Body 不包含边界前的 CRLF。
*/
typedef struct xmultipartpart {
	xstrview Headers;
	xbytesview Body;
	xcontentdisposition Disposition;
	xmediatype ContentType;
	xstrview TransferEncoding;
	size_t HeaderCount;
	uint32 Flags;
} xmultipartpart;



/* 整包解析限制；SIZE_MAX 表示对应字节或数量不设上限。 */
typedef struct xmultipartlimits {
	size_t MaxParts;
	size_t MaxHeaders;
	size_t MaxDelimiterBytes;
	size_t MaxHeaderBytes;
	size_t MaxPartBytes;
	size_t MaxBodyBytes;
} xmultipartlimits;



/* Multipart 错误码用于稳定表达语法位置，不替代线程错误对象。 */
typedef enum xmultiparterror {
	XMULTIPART_ERROR_NONE = 0,
	XMULTIPART_ERROR_ARGUMENT,
	XMULTIPART_ERROR_BOUNDARY,
	XMULTIPART_ERROR_DELIMITER,
	XMULTIPART_ERROR_TRUNCATED,
	XMULTIPART_ERROR_HEADER,
	XMULTIPART_ERROR_DUPLICATE_HEADER,
	XMULTIPART_ERROR_DISPOSITION,
	XMULTIPART_ERROR_CONTENT_TYPE,
	XMULTIPART_ERROR_TRANSFER_ENCODING,
	XMULTIPART_ERROR_PARTS_LIMIT,
	XMULTIPART_ERROR_HEADERS_LIMIT,
	XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
	XMULTIPART_ERROR_HEADER_BYTES_LIMIT,
	XMULTIPART_ERROR_PART_BYTES_LIMIT,
	XMULTIPART_ERROR_BODY_BYTES_LIMIT
} xmultiparterror;



/* Offset 是相对 multipart 正文起点的线缆字节位置。 */
typedef struct xmultiparterrorinfo {
	xmultiparterror Code;
	size_t Offset;
} xmultiparterrorinfo;



#if defined(XHTTP_FEATURE_MULTIPART_STREAM)

/* Reader 每次只发布一个借用事件或终态。 */
typedef enum xmultipartreadstatus {
	XMULTIPART_READ_ERROR = -1,
	XMULTIPART_READ_MORE = 0,
	XMULTIPART_READ_PART = 1,
	XMULTIPART_READ_DATA = 2,
	XMULTIPART_READ_PART_END = 3,
	XMULTIPART_READ_DONE = 4
} xmultipartreadstatus;



/*
	流 Reader 不持有输入和堆内存。
	公开计数用于限额、进度和诊断；State 仅由本模块推进。
*/
typedef struct xmultipartreader {
	xmultipartboundary Boundary;
	xmultipartlimits Limits;
	size_t Parts;
	size_t PartBytes;
	size_t BodyBytes;
	size_t WireBytes;
	size_t ErrorOffset;
	xmultiparterror Error;
	uint32 State;
} xmultipartreader;

#endif



XRT_EXTERN_C_BEGIN



/*
	初始化适合一般 HTTP 表单与 MIME 内容的整包解析限制。
	固定描述符只要求位于完整可访问的存储范围，不要求自然对齐。
*/
XRT_API void xrtMultipartLimitsInit(xmultipartlimits* pLimits);



/* 校验并复制已经解码的 MIME boundary；输出可以未对齐。 */
XRT_API bool xrtMultipartBoundaryParse(
	xstrview Text,
	xmultipartboundary* pBoundary
);



/* 从 multipart Content-Type 中严格读取并解码 boundary 参数。 */
XRT_API bool xrtMultipartBoundaryFromContentType(
	xstrview ContentType,
	xmultipartboundary* pBoundary
);



/* 返回借用边界结构的视图；无效结构返回空视图并设置错误。 */
XRT_API xstrview xrtMultipartBoundaryView(
	const xmultipartboundary* pBoundary
);



#if defined(XHTTP_FEATURE_MULTIPART_RANDOM)

/* 使用 128 位系统安全随机数原子生成 boundary；输出可以未对齐。 */
XRT_API bool xrtMultipartBoundaryRandom(
	xmultipartboundary* pBoundary
);

#endif



/*
	读取完整 multipart 正文中的下一项。
	Offset 初始为零；ITEM 返回 Part，END 表示已验证关闭边界和全部后缀。
	Boundary、Offset、Part 和可选 Error 可以未对齐。
*/
XRT_API xhttpnext xrtMultipartNext(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t* pOffset,
	xmultipartpart* pPart,
	xmultiparterrorinfo* pError
);



/* 按限制严格验证完整 multipart 正文，要求存在至少一个 Part 和关闭边界。 */
XRT_API bool xrtMultipartValidate(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
);



/*
	按标志严格验证完整 multipart 正文并返回 Part 数量。
	pPartCount 仅在成功时发布；Flags 只接受 xmultipartvalidateflags。
*/
XRT_API bool xrtMultipartValidateCount(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pLimits,
	uint32 Flags,
	size_t* pPartCount,
	xmultiparterrorinfo* pError
);



/*
	解析完整 multipart 正文到调用方数组。
	pParts 为空且 Capacity 为零时只返回所需数量；容量不足不写出部分结果。
	固定输入和输出描述符可以未对齐，函数只发布完整 Part。
*/
XRT_API bool xrtMultipartParse(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	xmultipartpart* pParts,
	size_t iCapacity,
	size_t* pCount,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
);



/* 验证 Part 具备 RFC 7578 form-data 所需的 disposition 类型和 name 参数。 */
XRT_API bool xrtMultipartFormPartValid(
	const xmultipartpart* pPart
);



/* 读取 form-data 字段名；结果不附加零字符。 */
XRT_API bool xrtMultipartPartNameWrite(
	const xmultipartpart* pPart,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 读取上传文件名；优先采用受支持的 filename*，结果不附加零字符。 */
XRT_API bool xrtMultipartPartFileNameWrite(
	const xmultipartpart* pPart,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



#if defined(XHTTP_FEATURE_MULTIPART_STREAM)

/*
	初始化无分配流 Reader，并拥有 boundary 与限制的副本。
	Reader 是有状态对象，必须自然对齐；固定输入描述符可以未对齐。
*/
XRT_API bool xrtMultipartReaderInit(
	xmultipartreader* pReader,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pLimits
);



/* 保留 boundary 与限制并重置解析进度，便于复用 Reader 对象。 */
XRT_API void xrtMultipartReaderReset(
	xmultipartreader* pReader
);



/*
	推进流解析。
	Consumed 是本次可从输入缓冲区移除的字节；调用方必须在 MORE 后保留未消费尾部。
	Part 仅在 PART 时有效，Data 仅在 DATA 时有效，二者都借用本次 Input。
	bEnd 表示当前 Input 之后不会再有属于该 multipart 正文的字节。
	Consumed、Part、Data 和可选 Error 可以未对齐；Reader 必须自然对齐。
*/
XRT_API xmultipartreadstatus xrtMultipartReaderRead(
	xmultipartreader* pReader,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xmultipartpart* pPart,
	xbytesview* pData,
	xmultiparterrorinfo* pError
);



/* 判断 Reader 是否已经消费关闭 boundary 和全部 epilogue。 */
XRT_API bool xrtMultipartReaderDone(
	const xmultipartreader* pReader
);

#endif



#if defined(XHTTP_FEATURE_MULTIPART_WRITE)

/*
	写出 multipart/form-data Content-Type。
	输出为空且容量为零时只查询所需字节数；结果不附加零字符。
*/
XRT_API bool xrtMultipartContentTypeWrite(
	const xmultipartboundary* pBoundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	写出原始 Part 的 boundary、字段和终止空行，不写正文与正文后的 CRLF。
	字段名称和值按 HTTP 字段语法校验，调用方仍可自由选择 MIME 字段集合。
*/
XRT_API bool xrtMultipartPartHeadWrite(
	const xmultipartboundary* pBoundary,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	写出标准 form-data Part 头，不写正文与正文后的 CRLF。
	Filename 为空指针时省略 filename 参数，ContentType 为空视图时省略对应字段。
*/
XRT_API bool xrtMultipartFormHeadWrite(
	const xmultipartboundary* pBoundary,
	xstrview Name,
	const xstrview* pFilename,
	xstrview ContentType,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出 Part 正文后的 CRLF，可与 PartHeadWrite 和零复制正文发送组合使用。 */
XRT_API bool xrtMultipartPartEndWrite(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 一次写出原始 Part；容量不足时不产生部分结果。 */
XRT_API bool xrtMultipartPartWrite(
	const xmultipartboundary* pBoundary,
	const xhttpfield* pFields,
	size_t iFieldCount,
	xbytesview Body,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出一个 form-data 普通字段 Part。 */
XRT_API bool xrtMultipartFieldWrite(
	const xmultipartboundary* pBoundary,
	xstrview Name,
	xbytesview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	写出一个 form-data 文件 Part。
	ContentType 为空时省略该字段；Filename 按 quoted-string 规则转义。
*/
XRT_API bool xrtMultipartFileWrite(
	const xmultipartboundary* pBoundary,
	xstrview Name,
	xstrview Filename,
	xstrview ContentType,
	xbytesview Body,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出关闭 boundary；它必须位于最后一个 PartEndWrite 之后。 */
XRT_API bool xrtMultipartCloseWrite(
	const xmultipartboundary* pBoundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif

#endif
