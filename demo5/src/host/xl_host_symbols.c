#include "xl_host_symbols.h"

#include "xl_host_runtime.h"
#include "../runtime/xlrt_generated.h"
#include "../runtime/xlrt_profile.h"
#include "../../lib/xrt/xrt.h"

extern xvalue xl_sys_loadlib(const char* sPath);
extern void* xl_sys_lib_proc(xvalue value, const char* sName);
extern int xl_sys_lib_close(xvalue value);
extern const char* xl_sys_lib_path(xvalue value);
extern int xl_sys_lib_loaded(xvalue value);

typedef struct XL_HostSymbolEntry {
	const char* sName;
	const void* pValue;
} XL_HostSymbolEntry;

/*
 * Wrap point helpers locally before handing them to TCC.
 * This avoids relying on direct addresses from the single-header xrt build.
 */
static xvalue __xlHostXvoCreatePoint(ptr pValue)
{
	return xvoCreatePoint(pValue);
}

static ptr __xlHostXvoGetPoint(xvalue value)
{
	return xvoGetPoint(value);
}

static xvalue __xlHostXvoCreateFunc(xfunction pFunc)
{
	return xvoCreateFunc(pFunc);
}

static xvalue __xlHostXvoCreateFuncEx(xfunction pFunc, xvalue pEnv)
{
	return xvoCreateFuncEx(pFunc, pEnv);
}

static xvalue __xlHostXvoCreateClass(uint32 iSize)
{
	return xvoCreateClass(iSize);
}

static ptr __xlHostXvoGetClass(xvalue value)
{
	return xvoGetClass(value);
}

static xfunction __xlHostXvoGetFunc(xvalue value)
{
	return xvoGetFunc(value);
}

static xvalue __xlHostXvoGetFuncEnv(xvalue value)
{
	return xvoGetFuncEnv(value);
}

static int __xlAddSymbol(TCCState* pTcc, const char* sName, const void* pValue)
{
	char sAlias[160];

	if ( (pTcc == NULL) || (sName == NULL) || (pValue == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( tcc_add_symbol(pTcc, sName, pValue) != 0 ) {
		return XL_RC_FAIL;
	}
#if defined(_WIN32) || defined(_WIN64)
	snprintf(sAlias, sizeof(sAlias), "_%s", sName);
	tcc_add_symbol(pTcc, sAlias, pValue);
	snprintf(sAlias, sizeof(sAlias), "__imp_%s", sName);
	tcc_add_symbol(pTcc, sAlias, pValue);
	snprintf(sAlias, sizeof(sAlias), "__imp__%s", sName);
	tcc_add_symbol(pTcc, sAlias, pValue);
#endif
	return XL_RC_OK;
}

static const XL_HostSymbolEntry __gXLHostSymbols[] = {
	{ "xrtThreadIsAttached", xrtThreadIsAttached },
	{ "xrtThreadAttachCurrent", xrtThreadAttachCurrent },
	{ "xrtThreadDetachCurrent", xrtThreadDetachCurrent },
	{ "xrtThreadCreate", xrtThreadCreate },
	{ "xrtThreadDestroy", xrtThreadDestroy },
	{ "xrtThreadWait", xrtThreadWait },
	{ "xrtThreadWaitTimeout", xrtThreadWaitTimeout },
	{ "xrtThreadStop", xrtThreadStop },
	{ "xrtThreadShouldStop", xrtThreadShouldStop },
	{ "xrtThreadGetState", xrtThreadGetState },
	{ "xrtThreadGetExitCode", xrtThreadGetExitCode },
	{ "xrtThreadGetCurrentId", xrtThreadGetCurrentId },
	{ "xrtThreadYield", xrtThreadYield },
	{ "xrtNow", xrtNow },
	{ "xrtTimeToStr", xrtTimeToStr },
	{ "xrtNowStr", xrtNowStr },
	{ "xrtDateStr", xrtDateStr },
	{ "xrtTimeStr", xrtTimeStr },
	{ "xrtRelativeTime", xrtRelativeTime },
	{ "xrtStrToTime", xrtStrToTime },
	{ "xrtSleep", xrtSleep },
	{ "xrtPathJoin", xrtPathJoin },
	{ "xrtPathGetNameExt", xrtPathGetNameExt },
	{ "xrtPathGetName", xrtPathGetName },
	{ "xrtPathGetExt", xrtPathGetExt },
	{ "xrtPathGetDir", xrtPathGetDir },
	{ "xrtPathIsAbs", xrtPathIsAbs },
	{ "xrtPathRandom", xrtPathRandom },
	{ "xrtPathExists", xrtPathExists },
	{ "xrtFileExists", xrtFileExists },
	{ "xrtDirExists", xrtDirExists },
	{ "xrtFileGetSize", xrtFileGetSize },
	{ "xrtFileAppend", xrtFileAppend },
	{ "xrtFileWriteAll", xrtFileWriteAll },
	{ "xrtFileReadAll", xrtFileReadAll },
	{ "xrtFileCopy", xrtFileCopy },
	{ "xrtFileMove", xrtFileMove },
	{ "xrtFileDelete", xrtFileDelete },
	{ "xrtDirCreateAll", xrtDirCreateAll },
	{ "xrtDirScan", xrtDirScan },
	{ "xrtStrComp", xrtStrComp },
	{ "xrtFindStr", xrtFindStr },
	{ "xrtSplit", xrtSplit },
	{ "xrtLTrim", xrtLTrim },
	{ "xrtRTrim", xrtRTrim },
	{ "xrtTrim", xrtTrim },
	{ "xrtLCase", xrtLCase },
	{ "xrtUCase", xrtUCase },
	{ "xrtReplace", xrtReplace },
	{ "xrtIntFormat", xrtIntFormat },
	{ "xrtNumFormat", xrtNumFormat },
	{ "xrtRand32", xrtRand32 },
	{ "xrtRandRange", xrtRandRange },
	{ "xrtRandStr", xrtRandStr },
	{ "xrtProcessConfigInit", xrtProcessConfigInit },
	{ "xrtProcessSpawn", xrtProcessSpawn },
	{ "xrtProcessDestroy", xrtProcessDestroy },
	{ "xrtProcessState", xrtProcessState },
	{ "xrtProcessIsRunning", xrtProcessIsRunning },
	{ "xrtProcessExitCode", xrtProcessExitCode },
	{ "xrtProcessGetExitInfo", xrtProcessGetExitInfo },
	{ "xrtProcessTerminalSupported", xrtProcessTerminalSupported },
	{ "xrtProcessWrite", xrtProcessWrite },
	{ "xrtProcessWriteText", xrtProcessWriteText },
	{ "xrtProcessCloseStdin", xrtProcessCloseStdin },
	{ "xrtProcessWait", xrtProcessWait },
	{ "xrtProcessWaitTimeout", xrtProcessWaitTimeout },
	{ "xrtProcessInterrupt", xrtProcessInterrupt },
	{ "xrtProcessTerminate", xrtProcessTerminate },
	{ "xrtProcessKill", xrtProcessKill },
	{ "xrtProcessKillTree", xrtProcessKillTree },
	{ "xrtProcessResizeTerminal", xrtProcessResizeTerminal },
	{ "xrtProcessGetStdout", xrtProcessGetStdout },
	{ "xrtProcessGetStderr", xrtProcessGetStderr },
	{ "xrtProcessReadStdoutSince", xrtProcessReadStdoutSince },
	{ "xrtProcessReadStderrSince", xrtProcessReadStderrSince },
	{ "xrtProcessReadEventsSince", xrtProcessReadEventsSince },
	{ "xrtProcessWaitFuture", xrtProcessWaitFuture },
	{ "xrtExecCapture", xrtExecCapture },
	{ "xrtProcessResultUnit", xrtProcessResultUnit },
	{ "xrtNetResolve", xrtNetResolve },
	{ "xrtNetAddrToStr", xrtNetAddrToStr },
	{ "xrtMalloc", xrtMalloc },
	{ "xrtFree", xrtFree },
	{ "xrtCopyStr", xrtCopyStr },
	{ "xFutureCreate", xFutureCreate },
	{ "xFutureAddRef", xFutureAddRef },
	{ "xFutureRelease", xFutureRelease },
	{ "xFutureGetResult", xFutureGetResult },
	{ "xFutureSetDebugName", xFutureSetDebugName },
	{ "xFutureState", xFutureState },
	{ "xFutureWait", xFutureWait },
	{ "xFutureRequestCancel", xFutureRequestCancel },
	{ "xPromiseCreate", xPromiseCreate },
	{ "xPromiseDestroy", xPromiseDestroy },
	{ "xPromiseResolve", xPromiseResolve },
	{ "xPromiseReject", xPromiseReject },
	{ "xTaskRunDelayed", xTaskRunDelayed },
	{ "xFutureThenInline", xFutureThenInline },
	{ "xFutureThenCurrent", xFutureThenCurrent },
	{ "xFutureThenEngine", xFutureThenEngine },
	{ "xFutureFinallyInline", xFutureFinallyInline },
	{ "xFutureFinallyEngine", xFutureFinallyEngine },
	{ "xFutureWhenAll", xFutureWhenAll },
	{ "xFutureRace", xFutureRace },
	{ "xvoCreateNull", xvoCreateNull },
	{ "xvoCreateBool", xvoCreateBool },
	{ "xvoCreateInt", xvoCreateInt },
	{ "xvoCreateFloat", xvoCreateFloat },
	{ "xvoCreateText", xvoCreateText },
	{ "xvoCreateTime", xvoCreateTime },
	{ "xvoCreatePoint", __xlHostXvoCreatePoint },
	{ "xvoCreateFunc", __xlHostXvoCreateFunc },
	{ "xvoCreateFuncEx", __xlHostXvoCreateFuncEx },
	{ "xvoCreateClass", __xlHostXvoCreateClass },
	{ "xvoCopy", xvoCopy },
	{ "xvoUnref", xvoUnref },
	{ "xvoGetBool", xvoGetBool },
	{ "xvoGetInt", xvoGetInt },
	{ "xvoGetFloat", xvoGetFloat },
	{ "xvoGetText", xvoGetText },
	{ "xvoGetTime", xvoGetTime },
	{ "xvoGetPoint", __xlHostXvoGetPoint },
	{ "xvoGetFunc", __xlHostXvoGetFunc },
	{ "xvoGetFuncEnv", __xlHostXvoGetFuncEnv },
	{ "xvoGetClass", __xlHostXvoGetClass },
	{ "xvoGetColl", xvoGetColl },
	{ "xvoGetTable", xvoGetTable },
	{ "xvoGetSize", xvoGetSize },
	{ "xvoArrayItemCount", xvoArrayItemCount },
	{ "xvoListItemCount", xvoListItemCount },
	{ "xvoTypeName", xvoTypeName },
	{ "xvoIsNumber", xvoIsNumber },
	{ "xvoIsBasic", xvoIsBasic },
	{ "xvoIsContainer", xvoIsContainer },
	{ "xvoCanCompareBasic", xvoCanCompareBasic },
	{ "xvoBasicCompare", xvoBasicCompare },
	{ "xvoBasicEqual", xvoBasicEqual },
	{ "xvoCreateArray", xvoCreateArray },
	{ "xvoArrayGetValue", xvoArrayGetValue },
	{ "xvoArraySetValue", xvoArraySetValue },
	{ "xvoArrayAppendValue", xvoArrayAppendValue },
	{ "xvoCreateList", xvoCreateList },
	{ "xvoListGetValue", xvoListGetValue },
	{ "xvoListSetValue", xvoListSetValue },
	{ "xvoCreateColl", xvoCreateColl },
	{ "xvoCollExists", xvoCollExists },
	{ "xvoCreateTable", xvoCreateTable },
	{ "xvoCollSetValue", xvoCollSetValue },
	{ "xvoTableGetValue", xvoTableGetValue },
	{ "xvoTableExists", xvoTableExists },
	{ "xvoTableSetValue", xvoTableSetValue },
	{ "xrtAVLTB_IterBegin", xrtAVLTB_IterBegin },
	{ "xrtAVLTB_IterNext", xrtAVLTB_IterNext },
	{ "xrtAVLTB_IterEnd", xrtAVLTB_IterEnd },
	{ "xrtAVLTreeIterBegin", xrtAVLTreeIterBegin },
	{ "xrtAVLTreeIterNext", xrtAVLTreeIterNext },
	{ "xrtAVLTreeIterEnd", xrtAVLTreeIterEnd },
	{ "xlrt_attach_generated_runtime", xlrt_attach_generated_runtime },
	{ "xlrt_detach_generated_runtime", xlrt_detach_generated_runtime },
	{ "xlrt_host_name", xlrt_host_name },
	{ "xlrt_host_ip", xlrt_host_ip },
	{ "xlrt_host_user", xlrt_host_user },
	{ "xlrt_host_os", xlrt_host_os },
	{ "xlrt_host_arch", xlrt_host_arch },
	{ "xlrt_host_executable", xlrt_host_executable },
	{ "xlrt_env_get", xlrt_env_get },
	{ "xlrt_env_exists", xlrt_env_exists },
	{ "xlrt_env_set", xlrt_env_set },
	{ "xlrt_env_cwd", xlrt_env_cwd },
	{ "xlrt_env_temp", xlrt_env_temp },
	{ "xlrt_env_home", xlrt_env_home },
	{ "xlrt_native_error_clear", xlrt_native_error_clear },
	{ "xlrt_native_error_raise", xlrt_native_error_raise },
	{ "xlrt_native_error_has", xlrt_native_error_has },
	{ "xlrt_native_error_kind", xlrt_native_error_kind },
	{ "xlrt_native_error_message", xlrt_native_error_message },
	{ "xlrt_native_error_code", xlrt_native_error_code },
	{ "xlrt_native_error_data", xlrt_native_error_data },
	{ "xl_sys_loadlib", xl_sys_loadlib },
	{ "xl_sys_lib_proc", xl_sys_lib_proc },
	{ "xl_sys_lib_close", xl_sys_lib_close },
	{ "xl_sys_lib_path", xl_sys_lib_path },
	{ "xl_sys_lib_loaded", xl_sys_lib_loaded },
	{ "xl_core_print_int", xl_core_print_int },
	{ "xl_core_print_float", xl_core_print_float },
	{ "xl_core_print_string", xl_core_print_string },
	{ "xl_core_print_bool", xl_core_print_bool },
	{ "xl_core_println_void", xl_core_println_void },
	{ "xl_core_println_int", xl_core_println_int },
	{ "xl_core_println_float", xl_core_println_float },
	{ "xl_core_println_string", xl_core_println_string },
	{ "xl_core_println_bool", xl_core_println_bool },
	{ "xl_io_eprint_int", xl_io_eprint_int },
	{ "xl_io_eprint_float", xl_io_eprint_float },
	{ "xl_io_eprint_string", xl_io_eprint_string },
	{ "xl_io_eprint_bool", xl_io_eprint_bool },
	{ "xl_io_eprintln_void", xl_io_eprintln_void },
	{ "xl_io_eprintln_int", xl_io_eprintln_int },
	{ "xl_io_eprintln_float", xl_io_eprintln_float },
	{ "xl_io_eprintln_string", xl_io_eprintln_string },
	{ "xl_io_eprintln_bool", xl_io_eprintln_bool },
	{ "xl_io_read_line", xl_io_read_line },
	{ "xl_io_flush", xl_io_flush },
	{ "xl_core_len_string", xl_core_len_string },
	{ "xl_core_len_array", xl_core_len_array },
	{ "xl_core_len_list", xl_core_len_list },
	{ "xl_core_len_set", xl_core_len_set },
	{ "xl_core_len_dict", xl_core_len_dict },
	{ "xl_point_to_string", xl_point_to_string },
	{ "xl_value_to_string", xl_value_to_string },
	{ "xl_value_clone_async_state", xl_value_clone_async_state },
	{ "xl_debug_text", xl_debug_text },
	{ "xl_debug_print", xl_debug_print },
	{ "xl_debug_println", xl_debug_println },
	{ "xl_log_debug", xl_log_debug },
	{ "xl_log_info", xl_log_info },
	{ "xl_log_warn", xl_log_warn },
	{ "xl_log_error", xl_log_error },
	{ "xl_trace_write", xl_trace_write },
	{ "xl_regex_is_match", xl_regex_is_match },
	{ "xl_regex_find", xl_regex_find },
	{ "xl_regex_captures", xl_regex_captures },
	{ "xl_regex_capture_count", xl_regex_capture_count },
	{ "xl_regex_replace_first", xl_regex_replace_first },
	{ "xl_regex_replace_all", xl_regex_replace_all },
	{ "xl_json_encode", xl_json_encode },
	{ "xl_json_decode", xl_json_decode },
	{ "xl_array_append", xl_array_append },
	{ "xl_list_append", xl_list_append },
	{ "xl_array_join", xl_array_join },
	{ "xl_list_join", xl_list_join },
	{ "xl_array_clear", xl_array_clear },
	{ "xl_list_clear", xl_list_clear },
	{ "xl_set_clear", xl_set_clear },
	{ "xl_dict_clear", xl_dict_clear },
	{ "xl_array_remove", xl_array_remove },
	{ "xl_list_remove", xl_list_remove },
	{ "xl_set_remove", xl_set_remove },
	{ "xl_dict_remove", xl_dict_remove },
	{ "xl_set_has", xl_set_has },
	{ "xl_dict_has", xl_dict_has },
	{ "xl_iter_keys", xl_iter_keys },
	{ "xl_iter_values", xl_iter_values },
	{ "xl_iter_items", xl_iter_items },
	{ "xl_channel_create", xl_channel_create },
	{ "xl_channel_send", xl_channel_send },
	{ "xl_channel_recv", xl_channel_recv },
	{ "xl_channel_len", xl_channel_len },
	{ "xl_channel_capacity", xl_channel_capacity },
	{ "xl_channel_closed", xl_channel_closed },
	{ "xl_channel_close", xl_channel_close },
	{ "xl_sync_mutex", xl_sync_mutex },
	{ "xl_sync_mutex_free", xl_sync_mutex_free },
	{ "xl_sync_mutex_lock", xl_sync_mutex_lock },
	{ "xl_sync_mutex_try_lock", xl_sync_mutex_try_lock },
	{ "xl_sync_mutex_unlock", xl_sync_mutex_unlock },
	{ "xl_sync_cond", xl_sync_cond },
	{ "xl_sync_cond_free", xl_sync_cond_free },
	{ "xl_sync_cond_wait", xl_sync_cond_wait },
	{ "xl_sync_cond_wait_timeout", xl_sync_cond_wait_timeout },
	{ "xl_sync_cond_signal", xl_sync_cond_signal },
	{ "xl_sync_cond_broadcast", xl_sync_cond_broadcast },
	{ "xl_sync_rwlock", xl_sync_rwlock },
	{ "xl_sync_rwlock_free", xl_sync_rwlock_free },
	{ "xl_sync_rwlock_read_lock", xl_sync_rwlock_read_lock },
	{ "xl_sync_rwlock_try_read_lock", xl_sync_rwlock_try_read_lock },
	{ "xl_sync_rwlock_read_unlock", xl_sync_rwlock_read_unlock },
	{ "xl_sync_rwlock_write_lock", xl_sync_rwlock_write_lock },
	{ "xl_sync_rwlock_try_write_lock", xl_sync_rwlock_try_write_lock },
	{ "xl_sync_rwlock_write_unlock", xl_sync_rwlock_write_unlock },
	{ "xl_sync_rwlock_downgrade", xl_sync_rwlock_downgrade },
	{ "xl_sync_rwlock_upgrade", xl_sync_rwlock_upgrade },
	{ "xl_atomic_make", xl_atomic_make },
	{ "xl_atomic_free", xl_atomic_free },
	{ "xl_atomic_load", xl_atomic_load },
	{ "xl_atomic_store", xl_atomic_store },
	{ "xl_atomic_exchange", xl_atomic_exchange },
	{ "xl_atomic_compare_exchange", xl_atomic_compare_exchange },
	{ "xl_atomic_add", xl_atomic_add },
	{ "xl_thread_current_id", xl_thread_current_id },
	{ "xl_thread_sleep", xl_thread_sleep },
	{ "xl_thread_yield_now", xl_thread_yield_now },
	{ "xl_time_now", xl_time_now },
	{ "xl_time_to_string", xl_time_to_string },
	{ "xl_time_now_text", xl_time_now_text },
	{ "xl_time_date_text", xl_time_date_text },
	{ "xl_time_time_text", xl_time_time_text },
	{ "xl_time_relative", xl_time_relative },
	{ "xl_time_parse", xl_time_parse },
	{ "xl_time_format", xl_time_format },
	{ "xl_time_parse_format", xl_time_parse_format },
	{ "xl_time_sleep", xl_time_sleep },
	{ "xl_time_is_leap_year", xl_time_is_leap_year },
	{ "xl_path_join", xl_path_join },
	{ "xl_path_name_ext", xl_path_name_ext },
	{ "xl_path_name", xl_path_name },
	{ "xl_path_ext", xl_path_ext },
	{ "xl_path_dir", xl_path_dir },
	{ "xl_path_exists", xl_path_exists },
	{ "xl_path_is_abs", xl_path_is_abs },
	{ "xl_path_random", xl_path_random },
	{ "xl_fs_exists", xl_fs_exists },
	{ "xl_fs_is_file", xl_fs_is_file },
	{ "xl_fs_is_dir", xl_fs_is_dir },
	{ "xl_fs_size", xl_fs_size },
	{ "xl_fs_set_size", xl_fs_set_size },
	{ "xl_fs_access_time", xl_fs_access_time },
	{ "xl_fs_change_time", xl_fs_change_time },
	{ "xl_fs_read_all", xl_fs_read_all },
	{ "xl_fs_write_all", xl_fs_write_all },
	{ "xl_fs_append_all", xl_fs_append_all },
	{ "xl_fs_mkdir_all", xl_fs_mkdir_all },
	{ "xl_fs_copy", xl_fs_copy },
	{ "xl_fs_move", xl_fs_move },
	{ "xl_fs_delete", xl_fs_delete },
	{ "xl_str_contains", xl_str_contains },
	{ "xl_str_empty", xl_str_empty },
	{ "xl_str_byte_len", xl_str_byte_len },
	{ "xl_str_index_of", xl_str_index_of },
	{ "xl_str_last_index_of", xl_str_last_index_of },
	{ "xl_str_slice", xl_str_slice },
	{ "xl_str_repeat", xl_str_repeat },
	{ "xl_str_split", xl_str_split },
	{ "xl_str_split_lines", xl_str_split_lines },
	{ "xl_str_starts_with", xl_str_starts_with },
	{ "xl_str_ends_with", xl_str_ends_with },
	{ "xl_str_concat", xl_str_concat },
	{ "xl_str_trim", xl_str_trim },
	{ "xl_str_trim_left", xl_str_trim_left },
	{ "xl_str_trim_right", xl_str_trim_right },
	{ "xl_str_lower", xl_str_lower },
	{ "xl_str_upper", xl_str_upper },
	{ "xl_str_replace", xl_str_replace },
	{ "xl_str_reverse", xl_str_reverse },
	{ "xl_str_pad_left", xl_str_pad_left },
	{ "xl_str_pad_right", xl_str_pad_right },
	{ "xl_rand_int32", xl_rand_int32 },
	{ "xl_rand_bool", xl_rand_bool },
	{ "xl_rand_range", xl_rand_range },
	{ "xl_rand_text", xl_rand_text },
	{ "xl_rand_text_from", xl_rand_text_from },
	{ "xl_host_name", xl_host_name },
	{ "xl_host_ip", xl_host_ip },
	{ "xl_host_user", xl_host_user },
	{ "xl_host_os", xl_host_os },
	{ "xl_host_arch", xl_host_arch },
	{ "xl_host_executable", xl_host_executable },
	{ "xl_env_get", xl_env_get },
	{ "xl_env_exists", xl_env_exists },
	{ "xl_env_set", xl_env_set },
	{ "xl_env_cwd", xl_env_cwd },
	{ "xl_env_temp", xl_env_temp },
	{ "xl_env_home", xl_env_home },
	{ "xl_dir_create", xl_dir_create },
	{ "xl_dir_list", xl_dir_list },
	{ "xl_dir_files", xl_dir_files },
	{ "xl_dir_dirs", xl_dir_dirs },
	{ "xl_dir_tree", xl_dir_tree },
	{ "xl_dir_copy", xl_dir_copy },
	{ "xl_dir_move", xl_dir_move },
	{ "xl_dir_delete", xl_dir_delete },
	{ "xl_fmt_int_text", xl_fmt_int_text },
	{ "xl_fmt_float_text", xl_fmt_float_text },
	{ "xl_fmt_bool_text", xl_fmt_bool_text },
	{ "xl_crypto_hmac_sha256", xl_crypto_hmac_sha256 },
	{ "xl_crypto_hmac_sha512", xl_crypto_hmac_sha512 },
	{ "xl_encode_hex_encode", xl_encode_hex_encode },
	{ "xl_encode_hex_decode", xl_encode_hex_decode },
	{ "xl_encode_base64_encode", xl_encode_base64_encode },
	{ "xl_encode_base64_decode", xl_encode_base64_decode },
	{ "xl_hash_hash32", xl_hash_hash32 },
	{ "xl_hash_hash64", xl_hash_hash64 },
	{ "xl_hash_hash64_micro", xl_hash_hash64_micro },
	{ "xl_hash_hash64_nano", xl_hash_hash64_nano },
	{ "xl_hash_sha256", xl_hash_sha256 },
	{ "xl_hash_sha512", xl_hash_sha512 },
	{ "xl_math_abs_int", xl_math_abs_int },
	{ "xl_math_abs_float", xl_math_abs_float },
	{ "xl_math_min_int", xl_math_min_int },
	{ "xl_math_min_float", xl_math_min_float },
	{ "xl_math_max_int", xl_math_max_int },
	{ "xl_math_max_float", xl_math_max_float },
	{ "xl_math_clamp_int", xl_math_clamp_int },
	{ "xl_math_clamp_float", xl_math_clamp_float },
	{ "xl_math_floor", xl_math_floor },
	{ "xl_math_ceil", xl_math_ceil },
	{ "xl_math_round", xl_math_round },
	{ "xl_math_trunc", xl_math_trunc },
	{ "xl_math_sqrt_int", xl_math_sqrt_int },
	{ "xl_math_sqrt", xl_math_sqrt },
	{ "xl_math_pow_int", xl_math_pow_int },
	{ "xl_math_pow", xl_math_pow },
	{ "xl_math_sin", xl_math_sin },
	{ "xl_math_cos", xl_math_cos },
	{ "xl_math_tan", xl_math_tan },
	{ "xl_math_exp", xl_math_exp },
	{ "xl_math_log", xl_math_log },
	{ "xl_math_log10", xl_math_log10 },
	{ "xl_pack_write_u8", xl_pack_write_u8 },
	{ "xl_pack_write_u16_le", xl_pack_write_u16_le },
	{ "xl_pack_write_u16_be", xl_pack_write_u16_be },
	{ "xl_pack_write_u32_le", xl_pack_write_u32_le },
	{ "xl_pack_write_u32_be", xl_pack_write_u32_be },
	{ "xl_pack_read_u8", xl_pack_read_u8 },
	{ "xl_pack_read_u16_le", xl_pack_read_u16_le },
	{ "xl_pack_read_u16_be", xl_pack_read_u16_be },
	{ "xl_pack_read_u32_le", xl_pack_read_u32_le },
	{ "xl_pack_read_u32_be", xl_pack_read_u32_be },
	{ "xl_query_encode", xl_query_encode },
	{ "xl_query_decode", xl_query_decode },
	{ "xl_url_parse", xl_url_parse },
	{ "xl_url_build", xl_url_build },
	{ "xl_http_headers", xl_http_headers },
	{ "xl_http_request", xl_http_request },
	{ "xl_http_response", xl_http_response },
	{ "xl_http_parse_headers", xl_http_parse_headers },
	{ "xl_mime_by_ext", xl_mime_by_ext },
	{ "xl_mime_by_path", xl_mime_by_path },
	{ "xl_mime_is_text", xl_mime_is_text },
	{ "xl_mime_parse", xl_mime_parse },
	{ "xl_mime_build", xl_mime_build },
	{ "xl_multipart_boundary", xl_multipart_boundary },
	{ "xl_multipart_parse", xl_multipart_parse },
	{ "xl_multipart_build", xl_multipart_build },
	{ "xl_httpd_request", xl_httpd_request },
	{ "xl_httpd_match", xl_httpd_match },
	{ "xl_httpd_response", xl_httpd_response },
	{ "xl_httpd_text", xl_httpd_text },
	{ "xl_net_ip_parse", xl_net_ip_parse },
	{ "xl_net_ip_build", xl_net_ip_build },
	{ "xl_net_ip_normalize", xl_net_ip_normalize },
	{ "xl_net_ip_is_ip", xl_net_ip_is_ip },
	{ "xl_net_ip_is_ipv4", xl_net_ip_is_ipv4 },
	{ "xl_net_ip_is_ipv6", xl_net_ip_is_ipv6 },
	{ "xl_net_dns_resolve", xl_net_dns_resolve },
	{ "xl_net_dns_resolve_all", xl_net_dns_resolve_all },
	{ "xl_net_dns_reverse", xl_net_dns_reverse },
	{ "xl_net_tcp_listen", xl_net_tcp_listen },
	{ "xl_net_tcp_accept", xl_net_tcp_accept },
	{ "xl_net_tcp_connect", xl_net_tcp_connect },
	{ "xl_net_tcp_send", xl_net_tcp_send },
	{ "xl_net_tcp_recv", xl_net_tcp_recv },
	{ "xl_net_tcp_local_port", xl_net_tcp_local_port },
	{ "xl_net_tcp_close", xl_net_tcp_close },
	{ "xl_net_udp_bind", xl_net_udp_bind },
	{ "xl_net_udp_send_to", xl_net_udp_send_to },
	{ "xl_net_udp_recv_from", xl_net_udp_recv_from },
	{ "xl_net_udp_local_port", xl_net_udp_local_port },
	{ "xl_net_udp_close", xl_net_udp_close },
	{ "xl_net_ws_key", xl_net_ws_key },
	{ "xl_net_ws_accept", xl_net_ws_accept },
	{ "xl_net_ws_request", xl_net_ws_request },
	{ "xl_net_ws_response", xl_net_ws_response },
	{ "xl_net_ws_frame_text", xl_net_ws_frame_text },
	{ "xl_net_ws_unframe", xl_net_ws_unframe },
	{ "xl_net_proxy_parse", xl_net_proxy_parse },
	{ "xl_net_proxy_basic_auth", xl_net_proxy_basic_auth },
	{ "xl_net_proxy_http_connect", xl_net_proxy_http_connect },
	{ "xl_net_proxy_socks5_greeting_hex", xl_net_proxy_socks5_greeting_hex },
	{ "xl_net_proxy_socks5_connect_hex", xl_net_proxy_socks5_connect_hex },
	{ "xl_net_tls_versions", xl_net_tls_versions },
	{ "xl_net_tls_is_tls_url", xl_net_tls_is_tls_url },
	{ "xl_net_tls_default_port", xl_net_tls_default_port },
	{ "xl_net_tls_sni_host", xl_net_tls_sni_host },
	{ "xl_net_tls_fingerprint_sha256", xl_net_tls_fingerprint_sha256 },
	{ "xl_net_tls_client_config", xl_net_tls_client_config },
	{ "xl_range_collect1", xl_range_collect1 },
	{ "xl_range_collect2", xl_range_collect2 },
	{ "xl_range_collect3", xl_range_collect3 },
	{ "xl_template_render", xl_template_render },
	{ "xl_process_exec", xl_process_exec },
	{ "xl_process_capture", xl_process_capture },
	{ "xl_process_exec_in", xl_process_exec_in },
	{ "xl_process_capture_in", xl_process_capture_in },
	{ "xl_process_terminal_supported", xl_process_terminal_supported },
	{ "xl_net_resolve", xl_net_resolve },
	{ "xl_mem_alloc", xl_mem_alloc },
	{ "xl_mem_free", xl_mem_free },
	{ "xl_mem_is_null", xl_mem_is_null },
	{ "xl_mem_fill", xl_mem_fill },
	{ "xl_mem_copy", xl_mem_copy },
	{ "xl_mem_write_string", xl_mem_write_string },
	{ "xl_mem_read_string", xl_mem_read_string }
};

int xlHostRegisterSymbols(TCCState* pTcc)
{
	size_t i;

	if ( pTcc == NULL ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < XL_COUNTOF(__gXLHostSymbols); ++i ) {
		if ( __xlAddSymbol(pTcc, __gXLHostSymbols[i].sName, __gXLHostSymbols[i].pValue) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

char* xlHostDumpSymbolsJson(void)
{
	size_t i;
	size_t iCap;
	size_t iLen;
	char* sOut;

	iCap = (XL_COUNTOF(__gXLHostSymbols) * 96u) + 64u;
	sOut = (char*)malloc(iCap);
	if ( sOut == NULL ) {
		return NULL;
	}

	iLen = (size_t)snprintf(sOut, iCap, "{\n\t\"symbols\": [\n");
	for ( i = 0; i < XL_COUNTOF(__gXLHostSymbols); ++i ) {
		iLen += (size_t)snprintf(
			sOut + iLen,
			iCap - iLen,
			"\t\t{\"name\": \"%s\"}%s\n",
			__gXLHostSymbols[i].sName,
			(i + 1u) < XL_COUNTOF(__gXLHostSymbols) ? "," : ""
		);
	}
	snprintf(sOut + iLen, iCap - iLen, "\t]\n}\n");
	return sOut;
}
