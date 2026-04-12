#ifndef XLRT_GENERATED_H
#define XLRT_GENERATED_H

#include "xlrt_profile.h"
#include "../../lib/xrt/xrt.h"

extern int xlrt_attach_generated_runtime(void);
extern void xlrt_detach_generated_runtime(int bAttachedHere);
extern const char* xlrt_host_name(void);
extern const char* xlrt_host_ip(void);
extern const char* xlrt_env_get(const char* sName);
extern int xlrt_env_exists(const char* sName);
extern const char* xlrt_env_cwd(void);
extern void xlrt_native_error_clear(void);
extern void xlrt_native_error_raise(const char* sKind, const char* sMessage, int iCode, const char* sData);
extern int xlrt_native_error_has(void);
extern const char* xlrt_native_error_kind(void);
extern const char* xlrt_native_error_message(void);
extern int xlrt_native_error_code(void);
extern const char* xlrt_native_error_data(void);

extern void xl_core_print_int(int value);
extern void xl_core_print_float(double value);
extern void xl_core_print_string(const char* value);
extern void xl_core_print_bool(int value);
extern void xl_core_println_void(void);
extern void xl_core_println_int(int value);
extern void xl_core_println_float(double value);
extern void xl_core_println_string(const char* value);
extern void xl_core_println_bool(int value);
extern int xl_core_len_string(const char* value);
extern int xl_core_len_array(xvalue value);
extern int xl_core_len_list(xvalue value);
extern int xl_core_len_set(xvalue value);
extern int xl_core_len_dict(xvalue value);
extern const char* xl_point_to_string(void* value);
extern const char* xl_value_to_string(xvalue value);
extern void xl_array_append(xvalue value, xvalue item);
extern void xl_list_append(xvalue value, xvalue item);
extern const char* xl_array_join(xvalue value, const char* sSep);
extern const char* xl_list_join(xvalue value, const char* sSep);
extern void xl_array_clear(xvalue value);
extern void xl_list_clear(xvalue value);
extern void xl_set_clear(xvalue value);
extern void xl_dict_clear(xvalue value);
extern int xl_array_remove(xvalue value, int index);
extern int xl_list_remove(xvalue value, int index);
extern int xl_set_remove(xvalue value, xvalue item);
extern int xl_dict_remove(xvalue value, const char* key);
extern xvalue xl_channel_create(int iCapacity);
extern int xl_channel_send(xvalue value, xvalue item);
extern xvalue xl_channel_recv(xvalue value);
extern int xl_channel_close(xvalue value);

extern xtime xl_time_now(void);
extern const char* xl_time_to_string(xtime value);
extern const char* xl_time_now_text(void);
extern const char* xl_time_date_text(void);
extern const char* xl_time_time_text(void);
extern const char* xl_time_relative(xtime value, xtime base);
extern xtime xl_time_parse(const char* sText);

extern const char* xl_path_join(const char* sLeft, const char* sRight);
extern const char* xl_path_name_ext(const char* sPath);
extern const char* xl_path_name(const char* sPath);
extern const char* xl_path_ext(const char* sPath);
extern const char* xl_path_dir(const char* sPath);
extern int xl_path_is_abs(const char* sPath);
extern const char* xl_path_random(const char* sHead, const char* sFoot, int iLen);

extern int xl_fs_exists(const char* sPath);
extern int xl_fs_is_file(const char* sPath);
extern int xl_fs_is_dir(const char* sPath);
extern int xl_fs_size(const char* sPath);
extern const char* xl_fs_read_all(const char* sPath);
extern int xl_fs_write_all(const char* sPath, const char* sText);
extern int xl_fs_append_all(const char* sPath, const char* sText);
extern int xl_fs_mkdir_all(const char* sPath);
extern int xl_fs_copy(const char* sSrc, const char* sDst);
extern int xl_fs_move(const char* sSrc, const char* sDst);
extern int xl_fs_delete(const char* sPath);

extern int xl_str_contains(const char* sText, const char* sPart);
extern int xl_str_index_of(const char* sText, const char* sPart);
extern int xl_str_last_index_of(const char* sText, const char* sPart);
extern const char* xl_str_slice(const char* sText, int iStart, int iCount);
extern const char* xl_str_repeat(const char* sText, int iCount);
extern xvalue xl_str_split(const char* sText, const char* sPart);
extern xvalue xl_str_split_lines(const char* sText);
extern int xl_str_starts_with(const char* sText, const char* sPart);
extern int xl_str_ends_with(const char* sText, const char* sPart);
extern const char* xl_str_concat(const char* sLeft, const char* sRight);
extern const char* xl_str_trim(const char* sText);
extern const char* xl_str_trim_left(const char* sText);
extern const char* xl_str_trim_right(const char* sText);
extern const char* xl_str_lower(const char* sText);
extern const char* xl_str_upper(const char* sText);
extern const char* xl_str_replace(const char* sText, const char* sPart, const char* sValue);

extern int xl_rand_int32(void);
extern int xl_rand_range(int iMin, int iMax);
extern const char* xl_rand_text(int iLen);

extern const char* xl_host_name(void);
extern const char* xl_host_ip(void);
extern const char* xl_env_get(const char* sName);
extern int xl_env_exists(const char* sName);
extern const char* xl_env_cwd(void);

extern const char* xl_dir_list(const char* sPath);
extern const char* xl_dir_files(const char* sPath);
extern const char* xl_dir_dirs(const char* sPath);
extern const char* xl_dir_tree(const char* sPath);

extern const char* xl_fmt_int_text(int value, const char* sPattern);
extern const char* xl_fmt_float_text(double value, const char* sPattern);

extern int xl_process_exec(const char* sCommand);
extern const char* xl_process_capture(const char* sCommand);
extern int xl_process_exec_in(const char* sCommand, const char* sWorkDir);
extern const char* xl_process_capture_in(const char* sCommand, const char* sWorkDir);

#endif
