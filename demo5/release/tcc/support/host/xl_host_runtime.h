#ifndef XL_HOST_RUNTIME_H
#define XL_HOST_RUNTIME_H

#include "../common/xl_common.h"

int xlrt_attach_generated_runtime(void);
void xlrt_detach_generated_runtime(int bAttachedHere);
const char* xlrt_host_name(void);
const char* xlrt_host_ip(void);
const char* xlrt_host_user(void);
const char* xlrt_host_os(void);
const char* xlrt_host_arch(void);
const char* xlrt_host_executable(void);
const char* xlrt_env_get(const char* sName);
int xlrt_env_exists(const char* sName);
int xlrt_env_set(const char* sName, const char* sValue);
const char* xlrt_env_cwd(void);
const char* xlrt_env_temp(void);
const char* xlrt_env_home(void);
void xlrt_native_error_clear(void);
void xlrt_native_error_raise(const char* sKind, const char* sMessage, int iCode, const char* sData);
int xlrt_native_error_has(void);
const char* xlrt_native_error_kind(void);
const char* xlrt_native_error_message(void);
int xlrt_native_error_code(void);
const char* xlrt_native_error_data(void);

#endif
