/*
ModuleLoadOrder
- [0] std.core -> <native:std.core> native=1 binding=std.core
- [1] std.error -> <native:std.error> native=1 binding=std.error
- [2] std.future -> <native:std.future> native=1 binding=std.future
- [3] std.str -> <native:std.str> native=1 binding=std.str
- [4] std.task -> <native:std.task> native=1 binding=std.task
- [5] std.time -> <native:std.time> native=1 binding=std.time
- [6] <root> -> D:\Git\x-lang\demo5\examples\native\type_method_std_api_basic.xl native=0 binding=
ModuleExports
- module=std.core
  abi init=xl_mod_std_core__init exports=xl_mod_std_core__exports descriptor=xl_mod_std_core__descriptor
  export identity -> xl_mod_std_core__identity
  export len -> xl_mod_std_core__len
  export print -> xl_mod_std_core__print
  export println -> xl_mod_std_core__println
  export version -> xl_mod_std_core__version
- module=std.error
  abi init=xl_mod_std_error__init exports=xl_mod_std_error__exports descriptor=xl_mod_std_error__descriptor
  export make -> xl_mod_std_error__make
  export type_error -> xl_mod_std_error__type_error
  export value_error -> xl_mod_std_error__value_error
  export io_error -> xl_mod_std_error__io_error
  export timeout_error -> xl_mod_std_error__timeout_error
  export cancelled_error -> xl_mod_std_error__cancelled_error
  export not_implemented_error -> xl_mod_std_error__not_implemented_error
  export native_error -> xl_mod_std_error__native_error
  export is_null -> xl_mod_std_error__is_null
  export kind -> xl_mod_std_error__kind
  export message -> xl_mod_std_error__message
  export code -> xl_mod_std_error__code
  export data -> xl_mod_std_error__data
  export stack -> xl_mod_std_error__stack
  export cause -> xl_mod_std_error__cause
  export text -> xl_mod_std_error__text
  export is_type -> xl_mod_std_error__is_type
  export is_native -> xl_mod_std_error__is_native
- module=std.future
  abi init=xl_mod_std_future__init exports=xl_mod_std_future__exports descriptor=xl_mod_std_future__descriptor
  export ready -> xl_mod_std_future__ready
  export wait -> xl_mod_std_future__wait
- module=std.str
  abi init=xl_mod_std_str__init exports=xl_mod_std_str__exports descriptor=xl_mod_std_str__descriptor
  export empty -> xl_mod_std_str__empty
  export byte_len -> xl_mod_std_str__byte_len
  export contains -> xl_mod_std_str__contains
  export index_of -> xl_mod_std_str__index_of
  export last_index_of -> xl_mod_std_str__last_index_of
  export slice -> xl_mod_std_str__slice
  export repeat -> xl_mod_std_str__repeat
  export split -> xl_mod_std_str__split
  export split_lines -> xl_mod_std_str__split_lines
  export starts_with -> xl_mod_std_str__starts_with
  export ends_with -> xl_mod_std_str__ends_with
  export trim -> xl_mod_std_str__trim
  export trim_left -> xl_mod_std_str__trim_left
  export trim_right -> xl_mod_std_str__trim_right
  export lower -> xl_mod_std_str__lower
  export upper -> xl_mod_std_str__upper
  export replace -> xl_mod_std_str__replace
  export reverse -> xl_mod_std_str__reverse
  export pad_left -> xl_mod_std_str__pad_left
  export pad_right -> xl_mod_std_str__pad_right
- module=std.task
  abi init=xl_mod_std_task__init exports=xl_mod_std_task__exports descriptor=xl_mod_std_task__descriptor
  export ready -> xl_mod_std_task__ready
  export done -> xl_mod_std_task__done
  export spawn -> xl_mod_std_task__spawn
  export all -> xl_mod_std_task__all
  export race -> xl_mod_std_task__race
  export cancel -> xl_mod_std_task__cancel
  export delay -> xl_mod_std_task__delay
- module=std.time
  abi init=xl_mod_std_time__init exports=xl_mod_std_time__exports descriptor=xl_mod_std_time__descriptor
  export now -> xl_mod_std_time__now
  export to_string -> xl_mod_std_time__to_string
  export now_text -> xl_mod_std_time__now_text
  export date_text -> xl_mod_std_time__date_text
  export time_text -> xl_mod_std_time__time_text
  export relative -> xl_mod_std_time__relative
  export parse -> xl_mod_std_time__parse
  export format -> xl_mod_std_time__format
  export parse_format -> xl_mod_std_time__parse_format
  export sleep -> xl_mod_std_time__sleep
  export is_leap_year -> xl_mod_std_time__is_leap_year
  export millis_per_second -> xl_mod_std_time__millis_per_second
  export seconds_per_day -> xl_mod_std_time__seconds_per_day
- module=<root>
  abi init=xl_mod__root___init exports=xl_mod__root___exports descriptor=xl_mod__root___descriptor
NativeRegistry
- key=std.core binding=std.core path=<native:std.core>
- key=std.crypto binding=std.crypto path=<native:std.crypto>
- key=std.debug binding=std.debug path=<native:std.debug>
- key=std.channel binding=std.channel path=<native:std.channel>
- key=std.clock binding=std.clock path=<native:std.clock>
- key=std.dict binding=std.dict path=<native:std.dict>
- key=std.dir binding=std.dir path=<native:std.dir>
- key=std.encode binding=std.encode path=<native:std.encode>
- key=std.env binding=std.env path=<native:std.env>
- key=std.error binding=std.error path=<native:std.error>
- key=std.fmt binding=std.fmt path=<native:std.fmt>
- key=std.fs binding=std.fs path=<native:std.fs>
- key=std.future binding=std.future path=<native:std.future>
- key=std.hash binding=std.hash path=<native:std.hash>
- key=std.host binding=std.host path=<native:std.host>
- key=std.net.http binding=std.net.http path=<native:std.net.http>
- key=std.net.httpd binding=std.net.httpd path=<native:std.net.httpd>
- key=std.net.dns binding=std.net.dns path=<native:std.net.dns>
- key=std.net.tcp binding=std.net.tcp path=<native:std.net.tcp>
- key=std.net.udp binding=std.net.udp path=<native:std.net.udp>
- key=std.net.ws binding=std.net.ws path=<native:std.net.ws>
- key=std.net.proxy binding=std.net.proxy path=<native:std.net.proxy>
- key=std.net.tls binding=std.net.tls path=<native:std.net.tls>
- key=std.io binding=std.io path=<native:std.io>
- key=std.iter binding=std.iter path=<native:std.iter>
- key=std.json binding=std.json path=<native:std.json>
- key=std.list binding=std.list path=<native:std.list>
- key=std.assert binding=std.assert path=<native:std.assert>
- key=std.atomic binding=std.atomic path=<native:std.atomic>
- key=std.log binding=std.log path=<native:std.log>
- key=std.math binding=std.math path=<native:std.math>
- key=std.module binding=std.module path=<native:std.module>
- key=std.net binding=std.net path=<native:std.net>
- key=std.net.ip binding=std.net.ip path=<native:std.net.ip>
- key=std.net.mime binding=std.net.mime path=<native:std.net.mime>
- key=std.net.multipart binding=std.net.multipart path=<native:std.net.multipart>
- key=std.net.url binding=std.net.url path=<native:std.net.url>
- key=std.os binding=std.os path=<native:std.os>
- key=std.pack binding=std.pack path=<native:std.pack>
- key=std.path binding=std.path path=<native:std.path>
- key=std.rand binding=std.rand path=<native:std.rand>
- key=std.regex binding=std.regex path=<native:std.regex>
- key=std.process binding=std.process path=<native:std.process>
- key=std.query binding=std.query path=<native:std.query>
- key=std.range binding=std.range path=<native:std.range>
- key=std.array binding=std.array path=<native:std.array>
- key=std.set binding=std.set path=<native:std.set>
- key=std.str binding=std.str path=<native:std.str>
- key=std.sys.lib binding=std.sys.lib path=<native:std.sys.lib>
- key=std.sys.env binding=std.sys.env path=<native:std.sys.env>
- key=std.sys.file binding=std.sys.file path=<native:std.sys.file>
- key=std.sys.mem binding=std.sys.mem path=<native:std.sys.mem>
- key=std.sys.net binding=std.sys.net path=<native:std.sys.net>
- key=std.sys.process binding=std.sys.process path=<native:std.sys.process>
- key=std.sys.wait binding=std.sys.wait path=<native:std.sys.wait>
- key=std.sync binding=std.sync path=<native:std.sync>
- key=std.template binding=std.template path=<native:std.template>
- key=std.thread binding=std.thread path=<native:std.thread>
- key=std.trace binding=std.trace path=<native:std.trace>
- key=std.time binding=std.time path=<native:std.time>
- key=std.task binding=std.task path=<native:std.task>
- key=std.types binding=std.types path=<native:std.types>
*/
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

static int xl_mod_std_core__version = 0;
static int xl_mod_std_task__ready = 0;
static int xl_mod_std_task__done = 0;
static int xl_mod_std_time__millis_per_second = 0;
static int xl_mod_std_time__seconds_per_day = 0;

static int xl_generated_module_init_done = 0;
static void xl_generated_module_init(void)
{
	if ( xl_generated_module_init_done )
	{
		return;
	}
	xl_mod_std_core__version = 5;
	if ( xl_generated_had_exception )
	{
				return;
	}
	if ( xl_generated_capture_native_error() )
	{
				return;
	}
	xl_mod_std_task__ready = 1;
	if ( xl_generated_had_exception )
	{
				return;
	}
	if ( xl_generated_capture_native_error() )
	{
				return;
	}
	xl_mod_std_task__done = 2;
	if ( xl_generated_had_exception )
	{
				return;
	}
	if ( xl_generated_capture_native_error() )
	{
				return;
	}
	xl_mod_std_time__millis_per_second = 1000;
	if ( xl_generated_had_exception )
	{
				return;
	}
	if ( xl_generated_capture_native_error() )
	{
				return;
	}
	xl_mod_std_time__seconds_per_day = 86400;
	if ( xl_generated_had_exception )
	{
				return;
	}
	if ( xl_generated_capture_native_error() )
	{
				return;
	}
	xl_generated_module_init_done = 1;
}

const XL_GeneratedModuleExport xl_mod_std_core__exports[] = {
	{ "identity", "xl_mod_std_core__identity" },
	{ "len", "xl_mod_std_core__len" },
	{ "print", "xl_mod_std_core__print" },
	{ "println", "xl_mod_std_core__println" },
	{ "version", "xl_mod_std_core__version" }
};
void xl_mod_std_core__init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod_std_core__descriptor = {
	"std.core",
	"package@0.0.0#abi1::std.core",
	"xl_mod_std_core__init",
	"xl_mod_std_core__exports",
	"xl_mod_std_core__descriptor",
	5,
	xl_mod_std_core__exports
};

const XL_GeneratedModuleExport xl_mod_std_error__exports[] = {
	{ "make", "xl_mod_std_error__make" },
	{ "type_error", "xl_mod_std_error__type_error" },
	{ "value_error", "xl_mod_std_error__value_error" },
	{ "io_error", "xl_mod_std_error__io_error" },
	{ "timeout_error", "xl_mod_std_error__timeout_error" },
	{ "cancelled_error", "xl_mod_std_error__cancelled_error" },
	{ "not_implemented_error", "xl_mod_std_error__not_implemented_error" },
	{ "native_error", "xl_mod_std_error__native_error" },
	{ "is_null", "xl_mod_std_error__is_null" },
	{ "kind", "xl_mod_std_error__kind" },
	{ "message", "xl_mod_std_error__message" },
	{ "code", "xl_mod_std_error__code" },
	{ "data", "xl_mod_std_error__data" },
	{ "stack", "xl_mod_std_error__stack" },
	{ "cause", "xl_mod_std_error__cause" },
	{ "text", "xl_mod_std_error__text" },
	{ "is_type", "xl_mod_std_error__is_type" },
	{ "is_native", "xl_mod_std_error__is_native" }
};
void xl_mod_std_error__init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod_std_error__descriptor = {
	"std.error",
	"package@0.0.0#abi1::std.error",
	"xl_mod_std_error__init",
	"xl_mod_std_error__exports",
	"xl_mod_std_error__descriptor",
	18,
	xl_mod_std_error__exports
};

const XL_GeneratedModuleExport xl_mod_std_future__exports[] = {
	{ "ready", "xl_mod_std_future__ready" },
	{ "wait", "xl_mod_std_future__wait" }
};
void xl_mod_std_future__init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod_std_future__descriptor = {
	"std.future",
	"package@0.0.0#abi1::std.future",
	"xl_mod_std_future__init",
	"xl_mod_std_future__exports",
	"xl_mod_std_future__descriptor",
	2,
	xl_mod_std_future__exports
};

const XL_GeneratedModuleExport xl_mod_std_str__exports[] = {
	{ "empty", "xl_mod_std_str__empty" },
	{ "byte_len", "xl_mod_std_str__byte_len" },
	{ "contains", "xl_mod_std_str__contains" },
	{ "index_of", "xl_mod_std_str__index_of" },
	{ "last_index_of", "xl_mod_std_str__last_index_of" },
	{ "slice", "xl_mod_std_str__slice" },
	{ "repeat", "xl_mod_std_str__repeat" },
	{ "split", "xl_mod_std_str__split" },
	{ "split_lines", "xl_mod_std_str__split_lines" },
	{ "starts_with", "xl_mod_std_str__starts_with" },
	{ "ends_with", "xl_mod_std_str__ends_with" },
	{ "trim", "xl_mod_std_str__trim" },
	{ "trim_left", "xl_mod_std_str__trim_left" },
	{ "trim_right", "xl_mod_std_str__trim_right" },
	{ "lower", "xl_mod_std_str__lower" },
	{ "upper", "xl_mod_std_str__upper" },
	{ "replace", "xl_mod_std_str__replace" },
	{ "reverse", "xl_mod_std_str__reverse" },
	{ "pad_left", "xl_mod_std_str__pad_left" },
	{ "pad_right", "xl_mod_std_str__pad_right" }
};
void xl_mod_std_str__init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod_std_str__descriptor = {
	"std.str",
	"package@0.0.0#abi1::std.str",
	"xl_mod_std_str__init",
	"xl_mod_std_str__exports",
	"xl_mod_std_str__descriptor",
	20,
	xl_mod_std_str__exports
};

const XL_GeneratedModuleExport xl_mod_std_task__exports[] = {
	{ "ready", "xl_mod_std_task__ready" },
	{ "done", "xl_mod_std_task__done" },
	{ "spawn", "xl_mod_std_task__spawn" },
	{ "all", "xl_mod_std_task__all" },
	{ "race", "xl_mod_std_task__race" },
	{ "cancel", "xl_mod_std_task__cancel" },
	{ "delay", "xl_mod_std_task__delay" }
};
void xl_mod_std_task__init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod_std_task__descriptor = {
	"std.task",
	"package@0.0.0#abi1::std.task",
	"xl_mod_std_task__init",
	"xl_mod_std_task__exports",
	"xl_mod_std_task__descriptor",
	7,
	xl_mod_std_task__exports
};

const XL_GeneratedModuleExport xl_mod_std_time__exports[] = {
	{ "now", "xl_mod_std_time__now" },
	{ "to_string", "xl_mod_std_time__to_string" },
	{ "now_text", "xl_mod_std_time__now_text" },
	{ "date_text", "xl_mod_std_time__date_text" },
	{ "time_text", "xl_mod_std_time__time_text" },
	{ "relative", "xl_mod_std_time__relative" },
	{ "parse", "xl_mod_std_time__parse" },
	{ "format", "xl_mod_std_time__format" },
	{ "parse_format", "xl_mod_std_time__parse_format" },
	{ "sleep", "xl_mod_std_time__sleep" },
	{ "is_leap_year", "xl_mod_std_time__is_leap_year" },
	{ "millis_per_second", "xl_mod_std_time__millis_per_second" },
	{ "seconds_per_day", "xl_mod_std_time__seconds_per_day" }
};
void xl_mod_std_time__init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod_std_time__descriptor = {
	"std.time",
	"package@0.0.0#abi1::std.time",
	"xl_mod_std_time__init",
	"xl_mod_std_time__exports",
	"xl_mod_std_time__descriptor",
	13,
	xl_mod_std_time__exports
};

const XL_GeneratedModuleExport xl_mod__root___exports[] = {
	{ NULL, NULL }
};
void xl_mod__root___init(void)
{
	xl_generated_module_init();
}
const XL_GeneratedModuleDescriptor xl_mod__root___descriptor = {
	"<root>",
	"type_method_std_api_basic@0.0.0#abi1::<root>",
	"xl_mod__root___init",
	"xl_mod__root___exports",
	"xl_mod__root___descriptor",
	0,
	xl_mod__root___exports
};

static int xl__xl_mod_std_core__identity__arity1__x__int__ret__int(int x);
static xvalue xl__xl_mod_std_core__identity__arity1__x__int__ret__int__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__print__arity1__x__int__ret__void(int x);
static xvalue xl__xl_mod_std_core__print__arity1__x__int__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__print__arity1__x__float__ret__void(double x);
static xvalue xl__xl_mod_std_core__print__arity1__x__float__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__print__arity1__x__string__ret__void(const char* x);
static xvalue xl__xl_mod_std_core__print__arity1__x__string__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__print__arity1__x__bool__ret__void(int x);
static xvalue xl__xl_mod_std_core__print__arity1__x__bool__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__println__arity0__ret__void();
static xvalue xl__xl_mod_std_core__println__arity0__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__println__arity1__x__int__ret__void(int x);
static xvalue xl__xl_mod_std_core__println__arity1__x__int__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__println__arity1__x__float__ret__void(double x);
static xvalue xl__xl_mod_std_core__println__arity1__x__float__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__println__arity1__x__string__ret__void(const char* x);
static xvalue xl__xl_mod_std_core__println__arity1__x__string__ret__void__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_core__println__arity1__x__bool__ret__void(int x);
static xvalue xl__xl_mod_std_core__println__arity1__x__bool__ret__void__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_core__len__arity1__x__string__ret__int(const char* x);
static xvalue xl__xl_mod_std_core__len__arity1__x__string__ret__int__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_core__len__arity1__x__array__ret__int(xvalue x);
static xvalue xl__xl_mod_std_core__len__arity1__x__array__ret__int__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_core__len__arity1__x__list__ret__int(xvalue x);
static xvalue xl__xl_mod_std_core__len__arity1__x__list__ret__int__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_core__len__arity1__x__set__ret__int(xvalue x);
static xvalue xl__xl_mod_std_core__len__arity1__x__set__ret__int__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_core__len__arity1__x__dict__ret__int(xvalue x);
static xvalue xl__xl_mod_std_core__len__arity1__x__dict__ret__int__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__make__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__make__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__make__arity2__message__string__code__int__ret__error(const char* message, int code);
static xvalue xl__xl_mod_std_error__make__arity2__message__string__code__int__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__make__arity3__message__string__code__int__data__string__ret__error(const char* message, int code, const char* data);
static xvalue xl__xl_mod_std_error__make__arity3__message__string__code__int__data__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__type_error__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__type_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__value_error__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__value_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__io_error__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__io_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__timeout_error__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__timeout_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__cancelled_error__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__cancelled_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__not_implemented_error__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__not_implemented_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__native_error__arity1__message__string__ret__error(const char* message);
static xvalue xl__xl_mod_std_error__native_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_error__is_null__arity1__value__error__ret__bool(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__is_null__arity1__value__error__ret__bool__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_error__kind__arity1__value__error__ret__string(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__kind__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_error__message__arity1__value__error__ret__string(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__message__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_error__code__arity1__value__error__ret__int(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__code__arity1__value__error__ret__int__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_error__data__arity1__value__error__ret__string(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__data__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_error__stack__arity1__value__error__ret__string(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__stack__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam);
static XL_GeneratedError* xl__xl_mod_std_error__cause__arity1__value__error__ret__error(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__cause__arity1__value__error__ret__error__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_error__text__arity1__value__error__ret__string(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__text__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_error__is_type__arity2__value__error__type_name__string__ret__bool(XL_GeneratedError* value, const char* type_name);
static xvalue xl__xl_mod_std_error__is_type__arity2__value__error__type_name__string__ret__bool__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_error__is_native__arity1__value__error__ret__bool(XL_GeneratedError* value);
static xvalue xl__xl_mod_std_error__is_native__arity1__value__error__ret__bool__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_str__empty__arity1__text__string__ret__bool(const char* text);
static xvalue xl__xl_mod_std_str__empty__arity1__text__string__ret__bool__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_str__byte_len__arity1__text__string__ret__int(const char* text);
static xvalue xl__xl_mod_std_str__byte_len__arity1__text__string__ret__int__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_str__contains__arity2__text__string__part__string__ret__bool(const char* text, const char* part);
static xvalue xl__xl_mod_std_str__contains__arity2__text__string__part__string__ret__bool__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_str__index_of__arity2__text__string__part__string__ret__int(const char* text, const char* part);
static xvalue xl__xl_mod_std_str__index_of__arity2__text__string__part__string__ret__int__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_str__last_index_of__arity2__text__string__part__string__ret__int(const char* text, const char* part);
static xvalue xl__xl_mod_std_str__last_index_of__arity2__text__string__part__string__ret__int__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__slice__arity3__text__string__start__int__count__int__ret__string(const char* text, int start, int count);
static xvalue xl__xl_mod_std_str__slice__arity3__text__string__start__int__count__int__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__repeat__arity2__text__string__count__int__ret__string(const char* text, int count);
static xvalue xl__xl_mod_std_str__repeat__arity2__text__string__count__int__ret__string__boxed(xvalue varENV, xvalue arrParam);
static xvalue xl__xl_mod_std_str__split__arity2__text__string__part__string__ret__list(const char* text, const char* part);
static xvalue xl__xl_mod_std_str__split__arity2__text__string__part__string__ret__list__boxed(xvalue varENV, xvalue arrParam);
static xvalue xl__xl_mod_std_str__split_lines__arity1__text__string__ret__list(const char* text);
static xvalue xl__xl_mod_std_str__split_lines__arity1__text__string__ret__list__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_str__starts_with__arity2__text__string__part__string__ret__bool(const char* text, const char* part);
static xvalue xl__xl_mod_std_str__starts_with__arity2__text__string__part__string__ret__bool__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_str__ends_with__arity2__text__string__part__string__ret__bool(const char* text, const char* part);
static xvalue xl__xl_mod_std_str__ends_with__arity2__text__string__part__string__ret__bool__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__trim__arity1__text__string__ret__string(const char* text);
static xvalue xl__xl_mod_std_str__trim__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__trim_left__arity1__text__string__ret__string(const char* text);
static xvalue xl__xl_mod_std_str__trim_left__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__trim_right__arity1__text__string__ret__string(const char* text);
static xvalue xl__xl_mod_std_str__trim_right__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__lower__arity1__text__string__ret__string(const char* text);
static xvalue xl__xl_mod_std_str__lower__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__upper__arity1__text__string__ret__string(const char* text);
static xvalue xl__xl_mod_std_str__upper__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__replace__arity3__text__string__part__string__value__string__ret__string(const char* text, const char* part, const char* value);
static xvalue xl__xl_mod_std_str__replace__arity3__text__string__part__string__value__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__reverse__arity1__text__string__ret__string(const char* text);
static xvalue xl__xl_mod_std_str__reverse__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__pad_left__arity3__text__string__width__int__fill__string__ret__string(const char* text, int width, const char* fill);
static xvalue xl__xl_mod_std_str__pad_left__arity3__text__string__width__int__fill__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_str__pad_right__arity3__text__string__width__int__fill__string__ret__string(const char* text, int width, const char* fill);
static xvalue xl__xl_mod_std_str__pad_right__arity3__text__string__width__int__fill__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cint_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam);
static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cfloat_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam);
static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cstring_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam);
static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cbool_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam);
static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cerror_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam);
static xtime xl__xl_mod_std_time__now__arity0__ret__time();
static xvalue xl__xl_mod_std_time__now__arity0__ret__time__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_time__to_string__arity1__value__time__ret__string(xtime value);
static xvalue xl__xl_mod_std_time__to_string__arity1__value__time__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_time__now_text__arity0__ret__string();
static xvalue xl__xl_mod_std_time__now_text__arity0__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_time__date_text__arity0__ret__string();
static xvalue xl__xl_mod_std_time__date_text__arity0__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_time__time_text__arity0__ret__string();
static xvalue xl__xl_mod_std_time__time_text__arity0__ret__string__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_time__relative__arity2__value__time__base__time__ret__string(xtime value, xtime base);
static xvalue xl__xl_mod_std_time__relative__arity2__value__time__base__time__ret__string__boxed(xvalue varENV, xvalue arrParam);
static xtime xl__xl_mod_std_time__parse__arity1__value__string__ret__time(const char* value);
static xvalue xl__xl_mod_std_time__parse__arity1__value__string__ret__time__boxed(xvalue varENV, xvalue arrParam);
static const char* xl__xl_mod_std_time__format__arity2__value__time__pattern__string__ret__string(xtime value, const char* pattern);
static xvalue xl__xl_mod_std_time__format__arity2__value__time__pattern__string__ret__string__boxed(xvalue varENV, xvalue arrParam);
static xtime xl__xl_mod_std_time__parse_format__arity2__value__string__pattern__string__ret__time(const char* value, const char* pattern);
static xvalue xl__xl_mod_std_time__parse_format__arity2__value__string__pattern__string__ret__time__boxed(xvalue varENV, xvalue arrParam);
static void xl__xl_mod_std_time__sleep__arity1__ms__int__ret__void(int ms);
static xvalue xl__xl_mod_std_time__sleep__arity1__ms__int__ret__void__boxed(xvalue varENV, xvalue arrParam);
static int xl__xl_mod_std_time__is_leap_year__arity1__year__int__ret__bool(int year);
static xvalue xl__xl_mod_std_time__is_leap_year__arity1__year__int__ret__bool__boxed(xvalue varENV, xvalue arrParam);

static int xl__xl_mod_std_core__identity__arity1__x__int__ret__int(int x)
{
	int xl_escape_kind_1_6 = 0;
	int xl_escape_level_1_6 = 0;
	int xl_escape_value_1_6 = 0;
 	int xl_ret_2_2 = x;
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	return xl_ret_2_2;
}

static xvalue xl__xl_mod_std_core__identity__arity1__x__int__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int x = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_92 = xl__xl_mod_std_core__identity__arity1__x__int__ret__int(x);
	return xvoCreateInt((int64)(xl_func_result_92));
}

static void xl__xl_mod_std_core__print__arity1__x__int__ret__void(int x)
{
	xl_core_print_int(x);
	return;
}

static xvalue xl__xl_mod_std_core__print__arity1__x__int__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int x = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xl__xl_mod_std_core__print__arity1__x__int__ret__void(x);
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__print__arity1__x__float__ret__void(double x)
{
	xl_core_print_float(x);
	return;
}

static xvalue xl__xl_mod_std_core__print__arity1__x__float__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	double x = ((double)xvoGetFloat((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xl__xl_mod_std_core__print__arity1__x__float__ret__void(x);
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__print__arity1__x__string__ret__void(const char* x)
{
	xl_core_print_string(x);
	return;
}

static xvalue xl__xl_mod_std_core__print__arity1__x__string__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* x = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xl__xl_mod_std_core__print__arity1__x__string__ret__void(x);
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__print__arity1__x__bool__ret__void(int x)
{
	xl_core_print_bool(x);
	return;
}

static xvalue xl__xl_mod_std_core__print__arity1__x__bool__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int x = (xvoGetBool((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))) ? 1 : 0);
	xl__xl_mod_std_core__print__arity1__x__bool__ret__void(x);
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__println__arity0__ret__void()
{
	xl_core_println_void();
	return;
}

static xvalue xl__xl_mod_std_core__println__arity0__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xl__xl_mod_std_core__println__arity0__ret__void();
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__println__arity1__x__int__ret__void(int x)
{
	xl_core_println_int(x);
	return;
}

static xvalue xl__xl_mod_std_core__println__arity1__x__int__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int x = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xl__xl_mod_std_core__println__arity1__x__int__ret__void(x);
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__println__arity1__x__float__ret__void(double x)
{
	xl_core_println_float(x);
	return;
}

static xvalue xl__xl_mod_std_core__println__arity1__x__float__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	double x = ((double)xvoGetFloat((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xl__xl_mod_std_core__println__arity1__x__float__ret__void(x);
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__println__arity1__x__string__ret__void(const char* x)
{
	xl_core_println_string(x);
	return;
}

static xvalue xl__xl_mod_std_core__println__arity1__x__string__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* x = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xl__xl_mod_std_core__println__arity1__x__string__ret__void(x);
	return xvoCreateNull();
}

static void xl__xl_mod_std_core__println__arity1__x__bool__ret__void(int x)
{
	xl_core_println_bool(x);
	return;
}

static xvalue xl__xl_mod_std_core__println__arity1__x__bool__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int x = (xvoGetBool((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))) ? 1 : 0);
	xl__xl_mod_std_core__println__arity1__x__bool__ret__void(x);
	return xvoCreateNull();
}

static int xl__xl_mod_std_core__len__arity1__x__string__ret__int(const char* x)
{
	return xl_core_len_string(x);
}

static xvalue xl__xl_mod_std_core__len__arity1__x__string__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* x = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_102 = xl__xl_mod_std_core__len__arity1__x__string__ret__int(x);
	return xvoCreateInt((int64)(xl_func_result_102));
}

static int xl__xl_mod_std_core__len__arity1__x__array__ret__int(xvalue x)
{
	return xl_core_len_array(x);
}

static xvalue xl__xl_mod_std_core__len__arity1__x__array__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xvalue x = xvoCopy((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))));
	int xl_func_result_103 = xl__xl_mod_std_core__len__arity1__x__array__ret__int(x);
	return xvoCreateInt((int64)(xl_func_result_103));
}

static int xl__xl_mod_std_core__len__arity1__x__list__ret__int(xvalue x)
{
	return xl_core_len_list(x);
}

static xvalue xl__xl_mod_std_core__len__arity1__x__list__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xvalue x = xvoCopy((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))));
	int xl_func_result_104 = xl__xl_mod_std_core__len__arity1__x__list__ret__int(x);
	return xvoCreateInt((int64)(xl_func_result_104));
}

static int xl__xl_mod_std_core__len__arity1__x__set__ret__int(xvalue x)
{
	return xl_core_len_set(x);
}

static xvalue xl__xl_mod_std_core__len__arity1__x__set__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xvalue x = xvoCopy((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))));
	int xl_func_result_105 = xl__xl_mod_std_core__len__arity1__x__set__ret__int(x);
	return xvoCreateInt((int64)(xl_func_result_105));
}

static int xl__xl_mod_std_core__len__arity1__x__dict__ret__int(xvalue x)
{
	return xl_core_len_dict(x);
}

static xvalue xl__xl_mod_std_core__len__arity1__x__dict__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xvalue x = xvoCopy((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))));
	int xl_func_result_106 = xl__xl_mod_std_core__len__arity1__x__dict__ret__int(x);
	return xvoCreateInt((int64)(xl_func_result_106));
}

static XL_GeneratedError* xl__xl_mod_std_error__make__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_1_6 = 0;
	int xl_escape_level_1_6 = 0;
	XL_GeneratedError* xl_escape_value_1_6 = NULL;
 	XL_GeneratedError* xl_ret_2_2 = Error(message, 0, "", "", NULL);
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

static xvalue xl__xl_mod_std_error__make__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_107 = xl__xl_mod_std_error__make__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_107); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__make__arity2__message__string__code__int__ret__error(const char* message, int code)
{
	int xl_escape_kind_4_6 = 0;
	int xl_escape_level_4_6 = 0;
	XL_GeneratedError* xl_escape_value_4_6 = NULL;
 	XL_GeneratedError* xl_ret_5_2 = Error(message, code, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_5_2;
}

static xvalue xl__xl_mod_std_error__make__arity2__message__string__code__int__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int code = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	XL_GeneratedError* xl_func_result_108 = xl__xl_mod_std_error__make__arity2__message__string__code__int__ret__error(message, code);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_108); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__make__arity3__message__string__code__int__data__string__ret__error(const char* message, int code, const char* data)
{
	int xl_escape_kind_7_6 = 0;
	int xl_escape_level_7_6 = 0;
	XL_GeneratedError* xl_escape_value_7_6 = NULL;
 	XL_GeneratedError* xl_ret_8_2 = Error(message, code, data, "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_8_2;
}

static xvalue xl__xl_mod_std_error__make__arity3__message__string__code__int__data__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int code = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	const char* data = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(2)))));
	XL_GeneratedError* xl_func_result_109 = xl__xl_mod_std_error__make__arity3__message__string__code__int__data__string__ret__error(message, code, data);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_109); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__type_error__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_10_6 = 0;
	int xl_escape_level_10_6 = 0;
	XL_GeneratedError* xl_escape_value_10_6 = NULL;
 	XL_GeneratedError* xl_ret_11_2 = TypeError(message, 0, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_11_2;
}

static xvalue xl__xl_mod_std_error__type_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_110 = xl__xl_mod_std_error__type_error__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_110); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__value_error__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_13_6 = 0;
	int xl_escape_level_13_6 = 0;
	XL_GeneratedError* xl_escape_value_13_6 = NULL;
 	XL_GeneratedError* xl_ret_14_2 = ValueError(message, 0, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_14_2;
}

static xvalue xl__xl_mod_std_error__value_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_111 = xl__xl_mod_std_error__value_error__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_111); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__io_error__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_16_6 = 0;
	int xl_escape_level_16_6 = 0;
	XL_GeneratedError* xl_escape_value_16_6 = NULL;
 	XL_GeneratedError* xl_ret_17_2 = IOError(message, 0, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_17_2;
}

static xvalue xl__xl_mod_std_error__io_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_112 = xl__xl_mod_std_error__io_error__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_112); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__timeout_error__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_19_6 = 0;
	int xl_escape_level_19_6 = 0;
	XL_GeneratedError* xl_escape_value_19_6 = NULL;
 	XL_GeneratedError* xl_ret_20_2 = TimeoutError(message, 0, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_20_2;
}

static xvalue xl__xl_mod_std_error__timeout_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_113 = xl__xl_mod_std_error__timeout_error__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_113); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__cancelled_error__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_22_6 = 0;
	int xl_escape_level_22_6 = 0;
	XL_GeneratedError* xl_escape_value_22_6 = NULL;
 	XL_GeneratedError* xl_ret_23_2 = CancelledError(message, 0, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_23_2;
}

static xvalue xl__xl_mod_std_error__cancelled_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_114 = xl__xl_mod_std_error__cancelled_error__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_114); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__not_implemented_error__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_25_6 = 0;
	int xl_escape_level_25_6 = 0;
	XL_GeneratedError* xl_escape_value_25_6 = NULL;
 	XL_GeneratedError* xl_ret_26_2 = NotImplementedError(message, 0, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_26_2;
}

static xvalue xl__xl_mod_std_error__not_implemented_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_115 = xl__xl_mod_std_error__not_implemented_error__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_115); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__native_error__arity1__message__string__ret__error(const char* message)
{
	int xl_escape_kind_28_6 = 0;
	int xl_escape_level_28_6 = 0;
	XL_GeneratedError* xl_escape_value_28_6 = NULL;
 	XL_GeneratedError* xl_ret_29_2 = NativeError(message, 0, "", "", NULL);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_29_2;
}

static xvalue xl__xl_mod_std_error__native_error__arity1__message__string__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* message = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_116 = xl__xl_mod_std_error__native_error__arity1__message__string__ret__error(message);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_116); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static int xl__xl_mod_std_error__is_null__arity1__value__error__ret__bool(XL_GeneratedError* value)
{
	int xl_escape_kind_31_6 = 0;
	int xl_escape_level_31_6 = 0;
	int xl_escape_value_31_6 = 0;
 	int xl_ret_32_2 = xl_generated_text_equal(xl_generated_error_text(value), NULL);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	return xl_ret_32_2;
}

static xvalue xl__xl_mod_std_error__is_null__arity1__value__error__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_117 = xl__xl_mod_std_error__is_null__arity1__value__error__ret__bool(value);
	return xvoCreateBool((xl_func_result_117) ? TRUE : FALSE);
}

static const char* xl__xl_mod_std_error__kind__arity1__value__error__ret__string(XL_GeneratedError* value)
{
	int xl_escape_kind_34_6 = 0;
	int xl_escape_level_34_6 = 0;
	const char* xl_escape_value_34_6 = NULL;
 	const char* xl_ret_35_2 = xl_generated_error_type(value);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_35_2;
}

static xvalue xl__xl_mod_std_error__kind__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_118 = xl__xl_mod_std_error__kind__arity1__value__error__ret__string(value);
	return ({ const char* xl_text_value = (xl_func_result_118 != NULL) ? (xl_func_result_118) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_error__message__arity1__value__error__ret__string(XL_GeneratedError* value)
{
	int xl_escape_kind_37_6 = 0;
	int xl_escape_level_37_6 = 0;
	const char* xl_escape_value_37_6 = NULL;
 	const char* xl_ret_38_2 = xl_generated_error_message(value);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_38_2;
}

static xvalue xl__xl_mod_std_error__message__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_119 = xl__xl_mod_std_error__message__arity1__value__error__ret__string(value);
	return ({ const char* xl_text_value = (xl_func_result_119 != NULL) ? (xl_func_result_119) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static int xl__xl_mod_std_error__code__arity1__value__error__ret__int(XL_GeneratedError* value)
{
	int xl_escape_kind_40_6 = 0;
	int xl_escape_level_40_6 = 0;
	int xl_escape_value_40_6 = 0;
 	int xl_ret_41_2 = xl_generated_error_code(value);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	return xl_ret_41_2;
}

static xvalue xl__xl_mod_std_error__code__arity1__value__error__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_120 = xl__xl_mod_std_error__code__arity1__value__error__ret__int(value);
	return xvoCreateInt((int64)(xl_func_result_120));
}

static const char* xl__xl_mod_std_error__data__arity1__value__error__ret__string(XL_GeneratedError* value)
{
	int xl_escape_kind_43_6 = 0;
	int xl_escape_level_43_6 = 0;
	const char* xl_escape_value_43_6 = NULL;
 	const char* xl_ret_44_2 = xl_generated_error_data(value);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_44_2;
}

static xvalue xl__xl_mod_std_error__data__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_121 = xl__xl_mod_std_error__data__arity1__value__error__ret__string(value);
	return ({ const char* xl_text_value = (xl_func_result_121 != NULL) ? (xl_func_result_121) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_error__stack__arity1__value__error__ret__string(XL_GeneratedError* value)
{
	int xl_escape_kind_46_6 = 0;
	int xl_escape_level_46_6 = 0;
	const char* xl_escape_value_46_6 = NULL;
 	const char* xl_ret_47_2 = xl_generated_error_stack(value);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_47_2;
}

static xvalue xl__xl_mod_std_error__stack__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_122 = xl__xl_mod_std_error__stack__arity1__value__error__ret__string(value);
	return ({ const char* xl_text_value = (xl_func_result_122 != NULL) ? (xl_func_result_122) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static XL_GeneratedError* xl__xl_mod_std_error__cause__arity1__value__error__ret__error(XL_GeneratedError* value)
{
	int xl_escape_kind_49_6 = 0;
	int xl_escape_level_49_6 = 0;
	XL_GeneratedError* xl_escape_value_49_6 = NULL;
 	XL_GeneratedError* xl_ret_50_2 = xl_generated_error_cause(value);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_50_2;
}

static xvalue xl__xl_mod_std_error__cause__arity1__value__error__ret__error__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	XL_GeneratedError* xl_func_result_123 = xl__xl_mod_std_error__cause__arity1__value__error__ret__error(value);
	return ({ XL_GeneratedError* xl_err_value = (xl_func_result_123); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : ""), FALSE); });
}

static const char* xl__xl_mod_std_error__text__arity1__value__error__ret__string(XL_GeneratedError* value)
{
	int xl_escape_kind_52_6 = 0;
	int xl_escape_level_52_6 = 0;
	const char* xl_escape_value_52_6 = NULL;
 	const char* xl_ret_53_2 = xl_generated_error_text(value);
	if ( xl_generated_had_exception )
	{
				return NULL;
	}
	if ( xl_generated_capture_native_error() )
	{
				return NULL;
	}
	return xl_ret_53_2;
}

static xvalue xl__xl_mod_std_error__text__arity1__value__error__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_124 = xl__xl_mod_std_error__text__arity1__value__error__ret__string(value);
	return ({ const char* xl_text_value = (xl_func_result_124 != NULL) ? (xl_func_result_124) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static int xl__xl_mod_std_error__is_type__arity2__value__error__type_name__string__ret__bool(XL_GeneratedError* value, const char* type_name)
{
	int xl_escape_kind_55_6 = 0;
	int xl_escape_level_55_6 = 0;
	int xl_escape_value_55_6 = 0;
 	int xl_ret_56_2 = xl_generated_text_equal(xl_generated_error_type(value), type_name);
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	return xl_ret_56_2;
}

static xvalue xl__xl_mod_std_error__is_type__arity2__value__error__type_name__string__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* type_name = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	int xl_func_result_125 = xl__xl_mod_std_error__is_type__arity2__value__error__type_name__string__ret__bool(value, type_name);
	return xvoCreateBool((xl_func_result_125) ? TRUE : FALSE);
}

static int xl__xl_mod_std_error__is_native__arity1__value__error__ret__bool(XL_GeneratedError* value)
{
	int xl_escape_kind_58_6 = 0;
	int xl_escape_level_58_6 = 0;
	int xl_escape_value_58_6 = 0;
 	int xl_ret_59_2 = xl_generated_text_equal(xl_generated_error_type(value), "NativeError");
	if ( xl_generated_had_exception )
	{
				return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
				return 0;
	}
	return xl_ret_59_2;
}

static xvalue xl__xl_mod_std_error__is_native__arity1__value__error__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_GeneratedError* value = xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_126 = xl__xl_mod_std_error__is_native__arity1__value__error__ret__bool(value);
	return xvoCreateBool((xl_func_result_126) ? TRUE : FALSE);
}

static XL_FutureInt xl__xl_mod_std_future__ready__arity1__value__int__ret__future_x3Cint_x3E(int value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__ready");
	int xl_escape_kind_1_12 = 0;
	int xl_escape_level_1_12 = 0;
	int xl_escape_value_1_12 = 0;
	int xl_async_ret_2_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__ready");
	return xl_future_int_ready(xl_async_ret_2_2);
	return xl_future_int_ready(0);
}

static XL_FutureFloat xl__xl_mod_std_future__ready__arity1__value__float__ret__future_x3Cfloat_x3E(double value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__ready");
	int xl_escape_kind_4_12 = 0;
	int xl_escape_level_4_12 = 0;
	double xl_escape_value_4_12 = 0.0;
	double xl_async_ret_5_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_float_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_float_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__ready");
	return xl_future_float_ready(xl_async_ret_5_2);
	return xl_future_float_ready(0.0);
}

static XL_FutureString xl__xl_mod_std_future__ready__arity1__value__string__ret__future_x3Cstring_x3E(const char* value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__ready");
	int xl_escape_kind_7_12 = 0;
	int xl_escape_level_7_12 = 0;
	const char* xl_escape_value_7_12 = NULL;
	const char* xl_async_ret_8_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_string_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_string_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__ready");
	return xl_future_string_ready(xl_async_ret_8_2);
	return xl_future_string_ready(NULL);
}

static XL_FutureBool xl__xl_mod_std_future__ready__arity1__value__bool__ret__future_x3Cbool_x3E(int value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__ready");
	int xl_escape_kind_10_12 = 0;
	int xl_escape_level_10_12 = 0;
	int xl_escape_value_10_12 = 0;
	int xl_async_ret_11_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_bool_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_bool_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__ready");
	return xl_future_bool_ready(xl_async_ret_11_2);
	return xl_future_bool_ready(0);
}

static XL_FutureError xl__xl_mod_std_future__ready__arity1__value__error__ret__future_x3Cerror_x3E(XL_GeneratedError* value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__ready");
	int xl_escape_kind_13_12 = 0;
	int xl_escape_level_13_12 = 0;
	XL_GeneratedError* xl_escape_value_13_12 = NULL;
	XL_GeneratedError* xl_async_ret_14_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_error_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_error_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__ready");
	return xl_future_error_ready(xl_async_ret_14_2);
	return xl_future_error_ready(NULL);
}

static XL_FutureInt xl__xl_mod_std_future__wait__arity1__value__future_x3Cint_x3E__ret__future_x3Cint_x3E(XL_FutureInt value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__wait");
	int xl_escape_kind_17_12 = 0;
	int xl_escape_level_17_12 = 0;
	int xl_escape_value_17_12 = 0;
	XL_FutureInt xl_async_future_18_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__wait");
	return xl_future_int_forward_state("value", xl_async_future_18_2, "xl_mod_std_future__wait", 1, 2);
	return xl_future_int_ready(0);
}

static XL_FutureFloat xl__xl_mod_std_future__wait__arity1__value__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E(XL_FutureFloat value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__wait");
	int xl_escape_kind_20_12 = 0;
	int xl_escape_level_20_12 = 0;
	double xl_escape_value_20_12 = 0.0;
	XL_FutureFloat xl_async_future_21_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_float_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_float_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__wait");
	return xl_future_float_forward_state("value", xl_async_future_21_2, "xl_mod_std_future__wait", 1, 2);
	return xl_future_float_ready(0.0);
}

static XL_FutureString xl__xl_mod_std_future__wait__arity1__value__future_x3Cstring_x3E__ret__future_x3Cstring_x3E(XL_FutureString value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__wait");
	int xl_escape_kind_23_12 = 0;
	int xl_escape_level_23_12 = 0;
	const char* xl_escape_value_23_12 = NULL;
	XL_FutureString xl_async_future_24_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_string_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_string_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__wait");
	return xl_future_string_forward_state("value", xl_async_future_24_2, "xl_mod_std_future__wait", 1, 2);
	return xl_future_string_ready(NULL);
}

static XL_FutureBool xl__xl_mod_std_future__wait__arity1__value__future_x3Cbool_x3E__ret__future_x3Cbool_x3E(XL_FutureBool value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__wait");
	int xl_escape_kind_26_12 = 0;
	int xl_escape_level_26_12 = 0;
	int xl_escape_value_26_12 = 0;
	XL_FutureBool xl_async_future_27_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_bool_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_bool_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__wait");
	return xl_future_bool_forward_state("value", xl_async_future_27_2, "xl_mod_std_future__wait", 1, 2);
	return xl_future_bool_ready(0);
}

static XL_FutureError xl__xl_mod_std_future__wait__arity1__value__future_x3Cerror_x3E__ret__future_x3Cerror_x3E(XL_FutureError value)
{
	xl_generated_async_trace_event("enter", "xl_mod_std_future__wait");
	int xl_escape_kind_29_12 = 0;
	int xl_escape_level_29_12 = 0;
	XL_GeneratedError* xl_escape_value_29_12 = NULL;
	XL_FutureError xl_async_future_30_2 = value;
	if ( xl_generated_had_exception )
	{
				return xl_future_error_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_error_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "xl_mod_std_future__wait");
	return xl_future_error_forward_state("value", xl_async_future_30_2, "xl_mod_std_future__wait", 1, 2);
	return xl_future_error_ready(NULL);
}

static int xl__xl_mod_std_str__empty__arity1__text__string__ret__bool(const char* text)
{
	return xl_str_empty(text);
}

static xvalue xl__xl_mod_std_str__empty__arity1__text__string__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_137 = xl__xl_mod_std_str__empty__arity1__text__string__ret__bool(text);
	return xvoCreateBool((xl_func_result_137) ? TRUE : FALSE);
}

static int xl__xl_mod_std_str__byte_len__arity1__text__string__ret__int(const char* text)
{
	return xl_str_byte_len(text);
}

static xvalue xl__xl_mod_std_str__byte_len__arity1__text__string__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_138 = xl__xl_mod_std_str__byte_len__arity1__text__string__ret__int(text);
	return xvoCreateInt((int64)(xl_func_result_138));
}

static int xl__xl_mod_std_str__contains__arity2__text__string__part__string__ret__bool(const char* text, const char* part)
{
	return xl_str_contains(text, part);
}

static xvalue xl__xl_mod_std_str__contains__arity2__text__string__part__string__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* part = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	int xl_func_result_139 = xl__xl_mod_std_str__contains__arity2__text__string__part__string__ret__bool(text, part);
	return xvoCreateBool((xl_func_result_139) ? TRUE : FALSE);
}

static int xl__xl_mod_std_str__index_of__arity2__text__string__part__string__ret__int(const char* text, const char* part)
{
	return xl_str_index_of(text, part);
}

static xvalue xl__xl_mod_std_str__index_of__arity2__text__string__part__string__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* part = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	int xl_func_result_140 = xl__xl_mod_std_str__index_of__arity2__text__string__part__string__ret__int(text, part);
	return xvoCreateInt((int64)(xl_func_result_140));
}

static int xl__xl_mod_std_str__last_index_of__arity2__text__string__part__string__ret__int(const char* text, const char* part)
{
	return xl_str_last_index_of(text, part);
}

static xvalue xl__xl_mod_std_str__last_index_of__arity2__text__string__part__string__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* part = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	int xl_func_result_141 = xl__xl_mod_std_str__last_index_of__arity2__text__string__part__string__ret__int(text, part);
	return xvoCreateInt((int64)(xl_func_result_141));
}

static const char* xl__xl_mod_std_str__slice__arity3__text__string__start__int__count__int__ret__string(const char* text, int start, int count)
{
	return xl_str_slice(text, start, count);
}

static xvalue xl__xl_mod_std_str__slice__arity3__text__string__start__int__count__int__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int start = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	int count = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(2)))));
	const char* xl_func_result_142 = xl__xl_mod_std_str__slice__arity3__text__string__start__int__count__int__ret__string(text, start, count);
	return ({ const char* xl_text_value = (xl_func_result_142 != NULL) ? (xl_func_result_142) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__repeat__arity2__text__string__count__int__ret__string(const char* text, int count)
{
	return xl_str_repeat(text, count);
}

static xvalue xl__xl_mod_std_str__repeat__arity2__text__string__count__int__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int count = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	const char* xl_func_result_143 = xl__xl_mod_std_str__repeat__arity2__text__string__count__int__ret__string(text, count);
	return ({ const char* xl_text_value = (xl_func_result_143 != NULL) ? (xl_func_result_143) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static xvalue xl__xl_mod_std_str__split__arity2__text__string__part__string__ret__list(const char* text, const char* part)
{
	return xl_str_split(text, part);
}

static xvalue xl__xl_mod_std_str__split__arity2__text__string__part__string__ret__list__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* part = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	xvalue xl_func_result_144 = xl__xl_mod_std_str__split__arity2__text__string__part__string__ret__list(text, part);
	return xvoCopy((xvalue)(xl_func_result_144));
}

static xvalue xl__xl_mod_std_str__split_lines__arity1__text__string__ret__list(const char* text)
{
	return xl_str_split_lines(text);
}

static xvalue xl__xl_mod_std_str__split_lines__arity1__text__string__ret__list__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xvalue xl_func_result_145 = xl__xl_mod_std_str__split_lines__arity1__text__string__ret__list(text);
	return xvoCopy((xvalue)(xl_func_result_145));
}

static int xl__xl_mod_std_str__starts_with__arity2__text__string__part__string__ret__bool(const char* text, const char* part)
{
	return xl_str_starts_with(text, part);
}

static xvalue xl__xl_mod_std_str__starts_with__arity2__text__string__part__string__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* part = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	int xl_func_result_146 = xl__xl_mod_std_str__starts_with__arity2__text__string__part__string__ret__bool(text, part);
	return xvoCreateBool((xl_func_result_146) ? TRUE : FALSE);
}

static int xl__xl_mod_std_str__ends_with__arity2__text__string__part__string__ret__bool(const char* text, const char* part)
{
	return xl_str_ends_with(text, part);
}

static xvalue xl__xl_mod_std_str__ends_with__arity2__text__string__part__string__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* part = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	int xl_func_result_147 = xl__xl_mod_std_str__ends_with__arity2__text__string__part__string__ret__bool(text, part);
	return xvoCreateBool((xl_func_result_147) ? TRUE : FALSE);
}

static const char* xl__xl_mod_std_str__trim__arity1__text__string__ret__string(const char* text)
{
	return xl_str_trim(text);
}

static xvalue xl__xl_mod_std_str__trim__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_148 = xl__xl_mod_std_str__trim__arity1__text__string__ret__string(text);
	return ({ const char* xl_text_value = (xl_func_result_148 != NULL) ? (xl_func_result_148) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__trim_left__arity1__text__string__ret__string(const char* text)
{
	return xl_str_trim_left(text);
}

static xvalue xl__xl_mod_std_str__trim_left__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_149 = xl__xl_mod_std_str__trim_left__arity1__text__string__ret__string(text);
	return ({ const char* xl_text_value = (xl_func_result_149 != NULL) ? (xl_func_result_149) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__trim_right__arity1__text__string__ret__string(const char* text)
{
	return xl_str_trim_right(text);
}

static xvalue xl__xl_mod_std_str__trim_right__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_150 = xl__xl_mod_std_str__trim_right__arity1__text__string__ret__string(text);
	return ({ const char* xl_text_value = (xl_func_result_150 != NULL) ? (xl_func_result_150) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__lower__arity1__text__string__ret__string(const char* text)
{
	return xl_str_lower(text);
}

static xvalue xl__xl_mod_std_str__lower__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_151 = xl__xl_mod_std_str__lower__arity1__text__string__ret__string(text);
	return ({ const char* xl_text_value = (xl_func_result_151 != NULL) ? (xl_func_result_151) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__upper__arity1__text__string__ret__string(const char* text)
{
	return xl_str_upper(text);
}

static xvalue xl__xl_mod_std_str__upper__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_152 = xl__xl_mod_std_str__upper__arity1__text__string__ret__string(text);
	return ({ const char* xl_text_value = (xl_func_result_152 != NULL) ? (xl_func_result_152) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__replace__arity3__text__string__part__string__value__string__ret__string(const char* text, const char* part, const char* value)
{
	return xl_str_replace(text, part, value);
}

static xvalue xl__xl_mod_std_str__replace__arity3__text__string__part__string__value__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* part = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	const char* value = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(2)))));
	const char* xl_func_result_153 = xl__xl_mod_std_str__replace__arity3__text__string__part__string__value__string__ret__string(text, part, value);
	return ({ const char* xl_text_value = (xl_func_result_153 != NULL) ? (xl_func_result_153) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__reverse__arity1__text__string__ret__string(const char* text)
{
	return xl_str_reverse(text);
}

static xvalue xl__xl_mod_std_str__reverse__arity1__text__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* xl_func_result_154 = xl__xl_mod_std_str__reverse__arity1__text__string__ret__string(text);
	return ({ const char* xl_text_value = (xl_func_result_154 != NULL) ? (xl_func_result_154) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__pad_left__arity3__text__string__width__int__fill__string__ret__string(const char* text, int width, const char* fill)
{
	return xl_str_pad_left(text, width, fill);
}

static xvalue xl__xl_mod_std_str__pad_left__arity3__text__string__width__int__fill__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int width = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	const char* fill = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(2)))));
	const char* xl_func_result_155 = xl__xl_mod_std_str__pad_left__arity3__text__string__width__int__fill__string__ret__string(text, width, fill);
	return ({ const char* xl_text_value = (xl_func_result_155 != NULL) ? (xl_func_result_155) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_str__pad_right__arity3__text__string__width__int__fill__string__ret__string(const char* text, int width, const char* fill)
{
	return xl_str_pad_right(text, width, fill);
}

static xvalue xl__xl_mod_std_str__pad_right__arity3__text__string__width__int__fill__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* text = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int width = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	const char* fill = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(2)))));
	const char* xl_func_result_156 = xl__xl_mod_std_str__pad_right__arity3__text__string__width__int__fill__string__ret__string(text, width, fill);
	return ({ const char* xl_text_value = (xl_func_result_156 != NULL) ? (xl_func_result_156) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

typedef struct xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_state {
	xpromise* pPromise;
	int iStep;
	XL_FutureInt left;
	XL_FutureInt right;
} xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_state;

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg);
static void xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg);

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 0;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 2);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureInt left = pState->left;
	XL_FutureInt right = pState->right;
	int xl_async_return_await_21_2 = (int)(intptr_t)((pIn != NULL) ? pIn->pValue : NULL);
	xl_async_return_await_21_2;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear stmt failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_async_after_step_0:
	;
	pState->left = left;
	pState->right = right;
	XL_FutureInt xl_async_future_1 = right;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( xl_async_future_1.handle == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async await future missing");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 3);
	xl_generated_async_trace_event("await", "await");
	xfuture* xl_async_hook_1 = xl_generated_future_finally_auto(xl_async_future_1.handle, xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_resume_1, pState);
	if ( xl_async_hook_1 == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async linear hook failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xFutureRelease(xl_async_hook_1);
	return;
}

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cint_x3E__right__future_x3Cint_x3E__ret__future_x3Cint_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 1;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 4);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureInt left = pState->left;
	XL_FutureInt right = pState->right;
	int xl_async_return_await_22_9 = (int)(intptr_t)((pIn != NULL) ? pIn->pValue : NULL);
	xl_async_after_step_1:
	;
	xl_generated_async_trace_event("return", "xl_mod_std_task__all");
	int xl_async_ret_22_2 = xl_async_return_await_22_9;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear return failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( !xl_generated_promise_resolve_int(pState->pPromise, xl_async_ret_22_2) ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async resolve failed");
	}
	if ( pState->pPromise != NULL ) {
		xPromiseDestroy(pState->pPromise);
	}
	xrtFree(pState);
	return;
}

typedef struct xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_state {
	xpromise* pPromise;
	int iStep;
	XL_FutureFloat left;
	XL_FutureFloat right;
} xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_state;

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg);
static void xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg);

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 0;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 2);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureFloat left = pState->left;
	XL_FutureFloat right = pState->right;
	double* xl_async_result_ptr_xl_async_return_await_25_2 = (double*)((pIn != NULL) ? pIn->pValue : NULL);
	double xl_async_return_await_25_2 = (xl_async_result_ptr_xl_async_return_await_25_2 != NULL) ? (double)(*xl_async_result_ptr_xl_async_return_await_25_2) : (double)0;
	xl_async_return_await_25_2;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear stmt failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_async_after_step_0:
	;
	pState->left = left;
	pState->right = right;
	XL_FutureFloat xl_async_future_1 = right;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( xl_async_future_1.handle == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async await future missing");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 3);
	xl_generated_async_trace_event("await", "await");
	xfuture* xl_async_hook_1 = xl_generated_future_finally_auto(xl_async_future_1.handle, xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_resume_1, pState);
	if ( xl_async_hook_1 == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async linear hook failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xFutureRelease(xl_async_hook_1);
	return;
}

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cfloat_x3E__right__future_x3Cfloat_x3E__ret__future_x3Cfloat_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 1;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 4);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureFloat left = pState->left;
	XL_FutureFloat right = pState->right;
	double* xl_async_result_ptr_xl_async_return_await_26_9 = (double*)((pIn != NULL) ? pIn->pValue : NULL);
	double xl_async_return_await_26_9 = (xl_async_result_ptr_xl_async_return_await_26_9 != NULL) ? (double)(*xl_async_result_ptr_xl_async_return_await_26_9) : (double)0;
	xl_async_after_step_1:
	;
	xl_generated_async_trace_event("return", "xl_mod_std_task__all");
	double xl_async_ret_26_2 = xl_async_return_await_26_9;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear return failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( !xl_generated_promise_resolve_float(pState->pPromise, xl_async_ret_26_2) ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async resolve failed");
	}
	if ( pState->pPromise != NULL ) {
		xPromiseDestroy(pState->pPromise);
	}
	xrtFree(pState);
	return;
}

typedef struct xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_state {
	xpromise* pPromise;
	int iStep;
	XL_FutureString left;
	XL_FutureString right;
} xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_state;

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg);
static void xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg);

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 0;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 2);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureString left = pState->left;
	XL_FutureString right = pState->right;
	const char* xl_async_return_await_29_2 = (const char*)(((pIn != NULL) && (pIn->pValue != NULL)) ? pIn->pValue : "");
	xl_async_return_await_29_2;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear stmt failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_async_after_step_0:
	;
	pState->left = left;
	pState->right = right;
	XL_FutureString xl_async_future_1 = right;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( xl_async_future_1.handle == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async await future missing");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 3);
	xl_generated_async_trace_event("await", "await");
	xfuture* xl_async_hook_1 = xl_generated_future_finally_auto(xl_async_future_1.handle, xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_resume_1, pState);
	if ( xl_async_hook_1 == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async linear hook failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xFutureRelease(xl_async_hook_1);
	return;
}

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cstring_x3E__right__future_x3Cstring_x3E__ret__future_x3Cstring_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 1;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 4);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureString left = pState->left;
	XL_FutureString right = pState->right;
	const char* xl_async_return_await_30_9 = (const char*)(((pIn != NULL) && (pIn->pValue != NULL)) ? pIn->pValue : "");
	xl_async_after_step_1:
	;
	xl_generated_async_trace_event("return", "xl_mod_std_task__all");
	const char* xl_async_ret_30_2 = xl_async_return_await_30_9;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear return failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( !xl_generated_promise_resolve_string(pState->pPromise, xl_async_ret_30_2) ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async resolve failed");
	}
	if ( pState->pPromise != NULL ) {
		xPromiseDestroy(pState->pPromise);
	}
	xrtFree(pState);
	return;
}

typedef struct xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_state {
	xpromise* pPromise;
	int iStep;
	XL_FutureBool left;
	XL_FutureBool right;
} xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_state;

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg);
static void xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg);

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 0;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 2);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureBool left = pState->left;
	XL_FutureBool right = pState->right;
	int xl_async_return_await_33_2 = (((intptr_t)((pIn != NULL) ? pIn->pValue : NULL)) != 0) ? 1 : 0;
	xl_async_return_await_33_2;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear stmt failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_async_after_step_0:
	;
	pState->left = left;
	pState->right = right;
	XL_FutureBool xl_async_future_1 = right;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( xl_async_future_1.handle == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async await future missing");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 3);
	xl_generated_async_trace_event("await", "await");
	xfuture* xl_async_hook_1 = xl_generated_future_finally_auto(xl_async_future_1.handle, xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_resume_1, pState);
	if ( xl_async_hook_1 == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async linear hook failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xFutureRelease(xl_async_hook_1);
	return;
}

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cbool_x3E__right__future_x3Cbool_x3E__ret__future_x3Cbool_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 1;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 4);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureBool left = pState->left;
	XL_FutureBool right = pState->right;
	int xl_async_return_await_34_9 = (((intptr_t)((pIn != NULL) ? pIn->pValue : NULL)) != 0) ? 1 : 0;
	xl_async_after_step_1:
	;
	xl_generated_async_trace_event("return", "xl_mod_std_task__all");
	int xl_async_ret_34_2 = xl_async_return_await_34_9;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear return failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( !xl_generated_promise_resolve_bool(pState->pPromise, xl_async_ret_34_2) ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async resolve failed");
	}
	if ( pState->pPromise != NULL ) {
		xPromiseDestroy(pState->pPromise);
	}
	xrtFree(pState);
	return;
}

typedef struct xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_state {
	xpromise* pPromise;
	int iStep;
	XL_FutureError left;
	XL_FutureError right;
} xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_state;

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg);
static void xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg);

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_resume(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 0;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 2);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureError left = pState->left;
	XL_FutureError right = pState->right;
	XL_GeneratedError* xl_async_return_await_37_2 = (XL_GeneratedError*)((pIn != NULL) ? pIn->pValue : NULL);
	xl_async_return_await_37_2;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear stmt failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_async_after_step_0:
	;
	pState->left = left;
	pState->right = right;
	XL_FutureError xl_async_future_1 = right;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( xl_async_future_1.handle == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async await future missing");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 3);
	xl_generated_async_trace_event("await", "await");
	xfuture* xl_async_hook_1 = xl_generated_future_finally_auto(xl_async_future_1.handle, xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_resume_1, pState);
	if ( xl_async_hook_1 == NULL ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async linear hook failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	xFutureRelease(xl_async_hook_1);
	return;
}

static void xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_resume_1(const xfuture_result* pIn, ptr pArg)
{
	xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_state* pState = (xl__xl_mod_std_task__all__arity2__left__future_x3Cerror_x3E__right__future_x3Cerror_x3E__ret__future_x3Cerror_x3E__async_linear_state*)pArg;
	if ( pState == NULL ) {
		return;
	}
	pState->iStep = 1;
	xl_generated_async_trace_event("resume", "await");
	xl_generated_async_trace_state_event("xl_mod_std_task__all", 4);
	if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {
		(void)xl_generated_promise_reject_result(pState->pPromise, pIn, "await failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	XL_FutureError left = pState->left;
	XL_FutureError right = pState->right;
	XL_GeneratedError* xl_async_return_await_38_9 = (XL_GeneratedError*)((pIn != NULL) ? pIn->pValue : NULL);
	xl_async_after_step_1:
	;
	xl_generated_async_trace_event("return", "xl_mod_std_task__all");
	XL_GeneratedError* xl_async_ret_38_2 = xl_async_return_await_38_9;
	if ( xl_generated_async_has_pending_error() )
	{
		(void)xl_generated_promise_reject_current(pState->pPromise, "async linear return failed");
		if ( pState->pPromise != NULL ) {
			xPromiseDestroy(pState->pPromise);
		}
		xrtFree(pState);
		return;
	}
	if ( !xl_generated_promise_resolve_error(pState->pPromise, xl_async_ret_38_2) ) {
		(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, "async resolve failed");
	}
	if ( pState->pPromise != NULL ) {
		xPromiseDestroy(pState->pPromise);
	}
	xrtFree(pState);
	return;
}

static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cint_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_FutureInt value = xl_future_int_ready(((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))))));
	int xl_func_result_172 = xl__xl_mod_std_task__cancel__arity1__value__future_x3Cint_x3E__ret__int(value);
	return xvoCreateInt((int64)(xl_func_result_172));
}

static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cfloat_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_FutureFloat value = xl_future_float_ready(((double)xvoGetFloat((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))))));
	int xl_func_result_173 = xl__xl_mod_std_task__cancel__arity1__value__future_x3Cfloat_x3E__ret__int(value);
	return xvoCreateInt((int64)(xl_func_result_173));
}

static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cstring_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_FutureString value = xl_future_string_ready(xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))))));
	int xl_func_result_174 = xl__xl_mod_std_task__cancel__arity1__value__future_x3Cstring_x3E__ret__int(value);
	return xvoCreateInt((int64)(xl_func_result_174));
}

static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cbool_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_FutureBool value = xl_future_bool_ready((xvoGetBool((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))) ? 1 : 0));
	int xl_func_result_175 = xl__xl_mod_std_task__cancel__arity1__value__future_x3Cbool_x3E__ret__int(value);
	return xvoCreateInt((int64)(xl_func_result_175));
}

static xvalue xl__xl_mod_std_task__cancel__arity1__value__future_x3Cerror_x3E__ret__int__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	XL_FutureError value = xl_future_error_ready(xl_generated_error_from_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))))));
	int xl_func_result_176 = xl__xl_mod_std_task__cancel__arity1__value__future_x3Cerror_x3E__ret__int(value);
	return xvoCreateInt((int64)(xl_func_result_176));
}

static xtime xl__xl_mod_std_time__now__arity0__ret__time()
{
	return xl_time_now();
}

static xvalue xl__xl_mod_std_time__now__arity0__ret__time__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xtime xl_func_result_178 = xl__xl_mod_std_time__now__arity0__ret__time();
	return xvoCreateTime(xl_func_result_178);
}

static const char* xl__xl_mod_std_time__to_string__arity1__value__time__ret__string(xtime value)
{
	return xl_time_to_string(value);
}

static xvalue xl__xl_mod_std_time__to_string__arity1__value__time__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xtime value = xvoGetTime((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))));
	const char* xl_func_result_179 = xl__xl_mod_std_time__to_string__arity1__value__time__ret__string(value);
	return ({ const char* xl_text_value = (xl_func_result_179 != NULL) ? (xl_func_result_179) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_time__now_text__arity0__ret__string()
{
	return xl_time_now_text();
}

static xvalue xl__xl_mod_std_time__now_text__arity0__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* xl_func_result_180 = xl__xl_mod_std_time__now_text__arity0__ret__string();
	return ({ const char* xl_text_value = (xl_func_result_180 != NULL) ? (xl_func_result_180) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_time__date_text__arity0__ret__string()
{
	return xl_time_date_text();
}

static xvalue xl__xl_mod_std_time__date_text__arity0__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* xl_func_result_181 = xl__xl_mod_std_time__date_text__arity0__ret__string();
	return ({ const char* xl_text_value = (xl_func_result_181 != NULL) ? (xl_func_result_181) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_time__time_text__arity0__ret__string()
{
	return xl_time_time_text();
}

static xvalue xl__xl_mod_std_time__time_text__arity0__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* xl_func_result_182 = xl__xl_mod_std_time__time_text__arity0__ret__string();
	return ({ const char* xl_text_value = (xl_func_result_182 != NULL) ? (xl_func_result_182) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static const char* xl__xl_mod_std_time__relative__arity2__value__time__base__time__ret__string(xtime value, xtime base)
{
	return xl_time_relative(value, base);
}

static xvalue xl__xl_mod_std_time__relative__arity2__value__time__base__time__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xtime value = xvoGetTime((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))));
	xtime base = xvoGetTime((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1))));
	const char* xl_func_result_183 = xl__xl_mod_std_time__relative__arity2__value__time__base__time__ret__string(value, base);
	return ({ const char* xl_text_value = (xl_func_result_183 != NULL) ? (xl_func_result_183) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static xtime xl__xl_mod_std_time__parse__arity1__value__string__ret__time(const char* value)
{
	return xl_time_parse(value);
}

static xvalue xl__xl_mod_std_time__parse__arity1__value__string__ret__time__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* value = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xtime xl_func_result_184 = xl__xl_mod_std_time__parse__arity1__value__string__ret__time(value);
	return xvoCreateTime(xl_func_result_184);
}

static const char* xl__xl_mod_std_time__format__arity2__value__time__pattern__string__ret__string(xtime value, const char* pattern)
{
	return xl_time_format(value, pattern);
}

static xvalue xl__xl_mod_std_time__format__arity2__value__time__pattern__string__ret__string__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	xtime value = xvoGetTime((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0))));
	const char* pattern = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	const char* xl_func_result_185 = xl__xl_mod_std_time__format__arity2__value__time__pattern__string__ret__string(value, pattern);
	return ({ const char* xl_text_value = (xl_func_result_185 != NULL) ? (xl_func_result_185) : ""; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); });
}

static xtime xl__xl_mod_std_time__parse_format__arity2__value__string__pattern__string__ret__time(const char* value, const char* pattern)
{
	return xl_time_parse_format(value, pattern);
}

static xvalue xl__xl_mod_std_time__parse_format__arity2__value__string__pattern__string__ret__time__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	const char* value = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	const char* pattern = xl_generated_copy_text(xl_value_to_string((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(1)))));
	xtime xl_func_result_186 = xl__xl_mod_std_time__parse_format__arity2__value__string__pattern__string__ret__time(value, pattern);
	return xvoCreateTime(xl_func_result_186);
}

static void xl__xl_mod_std_time__sleep__arity1__ms__int__ret__void(int ms)
{
	xl_time_sleep(ms);
	return;
}

static xvalue xl__xl_mod_std_time__sleep__arity1__ms__int__ret__void__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int ms = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	xl__xl_mod_std_time__sleep__arity1__ms__int__ret__void(ms);
	return xvoCreateNull();
}

static int xl__xl_mod_std_time__is_leap_year__arity1__year__int__ret__bool(int year)
{
	return xl_time_is_leap_year(year);
}

static xvalue xl__xl_mod_std_time__is_leap_year__arity1__year__int__ret__bool__boxed(xvalue varENV, xvalue arrParam)
{
	(void)varENV;
	int year = ((int)xvoGetInt((xvalue)(xvoArrayGetValue((xvalue)(arrParam), (uint32)(0)))));
	int xl_func_result_188 = xl__xl_mod_std_time__is_leap_year__arity1__year__int__ret__bool(year);
	return xvoCreateBool((xl_func_result_188) ? TRUE : FALSE);
}

static XL_FutureInt xl__main__arity0__ret__future_x3Cint_x3E()
{
	xl_generated_async_trace_event("enter", "main");
	int xl_escape_kind_8_12 = 0;
	int xl_escape_level_8_12 = 0;
	int xl_escape_value_8_12 = 0;
	xtime parsed = xl_time_parse_format("2024-02-29", "yyyy-mm-dd");
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	const char* text = xl_time_to_string(parsed);
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	const char* fmt = xl_time_format(parsed, "yyyy-mm-dd");
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	const char* rel = xl_time_relative(parsed, parsed);
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	XL_GeneratedError* e = xl__xl_mod_std_error__make__arity3__message__string__code__int__data__string__ret__error("boom", 7, "payload");
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	XL_FutureInt ready = xl__xl_mod_std_future__ready__arity1__value__int__ret__future_x3Cint_x3E(40);
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	XL_FutureInt delayed = xl_task_delay("xl_mod_std_task__delay", 1);
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	int xl_if_cond_17_2 = (xl_str_contains(text, "2024") == false);
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_17_2 )
	{
		int xl_async_ret_18_3 = 1;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_18_3);
	}
	int xl_if_cond_20_2 = (!xl_generated_text_equal(fmt, "2024-02-29"));
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_20_2 )
	{
		int xl_async_ret_21_3 = 2;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_21_3);
	}
	int xl_if_cond_23_2 = xl_generated_text_equal(rel, "");
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_23_2 )
	{
		int xl_async_ret_24_3 = 3;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_24_3);
	}
	int xl_if_cond_26_2 = (!xl_generated_text_equal(xl__xl_mod_std_error__kind__arity1__value__error__ret__string(e), "Error"));
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_26_2 )
	{
		int xl_async_ret_27_3 = 4;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_27_3);
	}
	int xl_if_cond_29_2 = (((!xl_generated_text_equal(xl__xl_mod_std_error__message__arity1__value__error__ret__string(e), "boom")) || (xl__xl_mod_std_error__code__arity1__value__error__ret__int(e) != 7)) || (!xl_generated_text_equal(xl__xl_mod_std_error__data__arity1__value__error__ret__string(e), "payload")));
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_29_2 )
	{
		int xl_async_ret_30_3 = 5;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_30_3);
	}
	int xl_if_cond_32_2 = (((!xl_generated_text_equal(xl__xl_mod_std_error__text__arity1__value__error__ret__string(e), "boom")) || (!xl__xl_mod_std_error__is_type__arity2__value__error__type_name__string__ret__bool(e, "Error"))) || xl__xl_mod_std_error__is_native__arity1__value__error__ret__bool(e));
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_32_2 )
	{
		int xl_async_ret_33_3 = 6;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_33_3);
	}
	int xl_if_cond_35_2 = ((!xl_generated_text_equal(xl_mod_std_module__key((void*)&xl_mod_std_core__descriptor), "std.core")) || (xl_mod_std_module__export_count((void*)&xl_mod_std_core__descriptor) < 1));
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_35_2 )
	{
		int xl_async_ret_36_3 = 7;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_36_3);
	}
	int xl_if_cond_38_2 = ((!xl_mod_std_module__has_export((void*)&xl_mod_std_core__descriptor, "println")) || xl_generated_text_equal(xl_mod_std_module__export_symbol((void*)&xl_mod_std_core__descriptor, "println"), ""));
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_38_2 )
	{
		int xl_async_ret_39_3 = 8;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_39_3);
	}
	int xl_if_cond_41_2 = (xl_generated_text_equal(xl_mod_std_module__abi_id((void*)&xl_mod_std_core__descriptor), "") || xl_generated_text_equal(xl_mod_std_module__descriptor_symbol((void*)&xl_mod_std_core__descriptor), ""));
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_if_cond_41_2 )
	{
		int xl_async_ret_42_3 = 9;
		if ( xl_generated_had_exception )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		if ( xl_generated_capture_native_error() )
		{
						return xl_future_int_error(xl_generated_capture_exception_error());
		}
		xl_generated_async_trace_event("return", "main");
		return xl_future_int_ready(xl_async_ret_42_3);
	}
	int xl_async_ret_45_2 = ((xl_future_int_await_state("xl_mod_std_future__wait", xl__xl_mod_std_future__wait__arity1__value__future_x3Cint_x3E__ret__future_x3Cint_x3E(ready), "main", 17, 18) + xl_task_cancel_int("xl_mod_std_task__cancel", delayed)) + 1);
	if ( xl_generated_had_exception )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	if ( xl_generated_capture_native_error() )
	{
				return xl_future_int_error(xl_generated_capture_exception_error());
	}
	xl_generated_async_trace_event("return", "main");
	return xl_future_int_ready(xl_async_ret_45_2);
	return xl_future_int_ready(0);
}

int xl_generated_entry(void)
{
	int xlrt_attached_here = xlrt_attach_generated_runtime();
	int xlrt_result = 0;
	xl_generated_runtime_reset();
	xl_generated_module_init();
	if ( xl_generated_had_exception )
	{
		xl_generated_async_trace_flush_to_env_file();
		xlrt_detach_generated_runtime(xlrt_attached_here);
		return 0;
	}
	if ( xl_generated_capture_native_error() )
	{
		xl_generated_async_trace_flush_to_env_file();
		xlrt_detach_generated_runtime(xlrt_attached_here);
		return 0;
	}
	xlrt_result = xl_future_int_await("main", xl__main__arity0__ret__future_x3Cint_x3E());
	xl_generated_async_trace_flush_to_env_file();
	xlrt_detach_generated_runtime(xlrt_attached_here);
	return xlrt_result;
}
