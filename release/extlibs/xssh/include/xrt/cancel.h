#ifndef XRT_CANCEL_H
#define XRT_CANCEL_H

#include <xrt/core.h>
#include <xrt/sync.h>



#if defined(XRT_FEATURE_CANCEL) && !defined(XRT_FEATURE_MUTEX)
	#error "XRT_FEATURE_CANCEL requires XRT_FEATURE_MUTEX"
#endif

#if defined(XRT_FEATURE_CANCEL) && !defined(XRT_FEATURE_COND)
	#error "XRT_FEATURE_CANCEL requires XRT_FEATURE_COND"
#endif



#if defined(XRT_FEATURE_CANCEL)
/* 取消令牌保存一次性取消状态，并可通过不可变父链继承取消。 */
typedef struct xcancel xcancel;



/* 取消监听保存一次回调注册及其并发生命周期。 */
typedef struct xcancelwatch xcancelwatch;



/* 取消回调由命中的取消请求线程或迟注册线程同步执行。 */
typedef void (*xcancelproc)(ptr pData);



XRT_EXTERN_C_BEGIN



/* 创建一个独立的取消令牌。 */
XRT_API xcancel* xrtCancelCreate(void);



/* 创建一个继承父令牌取消状态的子令牌；父令牌可为空。 */
XRT_API xcancel* xrtCancelChild(xcancel* pParent);



/* 增加取消令牌引用并返回原指针。 */
XRT_API xcancel* xrtCancelRef(xcancel* pCancel);



/* 释放取消令牌引用；空指针视为空操作。 */
XRT_API void xrtCancelDestroy(xcancel* pCancel);



/* 请求取消；仅首次请求返回 true 并触发监听。 */
XRT_API bool xrtCancelRequest(xcancel* pCancel);



/* 查询令牌或任一祖先是否已请求取消；空指针表示未取消。 */
XRT_API bool xrtCancelRequested(const xcancel* pCancel);



/* 监听令牌及其不可变父链；回调至多同步执行一次。 */
XRT_API xcancelwatch* xrtCancelWatch(
	xcancel* pCancel,
	xcancelproc pProc,
	ptr pData
);



/* 查询监听是否已命中取消。 */
XRT_API bool xrtCancelTriggered(const xcancelwatch* pWatch);



/* 注销并释放监听；从其他线程调用时等待正在执行的回调返回。 */
XRT_API void xrtCancelUnwatch(xcancelwatch* pWatch);



XRT_EXTERN_C_END
#endif

#endif
