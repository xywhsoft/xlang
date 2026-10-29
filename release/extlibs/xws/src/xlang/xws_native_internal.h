#ifndef XLANG_EXTLIB_XWS_NATIVE_INTERNAL_H
#define XLANG_EXTLIB_XWS_NATIVE_INTERNAL_H

#include "../../include/xws.h"
#include <xruntime.h>
#include "../../../../include/xl_extlib_abi.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XWS_EXPORT __declspec(dllexport)
#else
	#define XLANG_XWS_EXPORT
#endif



typedef void (*xws_native_worker_proc)(ptr pData);



typedef struct xws_native_connection {
	xatomic32 References;
	xatomic32 CallbackHeld;
	xatomic32 MessageOpcode;
	xatomic32 MessageFlags;
	xspinlock Lock;
	xwsconn* Connection;
	xvalue* Handler;
	xerror* CallbackError;
} xws_native_connection;



typedef struct xws_native_writer {
	xatomic32 References;
	xspinlock Lock;
	xnetworker* Worker;
	xwswriter* Writer;
	bool Busy;
	bool Closing;
	bool Finished;
} xws_native_writer;



/* 在目标网络 Worker 同步执行一个短命令；当前线程就是目标时不投递。 */
bool xwsNativeWorkerCall(
	xnetworker* pWorker,
	xws_native_worker_proc pProc,
	ptr pData
);



/* 把借用文本复制为可由 xlang Buffer 接管的稳定结果。 */
xbuffer* xwsNativeText(xstrview Text);



/* 返回当前 XRT 结构化错误的稳定消息。 */
XLANG_XWS_EXPORT const char* xws_last_error(void);



/* 增加包装连接中的原生 Connection 引用。 */
xwsconn* xwsNativeConnectionRef(void* pHandle);



/* 释放包装连接自身的并发生命周期引用。 */
void xwsNativeConnectionRelease(xws_native_connection* pConnection);

#endif
