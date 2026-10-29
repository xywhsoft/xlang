#ifndef XRT_MAIL_DATE_H
#define XRT_MAIL_DATE_H

#include <xrt/mail.h>
#include <xrt/time.h>



#if defined(XMAIL_FEATURE_MAIL_DATE) && !defined(XRT_FEATURE_TIME_TEXT)
	#error "XMAIL_FEATURE_MAIL_DATE requires XRT_FEATURE_TIME_TEXT"
#endif



#if defined(XMAIL_FEATURE_MAIL_DATE)

XRT_EXTERN_C_BEGIN



/* 按 RFC 5322 规范形式写入邮件日期；UTC 偏移必须精确到分钟。 */
XRT_API bool xrtMailDateWrite(
	xtime iTime,
	int iOffset,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 RFC 5322 规范日期。 */
XRT_API str xrtMailDate(xtime iTime, int iOffset, size_t* pOutputSize);



/* 解析现代 RFC 5322 日期；星期和秒可以省略，输入必须已展开字段折行。 */
XRT_API bool xrtMailDateParse(
	xstrview Text,
	xtime* pTime,
	int* pOffset
);



XRT_EXTERN_C_END

#endif

#endif
