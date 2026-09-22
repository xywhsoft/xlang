#ifndef XRT_HTTP_CLIENT_H
#define XRT_HTTP_CLIENT_H


#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CONTENT_TYPE)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_SET_COOKIE)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_QUERY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA_RANDOM)
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) && \
	!defined(XHTTP_FEATURE_URL)
	#error "XRT HTTP client request support requires URL support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) && \
	!defined(XHTTP_FEATURE_HTTP_HEADERS)
	#error "XRT HTTP client request support requires HTTP Headers support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) && \
	!defined(XHTTP_FEATURE_HTTP_BODY)
	#error "XRT HTTP client request support requires HTTP body support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS) && \
	!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST)
	#error "XRT HTTP client request Trailer support requires request support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XRT_FEATURE_HTTP_TE) || \
	 !defined(XRT_FEATURE_HTTP_CONNECTION))
	#error "XRT HTTP client request TE support requires request, TE and Connection support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH))
	#error "XRT HTTP client request authentication requires request and authentication support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BASIC) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_BASIC))
	#error "XRT HTTP client Basic authentication requires request authentication and Basic support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BEARER) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_BEARER))
	#error "XRT HTTP client Bearer authentication requires request authentication and Bearer support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_DIGEST) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS))
	#error "XRT HTTP client Digest authentication requires request authentication and Digest credentials"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XRT_FEATURE_HTTP_EXPECT) || \
	 !defined(XRT_FEATURE_HTTP_TE) || \
	 !defined(XRT_FEATURE_HTTP_CONNECTION) || \
	 !defined(XRT_FEATURE_HTTP_TRAILER))
	#error "XRT HTTP client prepare support requires request, Expect, TE, Connection and Trailer support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) && \
	!defined(XRT_FEATURE_HTTP1_HEAD)
	#error "XRT HTTP client prepare support requires HTTP/1 head support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) && \
	!defined(XRT_FEATURE_HTTP1_BODY)
	#error "XRT HTTP client prepare support requires HTTP/1 body support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) && \
	!defined(XRT_FEATURE_HTTP_HOST)
	#error "XRT HTTP client prepare support requires HTTP Host support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) && \
	!defined(XRT_FEATURE_HTTP_TARGET)
	#error "XRT HTTP client prepare support requires HTTP target support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION))
	#error "XRT HTTP/1 Digest prepare support requires request prepare and Digest session support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_QUERY) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XHTTP_FEATURE_QUERY_PARAMS))
	#error "XRT HTTP client request query support requires request and QueryParams support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XHTTP_FEATURE_QUERY_PARAMS))
	#error "XRT HTTP client request form support requires request and QueryParams support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XHTTP_FEATURE_FORM_DATA_MULTIPART))
	#error "XRT HTTP client request FormData support requires request and FormData multipart support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA_RANDOM) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA) || \
	 !defined(XHTTP_FEATURE_FORM_DATA_RANDOM))
	#error "XRT HTTP client random FormData support requires request FormData and random FormData support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST)

/* 客户端请求是拥有型可修改构建器，执行时由客户端取得独立快照。 */
typedef struct xhttprequest xhttprequest;



/* 请求构建错误用于区分方法、URL 和客户端可发送范围。 */
typedef enum xhttprequesterror {
	XHTTP_REQUEST_ERROR_METHOD = 1,
	XHTTP_REQUEST_ERROR_URL,
	XHTTP_REQUEST_ERROR_SCHEME,
	XHTTP_REQUEST_ERROR_HOST,
	XHTTP_REQUEST_ERROR_USERINFO,
	XHTTP_REQUEST_ERROR_TARGET,
	XHTTP_REQUEST_ERROR_HOST_HEADER,
	XHTTP_REQUEST_ERROR_CONTENT_LENGTH,
	XHTTP_REQUEST_ERROR_TRANSFER_ENCODING,
	XHTTP_REQUEST_ERROR_TRAILER,
	XHTTP_REQUEST_ERROR_TRACE_BODY,
	XHTTP_REQUEST_ERROR_EXPECT,
	XHTTP_REQUEST_ERROR_TE,
	XHTTP_REQUEST_ERROR_CONNECTION,
	XHTTP_REQUEST_ERROR_QUERY,
	XHTTP_REQUEST_ERROR_FORM,
	XHTTP_REQUEST_ERROR_FORM_DATA
} xhttprequesterror;

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE)

/* HTTP/1 request-target 形式覆盖直连、正向代理、CONNECT 与 OPTIONS 星号目标。 */
typedef enum xhttp1targetform {
	XHTTP1_TARGET_AUTO = 0,
	XHTTP1_TARGET_ORIGIN,
	XHTTP1_TARGET_ABSOLUTE,
	XHTTP1_TARGET_AUTHORITY,
	XHTTP1_TARGET_ASTERISK,
	XHTTP1_TARGET_CUSTOM
} xhttp1targetform;



/* 请求准备选项只决定线路 target，不承载连接、超时或重定向策略。 */
typedef struct xhttp1requestoptions {
	xhttp1targetform TargetForm;
	xstrview CustomTarget;
} xhttp1requestoptions;



/* 准备后的请求计划拥有线路 Header，并保留一个独立正文引用。 */
typedef struct xhttp1requestplan xhttp1requestplan;



/* 客户端发送正文只使用无正文、定长和 chunked 三种明确模式。 */
typedef enum xhttprequestbodymode {
	XHTTP_REQUEST_BODY_NONE = 0,
	XHTTP_REQUEST_BODY_FIXED,
	XHTTP_REQUEST_BODY_CHUNKED
} xhttprequestbodymode;

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE) && \
	!defined(XHTTP_FEATURE_HTTP_HEADERS)
	#error "XRT HTTP client response support requires HTTP Headers support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CONTENT_TYPE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE) || \
	 !defined(XHTTP_FEATURE_MIME))
	#error "XRT HTTP client Content-Type support requires response and MIME support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_SET_COOKIE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE) || \
	 !defined(XHTTP_FEATURE_SET_COOKIE))
	#error "XRT HTTP client Set-Cookie support requires response and Set-Cookie support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH))
	#error "XRT HTTP client response authentication requires response and authentication support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BASIC) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_BASIC))
	#error "XRT HTTP client response Basic authentication requires response authentication and Basic support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BEARER) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_BEARER_CHALLENGE))
	#error "XRT HTTP client response Bearer authentication requires response authentication and Bearer challenge support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE))
	#error "XRT HTTP client Digest challenge support requires response authentication and Digest challenge support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT))
	#error "XRT HTTP client Digest selection requires response Digest and client protocol support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO))
	#error "XRT HTTP client Digest info support requires response authentication and Digest info support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION))
	#error "XRT HTTP client Digest session response support requires Digest info and session support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)

/* 客户端响应是执行结果拥有的只读对象。 */
typedef struct xhttpresponse xhttpresponse;



/* 响应标志描述正文交付和协议升级后的稳定事实。 */
typedef enum xhttpresponseflag {
	XHTTP_RESPONSE_NONE = 0,
	XHTTP_RESPONSE_STREAMED = UINT32_C(0x00000001),
	XHTTP_RESPONSE_DECOMPRESSED = UINT32_C(0x00000002),
	XHTTP_RESPONSE_UPGRADED = UINT32_C(0x00000004)
} xhttpresponseflag;



/* 响应读取错误稳定区分参数、索引、Header 与结构化字段值。 */
typedef enum xhttpresponseerror {
	XHTTP_RESPONSE_ERROR_ARGUMENT = 1,
	XHTTP_RESPONSE_ERROR_INDEX,
	XHTTP_RESPONSE_ERROR_HEADER,
	XHTTP_RESPONSE_ERROR_CONTENT_TYPE,
	XHTTP_RESPONSE_ERROR_SET_COOKIE,
	XHTTP_RESPONSE_ERROR_AUTH
} xhttpresponseerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST)

/*
	创建拥有 Method、Url、Header 和正文引用的客户端请求。
	Url 必须是带非空 host 的绝对 HTTP 或 HTTPS URL，userinfo 不被接受。
*/
XRT_API xhttprequest* xrtHttpRequestCreate(
	xstrview Method,
	xstrview Url
);



/* 使用指定动态 Header 容量和限额创建客户端请求。 */
XRT_API xhttprequest* xrtHttpRequestCreateWithHeaders(
	xstrview Method,
	xstrview Url,
	const xhttpheadersconfig* pHeaders
);



/* 创建内容和所有动态存储独立的请求副本；正文增加一个共享引用。 */
XRT_API xhttprequest* xrtHttpRequestClone(
	const xhttprequest* pRequest
);



/* 销毁请求及其 Header、URL、方法和正文引用；空指针是安全的空操作。 */
XRT_API void xrtHttpRequestDestroy(xhttprequest* pRequest);



/* 失败原子地替换请求方法；Method 必须是非空 HTTP token。 */
XRT_API bool xrtHttpRequestSetMethod(
	xhttprequest* pRequest,
	xstrview Method
);



/* 返回借用的请求方法视图。 */
XRT_API xstrview xrtHttpRequestMethod(
	const xhttprequest* pRequest
);



/* 失败原子地替换绝对 HTTP 或 HTTPS URL。 */
XRT_API bool xrtHttpRequestSetUrl(
	xhttprequest* pRequest,
	xstrview Url
);



/* 返回借用的原始 URL 文本；fragment 保留在文本中但不会进入请求 target。 */
XRT_API xstrview xrtHttpRequestUrlText(
	const xhttprequest* pRequest
);



/* 返回借用请求 URL 存储的解析结果。 */
XRT_API const xurl* xrtHttpRequestUrl(
	const xhttprequest* pRequest
);



/* 追加一个拥有型 Header，允许同名字段。 */
XRT_API bool xrtHttpRequestAddHeader(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
);



/* 设置首个同名 Header 并删除其余同名字段。 */
XRT_API bool xrtHttpRequestSetHeader(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
);



/* 删除全部同名 Header 并返回删除数量。 */
XRT_API size_t xrtHttpRequestRemoveHeader(
	xhttprequest* pRequest,
	xstrview Name
);



/* 返回首个同名借用 Header，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpRequestHeader(
	const xhttprequest* pRequest,
	xstrview Name
);



/* 返回 Header 数量。 */
XRT_API size_t xrtHttpRequestHeaderCount(
	const xhttprequest* pRequest
);



/* 返回连续只读 Header 数组；空请求或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpRequestHeaderData(
	const xhttprequest* pRequest
);



/* 返回指定位置的借用 Header，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpRequestHeaderAt(
	const xhttprequest* pRequest,
	size_t iIndex
);



/*
	返回请求借用的可变 Header 容器。
	容器与请求同寿命，调用方不得销毁。
*/
XRT_API xhttpheaders* xrtHttpRequestHeaders(
	xhttprequest* pRequest
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH)

/* 校验并设置唯一 Authorization 字段。 */
XRT_API bool xrtHttpRequestSetAuth(
	xhttprequest* pRequest,
	xstrview Scheme,
	xstrview Data
);



/* 校验并设置唯一 Proxy-Authorization 字段。 */
XRT_API bool xrtHttpRequestSetProxyAuth(
	xhttprequest* pRequest,
	xstrview Scheme,
	xstrview Data
);



/* 删除全部 Authorization 字段并返回删除数量。 */
XRT_API size_t xrtHttpRequestClearAuth(xhttprequest* pRequest);



/* 删除全部 Proxy-Authorization 字段并返回删除数量。 */
XRT_API size_t xrtHttpRequestClearProxyAuth(xhttprequest* pRequest);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BASIC)

/* 从用户名和密码设置唯一 Basic Authorization 字段。 */
XRT_API bool xrtHttpRequestSetBasicAuth(
	xhttprequest* pRequest,
	xstrview User,
	xstrview Password
);



/* 从用户名和密码设置唯一 Basic Proxy-Authorization 字段。 */
XRT_API bool xrtHttpRequestSetProxyBasicAuth(
	xhttprequest* pRequest,
	xstrview User,
	xstrview Password
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BEARER)

/* 从 b64token 设置唯一 Bearer Authorization 字段。 */
XRT_API bool xrtHttpRequestSetBearerAuth(
	xhttprequest* pRequest,
	xstrview Token
);



/* 从 b64token 设置唯一 Bearer Proxy-Authorization 字段。 */
XRT_API bool xrtHttpRequestSetProxyBearerAuth(
	xhttprequest* pRequest,
	xstrview Token
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_DIGEST)

/* 规范写出并设置唯一 Digest Authorization 字段。 */
XRT_API bool xrtHttpRequestSetDigestAuth(
	xhttprequest* pRequest,
	const xhttpdigestauth* pDigest
);



/* 规范写出并设置唯一 Digest Proxy-Authorization 字段。 */
XRT_API bool xrtHttpRequestSetProxyDigestAuth(
	xhttprequest* pRequest,
	const xhttpdigestauth* pDigest
);

#endif



/*
	替换正文引用；请求保留独立引用，空 Body 清除正文。
	该函数不自动改写 Content-Type、Content-Length 或 Transfer-Encoding。
*/
XRT_API bool xrtHttpRequestSetBody(
	xhttprequest* pRequest,
	xhttpbody* pBody
);



/*
	复制字节并设置正文；ContentType 非空时同时失败原子地设置 Content-Type。
	长度分帧由客户端执行层根据正文元数据生成。
*/
XRT_API bool xrtHttpRequestSetBytes(
	xhttprequest* pRequest,
	xbytesview Data,
	xstrview ContentType
);



/* 返回请求借用的正文对象，可能为空。 */
XRT_API xhttpbody* xrtHttpRequestBody(
	const xhttprequest* pRequest
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TE)

/*
	失败原子地声明 HTTP/1 响应 Trailer 接收能力。
	函数保留已有字段并按需补充 TE: trailers 与 Connection: TE。
*/
XRT_API bool xrtHttp1RequestAcceptTrailers(
	xhttprequest* pRequest
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)

/* 追加一个拥有型 Trailer，并保留同名字段。 */
XRT_API bool xrtHttpRequestAddTrailer(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
);



/* 设置首个同名 Trailer，并删除其余同名字段。 */
XRT_API bool xrtHttpRequestSetTrailer(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
);



/* 删除全部同名 Trailer 并返回删除数量。 */
XRT_API size_t xrtHttpRequestRemoveTrailer(
	xhttprequest* pRequest,
	xstrview Name
);



/* 返回首个同名借用 Trailer，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpRequestTrailer(
	const xhttprequest* pRequest,
	xstrview Name
);



/* 返回 Trailer 数量。 */
XRT_API size_t xrtHttpRequestTrailerCount(
	const xhttprequest* pRequest
);



/* 返回连续只读 Trailer 数组；空请求或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpRequestTrailerData(
	const xhttprequest* pRequest
);



/* 返回指定位置的借用 Trailer，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpRequestTrailerAt(
	const xhttprequest* pRequest,
	size_t iIndex
);



/* 返回只读 Trailer 容器；从未添加字段时返回空指针且不分配。 */
XRT_API const xhttpheaders* xrtHttpRequestTrailers(
	const xhttprequest* pRequest
);



/* 返回可修改 Trailer 容器并在首次调用时创建它。 */
XRT_API xhttpheaders* xrtHttpRequestEditTrailers(
	xhttprequest* pRequest
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_QUERY)

/* 编码并失败原子地替换 URL 查询组件；空容器生成显式空查询。 */
XRT_API bool xrtHttpRequestSetQueryParams(
	xhttprequest* pRequest,
	const xqueryparams* pParams
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM)

/* 编码 QueryParams 并失败原子地设置 urlencoded 正文和 Content-Type。 */
XRT_API bool xrtHttpRequestSetForm(
	xhttprequest* pRequest,
	const xqueryparams* pParams
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA)

/* 使用给定 boundary 设置可流式 multipart/form-data 正文与 Content-Type。 */
XRT_API bool xrtHttpRequestSetFormData(
	xhttprequest* pRequest,
	const xformdata* pForm,
	const xmultipartboundary* pBoundary
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA_RANDOM)

/*
	生成安全随机 boundary 并失败原子地设置 multipart/form-data 请求。
	boundary 输出存储可以未对齐，但不得覆盖请求、FormData 或其借用数据。
*/
XRT_API bool xrtHttpRequestSetFormDataRandom(
	xhttprequest* pRequest,
	const xformdata* pForm,
	xmultipartboundary* pBoundary
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE)

/* 初始化默认准备选项；接受未对齐的完整存储并拒绝回绕地址。 */
XRT_API void xrtHttp1RequestOptionsInit(
	xhttp1requestoptions* pOptions
);



/*
	冻结请求并生成完整 HTTP/1.1 Header。
	用户 Header 与正文长度冲突时在任何网络操作前失败。
*/
XRT_API xhttp1requestplan* xrtHttp1RequestPrepare(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)

/*
	使用最终 request-target 生成源站 Digest 凭据并冻结请求计划。
	成功时发布由调用方释放的 Exchange；失败时请求对象不变且输出为空。
	ppExchange 允许未对齐，但不得覆盖请求或 Digest session 拥有的存储。
*/
XRT_API xhttp1requestplan* xrtHttp1RequestPrepareDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
);



/* 使用同一准备契约生成 Proxy-Authorization Digest 字段。 */
XRT_API xhttp1requestplan* xrtHttp1RequestPrepareProxyDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
);

#endif



/* 销毁请求计划、线路 Header 和正文引用；空指针是安全的空操作。 */
XRT_API void xrtHttp1RequestPlanDestroy(
	xhttp1requestplan* pPlan
);



/* 返回拥有型计划中的完整 HTTP/1.1 Header 字节。 */
XRT_API xbytesview xrtHttp1RequestPlanHead(
	const xhttp1requestplan* pPlan
);



/* 返回 chunked 计划冻结的完整 last-chunk；其他模式返回空视图。 */
XRT_API xbytesview xrtHttp1RequestPlanEnd(
	const xhttp1requestplan* pPlan
);



/* 返回计划拥有的请求方法。 */
XRT_API xstrview xrtHttp1RequestPlanMethod(
	const xhttp1requestplan* pPlan
);



/* 返回计划拥有的完整原始 URL；fragment 保留但不进入自动 target。 */
XRT_API xstrview xrtHttp1RequestPlanUrl(
	const xhttp1requestplan* pPlan
);



/* 返回计划拥有的实际 request-target。 */
XRT_API xstrview xrtHttp1RequestPlanTarget(
	const xhttp1requestplan* pPlan
);



/* 返回连接端点使用的无方括号 URL host，不受 Host Header 覆盖影响。 */
XRT_API xstrview xrtHttp1RequestPlanHost(
	const xhttp1requestplan* pPlan
);



/* 返回连接端点的显式端口或 scheme 默认端口。 */
XRT_API uint16 xrtHttp1RequestPlanPort(
	const xhttp1requestplan* pPlan
);



/* 判断连接端点是否要求 TLS。 */
XRT_API bool xrtHttp1RequestPlanSecure(
	const xhttp1requestplan* pPlan
);



/* 返回计划保留的借用正文对象，可能为空。 */
XRT_API xhttpbody* xrtHttp1RequestPlanBody(
	const xhttp1requestplan* pPlan
);



/* 返回发送正文的唯一分帧模式。 */
XRT_API xhttprequestbodymode xrtHttp1RequestPlanBodyMode(
	const xhttp1requestplan* pPlan
);



/* 返回正文源声明长度；未知 chunked 正文返回 XHTTP_BODY_UNKNOWN。 */
XRT_API uint64 xrtHttp1RequestPlanBodyLength(
	const xhttp1requestplan* pPlan
);



/* 判断请求是否显式要求发送后关闭连接。 */
XRT_API bool xrtHttp1RequestPlanClose(
	const xhttp1requestplan* pPlan
);



/* 判断事务是否必须先等待 100 Continue 或最终响应。 */
XRT_API bool xrtHttp1RequestPlanExpectContinue(
	const xhttp1requestplan* pPlan
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)

/* 销毁响应拥有的 Header、trailer、正文和文本；空指针是安全的空操作。 */
XRT_API void xrtHttpResponseDestroy(xhttpresponse* pResponse);



/* 返回 HTTP 版本；空响应返回零。 */
XRT_API xhttpversion xrtHttpResponseVersion(
	const xhttpresponse* pResponse
);



/* 返回三位响应状态码；空响应返回零。 */
XRT_API uint16 xrtHttpResponseStatus(
	const xhttpresponse* pResponse
);



/* 判断响应状态是否属于 200 到 299。 */
XRT_API bool xrtHttpResponseSuccess(
	const xhttpresponse* pResponse
);



/* 返回借用的 reason phrase，不包含前导空格。 */
XRT_API xstrview xrtHttpResponseReason(
	const xhttpresponse* pResponse
);



/* 返回响应稳定标志。 */
XRT_API uint32 xrtHttpResponseFlags(
	const xhttpresponse* pResponse
);



/* 返回首个同名借用 Header，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpResponseHeader(
	const xhttpresponse* pResponse,
	xstrview Name
);



/* 返回 Header 数量。 */
XRT_API size_t xrtHttpResponseHeaderCount(
	const xhttpresponse* pResponse
);



/* 返回连续只读 Header 数组；空响应或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpResponseHeaderData(
	const xhttpresponse* pResponse
);



/* 返回指定位置的借用 Header，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpResponseHeaderAt(
	const xhttpresponse* pResponse,
	size_t iIndex
);



/*
	返回响应借用的只读 Header 容器。
	容器与响应同寿命，调用方不得修改或销毁。
*/
XRT_API const xhttpheaders* xrtHttpResponseHeaders(
	const xhttpresponse* pResponse
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH)

/*
	跨全部 WWW-Authenticate 字段迭代服务端 challenge。
	结构输出允许未对齐，但必须互不覆盖且不得覆盖响应拥有的存储。
*/
XRT_API xhttpnext xrtHttpResponseChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
);



/* 跨全部 Proxy-Authenticate 字段迭代代理 challenge；输出契约与源站一致。 */
XRT_API xhttpnext xrtHttpResponseProxyChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BASIC)

/*
	迭代并解码源站 Basic challenge；查询和失败不推进游标。
	查询模式只发布所需 realm 字节数，实际成功时 realm 借用 Output。
*/
XRT_API xhttpnext xrtHttpResponseBasicChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
);



/* 迭代并解码代理 Basic challenge，缓冲与游标契约和源站入口一致。 */
XRT_API xhttpnext xrtHttpResponseProxyBasicChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BEARER)

/*
	迭代并解码源站 Bearer challenge；查询和失败不推进游标。
	实际成功时标准参数视图借用 Output，未知扩展参数由协议层忽略。
*/
XRT_API xhttpnext xrtHttpResponseBearerChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
);



/* 迭代并解码代理 Bearer challenge，缓冲与游标契约和源站入口一致。 */
XRT_API xhttpnext xrtHttpResponseProxyBearerChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST)

/* 迭代并解码源站 Digest challenge；查询和失败不推进游标。 */
XRT_API xhttpnext xrtHttpResponseDigestChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
);



/* 迭代并解码代理 Digest challenge；契约与源站入口一致。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE)

/*
	按线路顺序选择首个满足本地策略的源站 Digest challenge。
	查询模式完成全部候选验证并返回所选项的精确解码长度。
	全部写出区允许未对齐，但必须互不覆盖且不得覆盖响应拥有的存储。
*/
XRT_API xhttpnext xrtHttpResponseDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
);



/* 选择首个满足本地策略的代理 Digest challenge；契约与源站入口一致。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO)

/*
	解析唯一 Authentication-Info；缺失返回 END，重复或无效返回 ERROR。
	全部写出区允许未对齐，但必须互不覆盖且不得覆盖响应拥有的存储。
*/
XRT_API xhttpnext xrtHttpResponseDigestInfo(
	const xhttpresponse* pResponse,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
);



/* 解析唯一 Proxy-Authentication-Info；算法由原请求上下文提供。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestInfo(
	const xhttpresponse* pResponse,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)

/*
	解析并验证源站 Digest 回执。
	END 表示字段缺失；ITEM 时 pCheck 区分无效、有效、更新与已取代状态。
	pCheck 允许未对齐，但不得覆盖响应、会话或 Exchange 拥有的存储。
*/
XRT_API xhttpnext xrtHttpResponseDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
);



/* 解析并验证代理 Digest 回执；返回与状态契约和源站入口一致。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CONTENT_TYPE)

/*
	解析唯一 Content-Type；字段缺失返回 END，重复或无效返回 ERROR。
	输出存储可以未对齐，但不得覆盖响应拥有或借用的数据。
*/
XRT_API xhttpnext xrtHttpResponseContentType(
	const xhttpresponse* pResponse,
	xmediatype* pType
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_SET_COOKIE)

/*
	按线路顺序解析下一条独立 Set-Cookie 字段，不执行逗号合并。
	游标和输出可以未对齐，但必须互不覆盖且不得覆盖响应数据。
*/
XRT_API xhttpnext xrtHttpResponseSetCookieNext(
	const xhttpresponse* pResponse,
	size_t* pHeaderIndex,
	xsetcookie* pCookie
);

#endif



/* 返回首个同名借用 trailer，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpResponseTrailer(
	const xhttpresponse* pResponse,
	xstrview Name
);



/* 返回 trailer 数量。 */
XRT_API size_t xrtHttpResponseTrailerCount(
	const xhttpresponse* pResponse
);



/* 返回连续只读 trailer 数组；空响应或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpResponseTrailerData(
	const xhttpresponse* pResponse
);



/* 返回指定位置的借用 trailer，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpResponseTrailerAt(
	const xhttpresponse* pResponse,
	size_t iIndex
);



/*
	返回响应借用的只读 Trailer 容器。
	容器与响应同寿命，调用方不得修改或销毁。
*/
XRT_API const xhttpheaders* xrtHttpResponseTrailers(
	const xhttpresponse* pResponse
);



/* 返回缓冲模式下的连续正文；流式模式返回空视图。 */
XRT_API xbytesview xrtHttpResponseBody(
	const xhttpresponse* pResponse
);



/* 返回已缓冲或已通过流式回调交付的正文总字节数。 */
XRT_API uint64 xrtHttpResponseBodyBytes(
	const xhttpresponse* pResponse
);



/* 返回收到的编码正文载荷字节数，不包含 HTTP/1 chunk 元数据。 */
XRT_API uint64 xrtHttpResponseWireBodyBytes(
	const xhttpresponse* pResponse
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

/*
	返回自动解码前合并后的 Content-Encoding；未解码时返回空视图。
	Header 值按收到顺序用逗号和空格连接，并由响应拥有。
*/
XRT_API xstrview xrtHttpResponseOriginalEncoding(
	const xhttpresponse* pResponse
);

#endif



/* 复制正文并附加零字符；返回值由 xrtFree 释放，二进制零字节原样保留。 */
XRT_API str xrtHttpResponseBodyText(
	const xhttpresponse* pResponse
);



/* 返回借用的最终有效 URL 文本。 */
XRT_API xstrview xrtHttpResponseUrl(
	const xhttpresponse* pResponse
);



/* 返回到达最终响应前实际跟随的重定向跳数。 */
XRT_API size_t xrtHttpResponseRedirects(
	const xhttpresponse* pResponse
);

#endif



XRT_EXTERN_C_END

#endif
