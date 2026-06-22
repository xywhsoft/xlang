#include "include/xl.h"

#include "lib/xrt/xrt.h"
#include "libtcc.h"
#include "src/cgen/xl6_cgen.h"
#include "src/common/xl6_common.h"
#include "src/compile/xl6_compile_unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

typedef TCCState* (*XL_TccNewProc)(void);
typedef void (*XL_TccDeleteProc)(TCCState* s);
typedef void (*XL_TccSetLibPathProc)(TCCState* s, const char* path);
typedef void (*XL_TccSetErrorFuncProc)(TCCState* s, void* opaque, TCCErrorFunc* errorFunc);
typedef int (*XL_TccSetOptionsProc)(TCCState* s, const char* options);
typedef int (*XL_TccAddIncludePathProc)(TCCState* s, const char* path);
typedef int (*XL_TccAddLibraryPathProc)(TCCState* s, const char* path);
typedef int (*XL_TccAddLibraryProc)(TCCState* s, const char* name);
typedef int (*XL_TccAddFileProc)(TCCState* s, const char* path);
typedef int (*XL_TccAddSymbolProc)(TCCState* s, const char* name, const void* value);
typedef int (*XL_TccSetOutputTypeProc)(TCCState* s, int outputType);
typedef int (*XL_TccCompileStringProc)(TCCState* s, const char* source);
typedef int (*XL_TccOutputFileProc)(TCCState* s, const char* path);
typedef int (*XL_TccRunProc)(TCCState* s, int argc, char** argv);

typedef struct XL_TccApi {
    int ready;
    char* path;
#ifdef _WIN32
    HMODULE module;
#else
    void* module;
#endif
    XL_TccNewProc tcc_new;
    XL_TccDeleteProc tcc_delete;
    XL_TccSetLibPathProc tcc_set_lib_path;
    XL_TccSetErrorFuncProc tcc_set_error_func;
    XL_TccSetOptionsProc tcc_set_options;
    XL_TccAddIncludePathProc tcc_add_include_path;
    XL_TccAddLibraryPathProc tcc_add_library_path;
    XL_TccAddLibraryProc tcc_add_library;
    XL_TccAddFileProc tcc_add_file;
    XL_TccAddSymbolProc tcc_add_symbol;
    XL_TccSetOutputTypeProc tcc_set_output_type;
    XL_TccCompileStringProc tcc_compile_string;
    XL_TccOutputFileProc tcc_output_file;
    XL_TccRunProc tcc_run;
} XL_TccApi;

typedef struct XL_StringList {
    char** items;
    size_t count;
} XL_StringList;

typedef struct XL_CSymbol {
    char* name;
    void* value;
} XL_CSymbol;

struct XL_Context {
    char* workDir;
    char* tempDir;
    char* tccDllPath;
    char* tccLibPath;
    int saveTemps;
    char* lastError;
    XL_StringList includePaths;
    XL_StringList libraryPaths;
    XL_StringList linkLibraries;
    XL_StringList loadLibraries;
    XL_CSymbol* symbols;
    size_t symbolCount;
};

typedef struct XL_CwdScope {
    int active;
#ifdef _WIN32
    wchar_t* oldDir;
#else
    char* oldDir;
#endif
} XL_CwdScope;

static XL_TccApi xl__tcc;
static int xl__runtimeCount = 0;
static const float xl__mzerosf = -0.0f;
static const double xl__mzerodf = -0.0;

static char* xlDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtCopyStr((char*)src, strlen(src));
}

static void xlSetError(XL_Context* ctx, const char* text)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->lastError);
    ctx->lastError = xlDupText(text != NULL && text[0] != '\0' ? text : "xlang api error");
}

static void xlSetError2(XL_Context* ctx, const char* prefix, const char* detail)
{
    char text[1024];

    snprintf(text, sizeof(text), "%s%s%s",
             prefix != NULL ? prefix : "xlang api error",
             detail != NULL && detail[0] != '\0' ? ": " : "",
             detail != NULL ? detail : "");
    xlSetError(ctx, text);
}

static void xlStringListFree(XL_StringList* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        xrtFree(list->items[i]);
    }
    xrtFree(list->items);
    memset(list, 0, sizeof(*list));
}

static int xlStringListAppend(XL_StringList* list, const char* text)
{
    char** newItems;
    char* copy;

    if ( list == NULL || text == NULL || text[0] == '\0' ) {
        return 0;
    }
    copy = xlDupText(text);
    if ( copy == NULL ) {
        return 0;
    }
    newItems = (char**)xrtRealloc(list->items, (list->count + 1) * sizeof(char*));
    if ( newItems == NULL ) {
        xrtFree(copy);
        return 0;
    }
    list->items = newItems;
    list->items[list->count] = copy;
    list->count += 1;
    return 1;
}

static int xlContextAddSymbol(XL_Context* ctx, const char* name, void* value)
{
    XL_CSymbol* newItems;
    size_t i;

    if ( ctx == NULL || name == NULL || name[0] == '\0' || value == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->symbolCount; ++i ) {
        if ( strcmp(ctx->symbols[i].name, name) == 0 ) {
            ctx->symbols[i].value = value;
            return 1;
        }
    }
    newItems = (XL_CSymbol*)xrtRealloc(ctx->symbols, (ctx->symbolCount + 1) * sizeof(XL_CSymbol));
    if ( newItems == NULL ) {
        return 0;
    }
    ctx->symbols = newItems;
    ctx->symbols[ctx->symbolCount].name = xlDupText(name);
    ctx->symbols[ctx->symbolCount].value = value;
    if ( ctx->symbols[ctx->symbolCount].name == NULL ) {
        return 0;
    }
    ctx->symbolCount += 1;
    return 1;
}

static void xlFreeSymbols(XL_Context* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->symbolCount; ++i ) {
        xrtFree(ctx->symbols[i].name);
    }
    xrtFree(ctx->symbols);
    ctx->symbols = NULL;
    ctx->symbolCount = 0;
}

static int xlEnterWorkDir(XL_Context* ctx, XL_CwdScope* scope)
{
    if ( scope == NULL ) {
        return 0;
    }
    memset(scope, 0, sizeof(*scope));
    if ( ctx == NULL || ctx->workDir == NULL || ctx->workDir[0] == '\0' ) {
        return 1;
    }
#ifdef _WIN32
    {
        DWORD need;
        DWORD got;
        u16str workDirW;

        need = GetCurrentDirectoryW(0, NULL);
        if ( need == 0 ) {
            xlSetError(ctx, "cannot query current directory");
            return 0;
        }
        scope->oldDir = (wchar_t*)xrtMalloc(((size_t)need + 1) * sizeof(wchar_t));
        if ( scope->oldDir == NULL ) {
            xlSetError(ctx, "cannot allocate current directory buffer");
            return 0;
        }
        got = GetCurrentDirectoryW(need + 1, scope->oldDir);
        if ( got == 0 || got > need ) {
            xrtFree(scope->oldDir);
            scope->oldDir = NULL;
            xlSetError(ctx, "cannot save current directory");
            return 0;
        }
        workDirW = xrtUTF8to16((u8str)ctx->workDir, 0, NULL);
        if ( workDirW == NULL || !SetCurrentDirectoryW((LPCWSTR)workDirW) ) {
            xrtFree(workDirW);
            xrtFree(scope->oldDir);
            scope->oldDir = NULL;
            xlSetError2(ctx, "cannot enter work dir", ctx->workDir);
            return 0;
        }
        xrtFree(workDirW);
        scope->active = 1;
        return 1;
    }
#else
    {
        char oldBuf[1024];
        if ( getcwd(oldBuf, sizeof(oldBuf)) == NULL ) {
            xlSetError(ctx, "cannot save current directory");
            return 0;
        }
        scope->oldDir = xlDupText(oldBuf);
        if ( scope->oldDir == NULL || chdir(ctx->workDir) != 0 ) {
            xrtFree(scope->oldDir);
            scope->oldDir = NULL;
            xlSetError2(ctx, "cannot enter work dir", ctx->workDir);
            return 0;
        }
        scope->active = 1;
        return 1;
    }
#endif
}

static void xlLeaveWorkDir(XL_CwdScope* scope)
{
    if ( scope == NULL || !scope->active ) {
        return;
    }
#ifdef _WIN32
    if ( scope->oldDir != NULL ) {
        SetCurrentDirectoryW(scope->oldDir);
        xrtFree(scope->oldDir);
    }
#else
    if ( scope->oldDir != NULL ) {
        chdir(scope->oldDir);
        xrtFree(scope->oldDir);
    }
#endif
    memset(scope, 0, sizeof(*scope));
}

static void xlTccError(void* opaque, const char* msg)
{
    XL_Context* ctx = (XL_Context*)opaque;

    if ( ctx != NULL && msg != NULL && msg[0] != '\0' ) {
        xlSetError2(ctx, "tcc", msg);
    }
}

static void* xlLoadProc(XL_TccApi* api, const char* name)
{
#ifdef _WIN32
    return api != NULL && api->module != NULL ? (void*)GetProcAddress(api->module, name) : NULL;
#else
    return api != NULL && api->module != NULL ? dlsym(api->module, name) : NULL;
#endif
}

static int xlLoadTccFromPath(XL_Context* ctx, const char* path)
{
    XL_TccApi* api;

    api = &xl__tcc;
    if ( api->ready ) {
        return 1;
    }
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
#ifdef _WIN32
    {
        u16str pathW = xrtUTF8to16((u8str)path, 0, NULL);
        if ( pathW != NULL ) {
            api->module = LoadLibraryW((LPCWSTR)pathW);
            xrtFree(pathW);
        }
    }
#else
    api->module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if ( api->module == NULL ) {
        return 0;
    }

#define XL_LOAD_TCC_PROC(field) \
    do { \
        api->field = (XL_Tcc##field##Proc)xlLoadProc(api, #field); \
        if ( api->field == NULL ) { \
            xlSetError2(ctx, "libtcc missing symbol", #field); \
            return 0; \
        } \
    } while (0)

    api->tcc_new = (XL_TccNewProc)xlLoadProc(api, "tcc_new");
    api->tcc_delete = (XL_TccDeleteProc)xlLoadProc(api, "tcc_delete");
    api->tcc_set_lib_path = (XL_TccSetLibPathProc)xlLoadProc(api, "tcc_set_lib_path");
    api->tcc_set_error_func = (XL_TccSetErrorFuncProc)xlLoadProc(api, "tcc_set_error_func");
    api->tcc_set_options = (XL_TccSetOptionsProc)xlLoadProc(api, "tcc_set_options");
    api->tcc_add_include_path = (XL_TccAddIncludePathProc)xlLoadProc(api, "tcc_add_include_path");
    api->tcc_add_library_path = (XL_TccAddLibraryPathProc)xlLoadProc(api, "tcc_add_library_path");
    api->tcc_add_library = (XL_TccAddLibraryProc)xlLoadProc(api, "tcc_add_library");
    api->tcc_add_file = (XL_TccAddFileProc)xlLoadProc(api, "tcc_add_file");
    api->tcc_add_symbol = (XL_TccAddSymbolProc)xlLoadProc(api, "tcc_add_symbol");
    api->tcc_set_output_type = (XL_TccSetOutputTypeProc)xlLoadProc(api, "tcc_set_output_type");
    api->tcc_compile_string = (XL_TccCompileStringProc)xlLoadProc(api, "tcc_compile_string");
    api->tcc_output_file = (XL_TccOutputFileProc)xlLoadProc(api, "tcc_output_file");
    api->tcc_run = (XL_TccRunProc)xlLoadProc(api, "tcc_run");

    if ( api->tcc_new == NULL || api->tcc_delete == NULL || api->tcc_set_lib_path == NULL ||
         api->tcc_set_error_func == NULL || api->tcc_add_include_path == NULL ||
         api->tcc_add_library_path == NULL || api->tcc_add_library == NULL ||
         api->tcc_add_file == NULL || api->tcc_add_symbol == NULL ||
         api->tcc_set_output_type == NULL || api->tcc_compile_string == NULL ||
         api->tcc_output_file == NULL || api->tcc_run == NULL ) {
        xlSetError2(ctx, "invalid libtcc dll", path);
        return 0;
    }
    api->path = xlDupText(path);
    api->ready = 1;
    return 1;
}

static int xlEnsureTcc(XL_Context* ctx)
{
    const char* path;

    if ( xl__tcc.ready ) {
        return 1;
    }
    path = ctx != NULL && ctx->tccDllPath != NULL && ctx->tccDllPath[0] != '\0' ? ctx->tccDllPath : NULL;
    if ( xlLoadTccFromPath(ctx, path) ) {
        return 1;
    }
    if ( xlLoadTccFromPath(ctx, "libtcc.dll") ) {
        return 1;
    }
    if ( xlLoadTccFromPath(ctx, "release\\libtcc.dll") ) {
        return 1;
    }
    if ( xlLoadTccFromPath(ctx, "..\\demo5\\lib\\tcc\\libtcc.dll") ) {
        return 1;
    }
    xlSetError(ctx, "cannot load libtcc.dll");
    return 0;
}

static int xlAddTccSymbol(TCCState* s, const char* name, const void* value)
{
    return xl__tcc.tcc_add_symbol(s, name, value) == 0;
}

#define XL_ADD_XRT_SYMBOL(s, name) \
    do { \
        if ( !xlAddTccSymbol((s), #name, (const void*)(name)) ) { \
            return 0; \
        } \
    } while (0)

static int xlAddDefaultRuntimeSymbols(TCCState* s)
{
    XL_ADD_XRT_SYMBOL(s, xrtInit);
    if ( !xlAddTccSymbol(s, "__mzerosf", &xl__mzerosf) ||
         !xlAddTccSymbol(s, "__mzerodf", &xl__mzerodf) ) {
        return 0;
    }
    XL_ADD_XRT_SYMBOL(s, xrtUnit);
    XL_ADD_XRT_SYMBOL(s, xrtMalloc);
    XL_ADD_XRT_SYMBOL(s, xrtCalloc);
    XL_ADD_XRT_SYMBOL(s, xrtRealloc);
    XL_ADD_XRT_SYMBOL(s, xrtFree);
    XL_ADD_XRT_SYMBOL(s, xrtTempMemory);
    XL_ADD_XRT_SYMBOL(s, xrtFreeTempMemory);
    XL_ADD_XRT_SYMBOL(s, xrtCopyStr);
    XL_ADD_XRT_SYMBOL(s, xrtFormat);
    XL_ADD_XRT_SYMBOL(s, xrtHash32);
    XL_ADD_XRT_SYMBOL(s, xrtHash32_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtHash64);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtStrComp);
    XL_ADD_XRT_SYMBOL(s, xrtFindStr);
    XL_ADD_XRT_SYMBOL(s, xrtInStr);
    XL_ADD_XRT_SYMBOL(s, xrtCheckStr);
    XL_ADD_XRT_SYMBOL(s, xrtStrLike);
    XL_ADD_XRT_SYMBOL(s, xrtStrApprox);
    XL_ADD_XRT_SYMBOL(s, xrtStrSim);
    XL_ADD_XRT_SYMBOL(s, xrtIsUTF8);
    XL_ADD_XRT_SYMBOL(s, xrtIntFormat);
    XL_ADD_XRT_SYMBOL(s, xrtNumFormat);
    XL_ADD_XRT_SYMBOL(s, xrtLCase);
    XL_ADD_XRT_SYMBOL(s, xrtUCase);
    XL_ADD_XRT_SYMBOL(s, xrtTrim);
    XL_ADD_XRT_SYMBOL(s, xrtLTrim);
    XL_ADD_XRT_SYMBOL(s, xrtRTrim);
    XL_ADD_XRT_SYMBOL(s, xrtFilterStr);
    XL_ADD_XRT_SYMBOL(s, xrtReplace);
    XL_ADD_XRT_SYMBOL(s, xrtSplit);
    XL_ADD_XRT_SYMBOL(s, xrtIntApprox);
    XL_ADD_XRT_SYMBOL(s, xrtNumApprox);
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
    XL_ADD_XRT_SYMBOL(s, xrtTimeFormat);
    XL_ADD_XRT_SYMBOL(s, xrtTimeParse);
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
    XL_ADD_XRT_SYMBOL(s, xrtRandStr);
    XL_ADD_XRT_SYMBOL(s, xrtPathExists);
    XL_ADD_XRT_SYMBOL(s, xrtPathIsAbs);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetNameExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetName);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetDir);
    XL_ADD_XRT_SYMBOL(s, xrtPathJoin);
    XL_ADD_XRT_SYMBOL(s, xrtPathRandom);
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
    XL_ADD_XRT_SYMBOL(s, xrtFileDelete);
    XL_ADD_XRT_SYMBOL(s, xrtFileCopy);
    XL_ADD_XRT_SYMBOL(s, xrtFileMove);
    XL_ADD_XRT_SYMBOL(s, xrtDirExists);
    XL_ADD_XRT_SYMBOL(s, xrtDirCreate);
    XL_ADD_XRT_SYMBOL(s, xrtDirCreateAll);
    XL_ADD_XRT_SYMBOL(s, xrtDirScanEx);
    XL_ADD_XRT_SYMBOL(s, xrtDirCopy);
    XL_ADD_XRT_SYMBOL(s, xrtDirMove);
    XL_ADD_XRT_SYMBOL(s, xrtDirDelete);
    XL_ADD_XRT_SYMBOL(s, xrtParseJSON);
    XL_ADD_XRT_SYMBOL(s, xrtParseJSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyJSON);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyJSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtParseXSON);
    XL_ADD_XRT_SYMBOL(s, xrtParseXSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyXSON);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyXSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtHexEncode);
    XL_ADD_XRT_SYMBOL(s, xrtHexDecode);
    XL_ADD_XRT_SYMBOL(s, xrtBase64Encode);
    XL_ADD_XRT_SYMBOL(s, xrtBase64Decode);
    XL_ADD_XRT_SYMBOL(s, xrtSHA1);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256);
    XL_ADD_XRT_SYMBOL(s, xrtSHA384);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512);
    XL_ADD_XRT_SYMBOL(s, xrtMakeXIDS);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalIP);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalMAC);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalName);
    XL_ADD_XRT_SYMBOL(s, xrtUrlParseViewN);
    XL_ADD_XRT_SYMBOL(s, xrtUrlResolveTo);
    XL_ADD_XRT_SYMBOL(s, xrtUrlNormalizePathTo);
    XL_ADD_XRT_SYMBOL(s, xrtPercentEncodeTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedDecodeTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedAppendFieldTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedValidateN);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCreate);
    XL_ADD_XRT_SYMBOL(s, xrtRegexDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexIsMatch);
    XL_ADD_XRT_SYMBOL(s, xrtRegexFind);
    XL_ADD_XRT_SYMBOL(s, xrtProcessConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtExecCapture);
    XL_ADD_XRT_SYMBOL(s, xrtProcessResultUnit);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterBegin);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterEnd);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterNext);
    XL_ADD_XRT_SYMBOL(s, xrtListWalk);
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
    XL_ADD_XRT_SYMBOL(s, xrtTypeBoxValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayAppendValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetAddValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictItemAt);
    XL_ADD_XRT_SYMBOL(s, xTaskRunThread);
    XL_ADD_XRT_SYMBOL(s, xFutureWait);
    XL_ADD_XRT_SYMBOL(s, xFutureValue);
    XL_ADD_XRT_SYMBOL(s, xFutureError);
    XL_ADD_XRT_SYMBOL(s, xFutureRelease);
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
    XL_ADD_XRT_SYMBOL(s, xrtCoCreate);
    XL_ADD_XRT_SYMBOL(s, xrtCoDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtCoResume);
    XL_ADD_XRT_SYMBOL(s, xrtCoYield);
    XL_ADD_XRT_SYMBOL(s, xrtCoGetState);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineCreate);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineStart);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineStop);
    XL_ADD_XRT_SYMBOL(s, xrtNetAddrParse);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdCreate);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdBoundPort);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdStart);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdStop);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetStatus);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetBodyCopy);
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
    XL_ADD_XRT_SYMBOL(s, xvoCreateHandle);
    XL_ADD_XRT_SYMBOL(s, xvoAddRef);
    XL_ADD_XRT_SYMBOL(s, xvoUnref);
    XL_ADD_XRT_SYMBOL(s, xvoCopy);
    XL_ADD_XRT_SYMBOL(s, xvoType);
    XL_ADD_XRT_SYMBOL(s, xvoTypeDesc);
    XL_ADD_XRT_SYMBOL(s, xvoIsNull);
    XL_ADD_XRT_SYMBOL(s, xvoGetBool);
    XL_ADD_XRT_SYMBOL(s, xvoGetInt);
    XL_ADD_XRT_SYMBOL(s, xvoGetFloat);
    XL_ADD_XRT_SYMBOL(s, xvoGetText);
    XL_ADD_XRT_SYMBOL(s, xvoGetTime);
    XL_ADD_XRT_SYMBOL(s, xvoGetPoint);
    XL_ADD_XRT_SYMBOL(s, xvoGetCallable);
    XL_ADD_XRT_SYMBOL(s, xvoGetColl);
    XL_ADD_XRT_SYMBOL(s, xvoGetList);
    XL_ADD_XRT_SYMBOL(s, xvoGetTable);
    XL_ADD_XRT_SYMBOL(s, xvoGetRecordData);
    XL_ADD_XRT_SYMBOL(s, xvoGetHandleData);
    XL_ADD_XRT_SYMBOL(s, xvoToString);
    XL_ADD_XRT_SYMBOL(s, xvoInvoke);
    XL_ADD_XRT_SYMBOL(s, xvoCallableSig);
    XL_ADD_XRT_SYMBOL(s, xvoArrayAppendValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoArrayGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoArraySetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoListGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoSetGetValueAt);
    XL_ADD_XRT_SYMBOL(s, xvoTableItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoTableExists);
    XL_ADD_XRT_SYMBOL(s, xvoTableGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableRemove);
    XL_ADD_XRT_SYMBOL(s, xvoCollItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoCollExists);
    XL_ADD_XRT_SYMBOL(s, xvoCollSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetTypeDesc);
    return 1;
}

static int xlAddContextSymbols(XL_Context* ctx, TCCState* s)
{
    size_t i;

    if ( !xlAddDefaultRuntimeSymbols(s) ) {
        xlSetError(ctx, "cannot add XRT symbols to TCC state");
        return 0;
    }
    for ( i = 0; ctx != NULL && i < ctx->symbolCount; ++i ) {
        if ( !xlAddTccSymbol(s, ctx->symbols[i].name, ctx->symbols[i].value) ) {
            xlSetError2(ctx, "cannot add C function symbol", ctx->symbols[i].name);
            return 0;
        }
    }
    return 1;
}

static TCCState* xlCreateTccState(XL_Context* ctx, int outputType)
{
    TCCState* s;
    size_t i;
    const char* tccLibPath;

    if ( !xlEnsureTcc(ctx) ) {
        return NULL;
    }
    s = xl__tcc.tcc_new();
    if ( s == NULL ) {
        xlSetError(ctx, "cannot create TCC state");
        return NULL;
    }
    xl__tcc.tcc_set_error_func(s, ctx, xlTccError);
    tccLibPath = ctx != NULL && ctx->tccLibPath != NULL && ctx->tccLibPath[0] != '\0' ?
                 ctx->tccLibPath : "..\\demo5\\release\\tcc";
    xl__tcc.tcc_set_lib_path(s, tccLibPath);
    if ( xl__tcc.tcc_set_output_type(s, outputType) != 0 ) {
        xlSetError(ctx, "cannot set TCC output type");
        xl__tcc.tcc_delete(s);
        return NULL;
    }
    (void)xl__tcc.tcc_set_options(s, "-std=c11 -D_CRT_SECURE_NO_WARNINGS");
    (void)xl__tcc.tcc_add_include_path(s, ".");
    (void)xl__tcc.tcc_add_include_path(s, "src");
    (void)xl__tcc.tcc_add_include_path(s, "lib\\xrt");
    (void)xl__tcc.tcc_add_include_path(s, "..\\demo5\\lib\\tcc\\include");
    (void)xl__tcc.tcc_add_include_path(s, "..\\demo5\\release\\tcc\\include_win");
    (void)xl__tcc.tcc_add_include_path(s, "..\\demo5\\release\\tcc\\include_win\\winapi");
    (void)xl__tcc.tcc_add_library_path(s, "..\\demo5\\release\\tcc\\lib");
    if ( xrtFileExists("..\\demo5\\release\\tcc\\lib\\libtcc1-64.a") ) {
        (void)xl__tcc.tcc_add_file(s, "..\\demo5\\release\\tcc\\lib\\libtcc1-64.a");
    }
    for ( i = 0; ctx != NULL && i < ctx->includePaths.count; ++i ) {
        if ( xl__tcc.tcc_add_include_path(s, ctx->includePaths.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add include path", ctx->includePaths.items[i]);
            xl__tcc.tcc_delete(s);
            return NULL;
        }
    }
    for ( i = 0; ctx != NULL && i < ctx->libraryPaths.count; ++i ) {
        if ( xl__tcc.tcc_add_library_path(s, ctx->libraryPaths.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add library path", ctx->libraryPaths.items[i]);
            xl__tcc.tcc_delete(s);
            return NULL;
        }
    }
    if ( outputType == TCC_OUTPUT_MEMORY && !xlAddContextSymbols(ctx, s) ) {
        xl__tcc.tcc_delete(s);
        return NULL;
    }
    return s;
}

static void xlSetCompileDiagError(XL_Context* ctx, const XL6_CompileUnit* unit)
{
    char text[1200];
    const char* path;
    const char* phase;

    if ( unit != NULL && unit->parse.diags.count > 0 ) {
        const XL6_Diag* diag = &unit->parse.diags.items[0];
        path = unit->path != NULL ? unit->path : "<memory>";
        phase = unit->phase != NULL ? unit->phase : "compile";
        snprintf(text,
                 sizeof(text),
                 "%s:%d:%d: [%s] %s",
                 path,
                 diag->line,
                 diag->column,
                 phase,
                 diag->message != NULL ? diag->message : "compile error");
        xlSetError(ctx, text);
    } else {
        xlSetError(ctx, "xlang compile failed");
    }
}

static int xlCgenToMemory(XL_Context* ctx, XL6_CompileUnit* unit, char** outText)
{
    XL6_Output out;
    int ok;

    if ( outText == NULL ) {
        return 0;
    }
    *outText = NULL;
    if ( !xl6OutputOpenMemory(&out) ) {
        xlSetError(ctx, "cannot create cgen memory output");
        return 0;
    }
    ok = xl6CgenEmitC(&out, unit->parse.root, &unit->parse.diags);
    if ( ok && !xl6OutputHasFailed(&out) ) {
        *outText = xl6OutputTakeMemoryText(&out, NULL);
    }
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot generate C code", xl6OutputError(&out));
        ok = 0;
    } else if ( !ok ) {
        xlSetCompileDiagError(ctx, unit);
    } else if ( *outText == NULL ) {
        xlSetError(ctx, "cannot copy generated C code");
        ok = 0;
    }
    xl6OutputClose(&out);
    return ok && *outText != NULL;
}

static int xlLoadUnitFromSource(XL_Context* ctx, XL6_CompileUnit* unit, const char* source, const char* virtualPath)
{
    if ( source == NULL ) {
        xlSetError(ctx, "script source is empty");
        return 0;
    }
    xl6CompileUnitInit(unit);
    if ( !xl6CompileUnitLoadSource(unit, source, virtualPath, 1) ) {
        xlSetCompileDiagError(ctx, unit);
        return 0;
    }
    return 1;
}

static int xlLoadUnitFromFile(XL_Context* ctx, XL6_CompileUnit* unit, const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "script file path is empty");
        return 0;
    }
    xl6CompileUnitInit(unit);
    if ( !xl6CompileUnitLoadFile(unit, path, 1) ) {
        xlSetCompileDiagError(ctx, unit);
        return 0;
    }
    return 1;
}

static int xlAddDependenciesToTcc(XL_Context* ctx, TCCState* s, const XL6_CompileUnit* unit)
{
    size_t i;

    for ( i = 0; ctx != NULL && i < ctx->linkLibraries.count; ++i ) {
        const char* value = ctx->linkLibraries.items[i];
        if ( strchr(value, '\\') != NULL || strchr(value, '/') != NULL || strchr(value, ':') != NULL || strchr(value, '.') != NULL ) {
            if ( xl__tcc.tcc_add_file(s, value) != 0 ) {
                xlSetError2(ctx, "cannot add link file", value);
                return 0;
            }
        } else if ( xl__tcc.tcc_add_library(s, value) != 0 ) {
            xlSetError2(ctx, "cannot add link library", value);
            return 0;
        }
    }
    for ( i = 0; ctx != NULL && i < ctx->loadLibraries.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, ctx->loadLibraries.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add dependency library", ctx->loadLibraries.items[i]);
            return 0;
        }
    }
    for ( i = 0; unit != NULL && i < unit->importDeps.links.count; ++i ) {
        if ( xl__tcc.tcc_add_library(s, unit->importDeps.links.items[i]) != 0 &&
             xl__tcc.tcc_add_file(s, unit->importDeps.links.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add import link", unit->importDeps.links.items[i]);
            return 0;
        }
    }
    for ( i = 0; unit != NULL && i < unit->importDeps.loadlibs.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, unit->importDeps.loadlibs.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add import loadlib", unit->importDeps.loadlibs.items[i]);
            return 0;
        }
    }
    return 1;
}

static int xlCompileAndRunUnit(XL_Context* ctx, XL6_CompileUnit* unit)
{
    TCCState* s;
    char* cText;
    int rc;
    size_t i;

    cText = NULL;
    if ( !xlCgenToMemory(ctx, unit, &cText) ) {
        return 0;
    }
    s = xlCreateTccState(ctx, TCC_OUTPUT_MEMORY);
    if ( s == NULL ) {
        xrtFree(cText);
        return 0;
    }
    for ( i = 0; i < unit->importDeps.cSources.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, unit->importDeps.cSources.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot compile import C source", unit->importDeps.cSources.items[i]);
            xl__tcc.tcc_delete(s);
            xrtFree(cText);
            return 0;
        }
    }
    if ( !xlAddDependenciesToTcc(ctx, s, unit) ) {
        xl__tcc.tcc_delete(s);
        xrtFree(cText);
        return 0;
    }
    if ( xl__tcc.tcc_compile_string(s, cText) != 0 ) {
        if ( ctx->lastError == NULL ) {
            xlSetError(ctx, "TCC compile failed");
        }
        xl__tcc.tcc_delete(s);
        xrtFree(cText);
        return 0;
    }
    rc = xl__tcc.tcc_run(s, 1, NULL);
    xl__tcc.tcc_delete(s);
    xrtFree(cText);
    if ( rc != 0 ) {
        char text[128];
        snprintf(text, sizeof(text), "script returned %d", rc);
        xlSetError(ctx, text);
        return 0;
    }
    return 1;
}

static int xlCompileUnitToExe(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputExe)
{
    TCCState* s;
    char* cText;
    int ok;
    size_t i;

    if ( outputExe == NULL || outputExe[0] == '\0' ) {
        xlSetError(ctx, "output exe path is empty");
        return 0;
    }
    cText = NULL;
    if ( !xlCgenToMemory(ctx, unit, &cText) ) {
        return 0;
    }
    s = xlCreateTccState(ctx, TCC_OUTPUT_EXE);
    if ( s == NULL ) {
        xrtFree(cText);
        return 0;
    }
    (void)xl__tcc.tcc_set_options(s, "-DXL6_USE_XRT_LIFECYCLE");
    if ( xl__tcc.tcc_compile_string(s,
            "unsigned int __mzerosf = 0x80000000u;\n"
            "unsigned long long __mzerodf = 0x8000000000000000ull;\n") != 0 ) {
        if ( ctx->lastError == NULL ) {
            xlSetError(ctx, "cannot compile TCC runtime constants");
        }
        xl__tcc.tcc_delete(s);
        xrtFree(cText);
        return 0;
    }
    if ( xl__tcc.tcc_compile_string(s, cText) != 0 ) {
        if ( ctx->lastError == NULL ) {
            xlSetError(ctx, "TCC compile failed");
        }
        xl__tcc.tcc_delete(s);
        xrtFree(cText);
        return 0;
    }
    xrtFree(cText);
    if ( xrtFileExists("..\\demo5\\release\\tcc\\lib\\libtcc1-64.a") ) {
        (void)xl__tcc.tcc_add_file(s, "..\\demo5\\release\\tcc\\lib\\libtcc1-64.a");
    }
    for ( i = 0; i < unit->importDeps.cSources.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, unit->importDeps.cSources.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot compile import C source", unit->importDeps.cSources.items[i]);
            xl__tcc.tcc_delete(s);
            return 0;
        }
    }
    if ( !xlAddDependenciesToTcc(ctx, s, unit) ) {
        xl__tcc.tcc_delete(s);
        return 0;
    }
    if ( xrtFileExists("..\\..\\xrt\\release\\x64\\xrt.o") ) {
        if ( xl__tcc.tcc_add_file(s, "..\\..\\xrt\\release\\x64\\xrt.o") != 0 ) {
            xlSetError(ctx, "cannot add xrt object");
            xl__tcc.tcc_delete(s);
            return 0;
        }
    } else {
        xlSetError(ctx, "missing XRT object: ..\\..\\xrt\\release\\x64\\xrt.o");
        xl__tcc.tcc_delete(s);
        return 0;
    }
    (void)xl__tcc.tcc_add_library(s, "Ws2_32");
    (void)xl__tcc.tcc_add_library(s, "IPHLPAPI");
    (void)xl__tcc.tcc_add_library(s, "Shell32");
    ok = xl__tcc.tcc_output_file(s, outputExe) == 0;
    xl__tcc.tcc_delete(s);
    if ( !ok ) {
        if ( ctx->lastError == NULL ) {
            xlSetError2(ctx, "cannot output exe", outputExe);
        }
        return 0;
    }
    return 1;
}

XL_ContextOptions xlDefaultContextOptions(void)
{
    XL_ContextOptions options;

    memset(&options, 0, sizeof(options));
    options.size = (unsigned int)sizeof(options);
    options.version = XL_API_VERSION;
    options.workDir = ".";
    options.tempDir = "release\\tmp";
    options.tccDllPath = NULL;
    options.tccLibPath = "..\\demo5\\release\\tcc";
    options.saveTemps = 0;
    return options;
}

int xlRuntimeInit(void)
{
    if ( xl__runtimeCount == 0 ) {
        if ( xrtInit() == NULL ) {
            return 0;
        }
    }
    xl__runtimeCount += 1;
    return 1;
}

void xlRuntimeUnit(void)
{
    if ( xl__runtimeCount <= 0 ) {
        return;
    }
    xl__runtimeCount -= 1;
    if ( xl__runtimeCount == 0 ) {
        xrtUnit();
    }
}

XL_Context* xlCreateContext(const XL_ContextOptions* options)
{
    XL_ContextOptions localOptions;
    XL_Context* ctx;

    if ( !xlRuntimeInit() ) {
        return NULL;
    }
    localOptions = options != NULL ? *options : xlDefaultContextOptions();
    if ( localOptions.size != 0 && localOptions.size < sizeof(XL_ContextOptions) ) {
        xlRuntimeUnit();
        return NULL;
    }
    if ( localOptions.version != 0 && localOptions.version != XL_API_VERSION ) {
        xlRuntimeUnit();
        return NULL;
    }
    ctx = (XL_Context*)xrtCalloc(1, sizeof(XL_Context));
    if ( ctx == NULL ) {
        xlRuntimeUnit();
        return NULL;
    }
    ctx->workDir = xlDupText(localOptions.workDir != NULL ? localOptions.workDir : ".");
    ctx->tempDir = xlDupText(localOptions.tempDir != NULL ? localOptions.tempDir : "release\\tmp");
    ctx->tccDllPath = xlDupText(localOptions.tccDllPath != NULL ? localOptions.tccDllPath : "");
    ctx->tccLibPath = xlDupText(localOptions.tccLibPath != NULL ? localOptions.tccLibPath : "..\\demo5\\release\\tcc");
    ctx->saveTemps = localOptions.saveTemps != 0;
    if ( ctx->workDir == NULL || ctx->tempDir == NULL || ctx->tccDllPath == NULL || ctx->tccLibPath == NULL ) {
        xlDestroyContext(ctx);
        return NULL;
    }
    return ctx;
}

void xlDestroyContext(XL_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->workDir);
    xrtFree(ctx->tempDir);
    xrtFree(ctx->tccDllPath);
    xrtFree(ctx->tccLibPath);
    xrtFree(ctx->lastError);
    xlStringListFree(&ctx->includePaths);
    xlStringListFree(&ctx->libraryPaths);
    xlStringListFree(&ctx->linkLibraries);
    xlStringListFree(&ctx->loadLibraries);
    xlFreeSymbols(ctx);
    xrtFree(ctx);
    xlRuntimeUnit();
}

int xlSetWorkDir(XL_Context* ctx, const char* path)
{
    char* copy;

    if ( ctx == NULL || path == NULL || path[0] == '\0' ) {
        return 0;
    }
    copy = xlDupText(path);
    if ( copy == NULL ) {
        xlSetError(ctx, "cannot allocate work dir");
        return 0;
    }
    xrtFree(ctx->workDir);
    ctx->workDir = copy;
    return 1;
}

int xlSetTempDir(XL_Context* ctx, const char* path)
{
    char* copy;

    if ( ctx == NULL || path == NULL || path[0] == '\0' ) {
        return 0;
    }
    copy = xlDupText(path);
    if ( copy == NULL ) {
        xlSetError(ctx, "cannot allocate temp dir");
        return 0;
    }
    xrtFree(ctx->tempDir);
    ctx->tempDir = copy;
    return 1;
}

int xlSetSaveTemps(XL_Context* ctx, int enabled)
{
    if ( ctx == NULL ) {
        return 0;
    }
    ctx->saveTemps = enabled != 0;
    return 1;
}

int xlAddIncludePath(XL_Context* ctx, const char* path)
{
    if ( ctx == NULL || !xlStringListAppend(&ctx->includePaths, path) ) {
        xlSetError(ctx, "cannot add include path");
        return 0;
    }
    return 1;
}

int xlAddLibraryPath(XL_Context* ctx, const char* path)
{
    if ( ctx == NULL || !xlStringListAppend(&ctx->libraryPaths, path) ) {
        xlSetError(ctx, "cannot add library path");
        return 0;
    }
    return 1;
}

int xlAddLinkLibrary(XL_Context* ctx, const char* nameOrPath)
{
    if ( ctx == NULL || !xlStringListAppend(&ctx->linkLibraries, nameOrPath) ) {
        xlSetError(ctx, "cannot add link library");
        return 0;
    }
    return 1;
}

int xlLoadDependencyLibrary(XL_Context* ctx, const char* path)
{
    if ( ctx == NULL || path == NULL || path[0] == '\0' ) {
        return 0;
    }
#ifdef _WIN32
    {
        u16str pathW = xrtUTF8to16((u8str)path, 0, NULL);
        HMODULE module = NULL;
        if ( pathW != NULL ) {
            module = LoadLibraryW((LPCWSTR)pathW);
            xrtFree(pathW);
        }
        if ( module == NULL ) {
            xlSetError2(ctx, "cannot load dependency library", path);
            return 0;
        }
    }
#else
    if ( dlopen(path, RTLD_NOW | RTLD_GLOBAL) == NULL ) {
        xlSetError2(ctx, "cannot load dependency library", path);
        return 0;
    }
#endif
    if ( !xlStringListAppend(&ctx->loadLibraries, path) ) {
        xlSetError(ctx, "cannot remember dependency library");
        return 0;
    }
    return 1;
}

int xlRegisterCFunction(XL_Context* ctx, const char* symbolName, void* fn)
{
    if ( ctx == NULL || !xlContextAddSymbol(ctx, symbolName, fn) ) {
        xlSetError(ctx, "cannot register C function");
        return 0;
    }
    return 1;
}

int xlRunScriptString(XL_Context* ctx, const char* source)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlClearLastError(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        return 0;
    }
    ok = xlLoadUnitFromSource(ctx, &unit, source, "__xl_string.xl");
    if ( ok ) {
        ok = xlCompileAndRunUnit(ctx, &unit);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    return ok;
}

int xlRunScriptFile(XL_Context* ctx, const char* path)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlClearLastError(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        return 0;
    }
    ok = xlLoadUnitFromFile(ctx, &unit, path);
    if ( ok ) {
        ok = xlCompileAndRunUnit(ctx, &unit);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    return ok;
}

int xlCompileScriptStringToExe(XL_Context* ctx, const char* source, const char* outputExe)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlClearLastError(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        return 0;
    }
    ok = xlLoadUnitFromSource(ctx, &unit, source, "__xl_string.xl");
    if ( ok ) {
        ok = xlCompileUnitToExe(ctx, &unit, outputExe);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    return ok;
}

int xlCompileScriptFileToExe(XL_Context* ctx, const char* path, const char* outputExe)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlClearLastError(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        return 0;
    }
    ok = xlLoadUnitFromFile(ctx, &unit, path);
    if ( ok ) {
        ok = xlCompileUnitToExe(ctx, &unit, outputExe);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    return ok;
}

const char* xlGetLastError(XL_Context* ctx)
{
    if ( ctx == NULL || ctx->lastError == NULL ) {
        return "";
    }
    return ctx->lastError;
}

void xlClearLastError(XL_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->lastError);
    ctx->lastError = NULL;
}
