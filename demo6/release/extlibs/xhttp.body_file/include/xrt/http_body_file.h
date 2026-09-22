#ifndef XRT_HTTP_BODY_FILE_H
#define XRT_HTTP_BODY_FILE_H

#include "http_body.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE) && \
	(!defined(XHTTP_FEATURE_HTTP_BODY_ASYNC) || \
	 !defined(XRT_FEATURE_FILE_ASYNC))
	#error "XRT HTTP file body support requires async HTTP body and async file support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 默认单次异步读取 64 KiB；这是按请求分配上限，不是每对象固定缓冲。 */
#define XHTTP_BODY_FILE_READ_DEFAULT 65536u



/* ReadSize 限制每次异步文件读取的字节数，必须大于零。 */
typedef struct xhttpbodyfileconfig {
	size_t ReadSize;
} xhttpbodyfileconfig;



/* 文件正文错误保留准备阶段或读取阶段的底层 cause。 */
typedef enum xhttpbodyfileerror {
	XHTTP_BODY_FILE_ERROR_SUBMIT = 1,
	XHTTP_BODY_FILE_ERROR_OPEN,
	XHTTP_BODY_FILE_ERROR_SIZE,
	XHTTP_BODY_FILE_ERROR_RANGE,
	XHTTP_BODY_FILE_ERROR_ADOPT,
	XHTTP_BODY_FILE_ERROR_CREATE,
	XHTTP_BODY_FILE_ERROR_READ
} xhttpbodyfileerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 初始化默认文件正文配置；输出结构可以未对齐。 */
XRT_API void xrtHttpBodyFileConfigInit(
	xhttpbodyfileconfig* pConfig
);



/*
	创建采用异步文件的不可重放正文。
	成功后正文独占关闭 pFile；失败时调用方仍然拥有 pFile。
	配置在调用期间复制，可以未对齐；NULL 使用默认配置。
*/
XRT_API xhttpbody* xrtHttpBodyFileAdopt(
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
);



/*
	在任务池中打开文件并创建完整文件正文。
	成功 Future 的值为借用 xhttpbody，Future 释放时销毁其正文引用。
*/
XRT_API xfuture* xrtHttpBodyFileFuture(
	xtaskpool* pPool,
	cstr sPath,
	const xhttpbodyfileconfig* pConfig
);



/*
	在任务池中打开文件并创建严格字节区间正文。
	区间超出同一已打开文件的大小时，Future 以 RANGE 错误失败。
*/
XRT_API xfuture* xrtHttpBodyFileRangeFuture(
	xtaskpool* pPool,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
);

#endif



XRT_EXTERN_C_END

#endif

