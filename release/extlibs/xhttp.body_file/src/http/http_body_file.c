#include "../internal/xrt_http_body_file.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 文件正文工厂独占一个异步文件和不可变区间。 */
typedef struct xrt_http_body_file_factory {
	xasyncfile* File;
	uint64 Offset;
	uint64 Length;
	size_t ReadSize;
} xrt_http_body_file_factory;



/* 每个文件正文只有一个 Reader，并且最多保留一次异步读取。 */
typedef struct xrt_http_body_file_reader {
	xrt_http_body_file_factory* Factory;
	xrt_http_body_file_cursor Cursor;
} xrt_http_body_file_reader;



/* 文件准备任务复制路径，并保留完整文件或严格区间参数。 */
typedef struct xrt_http_body_file_prepare {
	xtaskpool* Pool;
	uint64 Offset;
	uint64 Length;
	xhttpbodyfileconfig Config;
	bool Range;
	char Path[];
} xrt_http_body_file_prepare;



/* 设置文件正文域错误，并接管可选 cause 引用。 */
static void __xrtHttpBodyFileError(
	xerrkind Kind,
	xhttpbodyfileerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : Kind;
	Desc.Domain = "http.body.file";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 把当前执行上下文错误包装为文件正文错误。 */
static void __xrtHttpBodyFileWrap(
	xerrkind Kind,
	xhttpbodyfileerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtHttpBodyFileError(
		Kind,
		Code,
		sOperation,
		sMessage,
		xrtTakeError()
	);
}



/* 静默关闭同步文件，并保留调用方正在处理的错误。 */
void __xrtHttpBodyFileCloseSync(xfile File)
{
	xerror* pSaved = xrtTakeError();

	if ( File != NULL ) {
		(void)xrtClose(File);
	}
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	}
}



/* 请求关闭异步文件，并保留调用方正在处理的错误。 */
void __xrtHttpBodyFileCloseAsync(xasyncfile* pFile)
{
	xerror* pSaved = xrtTakeError();
	xfuture* pFuture;

	if ( pFile != NULL ) {
		pFuture = xrtAsyncFileClose(pFile);
		xrtFutureDestroy(pFuture);
	}
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	}
}



/* 文件 Chunk 通过保留读取 Future 延长连续结果缓冲的生命周期。 */
static void __xrtHttpBodyFileChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	xrtFutureDestroy((xfuture*)pContext);
}



/* 释放 Reader 的待处理读取，并触发工厂文件关闭。 */
static void __xrtHttpBodyFileReaderClose(ptr pContext)
{
	xrt_http_body_file_reader* pReader =
		(xrt_http_body_file_reader*)pContext;
	xasyncfile* pFile;

	if ( pReader == NULL ) {
		return;
	}
	__xrtHttpBodyFileCursorCancel(&pReader->Cursor);
	pFile = pReader->Factory->File;
	pReader->Factory->File = NULL;
	__xrtHttpBodyFileCloseAsync(pFile);
	xrtFree(pReader);
}



/* 在 AGAIN 后返回当前读取 Future 的独立消费端引用。 */
static xfuture* __xrtHttpBodyFileReaderWait(ptr pContext)
{
	xrt_http_body_file_reader* pReader =
		(xrt_http_body_file_reader*)pContext;

	if ( pReader == NULL ) {
		__xhttpErrorSetInvalidState();
		return NULL;
	}
	return __xrtHttpBodyFileCursorWait(
		&pReader->Cursor
	);
}



/* 初始化一个连续异步文件区间。 */
void __xrtHttpBodyFileCursorInit(
	xrt_http_body_file_cursor* pCursor,
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	size_t iReadSize
)
{
	memset(pCursor, 0, sizeof(*pCursor));
	pCursor->File = pFile;
	pCursor->Offset = iOffset;
	pCursor->Remaining = iLength;
	pCursor->ReadSize = iReadSize;
}



/* 取消并释放尚未消费完的异步读取。 */
void __xrtHttpBodyFileCursorCancel(
	xrt_http_body_file_cursor* pCursor
)
{
	if ( (pCursor == NULL) ||
		(pCursor->Pending == NULL) ) {
		return;
	}
	(void)xrtFutureCancel(pCursor->Pending);
	xrtFutureDestroy(pCursor->Pending);
	pCursor->Pending = NULL;
	pCursor->Requested = 0;
	pCursor->ReadyOffset = 0;
	pCursor->Ready = false;
}



/* 在 AGAIN 后返回 cursor 当前读取的独立引用。 */
xfuture* __xrtHttpBodyFileCursorWait(
	xrt_http_body_file_cursor* pCursor
)
{
	if ( (pCursor == NULL) ||
		(pCursor->Pending == NULL) ) {
		__xhttpErrorSetInvalidState();
		return NULL;
	}
	return xrtFutureRef(pCursor->Pending);
}



/* 把已经完成的读取结果切分成不超过当前上限的独立 Chunk。 */
static xhttpbodystatus __xrtHttpBodyFileCursorReady(
	xrt_http_body_file_cursor* pCursor,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xfiledata* pData = (xfiledata*)xrtFutureValue(
		pCursor->Pending
	);
	xfuture* pLease;
	size_t iAvailable;
	size_t iSize;

	if ( pData == NULL ) {
		__xrtHttpBodyFileError(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_READ,
			"read",
			"the asynchronous file read returned no data result",
			NULL
		);
		return XHTTP_BODY_ERROR;
	}
	if ( !pCursor->Ready ) {
		if ( pData->Size != pCursor->Requested ) {
			__xrtHttpBodyFileError(
				XERR_IO,
				XHTTP_BODY_FILE_ERROR_READ,
				"read",
				"the file ended before the declared body range",
				NULL
			);
			return XHTTP_BODY_ERROR;
		}
		pCursor->Ready = true;
		pCursor->ReadyOffset = 0;
	}
	iAvailable = pData->Size - pCursor->ReadyOffset;
	iSize = iAvailable < iMaxBytes ? iAvailable : iMaxBytes;
	pLease = xrtFutureRef(pCursor->Pending);
	if ( pLease == NULL ) {
		return XHTTP_BODY_ERROR;
	}
	pChunk->Data = pData->Data + pCursor->ReadyOffset;
	pChunk->Size = iSize;
	pChunk->Release = __xrtHttpBodyFileChunkRelease;
	pChunk->Context = pLease;
	pCursor->ReadyOffset += iSize;
	pCursor->Offset += (uint64)iSize;
	pCursor->Remaining -= (uint64)iSize;
	if ( pCursor->ReadyOffset == pData->Size ) {
		xrtFutureDestroy(pCursor->Pending);
		pCursor->Pending = NULL;
		pCursor->Requested = 0;
		pCursor->ReadyOffset = 0;
		pCursor->Ready = false;
	}
	return XHTTP_BODY_DATA;
}



/* 推进文件读取状态机；磁盘读取只通过异步文件任务发生。 */
xhttpbodystatus __xrtHttpBodyFileCursorNext(
	xrt_http_body_file_cursor* pCursor,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xfuturestate State;
	size_t iRequest;

	if ( (pCursor == NULL) || (pCursor->File == NULL) ||
		(pCursor->ReadSize == 0) ||
		(pChunk == NULL) || (iMaxBytes == 0) ) {
		__xhttpErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	if ( pCursor->Pending != NULL ) {
		State = xrtFutureState(pCursor->Pending);
		if ( State == XFUTURE_PENDING ) {
			return XHTTP_BODY_AGAIN;
		}
		if ( State != XFUTURE_RESOLVED ) {
			(void)xrtFutureValue(pCursor->Pending);
			__xrtHttpBodyFileWrap(
				XERR_IO,
				XHTTP_BODY_FILE_ERROR_READ,
				"read",
				"the asynchronous file read failed"
			);
			return XHTTP_BODY_ERROR;
		}
		return __xrtHttpBodyFileCursorReady(
			pCursor,
			iMaxBytes,
			pChunk
		);
	}
	if ( pCursor->Remaining == 0 ) {
		return XHTTP_BODY_EOF;
	}
	iRequest = pCursor->Remaining < (uint64)iMaxBytes ?
		(size_t)pCursor->Remaining : iMaxBytes;
	if ( iRequest > pCursor->ReadSize ) {
		iRequest = pCursor->ReadSize;
	}
	pCursor->Pending = xrtAsyncFileReadAt(
		pCursor->File,
		pCursor->Offset,
		iRequest
	);
	if ( pCursor->Pending == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_READ,
			"submit-read",
			"failed to submit the next file body read"
		);
		return XHTTP_BODY_ERROR;
	}
	pCursor->Requested = iRequest;
	return XHTTP_BODY_AGAIN;
}



/* 通过共享 cursor 推进单区间文件正文。 */
static xhttpbodystatus __xrtHttpBodyFileReaderNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xrt_http_body_file_reader* pReader =
		(xrt_http_body_file_reader*)pContext;

	if ( pReader == NULL ) {
		__xhttpErrorSetInvalidState();
		return XHTTP_BODY_ERROR;
	}
	return __xrtHttpBodyFileCursorNext(
		&pReader->Cursor,
		iMaxBytes,
		pChunk
	);
}



/* 快速分配单消费 Reader，不执行任何文件系统调用。 */
static bool __xrtHttpBodyFileOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	xrt_http_body_file_factory* pFile =
		(xrt_http_body_file_factory*)pFactory;
	xrt_http_body_file_reader* pReader;

	if ( (pFile == NULL) || (pFile->File == NULL) ||
		(pOps == NULL) || (ppReader == NULL) ) {
		__xhttpErrorSetInvalidState();
		return false;
	}
	pReader = (xrt_http_body_file_reader*)xrtCalloc(
		1,
		sizeof(xrt_http_body_file_reader)
	);
	if ( pReader == NULL ) {
		return false;
	}
	pReader->Factory = pFile;
	__xrtHttpBodyFileCursorInit(
		&pReader->Cursor,
		pFile->File,
		pFile->Offset,
		pFile->Length,
		pFile->ReadSize
	);
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = __xrtHttpBodyFileReaderNext;
	pOps->Close = __xrtHttpBodyFileReaderClose;
	pOps->Wait = __xrtHttpBodyFileReaderWait;
	*ppReader = pReader;
	return true;
}



/* 销毁未打开的工厂，或回收 Reader 已经关闭后的空工厂。 */
static void __xrtHttpBodyFileDestroy(ptr pFactory)
{
	xrt_http_body_file_factory* pFile =
		(xrt_http_body_file_factory*)pFactory;

	if ( pFile == NULL ) {
		return;
	}
	__xrtHttpBodyFileCloseAsync(pFile->File);
	pFile->File = NULL;
	xrtFree(pFile);
}



/* 校验文件区间能由跨平台异步文件层表达。 */
bool __xrtHttpBodyFileRangeValid(
	uint64 iOffset,
	uint64 iLength
)
{
	if ( (iOffset > (uint64)INT64_MAX) ||
		(iLength > ((uint64)INT64_MAX - iOffset)) ) {
		__xrtHttpBodyFileError(
			XERR_RANGE,
			XHTTP_BODY_FILE_ERROR_RANGE,
			"range",
			"the file body range is outside the supported file offset space",
			NULL
		);
		return false;
	}
	return true;
}



/* 初始化默认的按需读取粒度，不为正文预分配固定缓冲。 */
XRT_API void xrtHttpBodyFileConfigInit(
	xhttpbodyfileconfig* pConfig
)
{
	xhttpbodyfileconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.ReadSize = XHTTP_BODY_FILE_READ_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 复制并校验调用方配置；NULL 使用稳定默认值。 */
static bool __xrtHttpBodyFileConfigCopy(
	xhttpbodyfileconfig* pOutput,
	const xhttpbodyfileconfig* pConfig
)
{
	memset(pOutput, 0, sizeof(*pOutput));
	pOutput->ReadSize = XHTTP_BODY_FILE_READ_DEFAULT;
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xhttpErrorSetInvalidArgument();
			return false;
		}
		memcpy(pOutput, pConfig, sizeof(*pOutput));
	}
	if ( pOutput->ReadSize == 0 ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 创建采用异步文件的不可重放正文。 */
XRT_API xhttpbody* xrtHttpBodyFileAdopt(
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
)
{
	static const xhttpbodyops Ops = {
		__xrtHttpBodyFileOpen,
		__xrtHttpBodyFileDestroy
	};
	xrt_http_body_file_factory* pFactory;
	xhttpbody* pBody;
	xhttpbodyfileconfig Config;

	if ( (pFile == NULL) ||
		((xrtAsyncFileFlags(pFile) & XFILE_READ) == 0u) ) {
		if ( pFile == NULL ) {
			__xhttpErrorSetInvalidArgument();
		} else {
			__xrtHttpBodyFileError(
				XERR_ARGUMENT,
				XHTTP_BODY_FILE_ERROR_ADOPT,
				"adopt",
				"the adopted asynchronous file is not readable",
				NULL
			);
		}
		return NULL;
	}
	if ( !__xrtHttpBodyFileRangeValid(iOffset, iLength) ) {
		return NULL;
	}
	if ( !__xrtHttpBodyFileConfigCopy(&Config, pConfig) ) {
		return NULL;
	}
	pFactory = (xrt_http_body_file_factory*)xrtCalloc(
		1,
		sizeof(xrt_http_body_file_factory)
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pFactory->File = pFile;
	pFactory->Offset = iOffset;
	pFactory->Length = iLength;
	pFactory->ReadSize = Config.ReadSize;
	pBody = xrtHttpBodyCreate(
		&Ops,
		pFactory,
		iLength,
		XHTTP_BODY_NONE
	);
	if ( pBody == NULL ) {
		pFactory->File = NULL;
		xrtFree(pFactory);
	}
	return pBody;
}



/* 释放 Future 拥有的文件正文结果。 */
static void __xrtHttpBodyFileValueFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpBodyDestroy((xhttpbody*)pValue);
}



/* 在任务池工作线程内打开、定长并采用同一个文件句柄。 */
static xtaskoutcome __xrtHttpBodyFilePrepareTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_http_body_file_prepare* pPrepare =
		(xrt_http_body_file_prepare*)pData;
	xfile File;
	xasyncfile* pAsync;
	xhttpbody* pBody;
	uint64 iFileSize;
	uint64 iOffset;
	uint64 iLength;

	File = xrtOpen(pPrepare->Path, XFILE_READ);
	if ( File == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_OPEN,
			"open",
			"failed to open the HTTP body file"
		);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_CANCELLED;
	}
	if ( !xrtFileSize(File, &iFileSize) ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_SIZE,
			"size",
			"failed to query the HTTP body file size"
		);
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	iOffset = pPrepare->Range ? pPrepare->Offset : 0;
	iLength = pPrepare->Range ? pPrepare->Length : iFileSize;
	if ( (iOffset > iFileSize) ||
		(iLength > (iFileSize - iOffset)) ) {
		__xrtHttpBodyFileError(
			XERR_RANGE,
			XHTTP_BODY_FILE_ERROR_RANGE,
			"range",
			"the requested HTTP body range exceeds the opened file",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	if ( !__xrtHttpBodyFileRangeValid(iOffset, iLength) ) {
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_CANCELLED;
	}
	pAsync = xrtAsyncFileAdopt(pPrepare->Pool, File);
	if ( pAsync == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_ADOPT,
			"adopt",
			"failed to adopt the opened HTTP body file"
		);
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseAsync(pAsync);
		return XTASK_CANCELLED;
	}
	pBody = xrtHttpBodyFileAdopt(
		pAsync,
		iOffset,
		iLength,
		&pPrepare->Config
	);
	if ( pBody == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_CREATE,
			"create",
			"failed to create the HTTP file body"
		);
		__xrtHttpBodyFileCloseAsync(pAsync);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtHttpBodyDestroy(pBody);
		return XTASK_CANCELLED;
	}
	pResult->Value = pBody;
	pResult->Destroy = __xrtHttpBodyFileValueFree;
	return XTASK_SUCCESS;
}



/* 释放任务池已经受理的文件准备参数。 */
static void __xrtHttpBodyFilePrepareFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtFree(pValue);
}



/* 复制路径并提交完整文件或严格区间准备任务。 */
static xfuture* __xrtHttpBodyFilePrepare(
	xtaskpool* pPool,
	cstr sPath,
	bool bRange,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
)
{
	xrt_http_body_file_prepare* pPrepare;
	xtaskargs Args;
	xfuture* pFuture;
	size_t iPathLength;
	xhttpbodyfileconfig Config;

	if ( (pPool == NULL) || (sPath == NULL) ||
		(sPath[0] == '\0') ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( bRange &&
		!__xrtHttpBodyFileRangeValid(iOffset, iLength) ) {
		return NULL;
	}
	if ( !__xrtHttpBodyFileConfigCopy(&Config, pConfig) ) {
		return NULL;
	}
	iPathLength = strlen(sPath);
	if ( iPathLength > (SIZE_MAX -
		sizeof(xrt_http_body_file_prepare) - 1) ) {
		__xhttpErrorSetSizeOverflow();
		return NULL;
	}
	pPrepare = (xrt_http_body_file_prepare*)xrtMalloc(
		sizeof(xrt_http_body_file_prepare) +
		iPathLength + 1
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	pPrepare->Pool = pPool;
	pPrepare->Offset = iOffset;
	pPrepare->Length = iLength;
	pPrepare->Config = Config;
	pPrepare->Range = bRange;
	memcpy(pPrepare->Path, sPath, iPathLength + 1);
	memset(&Args, 0, sizeof(Args));
	Args.Destroy = __xrtHttpBodyFilePrepareFree;
	pFuture = xrtTaskSubmit(
		pPool,
		__xrtHttpBodyFilePrepareTask,
		pPrepare,
		&Args
	);
	if ( pFuture == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_SUBMIT,
			"submit",
			"failed to submit the HTTP file body preparation"
		);
		xrtFree(pPrepare);
	}
	return pFuture;
}



/* 在任务池中准备完整文件正文。 */
XRT_API xfuture* xrtHttpBodyFileFuture(
	xtaskpool* pPool,
	cstr sPath,
	const xhttpbodyfileconfig* pConfig
)
{
	return __xrtHttpBodyFilePrepare(
		pPool,
		sPath,
		false,
		0,
		0,
		pConfig
	);
}



/* 在任务池中准备严格文件区间正文。 */
XRT_API xfuture* xrtHttpBodyFileRangeFuture(
	xtaskpool* pPool,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
)
{
	return __xrtHttpBodyFilePrepare(
		pPool,
		sPath,
		true,
		iOffset,
		iLength,
		pConfig
	);
}

#endif
