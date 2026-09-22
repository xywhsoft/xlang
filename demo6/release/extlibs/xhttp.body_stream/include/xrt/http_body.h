#ifndef XRT_HTTP_BODY_H
#define XRT_HTTP_BODY_H

#include <xrt.h>

#if defined(XHTTP_FEATURE_HTTP_BODY_ASYNC)
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_ASYNC) && \
	!defined(XHTTP_FEATURE_HTTP_BODY)
	#error "XRT async HTTP body support requires HTTP body support"
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_TRANSFORM) && \
	!defined(XHTTP_FEATURE_HTTP_BODY)
	#error "XRT HTTP body transform support requires HTTP body support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY)

/* 未知正文长度使用完整 uint64 范围之外的哨兵值。 */
#define XHTTP_BODY_UNKNOWN UINT64_MAX



typedef struct xhttpbody xhttpbody;
typedef struct xhttpbodyreader xhttpbodyreader;
struct xfuture;



/* 正文源标志只描述可以由正文对象保证的稳定能力。 */
typedef enum xhttpbodyflag {
	XHTTP_BODY_NONE = 0,
	XHTTP_BODY_REPLAYABLE = UINT32_C(0x00000001)
} xhttpbodyflag;



/* 正文读取结果把正常结束、暂不可读和真正失败分开表达。 */
typedef enum xhttpbodystatus {
	XHTTP_BODY_ERROR = -1,
	XHTTP_BODY_EOF = 0,
	XHTTP_BODY_DATA = 1,
	XHTTP_BODY_AGAIN = 2
} xhttpbodystatus;



/* HTTP 正文域错误用于识别来源违反契约和长度不一致。 */
typedef enum xhttpbodyerror {
	XHTTP_BODY_ERROR_REOPEN = 1,
	XHTTP_BODY_ERROR_SOURCE,
	XHTTP_BODY_ERROR_CONTRACT,
	XHTTP_BODY_ERROR_LENGTH
} xhttpbodyerror;



/* Chunk 释放过程必须只释放本次数据租约，不能销毁 Reader。 */
typedef void (*xhttpbodyreleaseproc)(
	ptr pContext,
	cbytes pData,
	size_t iSize
);



/*
	Chunk 在 DATA 时拥有一个独立数据租约。
	调用方必须执行 xrtHttpBodyChunkRelease，Reader 可以先于 Chunk 销毁。
*/
typedef struct xhttpbodychunk {
	cbytes Data;
	size_t Size;
	xhttpbodyreleaseproc Release;
	ptr Context;
} xhttpbodychunk;



/*
	Next 每次最多返回 MaxBytes；ERROR 时应设置当前执行上下文错误。
	DATA 必须返回非空数据、非零长度和释放过程。
*/
typedef xhttpbodystatus (*xhttpbodynextproc)(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
);



/* Reader 关闭过程只执行一次；其错误不会覆盖销毁调用方的当前错误。 */
typedef void (*xhttpbodycloseproc)(ptr pContext);



/*
	Wait 在 Reader 返回 AGAIN 后返回一个调用方拥有的可读性 Future。
	Future 完成只表示应重试 Next，不保证下一次读取一定产生 DATA。
	返回引用必须独立于 Reader，并在 Reader 关闭后继续保持有效。
	回调槽始终存在以保持独立编译模块的 ABI；同步源应设置为 NULL。
*/
typedef struct xfuture* (*xhttpbodywaitproc)(ptr pContext);



/* 每次 Open 返回独立 Reader 操作和上下文。 */
typedef struct xhttpbodyreaderops {
	xhttpbodynextproc Next;
	xhttpbodycloseproc Close;
	xhttpbodywaitproc Wait;
} xhttpbodyreaderops;



/*
	Open 必须是快速、非阻塞操作。
	REPLAYABLE 正文允许并发 Open，因此工厂必须同步自身共享状态。
	Open 返回失败时必须自行回收本次尝试创建的全部 Reader 资源。
*/
typedef bool (*xhttpbodyopenproc)(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
);



/* 工厂销毁过程只执行一次；其错误不会覆盖销毁调用方的当前错误。 */
typedef void (*xhttpbodydestroyproc)(ptr pFactory);



/* 正文工厂操作在 Create 时复制，调用方无需长期保存结构。 */
typedef struct xhttpbodyops {
	xhttpbodyopenproc Open;
	xhttpbodydestroyproc Destroy;
} xhttpbodyops;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY)

/*
	创建自定义正文源；成功后接管 Factory 的 Destroy 责任。
	Length 可以是 XHTTP_BODY_UNKNOWN，Flags 只接受公开标志。
*/
XRT_API xhttpbody* xrtHttpBodyCreate(
	const xhttpbodyops* pOps,
	ptr pFactory,
	uint64 iLength,
	uint32 iFlags
);



/* 创建可重放的空正文。 */
XRT_API xhttpbody* xrtHttpBodyEmpty(void);



/* 创建拥有正文副本的可重放正文。 */
XRT_API xhttpbody* xrtHttpBodyCopy(xbytesview Data);



/* 创建借用外部内存的可重放正文，外部内存必须覆盖正文对象使用期。 */
XRT_API xhttpbody* xrtHttpBodyBorrow(xbytesview Data);



/* 成功时接管由 xrtMalloc 分配的数据，失败时所有权仍归调用方。 */
XRT_API xhttpbody* xrtHttpBodyTake(ptr pData, size_t iSize);



/*
	创建带自定义释放过程的可重放正文。
	Release 不得为空；成功后在最后一个正文或 Chunk 引用释放时执行一次。
*/
XRT_API xhttpbody* xrtHttpBodyReference(
	xbytesview Data,
	xhttpbodyreleaseproc pRelease,
	ptr pContext
);



/* 增加正文对象引用并返回原指针。 */
XRT_API xhttpbody* xrtHttpBodyRef(xhttpbody* pBody);



/* 释放正文对象引用；空指针安全，清理回调不会改变当前错误。 */
XRT_API void xrtHttpBodyDestroy(xhttpbody* pBody);



/* 返回正文线性字节长度或 XHTTP_BODY_UNKNOWN。 */
XRT_API uint64 xrtHttpBodyLength(const xhttpbody* pBody);



/* 返回正文源公开能力标志。 */
XRT_API uint32 xrtHttpBodyFlags(const xhttpbody* pBody);



/* 判断正文能否重新 Open 并从头读取。 */
XRT_API bool xrtHttpBodyReplayable(const xhttpbody* pBody);



/*
	借用固定正文的连续字节视图。
	自定义或流式正文返回 false、清空 Data，且不修改线程原有错误。
	输出描述符不得覆盖正文对象或固定正文的底层字节。
*/
XRT_API bool xrtHttpBodyView(
	const xhttpbody* pBody,
	xbytesview* pData
);



/* 打开独立 Reader；非可重放正文一生最多尝试一次。 */
XRT_API xhttpbodyreader* xrtHttpBodyOpen(xhttpbody* pBody);



/*
	读取下一个拥有型 Chunk。
	已失败 Reader 稳定重放同一个错误，已结束 Reader 稳定返回 EOF。
	输出 Chunk 不得覆盖 Reader、Body 或固定正文的底层字节。
*/
XRT_API xhttpbodystatus xrtHttpBodyNext(
	xhttpbodyreader* pReader,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
);



/*
	复制读取一个 Chunk 并立即释放，适合不需要零复制的调用方。
	输出缓冲与 Size 不得相互覆盖，也不得覆盖 Reader、Body 或固定正文。
	别名参数错误不会推进 Reader，也不会改写 Size。
*/
XRT_API xhttpbodystatus xrtHttpBodyRead(
	xhttpbodyreader* pReader,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 先清空再释放 Chunk 租约；空结构安全，回调不会改变当前错误。 */
XRT_API void xrtHttpBodyChunkRelease(xhttpbodychunk* pChunk);



/* 返回 Reader 已发布的正文总字节数。 */
XRT_API uint64 xrtHttpBodyReaderBytes(const xhttpbodyreader* pReader);



/* 返回失败 Reader 借用的稳定错误，其他状态返回空指针。 */
XRT_API const xerror* xrtHttpBodyReaderError(
	const xhttpbodyreader* pReader
);



/* 关闭 Reader 并释放正文引用；未释放 Chunk 仍保持有效。 */
XRT_API void xrtHttpBodyReaderDestroy(xhttpbodyreader* pReader);

#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_ASYNC)

/*
	在 AGAIN 后取得一次可读性 Future；每个 AGAIN 只能成功取得一次。
	调用方拥有返回引用，Future 完成后必须重新调用 Next。
*/
XRT_API xfuture* xrtHttpBodyReaderWait(xhttpbodyreader* pReader);

#endif



XRT_EXTERN_C_END

#endif
