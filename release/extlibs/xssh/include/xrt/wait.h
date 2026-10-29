#ifndef XRT_WAIT_H
#define XRT_WAIT_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_WAIT) && !defined(XRT_FEATURE_TIME)
	#error "XRT_FEATURE_WAIT requires XRT_FEATURE_TIME"
#endif



#if defined(XRT_FEATURE_WAIT)

/* 截止时间使用 xrtClock 的单调微秒刻度。 */
typedef uint64 xdeadline;



/* 永不超时的截止时间。 */
#define XRT_DEADLINE_NEVER UINT64_MAX



/* 等待结果把正常控制流与真正错误分开表达。 */
typedef enum xwaitresult {
	XWAIT_ERROR = -1,
	XWAIT_OK = 0,
	XWAIT_TIMEOUT = 1,
	XWAIT_CANCELLED = 2,
	XWAIT_CLOSED = 3
} xwaitresult;



XRT_EXTERN_C_BEGIN



/* 从当前单调时钟和相对微秒数构造截止时间，溢出时返回 NEVER。 */
XRT_API xdeadline xrtDeadlineAfter(uint64 iTimeout);



/* 判断截止时间是否已经到达；NEVER 永远不会到达。 */
XRT_API bool xrtDeadlineExpired(xdeadline iDeadline);



/* 返回截止时间前剩余微秒数；已到达返回零，NEVER 返回 UINT64_MAX。 */
XRT_API uint64 xrtDeadlineRemaining(xdeadline iDeadline);



XRT_EXTERN_C_END

#endif

#endif
