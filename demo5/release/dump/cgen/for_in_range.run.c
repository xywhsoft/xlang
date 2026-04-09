#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int xl_generated_had_exception = 0;
int xl_generated_exception_kind = 0;
const char* xl_generated_exception_text = NULL;
long long xl_generated_exception_int = 0;
double xl_generated_exception_float = 0.0;
char xl_generated_async_trace[8192];
int xl_generated_async_trace_len = 0;

static void xl_generated_async_trace_append(const char* sText)
{
	size_t iLen;
	if ( sText == NULL ) {
		return;
	}
	iLen = strlen(sText);
	if ( (xl_generated_async_trace_len + (int)iLen) >= (int)(sizeof(xl_generated_async_trace) - 1) ) {
		iLen = (size_t)((int)(sizeof(xl_generated_async_trace) - 1) - xl_generated_async_trace_len);
	}
	if ( iLen == 0 ) {
		return;
	}
	memcpy(xl_generated_async_trace + xl_generated_async_trace_len, sText, iLen);
	xl_generated_async_trace_len += (int)iLen;
	xl_generated_async_trace[xl_generated_async_trace_len] = '\0';
}

static void xl_generated_async_trace_event(const char* sEvent, const char* sFunc)
{
	char sLine[256];
	if ( sEvent == NULL ) {
		sEvent = "event";
	}
	if ( sFunc == NULL ) {
		sFunc = "";
	}
	snprintf(sLine, sizeof(sLine), "async.%s=%s\n", sEvent, sFunc);
	xl_generated_async_trace_append(sLine);
}

static void xl_generated_runtime_reset(void)
{
	xl_generated_had_exception = 0;
	xl_generated_exception_kind = 0;
	xl_generated_exception_text = NULL;
	xl_generated_exception_int = 0;
	xl_generated_exception_float = 0.0;
	xl_generated_async_trace_len = 0;
	xl_generated_async_trace[0] = '\0';
}

static void xl_generated_raise_int(long long value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 1;
	xl_generated_exception_int = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_float(double value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 2;
	xl_generated_exception_float = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_string(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_text = value;
}

static void xl_generated_raise_text(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_text = value;
}

typedef struct XL_FutureInt {
	int value;
} XL_FutureInt;

static XL_FutureInt xl_future_int_ready(int value)
{
	XL_FutureInt future;
	future.value = value;
	return future;
}

static int xl_future_int_await(const char* sName, XL_FutureInt future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FutureFloat {
	double value;
} XL_FutureFloat;

static XL_FutureFloat xl_future_float_ready(double value)
{
	XL_FutureFloat future;
	future.value = value;
	return future;
}

static double xl_future_float_await(const char* sName, XL_FutureFloat future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FutureString {
	const char* value;
} XL_FutureString;

static XL_FutureString xl_future_string_ready(const char* value)
{
	XL_FutureString future;
	future.value = value;
	return future;
}

static const char* xl_future_string_await(const char* sName, XL_FutureString future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FuturePtr {
	void* value;
} XL_FuturePtr;

static XL_FuturePtr xl_future_ptr_ready(void* value)
{
	XL_FuturePtr future;
	future.value = value;
	return future;
}

static void* xl_future_ptr_await(const char* sName, XL_FuturePtr future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FutureVoid {
	int unused;
} XL_FutureVoid;

static XL_FutureVoid xl_future_void_ready(void)
{
	XL_FutureVoid future;
	future.unused = 0;
	return future;
}

static void xl_future_void_await(const char* sName, XL_FutureVoid future)
{
	(void)future;
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
}

static int main()
{
	{
		int xl_for_start_2_2 = 0;
		int xl_for_stop_2_2 = 0;
		int xl_for_step_2_2 = 1;
		xl_for_start_2_2 = 2;
		xl_for_stop_2_2 = 10;
		xl_for_step_2_2 = 2;
		if ( xl_for_step_2_2 != 0 )
		for ( int xl_for_index_2_2 = 0, xl_for_value_2_2 = xl_for_start_2_2; (xl_for_step_2_2 > 0) ? (xl_for_value_2_2 < xl_for_stop_2_2) : (xl_for_value_2_2 > xl_for_stop_2_2); xl_for_value_2_2 += xl_for_step_2_2, ++xl_for_index_2_2 )
		{
			int index = xl_for_index_2_2;
			int value = xl_for_value_2_2;
			if ( (index == 1) )
			{
				continue;
			}
			if ( (value == 6) )
			{
				return value;
			}
		}
	}
	return (-1);
}

int xl_generated_entry(void)
{
	xl_generated_runtime_reset();
	return main();
}
