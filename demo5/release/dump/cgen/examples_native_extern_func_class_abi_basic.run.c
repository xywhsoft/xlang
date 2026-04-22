#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "xrt.h"
#include "xlrt_generated.h"

#ifndef XL_GENERATED_XVO_TYPE_LOCAL
#define XL_GENERATED_XVO_TYPE_LOCAL 1
#define xvoType(value) (((value) == NULL) ? XVO_DT_NULL : ((value)->Type))
#define xvoTypeName(kind) ((str)(((kind) == XVO_DT_NULL) ? "null" : ((kind) == XVO_DT_BOOL) ? "bool" : ((kind) == XVO_DT_INT) ? "int" : ((kind) == XVO_DT_FLOAT) ? "float" : ((kind) == XVO_DT_TEXT) ? "string" : ((kind) == XVO_DT_TIME) ? "time" : ((kind) == XVO_DT_POINT) ? "point" : ((kind) == XVO_DT_FUNC) ? "function" : ((kind) == XVO_DT_ARRAY) ? "array" : ((kind) == XVO_DT_LIST) ? "list" : ((kind) == XVO_DT_COLL) ? "set" : ((kind) == XVO_DT_TABLE) ? "dict" : ((kind) == XVO_DT_CLASS) ? "class" : ((kind) == XVO_DT_CUSTOM) ? "custom" : "unknown"))
#endif

#ifndef XL_EXTERN_VAR
#ifdef _WIN32
#define XL_EXTERN_VAR __declspec(dllimport) extern
#else
#define XL_EXTERN_VAR extern
#endif
#endif

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
	const char* file;
	int line;
	int column;
	long long int_value;
	double float_value;
} XL_GeneratedExceptionFrame;

static void xl_generated_raise_error(XL_GeneratedError* value);
static int xl_generated_capture_native_error(void);

static XL_GeneratedError xl_generated_error_slots[16];
static int xl_generated_error_next = 0;
static XL_GeneratedExceptionFrame xl_generated_exception_frames[32];
static int xl_generated_exception_frame_depth = 0;
int xl_generated_had_exception = 0;
int xl_generated_exception_kind = 0;
const char* xl_generated_exception_text = NULL;
XL_GeneratedError* xl_generated_exception_error = NULL;
const char* xl_generated_exception_file = NULL;
int xl_generated_exception_line = 0;
int xl_generated_exception_column = 0;
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

static void xl_generated_async_trace_state_event(const char* sFunc, int iStateId)
{
	char sEvent[32];
	if ( iStateId < 0 ) {
		return;
	}
	snprintf(sEvent, sizeof(sEvent), "state_%d", iStateId);
	xl_generated_async_trace_event(sEvent, sFunc);
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

typedef struct XL_GeneratedGeneratorExecutor {
	int state;
	int done;
} XL_GeneratedGeneratorExecutor;

static void xl_generated_generator_executor_init(XL_GeneratedGeneratorExecutor* pExec)
{
	if ( pExec == NULL ) {
		return;
	}
	pExec->state = 0;
	pExec->done = 0;
}

static int xl_generated_generator_executor_enter(XL_GeneratedGeneratorExecutor* pExec, const char* sName)
{
	if ( pExec == NULL ) {
		return 0;
	}
	if ( pExec->done ) {
		xl_generated_generator_trace_event("eof", (sName != NULL) ? sName : "");
		return 0;
	}
	return 1;
}

static int xl_generated_generator_executor_can_resume(const XL_GeneratedGeneratorExecutor* pExec, int iStateId)
{
	return (pExec != NULL) && !pExec->done && (pExec->state == iStateId);
}

static void xl_generated_generator_executor_suspend(XL_GeneratedGeneratorExecutor* pExec, int iStateId, const char* sName)
{
	if ( pExec == NULL ) {
		return;
	}
	pExec->state = iStateId;
	xl_generated_generator_trace_event("yield", (sName != NULL) ? sName : "");
}

static int xl_generated_generator_executor_resume(XL_GeneratedGeneratorExecutor* pExec, int iStateId)
{
	if ( !xl_generated_generator_executor_can_resume(pExec, iStateId) ) {
		return 0;
	}
	pExec->state = 0;
	return 1;
}

static void xl_generated_generator_executor_finish(XL_GeneratedGeneratorExecutor* pExec, const char* sName)
{
	if ( pExec == NULL ) {
		return;
	}
	pExec->done = 1;
	pExec->state = -1;
	xl_generated_generator_trace_event("done", (sName != NULL) ? sName : "");
}

typedef void (*XL_GeneratedGeneratorNextRaw)(void);
typedef int (*XL_GeneratedGeneratorNextInt)(const char* sName, void* pState, int* pValue, xvalue xl_resume_value);
typedef int (*XL_GeneratedGeneratorNextFloat)(const char* sName, void* pState, double* pValue, xvalue xl_resume_value);
typedef int (*XL_GeneratedGeneratorNextText)(const char* sName, void* pState, const char** pValue, xvalue xl_resume_value);
typedef int (*XL_GeneratedGeneratorNextError)(const char* sName, void* pState, XL_GeneratedError** pValue, xvalue xl_resume_value);
typedef int (*XL_GeneratedGeneratorNextVar)(const char* sName, void* pState, xvalue* pValue, xvalue xl_resume_value);
enum {
	XL_GENERATED_GENERATOR_KIND_INT = 1,
	XL_GENERATED_GENERATOR_KIND_FLOAT = 2,
	XL_GENERATED_GENERATOR_KIND_STRING = 3,
	XL_GENERATED_GENERATOR_KIND_ERROR = 4,
	XL_GENERATED_GENERATOR_KIND_BOOL = 5,
	XL_GENERATED_GENERATOR_KIND_VAR = 6,
	XL_GENERATED_GENERATOR_KIND_VOID = 7
};
typedef struct XL_GeneratedGeneratorValue {
	XL_GeneratedGeneratorNextRaw pNext;
	void* pState;
	size_t iStateSize;
	int iValueKind;
	const char* sName;
} XL_GeneratedGeneratorValue;

static const char* xl_generated_error_text(XL_GeneratedError* value);

static XL_GeneratedGeneratorValue xl_generated_generator_null(void)
{
	XL_GeneratedGeneratorValue value;
	memset(&value, 0, sizeof(value));
	return value;
}

static XL_GeneratedGeneratorValue xl_generated_generator_make(XL_GeneratedGeneratorNextRaw pNext, size_t iStateSize, const void* pState, int iValueKind, const char* sName)
{
	XL_GeneratedGeneratorValue value = xl_generated_generator_null();
	void* pCopy;
	if ( (pNext == NULL) || (pState == NULL) || (iStateSize == 0u) ) {
		return value;
	}
	pCopy = xrtMalloc(iStateSize);
	if ( pCopy == NULL ) {
		return value;
	}
	memcpy(pCopy, pState, iStateSize);
	value.pNext = pNext;
	value.pState = pCopy;
	value.iStateSize = iStateSize;
	value.iValueKind = iValueKind;
	value.sName = sName;
	return value;
}

static XL_GeneratedGeneratorValue xl_generated_generator_clone(XL_GeneratedGeneratorValue value)
{
	if ( (value.pNext == NULL) || (value.pState == NULL) || (value.iStateSize == 0u) ) {
		return xl_generated_generator_null();
	}
	return xl_generated_generator_make(value.pNext, value.iStateSize, value.pState, value.iValueKind, value.sName);
}

static int xl_generated_generator_step(XL_GeneratedGeneratorValue* pGen, xvalue xl_resume_value, xvalue* pValue)
{
	int ok = 0;
	if ( pValue != NULL ) {
		*pValue = NULL;
	}
	if ( (pGen == NULL) || (pGen->pNext == NULL) || (pGen->pState == NULL) ) {
		return 0;
	}
	switch ( pGen->iValueKind ) {
		case XL_GENERATED_GENERATOR_KIND_FLOAT:
		{
			double value = 0.0;
			ok = ((XL_GeneratedGeneratorNextFloat)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : "generator", pGen->pState, &value, xl_resume_value);
			if ( ok && (pValue != NULL) ) {
				*pValue = xvoCreateFloat(value);
			}
			return ok;
		}
		case XL_GENERATED_GENERATOR_KIND_STRING:
		{
			const char* value = NULL;
			ok = ((XL_GeneratedGeneratorNextText)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : "generator", pGen->pState, &value, xl_resume_value);
			if ( ok && (pValue != NULL) ) {
				const char* sText = (value != NULL) ? value : "";
				*pValue = xvoCreateText((ptr)sText, (uint32)strlen(sText), FALSE);
			}
			return ok;
		}
		case XL_GENERATED_GENERATOR_KIND_ERROR:
		{
			XL_GeneratedError* value = NULL;
			ok = ((XL_GeneratedGeneratorNextError)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : "generator", pGen->pState, &value, xl_resume_value);
			if ( ok && (pValue != NULL) ) {
				const char* sText = xl_generated_error_text(value);
				if ( sText == NULL ) {
					sText = "";
				}
				*pValue = xvoCreateText((ptr)sText, (uint32)strlen(sText), FALSE);
			}
			return ok;
		}
		case XL_GENERATED_GENERATOR_KIND_BOOL:
		{
			int value = 0;
			ok = ((XL_GeneratedGeneratorNextInt)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : "generator", pGen->pState, &value, xl_resume_value);
			if ( ok && (pValue != NULL) ) {
				*pValue = xvoCreateBool(value ? TRUE : FALSE);
			}
			return ok;
		}
		case XL_GENERATED_GENERATOR_KIND_VAR:
		{
			xvalue value = NULL;
			ok = ((XL_GeneratedGeneratorNextVar)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : "generator", pGen->pState, &value, xl_resume_value);
			if ( ok && (pValue != NULL) ) {
				*pValue = (value != NULL) ? xvoCopy(value) : xvoCreateNull();
			}
			return ok;
		}
		case XL_GENERATED_GENERATOR_KIND_VOID:
		{
			void* value = NULL;
			ok = ((XL_GeneratedGeneratorNextText)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : "generator", pGen->pState, (const char**)&value, xl_resume_value);
			if ( ok && (pValue != NULL) ) {
				*pValue = xvoCreateNull();
			}
			return ok;
		}
		case XL_GENERATED_GENERATOR_KIND_INT:
		default:
		{
			int value = 0;
			ok = ((XL_GeneratedGeneratorNextInt)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : "generator", pGen->pState, &value, xl_resume_value);
			if ( ok && (pValue != NULL) ) {
				*pValue = xvoCreateInt((int64)value);
			}
			return ok;
		}
	}
}

static void xl_generated_async_trace_flush_to_env_file(void)
{
	const char* sTraceFile = getenv("XL_GENERATED_ASYNC_TRACE_FILE");
	FILE* pFile;
	if ( (sTraceFile == NULL) || (sTraceFile[0] == '\0') || (xl_generated_async_trace_len <= 0) ) {
		return;
	}
	pFile = fopen(sTraceFile, "wb");
	if ( pFile == NULL ) {
		return;
	}
	(void)fwrite(xl_generated_async_trace, 1u, (size_t)xl_generated_async_trace_len, pFile);
	(void)fclose(pFile);
}

static void xl_generated_runtime_reset(void)
{
	xl_generated_had_exception = 0;
	xl_generated_exception_kind = 0;
	xl_generated_exception_text = NULL;
	xl_generated_exception_error = NULL;
	xl_generated_exception_file = NULL;
	xl_generated_exception_line = 0;
	xl_generated_exception_column = 0;
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
	xl_generated_exception_file = NULL;
	xl_generated_exception_line = 0;
	xl_generated_exception_column = 0;
	xl_generated_exception_int = 0;
	xl_generated_exception_float = 0.0;
}

static void xl_generated_exception_set_source(const char* file, int line, int column)
{
	xl_generated_exception_file = file;
	xl_generated_exception_line = line;
	xl_generated_exception_column = column;
}

static void xl_generated_restore_exception(int kind, const char* text, XL_GeneratedError* errorValue, const char* file, int line, int column, long long intValue, double floatValue)
{
	xl_generated_had_exception = (kind != 0);
	xl_generated_exception_kind = kind;
	xl_generated_exception_text = text;
	xl_generated_exception_error = errorValue;
	xl_generated_exception_file = file;
	xl_generated_exception_line = line;
	xl_generated_exception_column = column;
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
	pFrame->file = xl_generated_exception_file;
	pFrame->line = xl_generated_exception_line;
	pFrame->column = xl_generated_exception_column;
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
	xl_generated_restore_exception(pFrame->kind, pFrame->text, pFrame->error, pFrame->file, pFrame->line, pFrame->column, pFrame->int_value, pFrame->float_value);
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

static xfuture* xl_generated_future_create_ready(ptr pValue, const char* sDebugName)
{
	xfuture* pFuture = xFutureCreate();
	xpromise* pPromise;
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( (sDebugName != NULL) && (sDebugName[0] != '\0') ) {
		(void)xFutureSetDebugName(pFuture, (str)sDebugName);
	}
	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return NULL;
	}
	if ( !xPromiseResolve(pPromise, pValue) ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}
	xPromiseDestroy(pPromise);
	return pFuture;
}

static xfuture* xl_generated_future_create_error(XL_GeneratedError* error, const char* sDebugName)
{
	xfuture* pFuture = xFutureCreate();
	xpromise* pPromise;
	const char* sText;
	str sOwned;
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( (sDebugName != NULL) && (sDebugName[0] != '\0') ) {
		(void)xFutureSetDebugName(pFuture, (str)sDebugName);
	}
	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return NULL;
	}
	sText = xl_generated_error_text(error);
	sOwned = xrtCopyStr((str)((sText != NULL) ? sText : "await failed"), 0);
	if ( sOwned == NULL ) {
		sOwned = xrtCopyStr((str)"await failed", 0);
	}
	if ( !xPromiseReject(pPromise, XRT_NET_ERROR, sOwned) ) {
		if ( sOwned != NULL ) {
			xrtFree(sOwned);
		}
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}
	xPromiseDestroy(pPromise);
	return pFuture;
}

typedef struct XL_GeneratedAsyncForwardStateCtx {
	xpromise* pPromise;
	const char* sAwaitName;
	const char* sFuncName;
	int iResumeStateId;
} XL_GeneratedAsyncForwardStateCtx;

static int xl_generated_async_has_pending_error(void)
{
	if ( xl_generated_had_exception ) {
		return 1;
	}
	return xl_generated_capture_native_error();
}

static int xl_generated_promise_reject_text(xpromise* pPromise, int32 iStatus, const char* sText)
{
	str sOwned;
	if ( pPromise == NULL ) {
		return 0;
	}
	sOwned = xrtCopyStr((str)((sText != NULL) ? sText : "await failed"), 0);
	if ( (sOwned == NULL) && ((sText == NULL) || (strcmp(sText, "await failed") != 0)) ) {
		sOwned = xrtCopyStr((str)"await failed", 0);
	}
	if ( (iStatus == XRT_NET_OK) || (iStatus == XRT_NET_AGAIN) ) {
		iStatus = XRT_NET_ERROR;
	}
	if ( !xPromiseReject(pPromise, iStatus, sOwned) ) {
		if ( sOwned != NULL ) {
			xrtFree(sOwned);
		}
		return 0;
	}
	return 1;
}

static int xl_generated_promise_reject_result(xpromise* pPromise, const xfuture_result* pIn, const char* sFallback)
{
	const char* sText;
	str sOwned;
	int32 iStatus;
	if ( pPromise == NULL ) {
		return 0;
	}
	sText = (sFallback != NULL) ? sFallback : "await failed";
	iStatus = XRT_NET_ERROR;
	if ( pIn != NULL ) {
		if ( (pIn->sError != NULL) && (pIn->sError[0] != '\0') ) {
			sText = (const char*)pIn->sError;
		}
		if ( (pIn->iStatus != XRT_NET_OK) && (pIn->iStatus != XRT_NET_AGAIN) ) {
			iStatus = pIn->iStatus;
		}
	}
	sOwned = xrtCopyStr((str)((sText != NULL) ? sText : "await failed"), 0);
	if ( (sOwned == NULL) && ((sText == NULL) || (strcmp(sText, "await failed") != 0)) ) {
		sOwned = xrtCopyStr((str)"await failed", 0);
	}
	if ( !xPromiseReject(pPromise, iStatus, sOwned) ) {
		if ( sOwned != NULL ) {
			xrtFree(sOwned);
		}
		return 0;
	}
	return 1;
}

static int xl_generated_promise_reject_current(xpromise* pPromise, const char* sFallback)
{
	XL_GeneratedError* error;
	if ( pPromise == NULL ) {
		return 0;
	}
	if ( !xl_generated_async_has_pending_error() ) {
		return xl_generated_promise_reject_text(pPromise, XRT_NET_ERROR, sFallback);
	}
	error = xl_generated_capture_exception_error();
	return xl_generated_promise_reject_text(pPromise, (error != NULL) ? error->code : XRT_NET_ERROR, xl_generated_error_text(error));
}

static int xl_generated_promise_resolve_void(xpromise* pPromise)
{
	return (pPromise != NULL) ? xPromiseResolve(pPromise, NULL) : 0;
}

static int xl_generated_promise_resolve_int(xpromise* pPromise, intptr_t value)
{
	return (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)value) : 0;
}

static int xl_generated_promise_resolve_bool(xpromise* pPromise, int value)
{
	return (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)(intptr_t)(value ? 1 : 0)) : 0;
}

static int xl_generated_promise_resolve_float(xpromise* pPromise, double value)
{
	double* pHeapValue;
	if ( pPromise == NULL ) {
		return 0;
	}
	pHeapValue = (double*)xrtMalloc(sizeof(double));
	if ( pHeapValue == NULL ) {
		return 0;
	}
	*pHeapValue = value;
	if ( !xPromiseResolve(pPromise, (ptr)pHeapValue) ) {
		xrtFree(pHeapValue);
		return 0;
	}
	return 1;
}

static int xl_generated_promise_resolve_string(xpromise* pPromise, const char* value)
{
	return (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)((value != NULL) ? value : "")) : 0;
}

static int xl_generated_promise_resolve_error(xpromise* pPromise, XL_GeneratedError* value)
{
	return (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)value) : 0;
}

static int xl_generated_promise_resolve_ptr(xpromise* pPromise, ptr value)
{
	return (pPromise != NULL) ? xPromiseResolve(pPromise, value) : 0;
}

static void xl_generated_async_forward_state_finally(const xfuture_result* pIn, ptr pArg)
{
	XL_GeneratedAsyncForwardStateCtx* pCtx = (XL_GeneratedAsyncForwardStateCtx*)pArg;
	if ( pCtx == NULL ) {
		return;
	}
	xl_generated_async_trace_event("resume", (pCtx->sAwaitName != NULL) ? pCtx->sAwaitName : "");
	if ( pCtx->iResumeStateId >= 0 ) {
		xl_generated_async_trace_state_event(pCtx->sFuncName, pCtx->iResumeStateId);
	}
	if ( pCtx->pPromise != NULL ) {
		if ( (pIn != NULL) && (pIn->iStatus == XRT_NET_OK) ) {
			(void)xPromiseResolve(pCtx->pPromise, pIn->pValue);
		} else {
			(void)xl_generated_promise_reject_result(pCtx->pPromise, pIn, "await failed");
		}
		xPromiseDestroy(pCtx->pPromise);
	}
	xrtFree(pCtx);
}

static xfuture* xl_generated_future_finally_auto(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg)
{
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( xFutureState(pFuture) != XFUTURE_PENDING ) {
		return xFutureFinallyInline(pFuture, pfnCont, pArg);
	}
	return xFutureFinallyEngine(pFuture, NULL, 0u, pfnCont, pArg);
}

static xfuture* xl_generated_future_forward_state(const char* sName, xfuture* pFuture, const char* sFunc, int iAwaitStateId, int iResumeStateId, const char* sDebugName)
{
	xfuture* pOut;
	xpromise* pPromise;
	xfuture* pHook;
	XL_GeneratedAsyncForwardStateCtx* pCtx;
	if ( iAwaitStateId >= 0 ) {
		xl_generated_async_trace_state_event(sFunc, iAwaitStateId);
	}
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( pFuture == NULL ) {
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		if ( iResumeStateId >= 0 ) {
			xl_generated_async_trace_state_event(sFunc, iResumeStateId);
		}
		return xl_generated_future_create_error(xl_generated_error_make("Error", "future unavailable", 0, "", ""), sDebugName);
	}
	pOut = xFutureCreate();
	if ( pOut == NULL ) {
		return xl_generated_future_create_error(xl_generated_error_make("Error", "async await forward alloc failed", 0, "", ""), sDebugName);
	}
	if ( (sDebugName != NULL) && (sDebugName[0] != '\0') ) {
		(void)xFutureSetDebugName(pOut, (str)sDebugName);
	}
	pPromise = xPromiseCreate(pOut);
	if ( pPromise == NULL ) {
		xFutureRelease(pOut);
		return xl_generated_future_create_error(xl_generated_error_make("Error", "async await forward promise failed", 0, "", ""), sDebugName);
	}
	pCtx = (XL_GeneratedAsyncForwardStateCtx*)xrtMalloc(sizeof(*pCtx));
	if ( pCtx == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pOut);
		return xl_generated_future_create_error(xl_generated_error_make("Error", "async await forward context failed", 0, "", ""), sDebugName);
	}
	pCtx->pPromise = pPromise;
	pCtx->sAwaitName = sName;
	pCtx->sFuncName = sFunc;
	pCtx->iResumeStateId = iResumeStateId;
	pHook = xl_generated_future_finally_auto(pFuture, xl_generated_async_forward_state_finally, pCtx);
	if ( pHook == NULL ) {
		xrtFree(pCtx);
		xPromiseDestroy(pPromise);
		xFutureRelease(pOut);
		return xl_generated_future_create_error(xl_generated_error_make("Error", "async await forward hook failed", 0, "", ""), sDebugName);
	}
	xFutureRelease(pHook);
	return pOut;
}

static int xl_generated_future_wait_result(const char* sName, xfuture* pFuture, xfuture_result* pResult)
{
	if ( pResult != NULL ) {
		memset(pResult, 0, sizeof(*pResult));
	}
	xl_generated_async_trace_event("await", (sName != NULL) ? sName : "");
	if ( (pFuture == NULL) || (pResult == NULL) ) {
		xl_generated_raise_error(xl_generated_error_make("Error", "future unavailable", 0, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return 0;
	}
	(void)xFutureWait(pFuture);
	(void)xFutureGetResult(pFuture, pResult);
	if ( pResult->iStatus != XRT_NET_OK ) {
		xl_generated_raise_error(xl_generated_error_make("Error", (const char*)((pResult->sError != NULL) ? pResult->sError : (str)"await failed"), pResult->iStatus, "", ""));
		xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
		return 0;
	}
	xl_generated_async_trace_event("resume", (sName != NULL) ? sName : "");
	return 1;
}

static int xl_generated_future_wait_result_state(const char* sName, xfuture* pFuture, xfuture_result* pResult, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	if ( iAwaitStateId >= 0 ) {
		xl_generated_async_trace_state_event(sFunc, iAwaitStateId);
	}
	if ( !xl_generated_future_wait_result(sName, pFuture, pResult) ) {
		return 0;
	}
	if ( iResumeStateId >= 0 ) {
		xl_generated_async_trace_state_event(sFunc, iResumeStateId);
	}
	return 1;
}

static int32 xl_generated_future_all_pick_last(const xfuture_result* pIn, ptr pArg, xfuture_result* pOut)
{
	xfuture_all_value* pAll;
	(void)pArg;
	if ( pOut == NULL ) {
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	if ( pIn == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"future all mapper input is null.";
		return pOut->iStatus;
	}
	if ( pIn->iStatus != XRT_NET_OK ) {
		pOut->iStatus = pIn->iStatus;
		pOut->sError = pIn->sError;
		return pOut->iStatus;
	}
	pAll = (xfuture_all_value*)pIn->pValue;
	if ( (pAll == NULL) || (pAll->iCount <= 0) || (pAll->arrValue == NULL) ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"future all mapper missing values.";
		return pOut->iStatus;
	}
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pAll->arrValue[pAll->iCount - 1];
	return pOut->iStatus;
}

static int32 xl_generated_task_delay_engine(xnetworker* pWorker, ptr pArg, xfuture_result* pOut)
{
	(void)pWorker;
	if ( pOut == NULL ) {
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pArg;
	return pOut->iStatus;
}

static const char* xl_generated_type_of_value(xvalue value)
{
	if ( value == NULL ) {
		return "null";
	}
	switch ( value->Type ) {
		case XVO_DT_NULL: return "null";
		case XVO_DT_BOOL: return "bool";
		case XVO_DT_INT: return "int";
		case XVO_DT_FLOAT: return "float";
		case XVO_DT_TEXT: return "string";
		case XVO_DT_TIME: return "time";
		case XVO_DT_POINT: return "point";
		case XVO_DT_FUNC: return "function";
		case XVO_DT_ARRAY: return "array";
		case XVO_DT_LIST: return "list";
		case XVO_DT_COLL: return "set";
		case XVO_DT_TABLE: return "dict";
		case XVO_DT_CLASS: return "class";
		case XVO_DT_CUSTOM: return "custom";
		default: return "unknown";
	}
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
	xl_generated_exception_file = NULL;
	xl_generated_exception_line = 0;
	xl_generated_exception_column = 0;
	xl_generated_exception_int = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_float(double value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 2;
	xl_generated_exception_error = NULL;
	xl_generated_exception_file = NULL;
	xl_generated_exception_line = 0;
	xl_generated_exception_column = 0;
	xl_generated_exception_float = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_string(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_error = NULL;
	xl_generated_exception_file = NULL;
	xl_generated_exception_line = 0;
	xl_generated_exception_column = 0;
	xl_generated_exception_text = value;
}

static void xl_generated_raise_text(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_error = NULL;
	xl_generated_exception_file = NULL;
	xl_generated_exception_line = 0;
	xl_generated_exception_column = 0;
	xl_generated_exception_text = value;
}

static void xl_generated_raise_error(XL_GeneratedError* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 4;
	xl_generated_exception_error = value;
	xl_generated_exception_file = NULL;
	xl_generated_exception_line = 0;
	xl_generated_exception_column = 0;
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
	xfuture* handle;
} XL_FutureInt;

static XL_FutureInt xl_future_int_ready(int value)
{
	XL_FutureInt future;
	future.handle = xl_generated_future_create_ready((ptr)(intptr_t)value, "future<int>");
	return future;
}

static XL_FutureInt xl_future_int_error(XL_GeneratedError* error)
{
	XL_FutureInt future;
	future.handle = xl_generated_future_create_error(error, "future<int>");
	return future;
}

static int xl_future_int_await(const char* sName, XL_FutureInt future)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {
		return 0;
	}
	return (int)(intptr_t)result.pValue;
}

static int xl_future_int_await_state(const char* sName, XL_FutureInt future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {
		return 0;
	}
	return (int)(intptr_t)result.pValue;
}

static XL_FutureInt xl_future_int_forward_state(const char* sName, XL_FutureInt future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	XL_FutureInt out;
	out.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, "future<int>");
	return out;
}

typedef struct XL_FutureBool {
	xfuture* handle;
} XL_FutureBool;

static XL_FutureBool xl_future_bool_ready(int value)
{
	XL_FutureBool future;
	future.handle = xl_generated_future_create_ready((ptr)(intptr_t)(value ? 1 : 0), "future<bool>");
	return future;
}

static XL_FutureBool xl_future_bool_error(XL_GeneratedError* error)
{
	XL_FutureBool future;
	future.handle = xl_generated_future_create_error(error, "future<bool>");
	return future;
}

static int xl_future_bool_await(const char* sName, XL_FutureBool future)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {
		return 0;
	}
	return ((intptr_t)result.pValue != 0) ? 1 : 0;
}

static int xl_future_bool_await_state(const char* sName, XL_FutureBool future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {
		return 0;
	}
	return ((intptr_t)result.pValue != 0) ? 1 : 0;
}

static XL_FutureBool xl_future_bool_forward_state(const char* sName, XL_FutureBool future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	XL_FutureBool out;
	out.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, "future<bool>");
	return out;
}

typedef struct XL_FutureFloat {
	xfuture* handle;
} XL_FutureFloat;

static XL_FutureFloat xl_future_float_ready(double value)
{
	XL_FutureFloat future;
	double* pValue = (double*)xrtMalloc(sizeof(double));
	if ( pValue == NULL ) {
		future.handle = xl_generated_future_create_error(xl_generated_error_make("Error", "future<float> alloc failed", 0, "", ""), "future<float>");
		return future;
	}
	*pValue = value;
	future.handle = xl_generated_future_create_ready((ptr)pValue, "future<float>");
	return future;
}

static XL_FutureFloat xl_future_float_error(XL_GeneratedError* error)
{
	XL_FutureFloat future;
	future.handle = xl_generated_future_create_error(error, "future<float>");
	return future;
}

static double xl_future_float_await(const char* sName, XL_FutureFloat future)
{
	xfuture_result result;
	double* pValue;
	if ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {
		return 0.0;
	}
	pValue = (double*)result.pValue;
	return (pValue != NULL) ? *pValue : 0.0;
}

static double xl_future_float_await_state(const char* sName, XL_FutureFloat future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	xfuture_result result;
	double* pValue;
	if ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {
		return 0.0;
	}
	pValue = (double*)result.pValue;
	return (pValue != NULL) ? *pValue : 0.0;
}

static XL_FutureFloat xl_future_float_forward_state(const char* sName, XL_FutureFloat future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	XL_FutureFloat out;
	out.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, "future<float>");
	return out;
}

typedef struct XL_FutureString {
	xfuture* handle;
} XL_FutureString;

static XL_FutureString xl_future_string_ready(const char* value)
{
	XL_FutureString future;
	future.handle = xl_generated_future_create_ready((ptr)((value != NULL) ? value : ""), "future<string>");
	return future;
}

static XL_FutureString xl_future_string_error(XL_GeneratedError* error)
{
	XL_FutureString future;
	future.handle = xl_generated_future_create_error(error, "future<string>");
	return future;
}

static const char* xl_future_string_await(const char* sName, XL_FutureString future)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {
		return NULL;
	}
	return (const char*)((result.pValue != NULL) ? result.pValue : "");
}

static const char* xl_future_string_await_state(const char* sName, XL_FutureString future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {
		return NULL;
	}
	return (const char*)((result.pValue != NULL) ? result.pValue : "");
}

static XL_FutureString xl_future_string_forward_state(const char* sName, XL_FutureString future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	XL_FutureString out;
	out.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, "future<string>");
	return out;
}

typedef struct XL_FutureError {
	xfuture* handle;
} XL_FutureError;

static XL_FutureError xl_future_error_ready(XL_GeneratedError* value)
{
	XL_FutureError future;
	future.handle = xl_generated_future_create_ready((ptr)value, "future<error>");
	return future;
}

static XL_FutureError xl_future_error_error(XL_GeneratedError* error)
{
	XL_FutureError future;
	future.handle = xl_generated_future_create_error(error, "future<error>");
	return future;
}

static XL_GeneratedError* xl_future_error_await(const char* sName, XL_FutureError future)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {
		return NULL;
	}
	return (XL_GeneratedError*)result.pValue;
}

static XL_GeneratedError* xl_future_error_await_state(const char* sName, XL_FutureError future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {
		return NULL;
	}
	return (XL_GeneratedError*)result.pValue;
}

static XL_FutureError xl_future_error_forward_state(const char* sName, XL_FutureError future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	XL_FutureError out;
	out.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, "future<error>");
	return out;
}

typedef struct XL_FuturePtr {
	xfuture* handle;
} XL_FuturePtr;

static XL_FuturePtr xl_future_ptr_ready(void* value)
{
	XL_FuturePtr future;
	future.handle = xl_generated_future_create_ready((ptr)value, "future<ptr>");
	return future;
}

static XL_FuturePtr xl_future_ptr_error(XL_GeneratedError* error)
{
	XL_FuturePtr future;
	future.handle = xl_generated_future_create_error(error, "future<ptr>");
	return future;
}

static void* xl_future_ptr_await(const char* sName, XL_FuturePtr future)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {
		return NULL;
	}
	return (void*)result.pValue;
}

static void* xl_future_ptr_await_state(const char* sName, XL_FuturePtr future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	xfuture_result result;
	if ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {
		return NULL;
	}
	return (void*)result.pValue;
}

static XL_FuturePtr xl_future_ptr_forward_state(const char* sName, XL_FuturePtr future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	XL_FuturePtr out;
	out.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, "future<ptr>");
	return out;
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

static XL_FutureInt xl_task_spawn_int(const char* sName, XL_FutureInt value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static XL_FutureInt xl_task_all_int(const char* sName, XL_FutureInt left, XL_FutureInt right)
{
	XL_FutureInt future;
	xfuture* arrFuture[2];
	xfuture* pAll;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_int_error(xl_generated_error_make("Error", "task.all received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	pAll = xFutureWhenAll(arrFuture, 2);
	if ( pAll == NULL ) {
		return xl_future_int_error(xl_generated_error_make("Error", "task.all could not create group future", 0, "", ""));
	}
	future.handle = xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL);
	xFutureRelease(pAll);
	if ( future.handle == NULL ) {
		return xl_future_int_error(xl_generated_error_make("Error", "task.all could not map group future", 0, "", ""));
	}
	return future;
}

static XL_FutureInt xl_task_race_int(const char* sName, XL_FutureInt left, XL_FutureInt right)
{
	XL_FutureInt future;
	xfuture* arrFuture[2];
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_int_error(xl_generated_error_make("Error", "task.race received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	future.handle = xFutureRace(arrFuture, 2);
	if ( future.handle == NULL ) {
		return xl_future_int_error(xl_generated_error_make("Error", "task.race could not create race future", 0, "", ""));
	}
	return future;
}

static int xl_task_cancel_int(const char* sName, XL_FutureInt value)
{
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	if ( value.handle == NULL ) {
		return 0;
	}
	(void)xFutureRequestCancel(value.handle);
	return 1;
}

static XL_FutureInt xl__xl_mod_std_task__spawn__arity1__value__future_x3Cint_x3E__ret__future_x3Cint_x3E(XL_FutureInt value)
{
	return xl_task_spawn_int("xl_mod_std_task__spawn", value);
}

static XL_FutureInt xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E(XL_FutureInt left, XL_FutureInt right)
{
	return xl_task_all_int("xl_mod_std_task__all", left, right);
}

static XL_FutureInt xl__xl_mod_std_task__race__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E(XL_FutureInt left, XL_FutureInt right)
{
	return xl_task_race_int("xl_mod_std_task__race", left, right);
}

static int xl__xl_mod_std_task__cancel__arity1__value__future_x3Cint_x3E__ret__int(XL_FutureInt value)
{
	return xl_task_cancel_int("xl_mod_std_task__cancel", value);
}

static int xl_task_default_int(void)
{
	return 0;
}

static XL_FutureFloat xl_task_spawn_float(const char* sName, XL_FutureFloat value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static XL_FutureFloat xl_task_all_float(const char* sName, XL_FutureFloat left, XL_FutureFloat right)
{
	XL_FutureFloat future;
	xfuture* arrFuture[2];
	xfuture* pAll;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_float_error(xl_generated_error_make("Error", "task.all received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	pAll = xFutureWhenAll(arrFuture, 2);
	if ( pAll == NULL ) {
		return xl_future_float_error(xl_generated_error_make("Error", "task.all could not create group future", 0, "", ""));
	}
	future.handle = xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL);
	xFutureRelease(pAll);
	if ( future.handle == NULL ) {
		return xl_future_float_error(xl_generated_error_make("Error", "task.all could not map group future", 0, "", ""));
	}
	return future;
}

static XL_FutureFloat xl_task_race_float(const char* sName, XL_FutureFloat left, XL_FutureFloat right)
{
	XL_FutureFloat future;
	xfuture* arrFuture[2];
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_float_error(xl_generated_error_make("Error", "task.race received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	future.handle = xFutureRace(arrFuture, 2);
	if ( future.handle == NULL ) {
		return xl_future_float_error(xl_generated_error_make("Error", "task.race could not create race future", 0, "", ""));
	}
	return future;
}

static int xl_task_cancel_float(const char* sName, XL_FutureFloat value)
{
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	if ( value.handle == NULL ) {
		return 0;
	}
	(void)xFutureRequestCancel(value.handle);
	return 1;
}

static XL_FutureFloat xl__xl_mod_std_task__spawn__arity1__value__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E(XL_FutureFloat value)
{
	return xl_task_spawn_float("xl_mod_std_task__spawn", value);
}

static XL_FutureFloat xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E(XL_FutureFloat left, XL_FutureFloat right)
{
	return xl_task_all_float("xl_mod_std_task__all", left, right);
}

static XL_FutureFloat xl__xl_mod_std_task__race__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E(XL_FutureFloat left, XL_FutureFloat right)
{
	return xl_task_race_float("xl_mod_std_task__race", left, right);
}

static int xl__xl_mod_std_task__cancel__arity1__value__future_x3Cfloat_x3E__ret__int(XL_FutureFloat value)
{
	return xl_task_cancel_float("xl_mod_std_task__cancel", value);
}

static double xl_task_default_float(void)
{
	return 0.0;
}

static XL_FutureString xl_task_spawn_string(const char* sName, XL_FutureString value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static XL_FutureString xl_task_all_string(const char* sName, XL_FutureString left, XL_FutureString right)
{
	XL_FutureString future;
	xfuture* arrFuture[2];
	xfuture* pAll;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_string_error(xl_generated_error_make("Error", "task.all received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	pAll = xFutureWhenAll(arrFuture, 2);
	if ( pAll == NULL ) {
		return xl_future_string_error(xl_generated_error_make("Error", "task.all could not create group future", 0, "", ""));
	}
	future.handle = xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL);
	xFutureRelease(pAll);
	if ( future.handle == NULL ) {
		return xl_future_string_error(xl_generated_error_make("Error", "task.all could not map group future", 0, "", ""));
	}
	return future;
}

static XL_FutureString xl_task_race_string(const char* sName, XL_FutureString left, XL_FutureString right)
{
	XL_FutureString future;
	xfuture* arrFuture[2];
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_string_error(xl_generated_error_make("Error", "task.race received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	future.handle = xFutureRace(arrFuture, 2);
	if ( future.handle == NULL ) {
		return xl_future_string_error(xl_generated_error_make("Error", "task.race could not create race future", 0, "", ""));
	}
	return future;
}

static int xl_task_cancel_string(const char* sName, XL_FutureString value)
{
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	if ( value.handle == NULL ) {
		return 0;
	}
	(void)xFutureRequestCancel(value.handle);
	return 1;
}

static XL_FutureString xl__xl_mod_std_task__spawn__arity1__value__future_x3Cstring_x3E__ret__future_x3Cstring_x3E(XL_FutureString value)
{
	return xl_task_spawn_string("xl_mod_std_task__spawn", value);
}

static XL_FutureString xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E(XL_FutureString left, XL_FutureString right)
{
	return xl_task_all_string("xl_mod_std_task__all", left, right);
}

static XL_FutureString xl__xl_mod_std_task__race__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E(XL_FutureString left, XL_FutureString right)
{
	return xl_task_race_string("xl_mod_std_task__race", left, right);
}

static int xl__xl_mod_std_task__cancel__arity1__value__future_x3Cstring_x3E__ret__int(XL_FutureString value)
{
	return xl_task_cancel_string("xl_mod_std_task__cancel", value);
}

static const char* xl_task_default_string(void)
{
	return NULL;
}

static XL_FutureError xl_task_spawn_error(const char* sName, XL_FutureError value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static XL_FutureError xl_task_all_error(const char* sName, XL_FutureError left, XL_FutureError right)
{
	XL_FutureError future;
	xfuture* arrFuture[2];
	xfuture* pAll;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_error_error(xl_generated_error_make("Error", "task.all received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	pAll = xFutureWhenAll(arrFuture, 2);
	if ( pAll == NULL ) {
		return xl_future_error_error(xl_generated_error_make("Error", "task.all could not create group future", 0, "", ""));
	}
	future.handle = xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL);
	xFutureRelease(pAll);
	if ( future.handle == NULL ) {
		return xl_future_error_error(xl_generated_error_make("Error", "task.all could not map group future", 0, "", ""));
	}
	return future;
}

static XL_FutureError xl_task_race_error(const char* sName, XL_FutureError left, XL_FutureError right)
{
	XL_FutureError future;
	xfuture* arrFuture[2];
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_error_error(xl_generated_error_make("Error", "task.race received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	future.handle = xFutureRace(arrFuture, 2);
	if ( future.handle == NULL ) {
		return xl_future_error_error(xl_generated_error_make("Error", "task.race could not create race future", 0, "", ""));
	}
	return future;
}

static int xl_task_cancel_error(const char* sName, XL_FutureError value)
{
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	if ( value.handle == NULL ) {
		return 0;
	}
	(void)xFutureRequestCancel(value.handle);
	return 1;
}

static XL_FutureError xl__xl_mod_std_task__spawn__arity1__value__future_x3Cerror_x3E__ret__future_x3Cerror_x3E(XL_FutureError value)
{
	return xl_task_spawn_error("xl_mod_std_task__spawn", value);
}

static XL_FutureError xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E(XL_FutureError left, XL_FutureError right)
{
	return xl_task_all_error("xl_mod_std_task__all", left, right);
}

static XL_FutureError xl__xl_mod_std_task__race__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E(XL_FutureError left, XL_FutureError right)
{
	return xl_task_race_error("xl_mod_std_task__race", left, right);
}

static int xl__xl_mod_std_task__cancel__arity1__value__future_x3Cerror_x3E__ret__int(XL_FutureError value)
{
	return xl_task_cancel_error("xl_mod_std_task__cancel", value);
}

static XL_GeneratedError* xl_task_default_error(void)
{
	return NULL;
}

static XL_FutureBool xl_task_spawn_bool(const char* sName, XL_FutureBool value)
{
	xl_generated_task_trace_event("spawn", (sName != NULL) ? sName : "");
	return value;
}

static XL_FutureBool xl_task_all_bool(const char* sName, XL_FutureBool left, XL_FutureBool right)
{
	XL_FutureBool future;
	xfuture* arrFuture[2];
	xfuture* pAll;
	xl_generated_task_trace_event("all", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_bool_error(xl_generated_error_make("Error", "task.all received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	pAll = xFutureWhenAll(arrFuture, 2);
	if ( pAll == NULL ) {
		return xl_future_bool_error(xl_generated_error_make("Error", "task.all could not create group future", 0, "", ""));
	}
	future.handle = xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL);
	xFutureRelease(pAll);
	if ( future.handle == NULL ) {
		return xl_future_bool_error(xl_generated_error_make("Error", "task.all could not map group future", 0, "", ""));
	}
	return future;
}

static XL_FutureBool xl_task_race_bool(const char* sName, XL_FutureBool left, XL_FutureBool right)
{
	XL_FutureBool future;
	xfuture* arrFuture[2];
	xl_generated_task_trace_event("race", (sName != NULL) ? sName : "");
	if ( (left.handle == NULL) || (right.handle == NULL) ) {
		return xl_future_bool_error(xl_generated_error_make("Error", "task.race received an empty future", 0, "", ""));
	}
	arrFuture[0] = left.handle;
	arrFuture[1] = right.handle;
	future.handle = xFutureRace(arrFuture, 2);
	if ( future.handle == NULL ) {
		return xl_future_bool_error(xl_generated_error_make("Error", "task.race could not create race future", 0, "", ""));
	}
	return future;
}

static int xl_task_cancel_bool(const char* sName, XL_FutureBool value)
{
	xl_generated_task_trace_event("cancel", (sName != NULL) ? sName : "");
	if ( value.handle == NULL ) {
		return 0;
	}
	(void)xFutureRequestCancel(value.handle);
	return 1;
}

static XL_FutureBool xl__xl_mod_std_task__spawn__arity1__value__future_x3Cbool_x3E__ret__future_x3Cbool_x3E(XL_FutureBool value)
{
	return xl_task_spawn_bool("xl_mod_std_task__spawn", value);
}

static XL_FutureBool xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E(XL_FutureBool left, XL_FutureBool right)
{
	return xl_task_all_bool("xl_mod_std_task__all", left, right);
}

static XL_FutureBool xl__xl_mod_std_task__race__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E(XL_FutureBool left, XL_FutureBool right)
{
	return xl_task_race_bool("xl_mod_std_task__race", left, right);
}

static int xl__xl_mod_std_task__cancel__arity1__value__future_x3Cbool_x3E__ret__int(XL_FutureBool value)
{
	return xl_task_cancel_bool("xl_mod_std_task__cancel", value);
}

static int xl_task_default_bool(void)
{
	return 0;
}

static XL_FutureInt xl_task_delay(const char* sName, int ms)
{
	xl_generated_task_trace_event("delay", (sName != NULL) ? sName : "");
	{
		XL_FutureInt future;
		uint32_t iDelay = (ms < 0) ? 0u : (uint32_t)ms;
		future.handle = xTaskRunDelayed(NULL, 0u, iDelay, xl_generated_task_delay_engine, (ptr)(intptr_t)ms);
		if ( future.handle == NULL ) {
			return xl_future_int_error(xl_generated_error_make("Error", "task.delay could not schedule future", 0, "", ""));
		}
		return future;
	}
}

static XL_FutureInt xl__xl_mod_std_task__delay__arity1__ms__int__ret__future_x3Cint_x3E(int ms)
{
	return xl_task_delay("xl_mod_std_task__delay", ms);
}

typedef struct XL_FutureVoid {
	xfuture* handle;
} XL_FutureVoid;

static XL_FutureVoid xl_future_void_ready(void)
{
	XL_FutureVoid future;
	future.handle = xl_generated_future_create_ready(NULL, "future<void>");
	return future;
}

static XL_FutureVoid xl_future_void_error(XL_GeneratedError* error)
{
	XL_FutureVoid future;
	future.handle = xl_generated_future_create_error(error, "future<void>");
	return future;
}

static void xl_future_void_await(const char* sName, XL_FutureVoid future)
{
	xfuture_result result;
	(void)xl_generated_future_wait_result(sName, future.handle, &result);
}

static void xl_future_void_await_state(const char* sName, XL_FutureVoid future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	xfuture_result result;
	(void)xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId);
}

static XL_FutureVoid xl_future_void_forward_state(const char* sName, XL_FutureVoid future, const char* sFunc, int iAwaitStateId, int iResumeStateId)
{
	XL_FutureVoid out;
	out.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, "future<void>");
	return out;
}

typedef struct XL_GeneratedModuleExport {
	const char* name;
	const char* symbol;
} XL_GeneratedModuleExport;

typedef struct XL_GeneratedModuleDescriptor {
	const char* key;
	const char* abiId;
	const char* initSymbol;
	const char* exportsSymbol;
	const char* descriptorSymbol;
	size_t exportCount;
	const XL_GeneratedModuleExport* exports;
} XL_GeneratedModuleDescriptor;

static const char* __xl_method_module_key(void* value)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	return (d != NULL && d->key != NULL) ? d->key : "";
}

static const char* __xl_method_module_abi_id(void* value)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	return (d != NULL && d->abiId != NULL) ? d->abiId : "";
}

static const char* __xl_method_module_init_symbol(void* value)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	return (d != NULL && d->initSymbol != NULL) ? d->initSymbol : "";
}

static const char* __xl_method_module_exports_symbol(void* value)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	return (d != NULL && d->exportsSymbol != NULL) ? d->exportsSymbol : "";
}

static const char* __xl_method_module_descriptor_symbol(void* value)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	return (d != NULL && d->descriptorSymbol != NULL) ? d->descriptorSymbol : "";
}

static int __xl_method_module_export_count(void* value)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	return (d != NULL) ? (int)d->exportCount : 0;
}

static xvalue __xl_method_module_exports(void* value)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	xvalue out = xvoCreateArray();
	size_t i;
	if ( out == NULL ) return NULL;
	if ( d != NULL && d->exports != NULL ) {
		for ( i = 0; i < d->exportCount; ++i ) {
			const char* name = (d->exports[i].name != NULL) ? d->exports[i].name : "";
			(void)xvoArrayAppendValue(out, xvoCreateText((ptr)name, (uint32)strlen(name), FALSE), TRUE);
		}
	}
	return out;
}

static int __xl_method_module_has_export(void* value, const char* name)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	size_t i;
	const char* target = (name != NULL) ? name : "";
	if ( d == NULL || d->exports == NULL ) return 0;
	for ( i = 0; i < d->exportCount; ++i ) {
		if ( d->exports[i].name != NULL && strcmp(d->exports[i].name, target) == 0 ) return 1;
	}
	return 0;
}

static const char* __xl_method_module_export_symbol(void* value, const char* name)
{
	const XL_GeneratedModuleDescriptor* d = (const XL_GeneratedModuleDescriptor*)value;
	size_t i;
	const char* target = (name != NULL) ? name : "";
	if ( d == NULL || d->exports == NULL ) return "";
	for ( i = 0; i < d->exportCount; ++i ) {
		if ( d->exports[i].name != NULL && strcmp(d->exports[i].name, target) == 0 ) return (d->exports[i].symbol != NULL) ? d->exports[i].symbol : "";
	}
	return "";
}

typedef struct XL_Class_NativePair {
	int x;
	int y;
} XL_Class_NativePair;

static int xl_generated_module_init_done = 0;
static void xl_generated_module_init(void)
{
	if ( xl_generated_module_init_done )
	{
		return;
	}
	xl_generated_module_init_done = 1;
}

const XL_GeneratedModuleExport xl_mod__root___exports[] = {
	{ NULL, NULL }
};
void xl_mod__root___init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod__root___descriptor = {
	"<root>",
	"extern_func_class_abi_basic@0.0.0#abi1::<root>",
	"xl_mod__root___init",
	"xl_mod__root___exports",
	"xl_mod__root___descriptor",
	0,
	xl_mod__root___exports
};

static XL_Class_NativePair xl_class_NativePair__create__7_2(XL_Class_NativePair self, int x, int y);
static XL_Class_NativePair xl__pair_make__arity2__x__int__y__int__ret__NativePair(int x, int y);
static int xl__pair_value_sum__arity1__pair__NativePair__ret__int(XL_Class_NativePair pair);
static XL_Class_NativePair xl__pair_add__arity2__pair__NativePair__delta__int__ret__NativePair(XL_Class_NativePair pair, int delta);
static int xl__main__arity0__ret__int();
static xvalue xl__main__arity0__ret__int__boxed(xvalue varENV, xvalue arrParam);

static XL_Class_NativePair xl_class_NativePair__create__7_2(XL_Class_NativePair self, int x, int y)
{
	int xl_escape_kind_7_2 = 0;
	int xl_escape_level_7_2 = 0;
	XL_Class_NativePair xl_escape_value_7_2 = ((XL_Class_NativePair){0});
	(self.x = x);
	if ( xl_generated_had_exception )
	{
				return ((XL_Class_NativePair){0});
	}
	if ( xl_generated_capture_native_error() )
	{
				return ((XL_Class_NativePair){0});
	}
	(self.y = y);
	if ( xl_generated_had_exception )
	{
				return ((XL_Class_NativePair){0});
	}
	if ( xl_generated_capture_native_error() )
	{
				return ((XL_Class_NativePair){0});
	}
 	XL_Class_NativePair xl_ret_7_2 = self;
	if ( xl_generated_had_exception )
	{
				return ((XL_Class_NativePair){0});
	}
	if ( xl_generated_capture_native_error() )
	{
				return ((XL_Class_NativePair){0});
	}
	return xl_ret_7_2;
}

extern XL_Class_NativePair pkg_clock_artifact_pair_make(int x, int y);
static XL_Class_NativePair xl__pair_make__arity2__x__int__y__int__ret__NativePair(int x, int y)
{
	return pkg_clock_artifact_pair_make(x, y);
}

extern int pkg_clock_artifact_pair_value_sum(XL_Class_NativePair pair);
static int xl__pair_value_sum__arity1__pair__NativePair__ret__int(XL_Class_NativePair pair)
{
	return pkg_clock_artifact_pair_value_sum(pair);
}

extern XL_Class_NativePair pkg_clock_artifact_pair_add(XL_Class_NativePair pair, int delta);
static XL_Class_NativePair xl__pair_add__arity2__pair__NativePair__delta__int__ret__NativePair(XL_Class_NativePair pair, int delta)
{
	return pkg_clock_artifact_pair_add(pair, delta);
}

static int xl__main__arity0__ret__int()
{
	int xl_escape_kind_17_6 = 0;
	int xl_escape_level_17_6 = 0;
	int xl_escape_value_17_6 = 0;
	XL_Class_NativePair a = xl__pair_make__arity2__x__int__y__int__ret__NativePair(12, 30);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	int xl_if_cond_19_2 = (xl__pair_value_sum__arity1__pair__NativePair__ret__int(a) != 42);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	if ( xl_if_cond_19_2 )
	{
 		int xl_ret_20_3 = 1;
		if ( xl_generated_had_exception )
		{
						return 0;
		}
		if ( xl_generated_capture_native_error() )
		{
						return 0;
		}
		return xl_ret_20_3;
	}
	XL_Class_NativePair b = xl__pair_add__arity2__pair__NativePair__delta__int__ret__NativePair(a, 1);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	int xl_if_cond_24_2 = (b.x != 13);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	if ( xl_if_cond_24_2 )
	{
 		int xl_ret_25_3 = 2;
		if ( xl_generated_had_exception )
		{
						return 0;
		}
		if ( xl_generated_capture_native_error() )
		{
						return 0;
		}
		return xl_ret_25_3;
	}
	int xl_if_cond_27_2 = (b.y != 31);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	if ( xl_if_cond_27_2 )
	{
 		int xl_ret_28_3 = 3;
		if ( xl_generated_had_exception )
		{
						return 0;
		}
		if ( xl_generated_capture_native_error() )
		{
						return 0;
		}
		return xl_ret_28_3;
	}
	int xl_if_cond_30_2 = (xl__pair_value_sum__arity1__pair__NativePair__ret__int(b) == 44);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	if ( xl_if_cond_30_2 )
	{
 		int xl_ret_31_3 = 42;
		if ( xl_generated_had_exception )
		{
						return 0;
		}
		if ( xl_generated_capture_native_error() )
		{
						return 0;
		}
		return xl_ret_31_3;
	}
 	int xl_ret_33_2 = 4;
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	return xl_ret_33_2;
}

static xvalue xl__main__arity0__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int xl_func_result_95 = xl__main__arity0__ret__int();
	return xvoCreateInt((int64)(xl_func_result_95));
}

int xl_generated_entry(void)
{
	int xlrt_attached_here = xlrt_attach_generated_runtime();
	int xlrt_result = 0;
	xl_generated_runtime_reset();
	xlrt_result = xl__main__arity0__ret__int();
	xl_generated_async_trace_flush_to_env_file();
	xlrt_detach_generated_runtime(xlrt_attached_here);
	return xlrt_result;
}
