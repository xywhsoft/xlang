#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "xrt.h"
#include "xlrt_generated.h"

typedef struct XL_GeneratedError XL_GeneratedError;
struct XL_GeneratedError {
	const char* type;
	const char* message;
	int code;
	const char* data;
	const char* stack;
	XL_GeneratedError* cause;
	char text[256];
};

typedef struct XL_GeneratedExceptionFrame {
	int phase;
	int has_exception;
	int needs_rethrow;
	int kind;
	const char* text;
	XL_GeneratedError* error;
	long long int_value;
	double float_value;
} XL_GeneratedExceptionFrame;

static void xl_generated_raise_error(XL_GeneratedError* value);

static XL_GeneratedError xl_generated_error_slots[16];
static int xl_generated_error_next = 0;
static XL_GeneratedExceptionFrame xl_generated_exception_frames[32];
static int xl_generated_exception_frame_depth = 0;
int xl_generated_had_exception = 0;
int xl_generated_exception_kind = 0;
const char* xl_generated_exception_text = NULL;
XL_GeneratedError* xl_generated_exception_error = NULL;
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
	xl_generated_exception_error = NULL;
	xl_generated_exception_int = 0;
	xl_generated_exception_float = 0.0;
	xl_generated_async_trace_len = 0;
	xl_generated_async_trace[0] = '\0';
	xl_generated_exception_frame_depth = 0;
	xlrt_native_error_clear();
}

static void xl_generated_clear_exception(void)
{
	xl_generated_had_exception = 0;
	xl_generated_exception_kind = 0;
	xl_generated_exception_text = NULL;
	xl_generated_exception_error = NULL;
	xl_generated_exception_int = 0;
	xl_generated_exception_float = 0.0;
}

static void xl_generated_restore_exception(int kind, const char* text, XL_GeneratedError* errorValue, long long intValue, double floatValue)
{
	xl_generated_had_exception = (kind != 0);
	xl_generated_exception_kind = kind;
	xl_generated_exception_text = text;
	xl_generated_exception_error = errorValue;
	xl_generated_exception_int = intValue;
	xl_generated_exception_float = floatValue;
}

static XL_GeneratedExceptionFrame* xl_generated_exception_frame_push(void)
{
	int iIndex = xl_generated_exception_frame_depth;
	if ( iIndex >= (int)(sizeof(xl_generated_exception_frames) / sizeof(xl_generated_exception_frames[0])) ) {
		iIndex = (int)(sizeof(xl_generated_exception_frames) / sizeof(xl_generated_exception_frames[0])) - 1;
	} else {
		xl_generated_exception_frame_depth += 1;
	}
	memset(&xl_generated_exception_frames[iIndex], 0, sizeof(xl_generated_exception_frames[iIndex]));
	return &xl_generated_exception_frames[iIndex];
}

static void xl_generated_exception_frame_pop(void)
{
	if ( xl_generated_exception_frame_depth > 0 ) {
		xl_generated_exception_frame_depth -= 1;
	}
}

static int xl_generated_exception_frame_capture(XL_GeneratedExceptionFrame* pFrame)
{
	if ( (pFrame == NULL) || !xl_generated_had_exception ) {
		return 0;
	}
	pFrame->has_exception = 1;
	pFrame->kind = xl_generated_exception_kind;
	pFrame->text = xl_generated_exception_text;
	pFrame->error = xl_generated_exception_error;
	pFrame->int_value = xl_generated_exception_int;
	pFrame->float_value = xl_generated_exception_float;
	xl_generated_clear_exception();
	return 1;
}

static void xl_generated_exception_frame_restore(const XL_GeneratedExceptionFrame* pFrame)
{
	if ( (pFrame == NULL) || !pFrame->has_exception ) {
		return;
	}
	xl_generated_restore_exception(pFrame->kind, pFrame->text, pFrame->error, pFrame->int_value, pFrame->float_value);
}

static XL_GeneratedError* xl_generated_error_alloc(void)
{
	XL_GeneratedError* pErr = &xl_generated_error_slots[xl_generated_error_next++ & 15];
	memset(pErr, 0, sizeof(*pErr));
	return pErr;
}

static const char* xl_generated_error_text(XL_GeneratedError* value)
{
	if ( value == NULL ) {
		return "";
	}
	if ( value->type == NULL ) {
		value->type = "Error";
	}
	if ( value->message == NULL ) {
		value->message = "";
	}
	if ( (strcmp(value->type, "Error") == 0) || (value->type[0] == '\0') ) {
		snprintf(value->text, sizeof(value->text), "%s", value->message);
	} else if ( value->message[0] == '\0' ) {
		snprintf(value->text, sizeof(value->text), "%s", value->type);
	} else {
		snprintf(value->text, sizeof(value->text), "%s: %s", value->type, value->message);
	}
	return value->text;
}

static XL_GeneratedError* xl_generated_error_make_ex(const char* kind, const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	XL_GeneratedError* pErr = xl_generated_error_alloc();
	pErr->type = (kind != NULL) ? kind : "Error";
	pErr->message = (message != NULL) ? message : "";
	pErr->code = code;
	pErr->data = (data != NULL) ? data : "";
	pErr->stack = (stack != NULL) ? stack : "";
	pErr->cause = cause;
	(void)xl_generated_error_text(pErr);
	return pErr;
}

static XL_GeneratedError* xl_generated_error_make(const char* kind, const char* message, int code, const char* data, const char* stack)
{
	return xl_generated_error_make_ex(kind, message, code, data, stack, NULL);
}

static XL_GeneratedError* xl_generated_error_from_text(const char* text)
{
	const char* sSep;
	char sKind[64];
	size_t iLen;
	if ( text == NULL ) {
		return xl_generated_error_make("Error", "", 0, "", "");
	}
	sSep = strstr(text, ": ");
	if ( sSep == NULL ) {
		return xl_generated_error_make("Error", text, 0, "", "");
	}
	iLen = (size_t)(sSep - text);
	if ( iLen >= sizeof(sKind) ) {
		iLen = sizeof(sKind) - 1;
	}
	memcpy(sKind, text, iLen);
	sKind[iLen] = '\0';
	return xl_generated_error_make(sKind, sSep + 2, 0, "", "");
}

static const char* xl_generated_exception_value_text(int kind, const char* text, long long intValue, double floatValue)
{
	static char sBuffer[128];
	if ( kind == 3 ) {
		return (text != NULL) ? text : "";
	}
	if ( kind == 4 ) {
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

static XL_GeneratedError* xl_generated_exception_value_error(int kind, const char* text, XL_GeneratedError* errorValue, long long intValue, double floatValue)
{
	char sBuffer[128];
	if ( (kind == 4) && (errorValue != NULL) ) {
		return errorValue;
	}
	if ( kind == 3 ) {
		return xl_generated_error_from_text(text);
	}
	if ( kind == 1 ) {
		snprintf(sBuffer, sizeof(sBuffer), "%lld", intValue);
		return xl_generated_error_make("Error", sBuffer, 0, "", "");
	}
	if ( kind == 2 ) {
		snprintf(sBuffer, sizeof(sBuffer), "%g", floatValue);
		return xl_generated_error_make("Error", sBuffer, 0, "", "");
	}
	return xl_generated_error_make("Error", (text != NULL) ? text : "", 0, "", "");
}

static XL_GeneratedError* xl_generated_capture_exception_error(void)
{
	XL_GeneratedError* value = xl_generated_exception_value_error(
		xl_generated_exception_kind,
		xl_generated_exception_text,
		xl_generated_exception_error,
		xl_generated_exception_int,
		xl_generated_exception_float
	);
	xl_generated_clear_exception();
	return value;
}

static const char* xl_generated_copy_text(const char* sText)
{
	const char* sValue = (sText != NULL) ? sText : "";
	const char* sCopy = (const char*)xrtCopyStr((str)sValue, strlen(sValue));
	return (sCopy != NULL) ? sCopy : sValue;
}

static int xl_generated_capture_native_error(void)
{
	if ( !xlrt_native_error_has() ) {
		return 0;
	}
	const char* sKind = xl_generated_copy_text(xlrt_native_error_kind());
	const char* sMessage = xl_generated_copy_text(xlrt_native_error_message());
	const char* sData = xl_generated_copy_text(xlrt_native_error_data());
	xl_generated_raise_error(xl_generated_error_make_ex(
		sKind,
		sMessage,
		xlrt_native_error_code(),
		sData,
		"",
		NULL
	));
	xlrt_native_error_clear();
	return 1;
}

static void xl_generated_raise_int(long long value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 1;
	xl_generated_exception_error = NULL;
	xl_generated_exception_int = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_float(double value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 2;
	xl_generated_exception_error = NULL;
	xl_generated_exception_float = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_string(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_error = NULL;
	xl_generated_exception_text = value;
}

static void xl_generated_raise_text(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_error = NULL;
	xl_generated_exception_text = value;
}

static void xl_generated_raise_error(XL_GeneratedError* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 4;
	xl_generated_exception_error = value;
	xl_generated_exception_text = xl_generated_error_text(value);
	xl_generated_exception_int = 0;
	xl_generated_exception_float = 0.0;
}

static XL_GeneratedError* Error(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("Error", message, code, data, stack, cause);
}

static XL_GeneratedError* TypeError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("TypeError", message, code, data, stack, cause);
}

static XL_GeneratedError* ValueError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("ValueError", message, code, data, stack, cause);
}

static XL_GeneratedError* IOError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("IOError", message, code, data, stack, cause);
}

static XL_GeneratedError* TimeoutError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("TimeoutError", message, code, data, stack, cause);
}

static XL_GeneratedError* CancelledError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("CancelledError", message, code, data, stack, cause);
}

static XL_GeneratedError* NotImplementedError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("NotImplementedError", message, code, data, stack, cause);
}

static XL_GeneratedError* NativeError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)
{
	return xl_generated_error_make_ex("NativeError", message, code, data, stack, cause);
}

static const char* xl_generated_error_type(XL_GeneratedError* value)
{
	return (value != NULL && value->type != NULL) ? value->type : "Error";
}

static const char* xl_generated_error_message(XL_GeneratedError* value)
{
	return (value != NULL && value->message != NULL) ? value->message : "";
}

static int xl_generated_error_code(XL_GeneratedError* value)
{
	return (value != NULL) ? value->code : 0;
}

static const char* xl_generated_error_data(XL_GeneratedError* value)
{
	return (value != NULL && value->data != NULL) ? value->data : "";
}

static const char* xl_generated_error_stack(XL_GeneratedError* value)
{
	return (value != NULL && value->stack != NULL) ? value->stack : "";
}

static XL_GeneratedError* xl_generated_error_cause(XL_GeneratedError* value)
{
	return (value != NULL) ? value->cause : NULL;
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

static int xl_generated_number_approx(double left, double right)
{
	double diff = fabs(left - right);
	double scale = fmax(fabs(left), fabs(right));
	if ( scale < 1.0 ) {
		scale = 1.0;
	}
	return diff <= (0.000001 * scale);
}

static int xl_generated_time_approx(xtime left, xtime right)
{
	xtime diff = left - right;
	if ( diff < 0 ) {
		diff = -diff;
	}
	return diff <= 1;
}

static int xl_generated_text_has_wildcard(const char* value)
{
	size_t i;
	if ( value == NULL ) {
		return 0;
	}
	for ( i = 0; value[i] != '\0'; ++i ) {
		if ( (value[i] == '*') || (value[i] == '?') ) {
			return 1;
		}
	}
	return 0;
}

static int xl_generated_text_pattern_match(const char* value, const char* pattern)
{
	if ( value == NULL ) {
		value = "";
	}
	if ( pattern == NULL ) {
		pattern = "";
	}
	while ( *pattern != '\0' ) {
		if ( *pattern == '*' ) {
			while ( *pattern == '*' ) {
				++pattern;
			}
			if ( *pattern == '\0' ) {
				return 1;
			}
			while ( *value != '\0' ) {
				if ( xl_generated_text_pattern_match(value, pattern) ) {
					return 1;
				}
				++value;
			}
			return 0;
		}
		if ( *value == '\0' ) {
			return 0;
		}
		if ( (*pattern != '?') && (*pattern != *value) ) {
			return 0;
		}
		++pattern;
		++value;
	}
	return *value == '\0';
}

static int xl_generated_text_approx(const char* left, const char* right)
{
	if ( left == NULL ) {
		left = "";
	}
	if ( right == NULL ) {
		right = "";
	}
	if ( xl_generated_text_pattern_match(left, right) ) {
		return 1;
	}
	if ( xl_generated_text_has_wildcard(left) ) {
		return xl_generated_text_pattern_match(right, left);
	}
	return 0;
}

static const char* xl_generated_null_assign_text(const char** pTarget, const char* value)
{
	if ( (pTarget != NULL) && (*pTarget == NULL) ) {
		*pTarget = value;
	}
	return (pTarget != NULL) ? *pTarget : value;
}

static XL_GeneratedError* xl_generated_null_assign_error(XL_GeneratedError** pTarget, XL_GeneratedError* value)
{
	if ( (pTarget != NULL) && (*pTarget == NULL) ) {
		*pTarget = value;
	}
	return (pTarget != NULL) ? *pTarget : value;
}

static int xl_generated_power_int(int base, int exponent)
{
	int result = 1;
	int factor = base;
	int power = exponent;
	if ( power < 0 ) {
		if ( base == 1 ) {
			return 1;
		}
		if ( base == -1 ) {
			return ((power & 1) == 0) ? 1 : -1;
		}
		return 0;
	}
	while ( power > 0 ) {
		if ( (power & 1) != 0 ) {
			result *= factor;
		}
		power >>= 1;
		if ( power > 0 ) {
			factor *= factor;
		}
	}
	return result;
}

static double xl_generated_power_float(double base, double exponent)
{
	return pow(base, exponent);
}

static int xl_generated_power_assign_int(int* pTarget, int exponent)
{
	if ( pTarget == NULL ) {
		return 0;
	}
	*pTarget = xl_generated_power_int(*pTarget, exponent);
	return *pTarget;
}

static double xl_generated_power_assign_float(double* pTarget, double exponent)
{
	if ( pTarget == NULL ) {
		return 0.0;
	}
	*pTarget = xl_generated_power_float(*pTarget, exponent);
	return *pTarget;
}

typedef struct XL_FutureInt {
	int status;
	int value;
	XL_GeneratedError* error;
} XL_FutureInt;

static XL_FutureInt xl_future_int_ready(int value)
{
	XL_FutureInt future;
	future.status = 0;
	future.value = value;
	future.error = NULL;
	return future;
}

static XL_FutureInt xl_future_int_error(XL_GeneratedError* error)
{
	XL_FutureInt future;
	future.status = 1;
	future.value = 0;
	future.error = error;
	return future;
}

static int xl_future_int_await(const char* sName, XL_FutureInt future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( future.status != 0 ) {
		xl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make("Error", "await failed", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return future.value;
	}
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FutureBool {
	int status;
	int value;
	XL_GeneratedError* error;
} XL_FutureBool;

static XL_FutureBool xl_future_bool_ready(int value)
{
	XL_FutureBool future;
	future.status = 0;
	future.value = value;
	future.error = NULL;
	return future;
}

static XL_FutureBool xl_future_bool_error(XL_GeneratedError* error)
{
	XL_FutureBool future;
	future.status = 1;
	future.value = 0;
	future.error = error;
	return future;
}

static int xl_future_bool_await(const char* sName, XL_FutureBool future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( future.status != 0 ) {
		xl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make("Error", "await failed", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return future.value;
	}
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FutureFloat {
	int status;
	double value;
	XL_GeneratedError* error;
} XL_FutureFloat;

static XL_FutureFloat xl_future_float_ready(double value)
{
	XL_FutureFloat future;
	future.status = 0;
	future.value = value;
	future.error = NULL;
	return future;
}

static XL_FutureFloat xl_future_float_error(XL_GeneratedError* error)
{
	XL_FutureFloat future;
	future.status = 1;
	future.value = 0.0;
	future.error = error;
	return future;
}

static double xl_future_float_await(const char* sName, XL_FutureFloat future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( future.status != 0 ) {
		xl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make("Error", "await failed", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return future.value;
	}
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FutureString {
	int status;
	const char* value;
	XL_GeneratedError* error;
} XL_FutureString;

static XL_FutureString xl_future_string_ready(const char* value)
{
	XL_FutureString future;
	future.status = 0;
	future.value = value;
	future.error = NULL;
	return future;
}

static XL_FutureString xl_future_string_error(XL_GeneratedError* error)
{
	XL_FutureString future;
	future.status = 1;
	future.value = NULL;
	future.error = error;
	return future;
}

static const char* xl_future_string_await(const char* sName, XL_FutureString future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( future.status != 0 ) {
		xl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make("Error", "await failed", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return future.value;
	}
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FutureError {
	int status;
	XL_GeneratedError* value;
	XL_GeneratedError* error;
} XL_FutureError;

static XL_FutureError xl_future_error_ready(XL_GeneratedError* value)
{
	XL_FutureError future;
	future.status = 0;
	future.value = value;
	future.error = NULL;
	return future;
}

static XL_FutureError xl_future_error_error(XL_GeneratedError* error)
{
	XL_FutureError future;
	future.status = 1;
	future.value = NULL;
	future.error = error;
	return future;
}

static XL_GeneratedError* xl_future_error_await(const char* sName, XL_FutureError future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( future.status != 0 ) {
		xl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make("Error", "await failed", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return future.value;
	}
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return future.value;
}

typedef struct XL_FuturePtr {
	int status;
	void* value;
	XL_GeneratedError* error;
} XL_FuturePtr;

static XL_FuturePtr xl_future_ptr_ready(void* value)
{
	XL_FuturePtr future;
	future.status = 0;
	future.value = value;
	future.error = NULL;
	return future;
}

static XL_FuturePtr xl_future_ptr_error(XL_GeneratedError* error)
{
	XL_FuturePtr future;
	future.status = 1;
	future.value = NULL;
	future.error = error;
	return future;
}

static void* xl_future_ptr_await(const char* sName, XL_FuturePtr future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( future.status != 0 ) {
		xl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make("Error", "await failed", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return future.value;
	}
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

typedef struct XL_GeneratorError {
	int count;
	int index;
	XL_GeneratedError* items[256];
} XL_GeneratorError;

static void xl_generator_error_init(XL_GeneratorError* pGen)
{
	if ( pGen == NULL ) {
		return;
	}
	pGen->count = 0;
	pGen->index = 0;
}

static void xl_generator_error_yield(const char* sName, XL_GeneratorError* pGen, XL_GeneratedError* value)
{
	if ( pGen == NULL ) {
		return;
	}
	if ( pGen->count < 256 ) {
		pGen->items[pGen->count++] = value;
	}
	xl_generated_generator_trace_event("yield", (sName != NULL) ? sName : "");
}

static void xl_generator_error_done(const char* sName, XL_GeneratorError* pGen)
{
	(void)pGen;
	xl_generated_generator_trace_event("done", (sName != NULL) ? sName : "");
}

static int xl_generator_error_next(const char* sName, XL_GeneratorError* pGen, XL_GeneratedError** pValue)
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

static XL_GeneratedError* xl_task_spawn_error(const char* sName, XL_GeneratedError* value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static XL_GeneratedError* xl_task_all_error(const char* sName, XL_GeneratedError* left, XL_GeneratedError* right)
{
	(void)left;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	return right;
}

static XL_GeneratedError* xl_task_race_error(const char* sName, XL_GeneratedError* left, XL_GeneratedError* right)
{
	(void)right;
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	return left;
}

static int xl_task_cancel_error(const char* sName, XL_GeneratedError* value)
{
	(void)value;
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	return 1;
}

static XL_GeneratedError* xl_task_default_error(void)
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

static int xl_task_delay(const char* sName, int ms)
{
	(void)ms;
	xl_generated_task_trace_event("delay", (sName != NULL) ? sName : "");
	return ms;
}

typedef struct XL_FutureVoid {
	int status;
	XL_GeneratedError* error;
	int unused;
} XL_FutureVoid;

static XL_FutureVoid xl_future_void_ready(void)
{
	XL_FutureVoid future;
	future.status = 0;
	future.error = NULL;
	future.unused = 0;
	return future;
}

static XL_FutureVoid xl_future_void_error(XL_GeneratedError* error)
{
	XL_FutureVoid future;
	future.status = 1;
	future.error = error;
	future.unused = 0;
	return future;
}

static void xl_future_void_await(const char* sName, XL_FutureVoid future)
{
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( future.status != 0 ) {
		xl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make("Error", "await failed", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return;
	}
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
}

static xvalue xl__pair__arity1__base__int__ret__array_x3Cvar_x3E(int base)
{
 	xvalue xl_ret_2_2 = ({ xvalue xl_arr_literal = xvoCreateArray(); (void)xvoArrayAppendValue(xl_arr_literal, xvoCreateInt((int64)((base + 2))), TRUE); (void)xvoArrayAppendValue(xl_arr_literal, ({ const char* xl_text_value = "ok"; xvoCreateText((ptr)xl_text_value, (uint32)strlen((xl_text_value != NULL) ? xl_text_value : ""), FALSE); }), TRUE); xl_arr_literal; });
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_2_2;
}

static int xl__main__arity0__ret__int()
{
	xvalue xl_unpack_6_2 = xl__pair__arity1__base__int__ret__array_x3Cvar_x3E(40);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	int number = xvoArrayGetInt((xvalue)(xl_unpack_6_2), (uint32)(0));
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	const char* text = xvoArrayGetText((xvalue)(xl_unpack_6_2), (uint32)(1));
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	int xl_if_cond_7_2 = (!xl_generated_text_equal(text, "ok"));
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	if ( xl_if_cond_7_2 )
	{
 		int xl_ret_8_3 = 1;
		if ( xl_generated_had_exception )
		{
						return 0;
		}
		if ( xl_generated_capture_native_error() )
		{
						return 0;
		}
		return xl_ret_8_3;
	}
 	int xl_ret_10_2 = number;
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	return xl_ret_10_2;
}

int xl_generated_entry(void)
{
	int xlrt_attached_here = xlrt_attach_generated_runtime();
	int xlrt_result = 0;
	xl_generated_runtime_reset();
	xlrt_result = xl__main__arity0__ret__int();
	xlrt_detach_generated_runtime(xlrt_attached_here);
	return xlrt_result;
}
