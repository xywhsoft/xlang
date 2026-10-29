#ifndef XRT_ERROR_H
#define XRT_ERROR_H

#include <xrt/core.h>



/* 跨模块稳定的错误类别。 */
typedef enum xerrkind {
	XERR_NONE = 0,
	XERR_ARGUMENT,
	XERR_TYPE,
	XERR_VALUE,
	XERR_RANGE,
	XERR_STATE,
	XERR_MEMORY,
	XERR_IO,
	XERR_NOT_FOUND,
	XERR_EXISTS,
	XERR_PERMISSION,
	XERR_AGAIN,
	XERR_TIMEOUT,
	XERR_CANCELLED,
	XERR_CLOSED,
	XERR_PROTOCOL,
	XERR_UNSUPPORTED,
	XERR_INTERNAL
} xerrkind;



/* 错误对象由 XRT 管理，对外保持不可变。 */
typedef struct xerror xerror;



/* 描述一个完整错误，所有字符串在创建时复制。 */
typedef struct xerrordesc {
	xerrkind Kind;
	int32 Code;
	int32 SystemCode;
	cstr Domain;
	cstr Operation;
	cstr Message;
	cstr Data;
	const xerror* Cause;
} xerrordesc;



/* 错误处理器只借用错误对象，保存时必须增加引用。 */
typedef void (*xerrorhandler)(const xerror* pError, ptr pUserData);



XRT_EXTERN_C_BEGIN



/* 从完整描述创建一个错误对象。 */
XRT_API xerror* xrtErrorBuild(const xerrordesc* pDesc);



/* 创建一个常用错误对象。 */
XRT_API xerror* xrtErrorCreate(xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage);



/* 创建带有原因链的错误对象。 */
XRT_API xerror* xrtErrorWrap(const xerror* pCause, xerrkind Kind, cstr sDomain, int32 iCode, cstr sMessage);



/* 增加错误对象引用并返回原指针。 */
XRT_API xerror* xrtErrorRef(const xerror* pError);



/* 释放错误对象引用。 */
XRT_API void xrtErrorFree(xerror* pError);



/* 返回错误的通用类别。 */
XRT_API xerrkind xrtErrorKind(const xerror* pError);



/* 返回错误所属的稳定域。 */
XRT_API cstr xrtErrorDomain(const xerror* pError);



/* 返回模块定义的错误代码。 */
XRT_API int32 xrtErrorCode(const xerror* pError);



/* 返回操作系统或外部库错误代码。 */
XRT_API int32 xrtErrorSystemCode(const xerror* pError);



/* 返回发生错误的操作名称。 */
XRT_API cstr xrtErrorOperation(const xerror* pError);



/* 返回错误消息。 */
XRT_API cstr xrtErrorMessage(const xerror* pError);



/* 返回可选的机器可读附加数据。 */
XRT_API cstr xrtErrorData(const xerror* pError);



/* 返回借用的原因错误。 */
XRT_API const xerror* xrtErrorCause(const xerror* pError);



/* 沿原因链查找指定通用类别，返回借用的错误对象。 */
XRT_API const xerror* xrtErrorIs(const xerror* pError, xerrkind Kind);



/* 沿原因链查找完全匹配的错误域和代码，返回借用的错误对象。 */
XRT_API const xerror* xrtErrorFind(const xerror* pError, cstr sDomain, int32 iCode);



/* 返回当前执行上下文借用的错误对象。 */
XRT_API const xerror* xrtGetError(void);



/* 取走当前执行上下文的错误对象。 */
XRT_API xerror* xrtTakeError(void);



/* 将错误对象设置到当前执行上下文，函数会增加引用。 */
XRT_API void xrtSetError(const xerror* pError);



/* 将错误对象所有权转移到当前执行上下文，调用后不得继续使用原引用。 */
XRT_API void xrtSetErrorTake(xerror* pError);



/* 创建常用错误并直接设置到当前执行上下文。 */
XRT_API void xrtSetErrorInfo(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sMessage
);



/* 设置无分配的通用错误；NONE 清除错误，无效类别设置参数错误。 */
XRT_API void xrtSetErrorKind(xerrkind Kind);



/* 清除当前执行上下文的错误。 */
XRT_API void xrtClearError(void);



/* 设置进程级错误通知处理器。 */
XRT_API void xrtSetErrorHandler(xerrorhandler pHandler, ptr pUserData);



XRT_EXTERN_C_END

#endif
