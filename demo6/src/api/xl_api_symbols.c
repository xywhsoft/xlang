#include "xl_api_internal.h"

#include "include/xl.h"
#include "lib/xrt/xrt.h"

#if defined(CONFIG_TCC_STATIC) && !defined(XL_STATIC_TCC)
#define XL_STATIC_TCC 1
#endif

#if defined(XL_STATIC_TCC) && !defined(TCC_DISABLE_BUILTIN_VFS)
#define XL_STATIC_TCC_VFS 1
#endif

#ifndef XL_ENABLE_EXE_COMPILER
#if defined(XL_RUNTIME_ONLY_DLL)
#define XL_ENABLE_EXE_COMPILER 0
#else
#define XL_ENABLE_EXE_COMPILER 1
#endif
#endif

#if defined(XL_STATIC_TCC_VFS)
#include "tcc_builtin_vfs.h"
#endif

static const float xl__mzerosf = -0.0f;
static const double xl__mzerodf = -0.0;

int xlApiAddTccSymbol(TCCState* s, const char* name, const void* value)
{
#ifdef XL_STATIC_TCC
    return tcc_add_symbol(s, name, value) == 0;
#else
    return xl__tcc.tcc_add_symbol(s, name, value) == 0;
#endif
}

#define XL_ADD_XRT_SYMBOL(s, name) \
    do { \
        if ( !xlApiAddTccSymbol((s), #name, (const void*)(name)) ) { \
            return 0; \
        } \
    } while (0)

#define XL_ADD_TCC_API_SYMBOL(s, member, name) \
    do { \
        if ( xl__tcc.member != NULL && !xlApiAddTccSymbol((s), (name), (const void*)xl__tcc.member) ) { \
            return 0; \
        } \
    } while (0)

static int xlAddDefaultTccApiSymbols(TCCState* s)
{
    if ( s == NULL || !xl__tcc.ready ) {
        return 1;
    }

    XL_ADD_TCC_API_SYMBOL(s, tcc_new, "tcc_new");
    XL_ADD_TCC_API_SYMBOL(s, tcc_delete, "tcc_delete");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_lib_path, "tcc_set_lib_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_error_func, "tcc_set_error_func");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_options, "tcc_set_options");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_include_path, "tcc_add_include_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_sysinclude_path, "tcc_add_sysinclude_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_define_symbol, "tcc_define_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_undefine_symbol, "tcc_undefine_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_library_path, "tcc_add_library_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_library, "tcc_add_library");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_file, "tcc_add_file");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_symbol, "tcc_add_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_output_type, "tcc_set_output_type");
    XL_ADD_TCC_API_SYMBOL(s, tcc_compile_string, "tcc_compile_string");
    XL_ADD_TCC_API_SYMBOL(s, tcc_output_file, "tcc_output_file");
    XL_ADD_TCC_API_SYMBOL(s, tcc_run, "tcc_run");
    XL_ADD_TCC_API_SYMBOL(s, tcc_relocate, "tcc_relocate");
    XL_ADD_TCC_API_SYMBOL(s, tcc_get_symbol, "tcc_get_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_list_symbols, "tcc_list_symbols");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_backtrace_func, "tcc_set_backtrace_func");
    if ( !xlApiAddTccSymbol(s, "xlDefaultContextOptions", (const void*)xlDefaultContextOptions) ||
         !xlApiAddTccSymbol(s, "xlRuntimeInit", (const void*)xlRuntimeInit) ||
         !xlApiAddTccSymbol(s, "xlRuntimeUnit", (const void*)xlRuntimeUnit) ||
         !xlApiAddTccSymbol(s, "xlCreateContext", (const void*)xlCreateContext) ||
         !xlApiAddTccSymbol(s, "xlDestroyContext", (const void*)xlDestroyContext) ||
         !xlApiAddTccSymbol(s, "xlSetWorkDir", (const void*)xlSetWorkDir) ||
         !xlApiAddTccSymbol(s, "xlSetTempDir", (const void*)xlSetTempDir) ||
         !xlApiAddTccSymbol(s, "xlSetSaveTemps", (const void*)xlSetSaveTemps) ||
         !xlApiAddTccSymbol(s, "xlAddIncludePath", (const void*)xlAddIncludePath) ||
         !xlApiAddTccSymbol(s, "xlAddLibraryPath", (const void*)xlAddLibraryPath) ||
         !xlApiAddTccSymbol(s, "xlAddLinkLibrary", (const void*)xlAddLinkLibrary) ||
         !xlApiAddTccSymbol(s, "xlLoadDependencyLibrary", (const void*)xlLoadDependencyLibrary) ||
         !xlApiAddTccSymbol(s, "xlRegisterCFunction", (const void*)xlRegisterCFunction) ||
         !xlApiAddTccSymbol(s, "xlRunScriptString", (const void*)xlRunScriptString) ||
         !xlApiAddTccSymbol(s, "xlRunScriptFile", (const void*)xlRunScriptFile) ||
         !xlApiAddTccSymbol(s, "xlRunScriptStringArg", (const void*)xlRunScriptStringArg) ||
         !xlApiAddTccSymbol(s, "xlRunScriptFileArg", (const void*)xlRunScriptFileArg) ||
         !xlApiAddTccSymbol(s, "xlCompileScriptStringToModule", (const void*)xlCompileScriptStringToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileScriptFileToModule", (const void*)xlCompileScriptFileToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceStringToModule", (const void*)xlCompileCSourceStringToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFileToModule", (const void*)xlCompileCSourceFileToModule) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFilesToModule", (const void*)xlCompileCSourceFilesToModule) ||
         !xlApiAddTccSymbol(s, "xlRunCSourceString", (const void*)xlRunCSourceString) ||
         !xlApiAddTccSymbol(s, "xlRunCSourceFile", (const void*)xlRunCSourceFile) ||
         !xlApiAddTccSymbol(s, "xlRunCSourceFiles", (const void*)xlRunCSourceFiles) ||
         !xlApiAddTccSymbol(s, "xlRunModuleMain", (const void*)xlRunModuleMain) ||
         !xlApiAddTccSymbol(s, "xlModuleGetSymbol", (const void*)xlModuleGetSymbol) ||
         !xlApiAddTccSymbol(s, "xlDestroyModule", (const void*)xlDestroyModule) ||
         !xlApiAddTccSymbol(s, "xlGetLastError", (const void*)xlGetLastError) ||
         !xlApiAddTccSymbol(s, "xlClearLastError", (const void*)xlClearLastError) ||
         !xlApiAddTccSymbol(s, "xlGetLastExitCode", (const void*)xlGetLastExitCode) ) {
        return 0;
    }
    if ( !xlApiAddTccSymbol(s, "xlTccRegisterXrtEnvironment", (const void*)xlTccRegisterXrtEnvironment) ) {
        return 0;
    }
#if XL_ENABLE_EXE_COMPILER
    if ( !xlApiAddTccSymbol(s, "xlCompileScriptStringToExe", (const void*)xlCompileScriptStringToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileScriptFileToExe", (const void*)xlCompileScriptFileToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFileToExe", (const void*)xlCompileCSourceFileToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFilesToExe", (const void*)xlCompileCSourceFilesToExe) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFileToSharedLibrary", (const void*)xlCompileCSourceFileToSharedLibrary) ||
         !xlApiAddTccSymbol(s, "xlCompileCSourceFilesToSharedLibrary", (const void*)xlCompileCSourceFilesToSharedLibrary) ||
         !xlApiAddTccSymbol(s, "xlcDefaultContextOptions", (const void*)xlcDefaultContextOptions) ||
         !xlApiAddTccSymbol(s, "xlcCreateContext", (const void*)xlcCreateContext) ||
         !xlApiAddTccSymbol(s, "xlcDestroyContext", (const void*)xlcDestroyContext) ||
         !xlApiAddTccSymbol(s, "xlcSetWorkDir", (const void*)xlcSetWorkDir) ||
         !xlApiAddTccSymbol(s, "xlcSetTempDir", (const void*)xlcSetTempDir) ||
         !xlApiAddTccSymbol(s, "xlcSetSaveTemps", (const void*)xlcSetSaveTemps) ||
         !xlApiAddTccSymbol(s, "xlcAddIncludePath", (const void*)xlcAddIncludePath) ||
         !xlApiAddTccSymbol(s, "xlcAddLibraryPath", (const void*)xlcAddLibraryPath) ||
         !xlApiAddTccSymbol(s, "xlcAddLinkLibrary", (const void*)xlcAddLinkLibrary) ||
         !xlApiAddTccSymbol(s, "xlcAddCSourceFile", (const void*)xlcAddCSourceFile) ||
         !xlApiAddTccSymbol(s, "xlcLoadDependencyLibrary", (const void*)xlcLoadDependencyLibrary) ||
         !xlApiAddTccSymbol(s, "xlcRegisterCFunction", (const void*)xlcRegisterCFunction) ||
         !xlApiAddTccSymbol(s, "xlcAddSourceFile", (const void*)xlcAddSourceFile) ||
         !xlApiAddTccSymbol(s, "xlcAddSourceString", (const void*)xlcAddSourceString) ||
         !xlApiAddTccSymbol(s, "xlcCompile", (const void*)xlcCompile) ||
         !xlApiAddTccSymbol(s, "xlcCheck", (const void*)xlcCheck) ||
         !xlApiAddTccSymbol(s, "xlcEmitTokens", (const void*)xlcEmitTokens) ||
         !xlApiAddTccSymbol(s, "xlcEmitAst", (const void*)xlcEmitAst) ||
         !xlApiAddTccSymbol(s, "xlcEmitIr", (const void*)xlcEmitIr) ||
         !xlApiAddTccSymbol(s, "xlcEmitC", (const void*)xlcEmitC) ||
         !xlApiAddTccSymbol(s, "xlcEmitDebugC", (const void*)xlcEmitDebugC) ||
         !xlApiAddTccSymbol(s, "xlcEmitHeader", (const void*)xlcEmitHeader) ||
         !xlApiAddTccSymbol(s, "xlcEmitAbi", (const void*)xlcEmitAbi) ||
         !xlApiAddTccSymbol(s, "xlcEmitExe", (const void*)xlcEmitExe) ||
         !xlApiAddTccSymbol(s, "xlcEmitDebugExe", (const void*)xlcEmitDebugExe) ||
         !xlApiAddTccSymbol(s, "xlcRuntimeContext", (const void*)xlcRuntimeContext) ||
         !xlApiAddTccSymbol(s, "xlcRelocateModule", (const void*)xlcRelocateModule) ||
         !xlApiAddTccSymbol(s, "xlcRun", (const void*)xlcRun) ||
         !xlApiAddTccSymbol(s, "xlcGetLastError", (const void*)xlcGetLastError) ||
         !xlApiAddTccSymbol(s, "xlcClearLastError", (const void*)xlcClearLastError) ||
         !xlApiAddTccSymbol(s, "xlcGetLastExitCode", (const void*)xlcGetLastExitCode) ||
         !xlApiAddTccSymbol(s, "xlcEmitSharedLibrary", (const void*)xlcEmitSharedLibrary) ) {
        return 0;
    }
#endif

#ifdef XL_STATIC_TCC_VFS
    if ( !xlApiAddTccSymbol(s, "tcc_vfs_open", (const void*)tcc_vfs_open) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_close", (const void*)tcc_vfs_close) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_read", (const void*)tcc_vfs_read) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_lseek", (const void*)tcc_vfs_lseek) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_fopen", (const void*)tcc_vfs_fopen) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_fclose", (const void*)tcc_vfs_fclose) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_mount_memory", (const void*)tcc_vfs_mount_memory) ||
         !xlApiAddTccSymbol(s, "tcc_vfs_clear_dynamic", (const void*)tcc_vfs_clear_dynamic) ) {
        return 0;
    }
#endif
    return 1;
}

int xlApiAddDefaultRuntimeSymbols(TCCState* s)
{
    XL_ADD_XRT_SYMBOL(s, xrtInit);
    if ( !xlApiAddTccSymbol(s, "__mzerosf", &xl__mzerosf) ||
         !xlApiAddTccSymbol(s, "__mzerodf", &xl__mzerodf) ) {
        return 0;
    }
    XL_ADD_XRT_SYMBOL(s, xrtUnit);
    XL_ADD_XRT_SYMBOL(s, xrtMalloc);
    XL_ADD_XRT_SYMBOL(s, xrtCalloc);
    XL_ADD_XRT_SYMBOL(s, xrtRealloc);
    XL_ADD_XRT_SYMBOL(s, xrtFree);
    XL_ADD_XRT_SYMBOL(s, xrtAtomicRefRetain);
    XL_ADD_XRT_SYMBOL(s, xrtAtomicRefRelease);
    XL_ADD_XRT_SYMBOL(s, xrtTempMemory);
    XL_ADD_XRT_SYMBOL(s, xrtFreeTempMemory);
    XL_ADD_XRT_SYMBOL(s, xrtTempScopeBegin);
    XL_ADD_XRT_SYMBOL(s, xrtTempScopeEnd);
    XL_ADD_XRT_SYMBOL(s, xrtTempScopeEndString);
    XL_ADD_XRT_SYMBOL(s, xrtTempStringTake);
    XL_ADD_XRT_SYMBOL(s, xrtBufferCreate);
    XL_ADD_XRT_SYMBOL(s, xrtBufferFrom);
    XL_ADD_XRT_SYMBOL(s, xrtBufferFromHex);
    XL_ADD_XRT_SYMBOL(s, xrtBufferFromBase64);
    XL_ADD_XRT_SYMBOL(s, xrtBufferDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtBufferInit);
    XL_ADD_XRT_SYMBOL(s, xrtBufferUnit);
    XL_ADD_XRT_SYMBOL(s, xrtBufferMalloc);
    XL_ADD_XRT_SYMBOL(s, xrtBufferInsert);
    XL_ADD_XRT_SYMBOL(s, xrtBufferAppend);
    XL_ADD_XRT_SYMBOL(s, xrtCopyStr);
    XL_ADD_XRT_SYMBOL(s, xrtStrConcat);
    XL_ADD_XRT_SYMBOL(s, xrtStrJoin);
    XL_ADD_XRT_SYMBOL(s, xrtFormat);
    XL_ADD_XRT_SYMBOL(s, xrtHash32);
    XL_ADD_XRT_SYMBOL(s, xrtHash32_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtHash64);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Micro);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Micro_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Nano);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Nano_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtStrComp);
    XL_ADD_XRT_SYMBOL(s, xrtStrByteLen);
    XL_ADD_XRT_SYMBOL(s, xrtUTF8to16);
    XL_ADD_XRT_SYMBOL(s, xrtUTF16to8);
    XL_ADD_XRT_SYMBOL(s, xrtStrUtf8CharSize);
    XL_ADD_XRT_SYMBOL(s, xrtStrCharLen);
    XL_ADD_XRT_SYMBOL(s, xrtStrCharToBytePos);
    XL_ADD_XRT_SYMBOL(s, xrtFindStr);
    XL_ADD_XRT_SYMBOL(s, xrtStrIndexOf);
    XL_ADD_XRT_SYMBOL(s, xrtStrLastIndexOf);
    XL_ADD_XRT_SYMBOL(s, xrtStrCount);
    XL_ADD_XRT_SYMBOL(s, xrtInStr);
    XL_ADD_XRT_SYMBOL(s, xrtCheckStr);
    XL_ADD_XRT_SYMBOL(s, xrtStrLike);
    XL_ADD_XRT_SYMBOL(s, xrtStrApprox);
    XL_ADD_XRT_SYMBOL(s, xrtStrSim);
    XL_ADD_XRT_SYMBOL(s, xrtIsUTF8);
    XL_ADD_XRT_SYMBOL(s, xrtConvCharset);
    XL_ADD_XRT_SYMBOL(s, xrtDetectCharset);
    XL_ADD_XRT_SYMBOL(s, xrtGetCharSize);
    XL_ADD_XRT_SYMBOL(s, xrtIntFormat);
    XL_ADD_XRT_SYMBOL(s, xrtUIntFormat);
    XL_ADD_XRT_SYMBOL(s, xrtNumFormat);
    XL_ADD_XRT_SYMBOL(s, xrtLCase);
    XL_ADD_XRT_SYMBOL(s, xrtUCase);
    XL_ADD_XRT_SYMBOL(s, xrtTrim);
    XL_ADD_XRT_SYMBOL(s, xrtLTrim);
    XL_ADD_XRT_SYMBOL(s, xrtRTrim);
    XL_ADD_XRT_SYMBOL(s, xrtFilterStr);
    XL_ADD_XRT_SYMBOL(s, xrtReplace);
    XL_ADD_XRT_SYMBOL(s, xrtStrSlice);
    XL_ADD_XRT_SYMBOL(s, xrtStrLeft);
    XL_ADD_XRT_SYMBOL(s, xrtStrRight);
    XL_ADD_XRT_SYMBOL(s, xrtStrMid);
    XL_ADD_XRT_SYMBOL(s, xrtStrCharAt);
    XL_ADD_XRT_SYMBOL(s, xrtStrByteAt);
    XL_ADD_XRT_SYMBOL(s, xrtStrRepeat);
    XL_ADD_XRT_SYMBOL(s, xrtStrIsEmpty);
    XL_ADD_XRT_SYMBOL(s, xrtStrIsBlank);
    XL_ADD_XRT_SYMBOL(s, xrtStrRemove);
    XL_ADD_XRT_SYMBOL(s, xrtStrRemoveAt);
    XL_ADD_XRT_SYMBOL(s, xrtStrInsert);
    XL_ADD_XRT_SYMBOL(s, xrtStrPadLeft);
    XL_ADD_XRT_SYMBOL(s, xrtStrPadRight);
    XL_ADD_XRT_SYMBOL(s, xrtStrPadCenter);
    XL_ADD_XRT_SYMBOL(s, xrtStrReverse);
    XL_ADD_XRT_SYMBOL(s, xrtStrSplitLines);
    XL_ADD_XRT_SYMBOL(s, xrtSplit);
    XL_ADD_XRT_SYMBOL(s, xrtIntApprox);
    XL_ADD_XRT_SYMBOL(s, xrtNumApprox);
    XL_ADD_XRT_SYMBOL(s, xrtMathMin);
    XL_ADD_XRT_SYMBOL(s, xrtMathMax);
    XL_ADD_XRT_SYMBOL(s, xrtMathClamp);
    XL_ADD_XRT_SYMBOL(s, xrtMathSign);
    XL_ADD_XRT_SYMBOL(s, xrtMathTrunc);
    XL_ADD_XRT_SYMBOL(s, xrtMathFract);
    XL_ADD_XRT_SYMBOL(s, xrtMathMod);
    XL_ADD_XRT_SYMBOL(s, xrtMathRad);
    XL_ADD_XRT_SYMBOL(s, xrtMathDeg);
    XL_ADD_XRT_SYMBOL(s, xrtMathIsNaN);
    XL_ADD_XRT_SYMBOL(s, xrtMathIsInf);
    XL_ADD_XRT_SYMBOL(s, xrtMathIsFinite);
    XL_ADD_XRT_SYMBOL(s, xrtMathLog2);
    XL_ADD_XRT_SYMBOL(s, xrtMathExp2);
    XL_ADD_XRT_SYMBOL(s, xrtMathLog1p);
    XL_ADD_XRT_SYMBOL(s, xrtMathExpm1);
    XL_ADD_XRT_SYMBOL(s, xrtMathCbrt);
    XL_ADD_XRT_SYMBOL(s, xrtMathHypot);
    XL_ADD_XRT_SYMBOL(s, xrtTimeApprox);
    XL_ADD_XRT_SYMBOL(s, xrtDateAdd);
    XL_ADD_XRT_SYMBOL(s, xrtDateDiff);
    XL_ADD_XRT_SYMBOL(s, xrtIsLeapYear);
    XL_ADD_XRT_SYMBOL(s, xrtDaysInMonth);
    XL_ADD_XRT_SYMBOL(s, xrtDaysInYear);
    XL_ADD_XRT_SYMBOL(s, xrtTimeSerial);
    XL_ADD_XRT_SYMBOL(s, xrtDateSerial);
    XL_ADD_XRT_SYMBOL(s, xrtDateTimeSerial);
    XL_ADD_XRT_SYMBOL(s, xrtStrToTime);
    XL_ADD_XRT_SYMBOL(s, xrtTryStrToTime);
    XL_ADD_XRT_SYMBOL(s, xrtTimeFormat);
    XL_ADD_XRT_SYMBOL(s, xrtTimeParse);
    XL_ADD_XRT_SYMBOL(s, xrtTryTimeParse);
    XL_ADD_XRT_SYMBOL(s, xrtRelativeTime);
    XL_ADD_XRT_SYMBOL(s, xrtNow);
    XL_ADD_XRT_SYMBOL(s, xrtNowUTC);
    XL_ADD_XRT_SYMBOL(s, xrtDate);
    XL_ADD_XRT_SYMBOL(s, xrtTime);
    XL_ADD_XRT_SYMBOL(s, xrtTimer);
    XL_ADD_XRT_SYMBOL(s, xrtSleep);
    XL_ADD_XRT_SYMBOL(s, xrtTimezoneOffset);
    XL_ADD_XRT_SYMBOL(s, xrtYear);
    XL_ADD_XRT_SYMBOL(s, xrtMonth);
    XL_ADD_XRT_SYMBOL(s, xrtDay);
    XL_ADD_XRT_SYMBOL(s, xrtHour);
    XL_ADD_XRT_SYMBOL(s, xrtMinute);
    XL_ADD_XRT_SYMBOL(s, xrtSecond);
    XL_ADD_XRT_SYMBOL(s, xrtWeekday);
    XL_ADD_XRT_SYMBOL(s, xrtDayOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtQuarter);
    XL_ADD_XRT_SYMBOL(s, xrtDatePart);
    XL_ADD_XRT_SYMBOL(s, xrtTimePart);
    XL_ADD_XRT_SYMBOL(s, xrtIsSameDay);
    XL_ADD_XRT_SYMBOL(s, xrtIsSameMonth);
    XL_ADD_XRT_SYMBOL(s, xrtIsSameYear);
    XL_ADD_XRT_SYMBOL(s, xrtTimeInRange);
    XL_ADD_XRT_SYMBOL(s, xrtTimeRangeOverlap);
    XL_ADD_XRT_SYMBOL(s, xrtToUnixTime);
    XL_ADD_XRT_SYMBOL(s, xrtFromUnixTime);
    XL_ADD_XRT_SYMBOL(s, xrtUTCToLocal);
    XL_ADD_XRT_SYMBOL(s, xrtLocalToUTC);
    XL_ADD_XRT_SYMBOL(s, xrtFirstDayOfMonth);
    XL_ADD_XRT_SYMBOL(s, xrtLastDayOfMonth);
    XL_ADD_XRT_SYMBOL(s, xrtFirstDayOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtLastDayOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtFirstDayOfWeek);
    XL_ADD_XRT_SYMBOL(s, xrtLastDayOfWeek);
    XL_ADD_XRT_SYMBOL(s, xrtWeekOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtWeekOfMonth);
    XL_ADD_XRT_SYMBOL(s, xrtStrToI64);
    XL_ADD_XRT_SYMBOL(s, xrtStrToNum);
    XL_ADD_XRT_SYMBOL(s, xrtI64ToStr);
    XL_ADD_XRT_SYMBOL(s, xrtNumToStr);
    XL_ADD_XRT_SYMBOL(s, xrtTimeToStr);
    XL_ADD_XRT_SYMBOL(s, xrtRand32);
    XL_ADD_XRT_SYMBOL(s, xrtRand64);
    XL_ADD_XRT_SYMBOL(s, xrtRandRange);
    XL_ADD_XRT_SYMBOL(s, xrtRandSeedThread);
    XL_ADD_XRT_SYMBOL(s, xrtRandCreate);
    XL_ADD_XRT_SYMBOL(s, xrtRandDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRandSeedObj);
    XL_ADD_XRT_SYMBOL(s, xrtRand32Obj);
    XL_ADD_XRT_SYMBOL(s, xrtRand64Obj);
    XL_ADD_XRT_SYMBOL(s, xrtRandRangeObj);
    XL_ADD_XRT_SYMBOL(s, xrtRandStr);
    XL_ADD_XRT_SYMBOL(s, xrtRandStrObj);
    XL_ADD_XRT_SYMBOL(s, xlogCreate);
    XL_ADD_XRT_SYMBOL(s, xlogDestroy);
    XL_ADD_XRT_SYMBOL(s, xlogDefault);
    XL_ADD_XRT_SYMBOL(s, xlogSetLevel);
    XL_ADD_XRT_SYMBOL(s, xlogGetLevel);
    XL_ADD_XRT_SYMBOL(s, xlogAddConsole);
    XL_ADD_XRT_SYMBOL(s, xlogAddFile);
    XL_ADD_XRT_SYMBOL(s, xlogAddRollingFile);
    XL_ADD_XRT_SYMBOL(s, xlogAppenderSetFormat);
    XL_ADD_XRT_SYMBOL(s, xlogLevelName);
    XL_ADD_XRT_SYMBOL(s, xlogWrite);
    XL_ADD_XRT_SYMBOL(s, xlogFlush);
    XL_ADD_XRT_SYMBOL(s, xrtPathExists);
    XL_ADD_XRT_SYMBOL(s, xrtPathIsAbs);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetNameExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetName);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetDir);
    XL_ADD_XRT_SYMBOL(s, xrtPathJoin);
    XL_ADD_XRT_SYMBOL(s, xrtPathRandom);
    XL_ADD_XRT_SYMBOL(s, xrtPathSep);
    XL_ADD_XRT_SYMBOL(s, xrtPathNormalize);
    XL_ADD_XRT_SYMBOL(s, xrtPathAbs);
    XL_ADD_XRT_SYMBOL(s, xrtPathRel);
    XL_ADD_XRT_SYMBOL(s, xrtPathCwd);
    XL_ADD_XRT_SYMBOL(s, xrtPathSetCwd);
    XL_ADD_XRT_SYMBOL(s, xrtPathHome);
    XL_ADD_XRT_SYMBOL(s, xrtPathTemp);
    XL_ADD_XRT_SYMBOL(s, xrtPathAppDir);
    XL_ADD_XRT_SYMBOL(s, xrtPathWithExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathWithName);
    XL_ADD_XRT_SYMBOL(s, xrtFileExists);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetSize);
    XL_ADD_XRT_SYMBOL(s, xrtFileSetSize);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAttr);
    XL_ADD_XRT_SYMBOL(s, xrtFileSetAttr);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAccessTime);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetChangeTime);
    XL_ADD_XRT_SYMBOL(s, xrtFileReadAll);
    XL_ADD_XRT_SYMBOL(s, xrtFileWriteAll);
    XL_ADD_XRT_SYMBOL(s, xrtFileAppend);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAll);
    XL_ADD_XRT_SYMBOL(s, xrtFilePutAll);
    XL_ADD_XRT_SYMBOL(s, xrtOpen);
    XL_ADD_XRT_SYMBOL(s, xrtClose);
    XL_ADD_XRT_SYMBOL(s, xrtSeek);
    XL_ADD_XRT_SYMBOL(s, xrtTell);
    XL_ADD_XRT_SYMBOL(s, xrtGetEOF);
    XL_ADD_XRT_SYMBOL(s, xrtEOF);
    XL_ADD_XRT_SYMBOL(s, xrtSetEOF);
    XL_ADD_XRT_SYMBOL(s, xrtRead);
    XL_ADD_XRT_SYMBOL(s, xrtGet);
    XL_ADD_XRT_SYMBOL(s, xrtWrite);
    XL_ADD_XRT_SYMBOL(s, xrtFileDelete);
    XL_ADD_XRT_SYMBOL(s, xrtFileCopy);
    XL_ADD_XRT_SYMBOL(s, xrtFileMove);
    XL_ADD_XRT_SYMBOL(s, xrtFileRename);
    XL_ADD_XRT_SYMBOL(s, xrtFileReplace);
    XL_ADD_XRT_SYMBOL(s, xrtFileTouch);
    XL_ADD_XRT_SYMBOL(s, xrtFileWriteAllAtomic);
    XL_ADD_XRT_SYMBOL(s, xrtFileAppendAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileReadAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileWriteAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFilePutAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileCopyAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileMoveAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileDeleteAsync);
    XL_ADD_XRT_SYMBOL(s, xrtAsyncFileBufDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtAsyncFileIoDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtDirExists);
    XL_ADD_XRT_SYMBOL(s, xrtDirCreate);
    XL_ADD_XRT_SYMBOL(s, xrtDirCreateAll);
    XL_ADD_XRT_SYMBOL(s, xrtDirScanEx);
    XL_ADD_XRT_SYMBOL(s, xrtDirCopy);
    XL_ADD_XRT_SYMBOL(s, xrtDirMove);
    XL_ADD_XRT_SYMBOL(s, xrtDirDelete);
    XL_ADD_XRT_SYMBOL(s, xrtDirIsEmpty);
    XL_ADD_XRT_SYMBOL(s, xrtDirCount);
    XL_ADD_XRT_SYMBOL(s, xrtDirSize);
    XL_ADD_XRT_SYMBOL(s, xrtDirIsRoot);
    XL_ADD_XRT_SYMBOL(s, xrtDirClean);
    XL_ADD_XRT_SYMBOL(s, xrtDirEnsureEmpty);
    XL_ADD_XRT_SYMBOL(s, xrtParseJSON);
    XL_ADD_XRT_SYMBOL(s, xrtJsonValid);
    XL_ADD_XRT_SYMBOL(s, xrtParseJSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyJSON);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyJSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtParseXSON);
    XL_ADD_XRT_SYMBOL(s, xrtXsonValid);
    XL_ADD_XRT_SYMBOL(s, xrtParseXSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyXSON);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyXSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtHexEncode);
    XL_ADD_XRT_SYMBOL(s, xrtHexDecode);
    XL_ADD_XRT_SYMBOL(s, xrtBase64Encode);
    XL_ADD_XRT_SYMBOL(s, xrtBase64Decode);
    XL_ADD_XRT_SYMBOL(s, xrtHexEncodeBytes);
    XL_ADD_XRT_SYMBOL(s, xrtBase64EncodeBytes);
    XL_ADD_XRT_SYMBOL(s, xrtPercentEncode);
    XL_ADD_XRT_SYMBOL(s, xrtPercentDecode);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncode);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlDecode);
    XL_ADD_XRT_SYMBOL(s, xrtSHA1);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256Init);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256Update);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256Final);
    XL_ADD_XRT_SYMBOL(s, xrtSHA384);
    XL_ADD_XRT_SYMBOL(s, xrtSHA384Init);
    XL_ADD_XRT_SYMBOL(s, xrtSHA384Final);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512Init);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512Update);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512Final);
    XL_ADD_XRT_SYMBOL(s, xrtHMAC_SHA256);
    XL_ADD_XRT_SYMBOL(s, xrtHMAC_SHA384);
    XL_ADD_XRT_SYMBOL(s, xrtHMAC_SHA512);
    XL_ADD_XRT_SYMBOL(s, xrtChaCha20);
    XL_ADD_XRT_SYMBOL(s, xrtChaCha20Poly1305Encrypt);
    XL_ADD_XRT_SYMBOL(s, xrtChaCha20Poly1305Decrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES128GCMEncrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES128GCMDecrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES256GCMEncrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES256GCMDecrypt);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExtract);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExpand);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExtract_SHA384);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExpand_SHA384);
    XL_ADD_XRT_SYMBOL(s, xrtX25519Keypair);
    XL_ADD_XRT_SYMBOL(s, xrtX25519SharedSecret);
    XL_ADD_XRT_SYMBOL(s, xrtX448Keypair);
    XL_ADD_XRT_SYMBOL(s, xrtX448SharedSecret);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519Keypair);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519PublicKey);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519Sign);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519Verify);
    XL_ADD_XRT_SYMBOL(s, xrtRSAPSSVerify);
    XL_ADD_XRT_SYMBOL(s, xrtRSAPKCS1Verify);
    XL_ADD_XRT_SYMBOL(s, xrtMakeXIDS);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalIP);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalMAC);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalName);
    XL_ADD_XRT_SYMBOL(s, xrtUrlParseView);
    XL_ADD_XRT_SYMBOL(s, xrtUrlParseViewN);
    XL_ADD_XRT_SYMBOL(s, xrtUrlResolveTo);
    XL_ADD_XRT_SYMBOL(s, xrtUrlNormalizePathTo);
    XL_ADD_XRT_SYMBOL(s, xrtUrlViewCopyTargetTo);
    XL_ADD_XRT_SYMBOL(s, xrtPercentEncodeTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedDecodeTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedAppendFieldTo);
    XL_ADD_XRT_SYMBOL(s, xrtQueryValidate);
    XL_ADD_XRT_SYMBOL(s, xrtQueryAppendPairTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedValidateN);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCreate);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCreateEx);
    XL_ADD_XRT_SYMBOL(s, xrtRegexDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexIsMatch);
    XL_ADD_XRT_SYMBOL(s, xrtRegexFullMatch);
    XL_ADD_XRT_SYMBOL(s, xrtRegexFind);
    XL_ADD_XRT_SYMBOL(s, xrtRegexFindMatchAt);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchOk);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchByteStart);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchByteEnd);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchStart);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchEnd);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchText);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchTextCopy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCaptureCount);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCaptureName);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetCreateEx);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetIsMatchAt);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetFirstAt);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetMatchesAt);
    XL_ADD_XRT_SYMBOL(s, xrtProcessConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtProcessSpawn);
    XL_ADD_XRT_SYMBOL(s, xrtProcessDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtProcessIsRunning);
    XL_ADD_XRT_SYMBOL(s, xrtProcessExitCode);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWrite);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWriteText);
    XL_ADD_XRT_SYMBOL(s, xrtProcessCloseStdin);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWait);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtProcessTerminate);
    XL_ADD_XRT_SYMBOL(s, xrtProcessKill);
    XL_ADD_XRT_SYMBOL(s, xrtProcessKillTree);
    XL_ADD_XRT_SYMBOL(s, xrtProcessGetStdout);
    XL_ADD_XRT_SYMBOL(s, xrtProcessGetStderr);
    XL_ADD_XRT_SYMBOL(s, xrtExecCapture);
    XL_ADD_XRT_SYMBOL(s, xrtProcessRun);
    XL_ADD_XRT_SYMBOL(s, xrtProcessResultUnit);
    XL_ADD_XRT_SYMBOL(s, xrtProcessResultCopy);
    XL_ADD_XRT_SYMBOL(s, xrtProcessResultSuccess);
    XL_ADD_XRT_SYMBOL(s, xrtProcessPipelineRun);
    XL_ADD_XRT_SYMBOL(s, xrtProcessPipelineResultUnit);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterBegin);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterEnd);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterNext);
    XL_ADD_XRT_SYMBOL(s, xrtListWalk);
    XL_ADD_XRT_SYMBOL(s, xrtDictWalk);
    XL_ADD_XRT_SYMBOL(s, xrtCallableCreate);
    XL_ADD_XRT_SYMBOL(s, xrtCallableAddRef);
    XL_ADD_XRT_SYMBOL(s, xrtCallableUnref);
    XL_ADD_XRT_SYMBOL(s, xrtCallableSig);
    XL_ADD_XRT_SYMBOL(s, xrtCallableInvoke);
    XL_ADD_XRT_SYMBOL(s, xrtCallResultInit);
    XL_ADD_XRT_SYMBOL(s, xrtCallResultSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtCallResultUnit);
    XL_ADD_XRT_SYMBOL(s, xrtFuncSigTypeId);
    XL_ADD_XRT_SYMBOL(s, xrtTypeNull);
    XL_ADD_XRT_SYMBOL(s, xrtTypeBool);
    XL_ADD_XRT_SYMBOL(s, xrtTypeInt);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFloat);
    XL_ADD_XRT_SYMBOL(s, xrtTypeString);
    XL_ADD_XRT_SYMBOL(s, xrtTypeTime);
    XL_ADD_XRT_SYMBOL(s, xrtTypePoint);
    XL_ADD_XRT_SYMBOL(s, xrtTypeType);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFunction);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFuture);
    XL_ADD_XRT_SYMBOL(s, xrtTypeArray);
    XL_ADD_XRT_SYMBOL(s, xrtTypeList);
    XL_ADD_XRT_SYMBOL(s, xrtTypeSet);
    XL_ADD_XRT_SYMBOL(s, xrtTypeDict);
    XL_ADD_XRT_SYMBOL(s, xrtTypeValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypeRecord);
    XL_ADD_XRT_SYMBOL(s, xrtTypeHandle);
    XL_ADD_XRT_SYMBOL(s, xrtTypeSame);
    XL_ADD_XRT_SYMBOL(s, xrtTypeIsA);
    XL_ADD_XRT_SYMBOL(s, xrtTypeName);
    XL_ADD_XRT_SYMBOL(s, xrtTypeGenericArgCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypeGenericArg);
    XL_ADD_XRT_SYMBOL(s, xrtTypeMethods);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFindMethod);
    XL_ADD_XRT_SYMBOL(s, xrtTypeToStringValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypeBoxValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypeUnboxValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypeDropValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayFromValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayToValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayRetain);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayRelease);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayCloneEx);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayMoveToShared);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayClone);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayAppend);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayAppendValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArraySet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArraySetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayInsertValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayTakeValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayPopValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayReverse);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayIndexOfValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayContainsValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayExtend);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayConcat);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayItemType);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListFromValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListToValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListRetain);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListRelease);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListCloneEx);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListMoveToShared);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListSet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListTakeValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListContainsValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListMerge);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListClone);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListItemType);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetFromValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetToValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetRetain);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetRelease);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetCloneEx);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetMoveToShared);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetAdd);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetAddValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetExists);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetExistsValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetRemoveValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetMerge);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetClone);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetUnion);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetIntersection);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetDifference);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetSymmetricDifference);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetIsSubset);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetIsSuperset);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetItemType);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictFromValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictToValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictRetain);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictRelease);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictCloneEx);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictMoveToShared);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictSet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictExists);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictTakeValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictMerge);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictClone);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictItemType);
    XL_ADD_XRT_SYMBOL(s, xTaskRunThread);
    XL_ADD_XRT_SYMBOL(s, xFutureCreate);
    XL_ADD_XRT_SYMBOL(s, xFutureAddRef);
    XL_ADD_XRT_SYMBOL(s, xFutureWait);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitCo);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitValue);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitCoValue);
    XL_ADD_XRT_SYMBOL(s, xFutureState);
    XL_ADD_XRT_SYMBOL(s, xFutureStatus);
    XL_ADD_XRT_SYMBOL(s, xFutureValue);
    XL_ADD_XRT_SYMBOL(s, xFutureValueIsOwned);
    XL_ADD_XRT_SYMBOL(s, xFutureGetResult);
    XL_ADD_XRT_SYMBOL(s, xFutureSetDebugName);
    XL_ADD_XRT_SYMBOL(s, xFutureError);
    XL_ADD_XRT_SYMBOL(s, xFutureRelease);
    XL_ADD_XRT_SYMBOL(s, xFutureRequestCancel);
    XL_ADD_XRT_SYMBOL(s, xFutureForwardCancelTo);
    XL_ADD_XRT_SYMBOL(s, xFutureWhenAny);
    XL_ADD_XRT_SYMBOL(s, xFutureWhenAll);
    XL_ADD_XRT_SYMBOL(s, xFutureRace);
    XL_ADD_XRT_SYMBOL(s, xFutureGetGroupSourceIndex);
    XL_ADD_XRT_SYMBOL(s, xFutureGetAllValueCount);
    XL_ADD_XRT_SYMBOL(s, xFutureGetAllValueItem);
    XL_ADD_XRT_SYMBOL(s, xPromiseCreate);
    XL_ADD_XRT_SYMBOL(s, xPromiseDestroy);
    XL_ADD_XRT_SYMBOL(s, xPromiseGetFuture);
    XL_ADD_XRT_SYMBOL(s, xPromisePeekFuture);
    XL_ADD_XRT_SYMBOL(s, xPromiseResolve);
    XL_ADD_XRT_SYMBOL(s, xPromiseResolveOwned);
    XL_ADD_XRT_SYMBOL(s, xPromiseReject);
    XL_ADD_XRT_SYMBOL(s, xPromiseCancel);
    XL_ADD_XRT_SYMBOL(s, xPromiseClose);
    XL_ADD_XRT_SYMBOL(s, xrtChannelCreate);
    XL_ADD_XRT_SYMBOL(s, xrtChannelDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtChannelSend);
    XL_ADD_XRT_SYMBOL(s, xrtChannelRecv);
    XL_ADD_XRT_SYMBOL(s, xrtChannelTryRecv);
    XL_ADD_XRT_SYMBOL(s, xrtChannelClose);
    XL_ADD_XRT_SYMBOL(s, xrtChannelCount);
    XL_ADD_XRT_SYMBOL(s, xrtChannelIsClosed);
    XL_ADD_XRT_SYMBOL(s, xrtMutexCreate);
    XL_ADD_XRT_SYMBOL(s, xrtMutexDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtMutexLock);
    XL_ADD_XRT_SYMBOL(s, xrtMutexTryLock);
    XL_ADD_XRT_SYMBOL(s, xrtMutexUnlock);
    XL_ADD_XRT_SYMBOL(s, xrtSemCreate);
    XL_ADD_XRT_SYMBOL(s, xrtSemDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtSemWait);
    XL_ADD_XRT_SYMBOL(s, xrtSemTryWait);
    XL_ADD_XRT_SYMBOL(s, xrtSemWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtSemPost);
    XL_ADD_XRT_SYMBOL(s, xrtSemPostMultiple);
    XL_ADD_XRT_SYMBOL(s, xrtCondCreate);
    XL_ADD_XRT_SYMBOL(s, xrtCondDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtCondWait);
    XL_ADD_XRT_SYMBOL(s, xrtCondWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtCondSignal);
    XL_ADD_XRT_SYMBOL(s, xrtCondBroadcast);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockCreate);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockReadLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockTryReadLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockReadUnlock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockWriteLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockTryWriteLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockWriteUnlock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockDowngrade);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockUpgrade);
    XL_ADD_XRT_SYMBOL(s, xrtThreadCreate);
    XL_ADD_XRT_SYMBOL(s, xrtThreadDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtThreadWait);
    XL_ADD_XRT_SYMBOL(s, xrtThreadWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtThreadStop);
    XL_ADD_XRT_SYMBOL(s, xrtThreadGetState);
    XL_ADD_XRT_SYMBOL(s, xrtThreadGetExitCode);
    XL_ADD_XRT_SYMBOL(s, xrtThreadGetCurrentId);
    XL_ADD_XRT_SYMBOL(s, xrtCoCreate);
    XL_ADD_XRT_SYMBOL(s, xrtCoDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtCoCancel);
    XL_ADD_XRT_SYMBOL(s, xrtCoResume);
    XL_ADD_XRT_SYMBOL(s, xrtCoYield);
    XL_ADD_XRT_SYMBOL(s, xrtCoIsCancelRequested);
    XL_ADD_XRT_SYMBOL(s, xrtCoGetState);
    XL_ADD_XRT_SYMBOL(s, xrtClearError);
    XL_ADD_XRT_SYMBOL(s, xrtGetError);
    XL_ADD_XRT_SYMBOL(s, xrtRandomBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineCreate);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineStart);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineStop);
    XL_ADD_XRT_SYMBOL(s, xrtNetSyncShutdownHiddenEngine);
    XL_ADD_XRT_SYMBOL(s, xrtNetSyncGetHiddenEngine);
    XL_ADD_XRT_SYMBOL(s, xrtHttpCloseIdleConnections);
    XL_ADD_XRT_SYMBOL(s, xrtNetAddrParse);
    XL_ADD_XRT_SYMBOL(s, xrtNetConnectConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamCreate);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamConnect);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamClose);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSend);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamPendingSend);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamWaitTimeoutEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetChainBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetChainPeek);
    XL_ADD_XRT_SYMBOL(s, xrtNetChainConsume);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpConnect);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpConnectEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpConnectAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListen);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListenEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListenerDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpAccept);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpAcceptAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListenerPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpStreamDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpStreamClose);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamAvailable);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRead);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamReadBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamReadText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRecvBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRecvText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRecvTextAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSendBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSendText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSendTextAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamDrain);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamDrainAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamLocalAddrText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRemoteAddrText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamLocalPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRemotePort);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpBind);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpBindEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpLocalPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpLocalAddrText);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpSendBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpSendText);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpSendTextAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpRecv);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpRecvAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketBytesCopy);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketText);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketFromText);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketFromPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionCreate);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionFeedCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionPendingRecv);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionPendingCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionPeekCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionConsumeCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionDriveHandshake);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionIsReady);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionReadPlain);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionWritePlain);
    XL_ADD_XRT_SYMBOL(s, xrtHttpHeaderSplitLine);
    XL_ADD_XRT_SYMBOL(s, xrtHttpHeaderBuildLineTo);
    XL_ADD_XRT_SYMBOL(s, xrtHttpMediaTypeParse);
    XL_ADD_XRT_SYMBOL(s, xrtHttpMediaTypeFindParam);
    XL_ADD_XRT_SYMBOL(s, xrtHttpContentDispositionParse);
    XL_ADD_XRT_SYMBOL(s, xrtHttpContentDispositionDecodeFileNameTo);
    XL_ADD_XRT_SYMBOL(s, xrtMultipartNextN);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestInit);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestUnit);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestCreate);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetMethod);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetURL);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetBodyCopy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetIdleTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetVerifyPeer);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseStatusCode);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseReason);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseContentLength);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseBody);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseBodyTextCopy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpExecuteSync);
    XL_ADD_XRT_SYMBOL(s, xrtHttpExecuteSyncDefault);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdCreate);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdBoundPort);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdStart);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdStop);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdDrain);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetStatus);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseAddHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetBodyCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtWebStaticConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppCreate);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRetain);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRelease);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtWebMethodMask);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRouteAsyncEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRouteAsync);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppGet);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppPost);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppAny);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStatic);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStaticDefault);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStaticEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppError);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerCreate);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerCreateHostEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerCreateHostTlsEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStart);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStartOk);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStop);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerDrain);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerPort);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRequestCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerConnectionCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerSetApp);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerReloadApp);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRemoveHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerTlsHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRemoveTlsHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerError);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRouteAsyncEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRouteAsync);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerGet);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerPost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerAny);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStatic);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStaticDefault);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStaticEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMethod);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMethodCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMethodId);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestTarget);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestTargetCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestPath);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestPathCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQuery);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryHasValueAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeader);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderNameAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderValueAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestBodyTextCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestContentLength);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestContentType);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestIsFormUrlEncoded);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormPairAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormHasValueAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormValueCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestIsMultipartForm);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartPartAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartIsFileAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartFileNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartContentTypeCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartBodyViewAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartBodySizeAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartBodyTextCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartValueCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestLocalAddrCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestRemoteAddrCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestTlsSNICopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestLocalPort);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestRemotePort);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParam);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamNameAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamValueViewAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryValue);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryValueCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookie);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookiePairAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseStatus);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseSetHeader);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseAddHeader);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseCookie);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseDeleteCookie);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseJsonText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseReserveBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseAppendBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseAppendText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseStart);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseSend);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseWriteText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseEnd);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseFile);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseRedirect);
    XL_ADD_XRT_SYMBOL(s, xrtWebMimeByPath);
    XL_ADD_XRT_SYMBOL(s, xteParse);
    XL_ADD_XRT_SYMBOL(s, xteDestroyTemplate);
    XL_ADD_XRT_SYMBOL(s, xteMake);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetNodeCount);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetExprCount);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetArgCount);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetStringPoolSize);
    XL_ADD_XRT_SYMBOL(s, xvoCreateNull);
    XL_ADD_XRT_SYMBOL(s, xvoCreateBool);
    XL_ADD_XRT_SYMBOL(s, xvoCreateInt);
    XL_ADD_XRT_SYMBOL(s, xvoCreateFloat);
    XL_ADD_XRT_SYMBOL(s, xvoCreateText);
    XL_ADD_XRT_SYMBOL(s, xvoCreateTime);
    XL_ADD_XRT_SYMBOL(s, xvoCreatePoint);
    XL_ADD_XRT_SYMBOL(s, xvoCreateArray);
    XL_ADD_XRT_SYMBOL(s, xvoCreateList);
    XL_ADD_XRT_SYMBOL(s, xvoCreateSet);
    XL_ADD_XRT_SYMBOL(s, xvoCreateTable);
    XL_ADD_XRT_SYMBOL(s, xvoCreateColl);
    XL_ADD_XRT_SYMBOL(s, xvoCreateCallable);
    XL_ADD_XRT_SYMBOL(s, xvoCreateRecord);
    XL_ADD_XRT_SYMBOL(s, xvoCreateObject);
    XL_ADD_XRT_SYMBOL(s, xvoCreateRecordMove);
    XL_ADD_XRT_SYMBOL(s, xvoCreateHandle);
    XL_ADD_XRT_SYMBOL(s, xvoCreateArrayArgs);
    XL_ADD_XRT_SYMBOL(s, xvoCreateListArgs);
    XL_ADD_XRT_SYMBOL(s, xvoCreateSetArgs);
    XL_ADD_XRT_SYMBOL(s, xvoCreateTableArgs);
    XL_ADD_XRT_SYMBOL(s, xvoAddRef);
    XL_ADD_XRT_SYMBOL(s, xvoShare);
    XL_ADD_XRT_SYMBOL(s, xvoMoveToShared);
    XL_ADD_XRT_SYMBOL(s, xvoEnsureUnique);
    XL_ADD_XRT_SYMBOL(s, xvoIsShared);
    XL_ADD_XRT_SYMBOL(s, xvoUnref);
    XL_ADD_XRT_SYMBOL(s, xvoCopy);
    XL_ADD_XRT_SYMBOL(s, xvoDeepCopy);
    XL_ADD_XRT_SYMBOL(s, xvoCopyEx);
    XL_ADD_XRT_SYMBOL(s, xvoDeepCopyEx);
    XL_ADD_XRT_SYMBOL(s, xvoType);
    XL_ADD_XRT_SYMBOL(s, xvoTypeDesc);
    XL_ADD_XRT_SYMBOL(s, xvoIsNull);
    XL_ADD_XRT_SYMBOL(s, xvoIsArray);
    XL_ADD_XRT_SYMBOL(s, xvoGetBool);
    XL_ADD_XRT_SYMBOL(s, xvoGetInt);
    XL_ADD_XRT_SYMBOL(s, xvoGetFloat);
    XL_ADD_XRT_SYMBOL(s, xvoGetText);
    XL_ADD_XRT_SYMBOL(s, xvoGetTime);
    XL_ADD_XRT_SYMBOL(s, xvoGetPoint);
    XL_ADD_XRT_SYMBOL(s, xvoGetCallable);
    XL_ADD_XRT_SYMBOL(s, xvoGetArray);
    XL_ADD_XRT_SYMBOL(s, xvoGetMutableArray);
    XL_ADD_XRT_SYMBOL(s, xvoGetColl);
    XL_ADD_XRT_SYMBOL(s, xvoGetMutableColl);
    XL_ADD_XRT_SYMBOL(s, xvoGetList);
    XL_ADD_XRT_SYMBOL(s, xvoGetMutableList);
    XL_ADD_XRT_SYMBOL(s, xvoGetSet);
    XL_ADD_XRT_SYMBOL(s, xvoGetMutableSet);
    XL_ADD_XRT_SYMBOL(s, xvoGetTable);
    XL_ADD_XRT_SYMBOL(s, xvoGetMutableTable);
    XL_ADD_XRT_SYMBOL(s, xvoGetRecordData);
    XL_ADD_XRT_SYMBOL(s, xvoGetRecordOwner);
    XL_ADD_XRT_SYMBOL(s, xvoGetHandleData);
    XL_ADD_XRT_SYMBOL(s, xvoGetHandleSlot);
    XL_ADD_XRT_SYMBOL(s, xvoToString);
    XL_ADD_XRT_SYMBOL(s, xvoInvoke);
    XL_ADD_XRT_SYMBOL(s, xvoCallableSig);
    XL_ADD_XRT_SYMBOL(s, xvoCreateArrayEx);
    XL_ADD_XRT_SYMBOL(s, xvoArrayAppendValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayInsertValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayRemove);
    XL_ADD_XRT_SYMBOL(s, xvoArrayTakeValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayPopValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayClear);
    XL_ADD_XRT_SYMBOL(s, xvoArrayAlloc);
    XL_ADD_XRT_SYMBOL(s, xvoArraySwap);
    XL_ADD_XRT_SYMBOL(s, xvoArrayItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoArrayGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoArraySetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoListGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListRemove);
    XL_ADD_XRT_SYMBOL(s, xvoListTakeValue);
    XL_ADD_XRT_SYMBOL(s, xvoListClear);
    XL_ADD_XRT_SYMBOL(s, xvoSetItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoSetGetValueAt);
    XL_ADD_XRT_SYMBOL(s, xvoSetAddValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetExistsValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetRemoveValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetClear);
    XL_ADD_XRT_SYMBOL(s, xvoTableItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoTableExists);
    XL_ADD_XRT_SYMBOL(s, xvoTableGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableMerge);
    XL_ADD_XRT_SYMBOL(s, xvoTableRemove);
    XL_ADD_XRT_SYMBOL(s, xvoTableTakeValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableClear);
    XL_ADD_XRT_SYMBOL(s, xvoTableWalk);
    XL_ADD_XRT_SYMBOL(s, xvoCollItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoCollExists);
    XL_ADD_XRT_SYMBOL(s, xvoCollSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoBasicCompare);
    XL_ADD_XRT_SYMBOL(s, xvoBasicEqual);
    XL_ADD_XRT_SYMBOL(s, xvoValueTruthy);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetI64);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetMutableI64);
    XL_ADD_XRT_SYMBOL(s, xvoIndexSetI64);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetKey);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetMutableKey);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetMutableValue);
    XL_ADD_XRT_SYMBOL(s, xvoValueContains);
    XL_ADD_XRT_SYMBOL(s, xvoPrint);
    XL_ADD_XRT_SYMBOL(s, xvoSetTypeDesc);
    if ( !xlApiAddTccSymbol(s, "xCore", (const void*)&xCore) ) {
        return 0;
    }
    return xlAddDefaultTccApiSymbols(s);
}
