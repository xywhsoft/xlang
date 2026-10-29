/* 原生绑定的错误槽必须按线程隔离。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_netrc.h"
#include <stdio.h>
#include <string.h>

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
#include "../netrc_native.c"

typedef struct netrc_thread_test {
	xevent* InvalidDone;
	xevent* ValidDone;
} netrc_thread_test;

static int32 invalid_worker(ptr pData)
{
	netrc_thread_test* pTest = (netrc_thread_test*)pData;
	static const char sInvalid[] = "machine";
	xvalue* pResult = xnetrc_parse(sInvalid, sizeof(sInvalid) - 1u,
		true, 0u, 0u, 0u, 0u, 0u);
	bool bOk = xrtValueType(pResult) == XVALUE_NULL &&
		xnetrc_error_code() == XRT_NETRC_ERROR_SYNTAX &&
		xnetrc_error_line() == 1;
	if ( !xrtEventSet(pTest->InvalidDone) ) { return 1; }
	if ( xrtEventWaitFor(pTest->ValidDone, 5000000u) != XWAIT_OK ) { return 1; }
	return bOk && xnetrc_error_code() == XRT_NETRC_ERROR_SYNTAX &&
		xnetrc_error_line() == 1 ? 0 : 1;
}

static int32 valid_worker(ptr pData)
{
	netrc_thread_test* pTest = (netrc_thread_test*)pData;
	static const char sValid[] = "machine host login user\n";
	xvalue* pResult = NULL;
	bool bOk = xrtEventWaitFor(pTest->InvalidDone, 5000000u) == XWAIT_OK;
	if ( bOk ) {
		pResult = xnetrc_parse(sValid, sizeof(sValid) - 1u,
			true, 0u, 0u, 0u, 0u, 0u);
		bOk = xrtValueType(pResult) == XVALUE_OBJECT &&
			xnetrc_error_code() == XRT_NETRC_OK;
	}
	xrtValueRelease(pResult);
	if ( !xrtEventSet(pTest->ValidDone) ) { return 1; }
	return bOk ? 0 : 1;
}

int main(void)
{
	netrc_thread_test Test = { 0 };
	xthread* pInvalid = NULL;
	xthread* pValid = NULL;
	bool bOk = false;
	Test.InvalidDone = xrtEventCreate(true, false);
	Test.ValidDone = xrtEventCreate(true, false);
	if ( Test.InvalidDone == NULL || Test.ValidDone == NULL ) { goto cleanup; }
	pInvalid = xrtThreadCreate(invalid_worker, &Test, 0u);
	pValid = xrtThreadCreate(valid_worker, &Test, 0u);
	if ( pInvalid == NULL || pValid == NULL ) { goto cleanup; }
	bOk = xrtThreadWait(pInvalid) == XWAIT_OK &&
		xrtThreadWait(pValid) == XWAIT_OK &&
		xrtThreadExitCode(pInvalid) == 0 &&
		xrtThreadExitCode(pValid) == 0 &&
		xnetrc_error_code() == XRT_NETRC_OK;
cleanup:
	if ( pInvalid != NULL ) { (void)xrtThreadWait(pInvalid); }
	if ( pValid != NULL ) { (void)xrtThreadWait(pValid); }
	if ( pInvalid != NULL ) { xrtThreadDestroy(pInvalid); }
	if ( pValid != NULL ) { xrtThreadDestroy(pValid); }
	if ( Test.InvalidDone != NULL ) { xrtEventDestroy(Test.InvalidDone); }
	if ( Test.ValidDone != NULL ) { xrtEventDestroy(Test.ValidDone); }
	if ( !bOk ) { fputs("test_native_tls: FAIL\n", stderr); }
	else { puts("test_native_tls: PASS"); }
	return bOk ? 0 : 1;
}
