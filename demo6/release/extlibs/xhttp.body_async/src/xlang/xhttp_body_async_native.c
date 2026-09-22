#include "../../include/xhttp_body_async_package.h"



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_BODY_ASYNC_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_BODY_ASYNC_EXPORT
#endif



/* 在 Reader 返回 AGAIN 后取得一次调用方拥有的可读性 Future。 */
XLANG_XHTTP_BODY_ASYNC_EXPORT xfuture* xhttp_body_async_wait(
	void* pReader
)
{
	return xrtHttpBodyReaderWait((xhttpbodyreader*)pReader);
}
