#include "xws_native_internal.h"



typedef struct xws_native_worker_call {
	xatomic32 Completed;
	xevent Done;
	xnetpost Post;
	xws_native_worker_proc Proc;
	ptr Data;
	xerror* Error;
} xws_native_worker_call;



/* 网络 Post 入口隔离 Worker 原错误，保存本次命令错误并唤醒等待线程。 */
static void xwsNativeWorkerRun(xnetworker* pWorker, ptr pData)
{
	xws_native_worker_call* pCall = (xws_native_worker_call*)pData;
	xerror* pPrevious;

	(void)pWorker;
	pPrevious = xrtTakeError();
	pCall->Proc(pCall->Data);
	pCall->Error = xrtTakeError();
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
		xrtErrorFree(pPrevious);
	}
	xrtAtomic32Store(&pCall->Completed, 1u, XMEMORY_RELEASE);
	(void)xrtEventSet(&pCall->Done);
}



/* 在目标 Worker 上执行短命令，避免把 xws 的 Worker 契约泄漏为易错调用要求。 */
bool xwsNativeWorkerCall(
	xnetworker* pWorker,
	xws_native_worker_proc pProc,
	ptr pData
)
{
	xws_native_worker_call Call;
	xwaitresult Wait;
	bool bPosted;

	if ( (pWorker == NULL) || (pProc == NULL) ) {
		return false;
	}
	if ( xrtNetWorkerIsCurrent(pWorker) ) {
		pProc(pData);
		return true;
	}
	memset(&Call, 0, sizeof(Call));
	if ( !xrtEventInit(&Call.Done, false, false) ) {
		return false;
	}
	if ( !xrtNetPostInit(&Call.Post) ) {
		(void)xrtEventDestroy(&Call.Done);
		return false;
	}
	xrtAtomic32Init(&Call.Completed, 0u);
	Call.Proc = pProc;
	Call.Data = pData;
	bPosted = xrtNetPost(pWorker, &Call.Post, xwsNativeWorkerRun, &Call);
	if ( bPosted ) {
		Wait = xrtEventWait(&Call.Done);
		while ( xrtAtomic32Load(&Call.Completed, XMEMORY_ACQUIRE) == 0u ) {
			xrtSleep(1u);
		}
	} else {
		Wait = XWAIT_ERROR;
	}
	if ( (Wait == XWAIT_OK) && (Call.Error != NULL) ) {
		xrtSetError(Call.Error);
	}
	xrtErrorFree(Call.Error);
	(void)xrtEventDestroy(&Call.Done);
	return bPosted && (Wait == XWAIT_OK);
}



/* 复制一个允许为空的文本视图。 */
xbuffer* xwsNativeText(xstrview Text)
{
	xbytesview Data;

	Data.Data = (const unsigned char*)(Text.Data != NULL ? Text.Data : "");
	Data.Size = Text.Size;
	return xrtBufferFrom(Data);
}



/* 扩展层统一从 XRT 结构化错误读取文本。 */
XLANG_XWS_EXPORT const char* xws_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	return (sMessage != NULL) && (sMessage[0] != 0) ?
		sMessage : "WebSocket operation failed";
}
