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

int xl_mod_std_task__ready = 1;
int xl_mod_std_task__done = 2;
static XL_FutureInt slow_add()
{
	xl_generated_async_trace_event("enter", "slow_add");
	int xl_async_ret_4_2 = 4;
	xl_generated_async_trace_event("return", "slow_add");
	return xl_future_int_ready(xl_async_ret_4_2);
	return xl_future_int_ready(0);
}

static XL_FutureInt slow_mul()
{
	xl_generated_async_trace_event("enter", "slow_mul");
	int xl_async_ret_8_2 = 6;
	xl_generated_async_trace_event("return", "slow_mul");
	return xl_future_int_ready(xl_async_ret_8_2);
	return xl_future_int_ready(0);
}

static XL_FutureInt slow_sub()
{
	xl_generated_async_trace_event("enter", "slow_sub");
	int xl_async_ret_12_2 = 3;
	xl_generated_async_trace_event("return", "slow_sub");
	return xl_future_int_ready(xl_async_ret_12_2);
	return xl_future_int_ready(0);
}

static int main()
{
	int winner = xl_task_race_int("xl_mod_std_task__race", xl_task_spawn_int("xl_mod_std_task__spawn", xl_future_int_await("slow_add", slow_add())), xl_task_all_int("xl_mod_std_task__all", xl_task_spawn_int("xl_mod_std_task__spawn", xl_future_int_await("slow_mul", slow_mul())), xl_task_spawn_int("xl_mod_std_task__spawn", xl_future_int_await("slow_sub", slow_sub()))));
	int canceled = xl_task_cancel_int("xl_mod_std_task__cancel", xl_task_spawn_int("xl_mod_std_task__spawn", xl_future_int_await("slow_mul", slow_mul())));
	return (winner + canceled);
}

int xl_generated_entry(void)
{
	xl_generated_runtime_reset();
	return main();
}
