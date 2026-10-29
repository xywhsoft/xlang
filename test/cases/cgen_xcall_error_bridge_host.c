#include "../../build/test/__xl_runtime/xruntime_decl.h"
#include <stdio.h>
#include <string.h>



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
		if ( pError == NULL ) {
			fprintf(stderr, "xcall bridge returned false without a diagnostic\n");
		}
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
		if ( !__Observed && pError != NULL ) {
			fprintf(stderr,
				"xcall bridge diagnostic mismatch: domain=%s operation=%s message=%s file=%s line=%d column=%d\n",
				xrtErrorDomain(pError), xrtErrorOperation(pError),
				xrtErrorMessage(pError), xrtErrorFile(pError),
				(int)xrtErrorLine(pError), (int)xrtErrorColumn(pError));
		}
	}
	xrtCallResultUnit(&Result);
	xrtClearError();
}



/* 返回 Native 侧是否取得了原始语言异常。 */
bool xcall_error_bridge_observed(void)
{
	return __Observed;
}
