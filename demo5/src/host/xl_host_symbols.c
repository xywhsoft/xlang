#include "xl_host_symbols.h"

#include "xl_host_runtime.h"
#include "../runtime/xlrt_generated.h"
#include "../runtime/xlrt_profile.h"
#include "../../lib/xrt/xrt.h"

typedef struct XL_HostSymbolEntry {
	const char* sName;
	const void* pValue;
} XL_HostSymbolEntry;

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
	{ "xrtNow", xrtNow },
	{ "xrtTimeToStr", xrtTimeToStr },
	{ "xrtNowStr", xrtNowStr },
	{ "xrtDateStr", xrtDateStr },
	{ "xrtTimeStr", xrtTimeStr },
	{ "xrtRelativeTime", xrtRelativeTime },
	{ "xrtStrToTime", xrtStrToTime },
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
	{ "xrtLCase", xrtLCase },
	{ "xrtUCase", xrtUCase },
	{ "xrtReplace", xrtReplace },
	{ "xrtIntFormat", xrtIntFormat },
	{ "xrtNumFormat", xrtNumFormat },
	{ "xrtRand32", xrtRand32 },
	{ "xrtRandRange", xrtRandRange },
	{ "xrtRandStr", xrtRandStr },
	{ "xrtProcessConfigInit", xrtProcessConfigInit },
	{ "xrtExecCapture", xrtExecCapture },
	{ "xrtProcessResultUnit", xrtProcessResultUnit },
	{ "xrtCopyStr", xrtCopyStr },
	{ "xvoCreateNull", xvoCreateNull },
	{ "xvoCreateBool", xvoCreateBool },
	{ "xvoCreateInt", xvoCreateInt },
	{ "xvoCreateFloat", xvoCreateFloat },
	{ "xvoCreateText", xvoCreateText },
	{ "xvoCreateTime", xvoCreateTime },
	{ "xvoCopy", xvoCopy },
	{ "xvoUnref", xvoUnref },
	{ "xvoGetBool", xvoGetBool },
	{ "xvoGetInt", xvoGetInt },
	{ "xvoGetFloat", xvoGetFloat },
	{ "xvoGetText", xvoGetText },
	{ "xvoGetTime", xvoGetTime },
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
	{ "xlrt_env_get", xlrt_env_get },
	{ "xlrt_env_exists", xlrt_env_exists },
	{ "xlrt_env_cwd", xlrt_env_cwd },
	{ "xlrt_native_error_clear", xlrt_native_error_clear },
	{ "xlrt_native_error_raise", xlrt_native_error_raise },
	{ "xlrt_native_error_has", xlrt_native_error_has },
	{ "xlrt_native_error_kind", xlrt_native_error_kind },
	{ "xlrt_native_error_message", xlrt_native_error_message },
	{ "xlrt_native_error_code", xlrt_native_error_code },
	{ "xlrt_native_error_data", xlrt_native_error_data },
	{ "xl_core_print_int", xl_core_print_int },
	{ "xl_core_print_float", xl_core_print_float },
	{ "xl_core_print_string", xl_core_print_string },
	{ "xl_core_print_bool", xl_core_print_bool },
	{ "xl_core_println_void", xl_core_println_void },
	{ "xl_core_println_int", xl_core_println_int },
	{ "xl_core_println_float", xl_core_println_float },
	{ "xl_core_println_string", xl_core_println_string },
	{ "xl_core_println_bool", xl_core_println_bool },
	{ "xl_core_len_string", xl_core_len_string },
	{ "xl_core_len_array", xl_core_len_array },
	{ "xl_core_len_list", xl_core_len_list },
	{ "xl_core_len_set", xl_core_len_set },
	{ "xl_core_len_dict", xl_core_len_dict },
	{ "xl_array_append", xl_array_append },
	{ "xl_list_append", xl_list_append },
	{ "xl_array_clear", xl_array_clear },
	{ "xl_list_clear", xl_list_clear },
	{ "xl_set_clear", xl_set_clear },
	{ "xl_dict_clear", xl_dict_clear },
	{ "xl_array_remove", xl_array_remove },
	{ "xl_list_remove", xl_list_remove },
	{ "xl_set_remove", xl_set_remove },
	{ "xl_dict_remove", xl_dict_remove },
	{ "xl_time_now", xl_time_now },
	{ "xl_time_to_string", xl_time_to_string },
	{ "xl_time_now_text", xl_time_now_text },
	{ "xl_time_date_text", xl_time_date_text },
	{ "xl_time_time_text", xl_time_time_text },
	{ "xl_time_relative", xl_time_relative },
	{ "xl_time_parse", xl_time_parse },
	{ "xl_path_join", xl_path_join },
	{ "xl_path_name_ext", xl_path_name_ext },
	{ "xl_path_name", xl_path_name },
	{ "xl_path_ext", xl_path_ext },
	{ "xl_path_dir", xl_path_dir },
	{ "xl_path_is_abs", xl_path_is_abs },
	{ "xl_path_random", xl_path_random },
	{ "xl_fs_exists", xl_fs_exists },
	{ "xl_fs_is_file", xl_fs_is_file },
	{ "xl_fs_is_dir", xl_fs_is_dir },
	{ "xl_fs_size", xl_fs_size },
	{ "xl_fs_read_all", xl_fs_read_all },
	{ "xl_fs_write_all", xl_fs_write_all },
	{ "xl_fs_append_all", xl_fs_append_all },
	{ "xl_fs_mkdir_all", xl_fs_mkdir_all },
	{ "xl_fs_copy", xl_fs_copy },
	{ "xl_fs_move", xl_fs_move },
	{ "xl_fs_delete", xl_fs_delete },
	{ "xl_str_contains", xl_str_contains },
	{ "xl_str_starts_with", xl_str_starts_with },
	{ "xl_str_ends_with", xl_str_ends_with },
	{ "xl_str_concat", xl_str_concat },
	{ "xl_str_lower", xl_str_lower },
	{ "xl_str_upper", xl_str_upper },
	{ "xl_str_replace", xl_str_replace },
	{ "xl_rand_int32", xl_rand_int32 },
	{ "xl_rand_range", xl_rand_range },
	{ "xl_rand_text", xl_rand_text },
	{ "xl_host_name", xl_host_name },
	{ "xl_host_ip", xl_host_ip },
	{ "xl_env_get", xl_env_get },
	{ "xl_env_exists", xl_env_exists },
	{ "xl_env_cwd", xl_env_cwd },
	{ "xl_dir_list", xl_dir_list },
	{ "xl_dir_files", xl_dir_files },
	{ "xl_dir_dirs", xl_dir_dirs },
	{ "xl_dir_tree", xl_dir_tree },
	{ "xl_fmt_int_text", xl_fmt_int_text },
	{ "xl_fmt_float_text", xl_fmt_float_text },
	{ "xl_process_exec", xl_process_exec },
	{ "xl_process_capture", xl_process_capture },
	{ "xl_process_exec_in", xl_process_exec_in },
	{ "xl_process_capture_in", xl_process_capture_in }
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
