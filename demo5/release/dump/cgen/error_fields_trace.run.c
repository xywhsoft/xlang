#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "xrt.h"
#include "xlrt_generated.h"

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

static void xl_generated_task_trace_event(const char* sEvent, const char* sName)
{
	char sLine[256];
	if ( sEvent == NULL ) {
		sEvent = "event";
	}
	if ( sName == NULL ) {
		sName = "";
	}
	snprintf(sLine, sizeof(sLine), "task.%s=%s\n", sEvent, sName);
	xl_generated_async_trace_append(sLine);
}

static void xl_generated_generator_trace_event(const char* sEvent, const char* sName)
{
	char sLine[256];
	if ( sEvent == NULL ) {
		sEvent = "event";
	}
	if ( sName == NULL ) {
		sName = "";
	}
	snprintf(sLine, sizeof(sLine), "generator.%s=%s\n", sEvent, sName);
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

static void xl_generated_clear_exception(void)
{
	xl_generated_had_exception = 0;
	xl_generated_exception_kind = 0;
	xl_generated_exception_text = NULL;
	xl_generated_exception_int = 0;
	xl_generated_exception_float = 0.0;
}

static void xl_generated_restore_exception(int kind, const char* text, long long intValue, double floatValue)
{
	xl_generated_had_exception = (kind != 0);
	xl_generated_exception_kind = kind;
	xl_generated_exception_text = text;
	xl_generated_exception_int = intValue;
	xl_generated_exception_float = floatValue;
}

static const char* xl_generated_exception_value_text(int kind, const char* text, long long intValue, double floatValue)
{
	static char sBuffer[128];
	if ( kind == 3 ) {
		return (text != NULL) ? text : "";
	}
	if ( kind == 1 ) {
		snprintf(sBuffer, sizeof(sBuffer), "%lld", intValue);
		return sBuffer;
	}
	if ( kind == 2 ) {
		snprintf(sBuffer, sizeof(sBuffer), "%g", floatValue);
		return sBuffer;
	}
	return "";
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

static const char* xl_generated_error_make(const char* kind, const char* message)
{
	static char sSlots[8][256];
	static int iNext = 0;
	char* sOut = sSlots[iNext++ & 7];
	if ( kind == NULL ) {
		kind = "Error";
	}
	if ( message == NULL ) {
		message = "";
	}
	if ( strcmp(kind, "Error") == 0 ) {
		snprintf(sOut, 256, "%s", message);
	} else if ( message[0] == '\0' ) {
		snprintf(sOut, 256, "%s", kind);
	} else {
		snprintf(sOut, 256, "%s: %s", kind, message);
	}
	return sOut;
}

static const char* Error(const char* message)
{
	return xl_generated_error_make("Error", message);
}

static const char* TypeError(const char* message)
{
	return xl_generated_error_make("TypeError", message);
}

static const char* ValueError(const char* message)
{
	return xl_generated_error_make("ValueError", message);
}

static const char* IOError(const char* message)
{
	return xl_generated_error_make("IOError", message);
}

static const char* TimeoutError(const char* message)
{
	return xl_generated_error_make("TimeoutError", message);
}

static const char* CancelledError(const char* message)
{
	return xl_generated_error_make("CancelledError", message);
}

static const char* NotImplementedError(const char* message)
{
	return xl_generated_error_make("NotImplementedError", message);
}

static const char* NativeError(const char* message)
{
	return xl_generated_error_make("NativeError", message);
}

static const char* xl_generated_error_type(const char* value)
{
	static char sType[64];
	const char* sSep;
	size_t iLen;
	if ( value == NULL ) {
		return "Error";
	}
	sSep = strstr(value, ": ");
	if ( sSep == NULL ) {
		return "Error";
	}
	iLen = (size_t)(sSep - value);
	if ( iLen >= sizeof(sType) ) {
		iLen = sizeof(sType) - 1;
	}
	memcpy(sType, value, iLen);
	sType[iLen] = '\0';
	return sType;
}

static const char* xl_generated_error_message(const char* value)
{
	const char* sSep;
	if ( value == NULL ) {
		return "";
	}
	sSep = strstr(value, ": ");
	return (sSep != NULL) ? (sSep + 2) : value;
}

static int xl_generated_error_code(const char* value)
{
	(void)value;
	return 0;
}

static const char* xl_generated_error_data(const char* value)
{
	(void)value;
	return "";
}

static const char* xl_generated_error_stack(const char* value)
{
	(void)value;
	return "";
}

static const char* xl_generated_error_cause(const char* value)
{
	(void)value;
	return NULL;
}

static int xl_generated_text_equal(const char* left, const char* right)
{
	if ( left == NULL ) {
		left = "";
	}
	if ( right == NULL ) {
		right = "";
	}
	return strcmp(left, right) == 0;
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

typedef struct XL_GeneratorInt {
	int count;
	int index;
	int items[256];
} XL_GeneratorInt;

static void xl_generator_int_init(XL_GeneratorInt* pGen)
{
	if ( pGen == NULL ) {
		return;
	}
	pGen->count = 0;
	pGen->index = 0;
}

static void xl_generator_int_yield(const char* sName, XL_GeneratorInt* pGen, int value)
{
	if ( pGen == NULL ) {
		return;
	}
	if ( pGen->count < 256 ) {
		pGen->items[pGen->count++] = value;
	}
	xl_generated_generator_trace_event("yield", (sName != NULL) ? sName : "");
}

static void xl_generator_int_done(const char* sName, XL_GeneratorInt* pGen)
{
	(void)pGen;
	xl_generated_generator_trace_event("done", (sName != NULL) ? sName : "");
}

static int xl_generator_int_next(const char* sName, XL_GeneratorInt* pGen, int* pValue)
{
	if ( (pGen == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( pGen->index >= pGen->count ) {
		xl_generated_generator_trace_event("eof", (sName != NULL) ? sName : "");
		return 0;
	}
	*pValue = pGen->items[pGen->index++];
	xl_generated_generator_trace_event("next", (sName != NULL) ? sName : "");
	return 1;
}

typedef struct XL_GeneratorFloat {
	int count;
	int index;
	double items[256];
} XL_GeneratorFloat;

static void xl_generator_float_init(XL_GeneratorFloat* pGen)
{
	if ( pGen == NULL ) {
		return;
	}
	pGen->count = 0;
	pGen->index = 0;
}

static void xl_generator_float_yield(const char* sName, XL_GeneratorFloat* pGen, double value)
{
	if ( pGen == NULL ) {
		return;
	}
	if ( pGen->count < 256 ) {
		pGen->items[pGen->count++] = value;
	}
	xl_generated_generator_trace_event("yield", (sName != NULL) ? sName : "");
}

static void xl_generator_float_done(const char* sName, XL_GeneratorFloat* pGen)
{
	(void)pGen;
	xl_generated_generator_trace_event("done", (sName != NULL) ? sName : "");
}

static int xl_generator_float_next(const char* sName, XL_GeneratorFloat* pGen, double* pValue)
{
	if ( (pGen == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( pGen->index >= pGen->count ) {
		xl_generated_generator_trace_event("eof", (sName != NULL) ? sName : "");
		return 0;
	}
	*pValue = pGen->items[pGen->index++];
	xl_generated_generator_trace_event("next", (sName != NULL) ? sName : "");
	return 1;
}

typedef struct XL_GeneratorString {
	int count;
	int index;
	const char* items[256];
} XL_GeneratorString;

static void xl_generator_string_init(XL_GeneratorString* pGen)
{
	if ( pGen == NULL ) {
		return;
	}
	pGen->count = 0;
	pGen->index = 0;
}

static void xl_generator_string_yield(const char* sName, XL_GeneratorString* pGen, const char* value)
{
	if ( pGen == NULL ) {
		return;
	}
	if ( pGen->count < 256 ) {
		pGen->items[pGen->count++] = value;
	}
	xl_generated_generator_trace_event("yield", (sName != NULL) ? sName : "");
}

static void xl_generator_string_done(const char* sName, XL_GeneratorString* pGen)
{
	(void)pGen;
	xl_generated_generator_trace_event("done", (sName != NULL) ? sName : "");
}

static int xl_generator_string_next(const char* sName, XL_GeneratorString* pGen, const char** pValue)
{
	if ( (pGen == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( pGen->index >= pGen->count ) {
		xl_generated_generator_trace_event("eof", (sName != NULL) ? sName : "");
		return 0;
	}
	*pValue = pGen->items[pGen->index++];
	xl_generated_generator_trace_event("next", (sName != NULL) ? sName : "");
	return 1;
}

typedef struct XL_GeneratorBool {
	int count;
	int index;
	int items[256];
} XL_GeneratorBool;

static void xl_generator_bool_init(XL_GeneratorBool* pGen)
{
	if ( pGen == NULL ) {
		return;
	}
	pGen->count = 0;
	pGen->index = 0;
}

static void xl_generator_bool_yield(const char* sName, XL_GeneratorBool* pGen, int value)
{
	if ( pGen == NULL ) {
		return;
	}
	if ( pGen->count < 256 ) {
		pGen->items[pGen->count++] = value;
	}
	xl_generated_generator_trace_event("yield", (sName != NULL) ? sName : "");
}

static void xl_generator_bool_done(const char* sName, XL_GeneratorBool* pGen)
{
	(void)pGen;
	xl_generated_generator_trace_event("done", (sName != NULL) ? sName : "");
}

static int xl_generator_bool_next(const char* sName, XL_GeneratorBool* pGen, int* pValue)
{
	if ( (pGen == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( pGen->index >= pGen->count ) {
		xl_generated_generator_trace_event("eof", (sName != NULL) ? sName : "");
		return 0;
	}
	*pValue = pGen->items[pGen->index++];
	xl_generated_generator_trace_event("next", (sName != NULL) ? sName : "");
	return 1;
}

typedef struct XL_GeneratorPtr {
	int count;
	int index;
	void* items[256];
} XL_GeneratorPtr;

static void xl_generator_ptr_init(XL_GeneratorPtr* pGen)
{
	if ( pGen == NULL ) {
		return;
	}
	pGen->count = 0;
	pGen->index = 0;
}

static void xl_generator_ptr_yield(const char* sName, XL_GeneratorPtr* pGen, void* value)
{
	if ( pGen == NULL ) {
		return;
	}
	if ( pGen->count < 256 ) {
		pGen->items[pGen->count++] = value;
	}
	xl_generated_generator_trace_event("yield", (sName != NULL) ? sName : "");
}

static void xl_generator_ptr_done(const char* sName, XL_GeneratorPtr* pGen)
{
	(void)pGen;
	xl_generated_generator_trace_event("done", (sName != NULL) ? sName : "");
}

static int xl_generator_ptr_next(const char* sName, XL_GeneratorPtr* pGen, void** pValue)
{
	if ( (pGen == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( pGen->index >= pGen->count ) {
		xl_generated_generator_trace_event("eof", (sName != NULL) ? sName : "");
		return 0;
	}
	*pValue = pGen->items[pGen->index++];
	xl_generated_generator_trace_event("next", (sName != NULL) ? sName : "");
	return 1;
}

static int xl_task_spawn_int(const char* sName, int value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static int xl_task_all_int(const char* sName, int left, int right)
{
	(void)left;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	return right;
}

static int xl_task_race_int(const char* sName, int left, int right)
{
	(void)right;
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	return left;
}

static int xl_task_cancel_int(const char* sName, int value)
{
	(void)value;
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	return 1;
}

static int xl_task_default_int(void)
{
	return 0;
}

static double xl_task_spawn_float(const char* sName, double value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static double xl_task_all_float(const char* sName, double left, double right)
{
	(void)left;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	return right;
}

static double xl_task_race_float(const char* sName, double left, double right)
{
	(void)right;
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	return left;
}

static int xl_task_cancel_float(const char* sName, double value)
{
	(void)value;
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	return 1;
}

static double xl_task_default_float(void)
{
	return 0.0;
}

static const char* xl_task_spawn_string(const char* sName, const char* value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static const char* xl_task_all_string(const char* sName, const char* left, const char* right)
{
	(void)left;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	return right;
}

static const char* xl_task_race_string(const char* sName, const char* left, const char* right)
{
	(void)right;
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	return left;
}

static int xl_task_cancel_string(const char* sName, const char* value)
{
	(void)value;
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	return 1;
}

static const char* xl_task_default_string(void)
{
	return NULL;
}

static int xl_task_spawn_bool(const char* sName, int value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static int xl_task_all_bool(const char* sName, int left, int right)
{
	(void)left;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	return right;
}

static int xl_task_race_bool(const char* sName, int left, int right)
{
	(void)right;
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	return left;
}

static int xl_task_cancel_bool(const char* sName, int value)
{
	(void)value;
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	return 1;
}

static int xl_task_default_bool(void)
{
	return 0;
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

static int xl_mod_std_core__identity(int x)
{
	return x;
}

int xl_mod_std_core__version = 5;
static int 入口()
{
	{
		int xl_try_has_exc_5_2 = 0;
		int xl_try_rethrow_5_2 = 0;
		int xl_try_phase_5_2 = 0;
		int xl_try_kind_5_2 = 0;
		const char* xl_try_text_5_2 = NULL;
		long long xl_try_int_5_2 = 0;
		double xl_try_float_5_2 = 0.0;
		const char* e = NULL;
		/* try */
		{
			xl_generated_raise_string((const char*)(IOError("disk")));
			goto xl_try_exit_5_2;
		}
		goto xl_try_check_5_2;
		xl_try_exit_5_2:
		;
		xl_try_check_5_2:
		;
		if ( xl_generated_had_exception )
		{
			xl_try_has_exc_5_2 = 1;
			xl_try_kind_5_2 = xl_generated_exception_kind;
			xl_try_text_5_2 = xl_generated_exception_text;
			xl_try_int_5_2 = xl_generated_exception_int;
			xl_try_float_5_2 = xl_generated_exception_float;
			xl_generated_clear_exception();
			if ( xl_try_phase_5_2 == 0 )
			{
				e = xl_generated_exception_value_text(xl_try_kind_5_2, xl_try_text_5_2, xl_try_int_5_2, xl_try_float_5_2);
				xl_try_phase_5_2 = 1;
				{
					xl_core_println_string("A");
					if ( (!xl_generated_text_equal(xl_generated_error_type(e), "IOError")) )
					{
						xl_core_println_string("B");
						return 1;
					}
					xl_core_println_string("C");
					if ( (!xl_generated_text_equal(xl_generated_error_message(e), "disk")) )
					{
						xl_core_println_string("D");
						return 2;
					}
					xl_core_println_string("E");
					if ( (xl_generated_error_code(e) != 0) )
					{
						xl_core_println_string("F");
						return 3;
					}
					xl_core_println_string("G");
					if ( (xl_str_contains(xl_generated_error_message(e), "disk") == false) )
					{
						xl_core_println_string("H");
						return 4;
					}
					xl_core_println_string("I");
					return 9;
				}
				goto xl_try_end_5_2;
			}
			xl_try_rethrow_5_2 = 1;
		}
		if ( xl_try_rethrow_5_2 && !xl_generated_had_exception )
		{
			xl_generated_restore_exception(xl_try_kind_5_2, xl_try_text_5_2, xl_try_int_5_2, xl_try_float_5_2);
			return 0;
		}
		xl_try_end_5_2:
		;
	}
}

int xl_generated_entry(void)
{
	int xlrt_attached_here = xlrt_attach_generated_runtime();
	int xlrt_result = 0;
	xl_generated_runtime_reset();
	xlrt_result = 入口();
	xlrt_detach_generated_runtime(xlrt_attached_here);
	return xlrt_result;
}
