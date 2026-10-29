#ifndef XRT_SPIN_H
#define XRT_SPIN_H

#include <xrt/atomic.h>



#if defined(XRT_FEATURE_SPIN) && !defined(XRT_FEATURE_ATOMIC)
	#error "XRT_FEATURE_SPIN requires XRT_FEATURE_ATOMIC"
#endif



#if defined(XRT_FEATURE_SPIN)

#define XRT_SPIN_MAGIC UINT32_C(0x5853504e)



/* 短临界区自旋锁不记录所有者，也不支持递归进入。 */
typedef struct xspinlock {
	xatomic32 State;
	uint32 Magic;
} xspinlock;



/* 静态初始化器只用于对象定义。 */
#define XRT_SPIN_INIT { XRT_ATOMIC32_INIT(0u), XRT_SPIN_MAGIC }



XRT_EXTERN_C_BEGIN



/* 初始化调用方提供的自旋锁存储。 */
XRT_API bool xrtSpinInit(xspinlock* pSpin);



/* 释放自旋锁状态；锁仍被持有时失败。 */
XRT_API bool xrtSpinUnit(xspinlock* pSpin);



/* 创建一个动态分配的自旋锁。 */
XRT_API xspinlock* xrtSpinCreate(void);



/* 释放动态自旋锁；空指针视为空操作。 */
XRT_API bool xrtSpinDestroy(xspinlock* pSpin);



/* 自适应等待并进入短临界区。 */
XRT_API bool xrtSpinLock(xspinlock* pSpin);



/* 尝试进入短临界区；锁繁忙时不设置错误。 */
XRT_API bool xrtSpinTryLock(xspinlock* pSpin);



/* 离开短临界区。 */
XRT_API bool xrtSpinUnlock(xspinlock* pSpin);



XRT_EXTERN_C_END

#endif

#endif
