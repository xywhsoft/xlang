#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>



typedef struct xvalue xvalue;
typedef struct xerror xerror;
typedef struct xrtcallframe {
	xvalue* Self;
	const void* Signature;
	size_t ArgumentCount;
	xvalue* const* Arguments;
	size_t KeywordCount;
	const void* KeywordNames;
	xvalue* const* KeywordValues;
	void* Context;
} xrtcallframe;
typedef struct xrtcallresult {
	size_t Count;
	size_t OverflowCapacity;
	xvalue* Inline[4];
	xvalue** Overflow;
} xrtcallresult;

const xerror* xrtGetError(void);
const xerror* xrtErrorCause(const xerror* pError);
const char* xrtErrorDomain(const xerror* pError);
const char* xrtErrorFile(const xerror* pError);
int32_t xrtErrorLine(const xerror* pError);
int32_t xrtErrorColumn(const xerror* pError);
const char* xrtErrorOperation(const xerror* pError);
const char* xrtErrorMessage(const xerror* pError);
void xrtClearError(void);
void xrtCallResultInit(xrtcallresult* pResult);
void xrtCallResultUnit(xrtcallresult* pResult);
bool xrtValueInvoke(const xvalue* pCallable,
	const xrtcallframe* pFrame, xrtcallresult* pResult);



static bool __Observed = false;



/* 通过 XRT callable 调用语言闭包，并检查 Native 可见的原因链。 */
void xcall_error_bridge_invoke(xvalue* pCallback)
{
	xrtcallframe Frame;
	xrtcallresult Result;
	const xerror* pError;

	memset(&Frame, 0, sizeof(Frame));
	xrtCallResultInit(&Result);
	if ( !xrtValueInvoke(pCallback, &Frame, &Result) ) {
		pError = xrtGetError();
		while ( (pError != NULL) && (xrtErrorCause(pError) != NULL) ) {
			pError = xrtErrorCause(pError);
		}
		__Observed = (pError != NULL) &&
			(strcmp(xrtErrorDomain(pError), "xlang") == 0) &&
			(strcmp(xrtErrorOperation(pError), "callback") == 0) &&
			(strcmp(xrtErrorMessage(pError), "xcall bridge boom") == 0) &&
			(strcmp(xrtErrorFile(pError), "") == 0) &&
			(xrtErrorLine(pError) > 0) &&
			(xrtErrorColumn(pError) > 0);
	}
	xrtCallResultUnit(&Result);
	xrtClearError();
}



/* 返回 Native 侧是否取得了原始语言异常。 */
bool xcall_error_bridge_observed(void)
{
	return __Observed;
}
