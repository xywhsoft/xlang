#ifndef XRT_FORM_DATA_H
#define XRT_FORM_DATA_H


#if defined(XHTTP_FEATURE_FORM_DATA_MULTIPART) || \
	defined(XHTTP_FEATURE_FORM_DATA_PARSE) || \
	defined(XHTTP_FEATURE_FORM_DATA_RANDOM)
#endif



#if defined(XHTTP_FEATURE_FORM_DATA) && \
	(!defined(XHTTP_FEATURE_HTTP_BODY) || \
	 !defined(XHTTP_FEATURE_MIME))
	#error "XRT FormData requires HTTP body and MIME support"
#endif

#if defined(XHTTP_FEATURE_FORM_DATA_MULTIPART) && \
	(!defined(XHTTP_FEATURE_FORM_DATA) || \
	 !defined(XHTTP_FEATURE_MULTIPART_WRITE) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_COMPOSE))
	#error "XRT FormData multipart encoding requires FormData, multipart writer and body compose support"
#endif

#if defined(XHTTP_FEATURE_FORM_DATA_PARSE) && \
	(!defined(XHTTP_FEATURE_FORM_DATA) || \
	 !defined(XHTTP_FEATURE_MULTIPART))
	#error "XRT FormData parsing requires FormData and multipart support"
#endif

#if defined(XHTTP_FEATURE_FORM_DATA_RANDOM) && \
	(!defined(XHTTP_FEATURE_FORM_DATA_MULTIPART) || \
	 !defined(XHTTP_FEATURE_MULTIPART_RANDOM))
	#error "XRT random FormData encoding requires multipart encoding and random boundary support"
#endif



#if defined(XHTTP_FEATURE_FORM_DATA)

/* FormData Part 标志区分参数缺席与存在但值为空。 */
typedef enum xformdatapartflags {
	XFORM_DATA_PART_NONE = 0,
	XFORM_DATA_PART_FILENAME = UINT32_C(0x00000001),
	XFORM_DATA_PART_CONTENT_TYPE = UINT32_C(0x00000002)
} xformdatapartflags;



/* FormData 域错误稳定表达参数、值、配额和正文约束。 */
typedef enum xformdataerror {
	XFORM_DATA_ERROR_ARGUMENT = 1,
	XFORM_DATA_ERROR_VALUE,
	XFORM_DATA_ERROR_LIMIT,
	XFORM_DATA_ERROR_BODY,
	XFORM_DATA_ERROR_MULTIPART
} xformdataerror;



/* 有序容器允许重复且区分字段名称大小写。 */
typedef struct xformdata xformdata;



/*
	MaxPartBytes 与 MaxBodyBytes 使用 XHTTP_BODY_UNKNOWN 表示不设上限。
	存在未知长度正文时，只有对应限制也为无限才允许加入容器。
*/
typedef struct xformdataconfig {
	size_t InitialParts;
	size_t MaxParts;
	size_t MaxName;
	size_t MaxFilename;
	size_t MaxContentType;
	size_t MaxMetadata;
	uint64 MaxPartBytes;
	uint64 MaxBodyBytes;
} xformdataconfig;



/* Part 视图和正文指针借用容器，并在下一次容器修改前有效。 */
typedef struct xformdatapart {
	xstrview Name;
	xstrview Filename;
	xstrview ContentType;
	xhttpbody* Body;
	uint64 Length;
	uint32 Flags;
} xformdatapart;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_FORM_DATA)

/* 初始化适合 HTTP 表单的限制；输出可位于合法的未对齐存储。 */
XRT_API void xrtFormDataConfigInit(xformdataconfig* pConfig);



/* 创建拥有型空容器；配置为空时采用默认值，非空配置会立即复制。 */
XRT_API xformdata* xrtFormDataCreate(
	const xformdataconfig* pConfig
);



/* 深复制元数据并为每个正文取得独立引用。 */
XRT_API xformdata* xrtFormDataClone(const xformdata* pForm);



/* 销毁容器；空指针是安全的空操作。 */
XRT_API void xrtFormDataDestroy(xformdata* pForm);



/* 清空全部 Part，但保留条目数组容量供后续复用。 */
XRT_API void xrtFormDataClear(xformdata* pForm);



/* 预留至少指定数量的 Part 槽位。 */
XRT_API bool xrtFormDataReserve(xformdata* pForm, size_t iParts);



/* 返回当前 Part 数量；空容器返回零。 */
XRT_API size_t xrtFormDataCount(const xformdata* pForm);



/* 返回名称、文件名和媒体类型的有效元数据字节总数。 */
XRT_API size_t xrtFormDataMetadata(const xformdata* pForm);



/* 追加一个正文 Part；Filename 为空指针时参数缺席。 */
XRT_API bool xrtFormDataAppendBody(
	xformdata* pForm,
	xstrview Name,
	xhttpbody* pBody,
	const xstrview* pFilename,
	xstrview ContentType
);



/* 复制文本值并追加普通字段。 */
XRT_API bool xrtFormDataAppendText(
	xformdata* pForm,
	xstrview Name,
	xstrview Value
);



/* 复制二进制值并追加普通字段或文件字段。 */
XRT_API bool xrtFormDataAppendBytes(
	xformdata* pForm,
	xstrview Name,
	xbytesview Data,
	const xstrview* pFilename,
	xstrview ContentType
);



/* 在首个同名位置替换全部同名项；不存在时追加。 */
XRT_API bool xrtFormDataSetBody(
	xformdata* pForm,
	xstrview Name,
	xhttpbody* pBody,
	const xstrview* pFilename,
	xstrview ContentType
);



/* 复制文本值并原子替换全部同名项。 */
XRT_API bool xrtFormDataSetText(
	xformdata* pForm,
	xstrview Name,
	xstrview Value
);



/* 复制二进制值并原子替换全部同名项。 */
XRT_API bool xrtFormDataSetBytes(
	xformdata* pForm,
	xstrview Name,
	xbytesview Data,
	const xstrview* pFilename,
	xstrview ContentType
);



/* 删除全部同名 Part 并返回删除数量。 */
XRT_API size_t xrtFormDataRemove(
	xformdata* pForm,
	xstrview Name
);



/* 返回同名 Part 数量。 */
XRT_API size_t xrtFormDataCountName(
	const xformdata* pForm,
	xstrview Name
);



/* 判断是否存在同名 Part；未匹配不是错误。 */
XRT_API bool xrtFormDataHas(
	const xformdata* pForm,
	xstrview Name
);



/* 复制指定位置的借用 Part 视图。 */
XRT_API bool xrtFormDataAt(
	const xformdata* pForm,
	size_t iIndex,
	xformdatapart* pPart
);



/* 复制首个同名 Part；无匹配返回 false 且不是错误。 */
XRT_API bool xrtFormDataGet(
	const xformdata* pForm,
	xstrview Name,
	xformdatapart* pPart
);



/* 从 Index 开始查找下一个同名项，ITEM 时 Index 指向下一位置。 */
XRT_API xhttpnext xrtFormDataFind(
	const xformdata* pForm,
	xstrview Name,
	size_t* pIndex,
	xformdatapart* pPart
);

#endif



#if defined(XHTTP_FEATURE_FORM_DATA_MULTIPART)

/* 按给定 boundary 创建零合并复制的 multipart/form-data 正文。 */
XRT_API xhttpbody* xrtFormDataBody(
	const xformdata* pForm,
	const xmultipartboundary* pBoundary
);

#endif



#if defined(XHTTP_FEATURE_FORM_DATA_RANDOM)

/* 生成安全随机 boundary，并创建对应 multipart/form-data 正文。 */
XRT_API xhttpbody* xrtFormDataBodyRandom(
	const xformdata* pForm,
	xmultipartboundary* pBoundary
);

#endif



#if defined(XHTTP_FEATURE_FORM_DATA_PARSE)

/* 把完整 multipart/form-data 正文复制为拥有型容器；输入描述符会立即快照。 */
XRT_API xformdata* xrtFormDataParse(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
);



/* 从 Content-Type 读取 boundary，再解析完整正文；错误输出支持未对齐存储。 */
XRT_API xformdata* xrtFormDataParseContentType(
	xstrview ContentType,
	xbytesview Body,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
);

#endif



XRT_EXTERN_C_END

#endif

