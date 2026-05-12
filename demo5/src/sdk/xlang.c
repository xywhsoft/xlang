#include "xlang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../ast/xl_ast.h"
#include "../codegen/c/xl_cgen.h"
#include "../common/xl_path.h"
#include "../diag/xl_diag.h"
#include "../driver/xl_module_link.h"
#include "../driver/xl_xle_load.h"
#include "../host/xl_host.h"
#include "../lower/xl_lower.h"
#include "../runtime/xl_exec_ctx.h"
#include "../sem/xl_resolve.h"
#include "xrt.h"

typedef struct xlang_registered_symbol {
	char name[128];
	void* address;
} xlang_registered_symbol;

typedef struct xlang_registered_path {
	char value[XL_PATH_MAX_LEN];
} xlang_registered_path;

struct xlang_runtime {
	char release_root[XL_PATH_MAX_LEN];
	char last_error[XLANG_TEXT_MAX];
	xlang_registered_symbol symbols[XL_HOST_EXTERN_SYMBOL_MAX];
	size_t symbol_count;
	xlang_registered_path load_libraries[XL_HOST_LOAD_LIBRARY_MAX];
	size_t load_library_count;
	xlang_registered_path link_libraries[XL_HOST_LINK_LIBRARY_MAX];
	size_t link_library_count;
	xlang_registered_path library_paths[XL_HOST_LIBRARY_PATH_MAX];
	size_t library_path_count;
	xlang_registered_path include_paths[XL_HOST_INCLUDE_PATH_MAX];
	size_t include_path_count;
};

static void xlang_copy_text(char* out, size_t out_size, const char* text)
{
	size_t len;

	if ( (out == NULL) || (out_size == 0u) ) {
		return;
	}
	if ( text == NULL ) {
		text = "";
	}
	len = strlen(text);
	if ( len >= out_size ) {
		len = out_size - 1u;
	}
	if ( len != 0u ) {
		memcpy(out, text, len);
	}
	out[len] = '\0';
}

static void xlang_set_error(xlang_runtime* runtime, const char* text)
{
	if ( runtime != NULL ) {
		xlang_copy_text(runtime->last_error, sizeof(runtime->last_error), text);
	}
}

static int xlang_add_path(xlang_registered_path* items, size_t* count, size_t max_count, const char* value)
{
	size_t i;

	if ( (items == NULL) || (count == NULL) || (value == NULL) || (value[0] == '\0') ) {
		return XLANG_BAD_ARG;
	}
	for ( i = 0u; i < *count; ++i ) {
		if ( strcmp(items[i].value, value) == 0 ) {
			return XLANG_OK;
		}
	}
	if ( *count >= max_count ) {
		return XLANG_ERROR;
	}
	xlang_copy_text(items[*count].value, sizeof(items[*count].value), value);
	*count += 1u;
	return XLANG_OK;
}

static const XL_AstNode* xlang_ast_child(const XL_AstNode* node, size_t index)
{
	if ( (node == NULL) || (index >= node->iChildCount) ) {
		return NULL;
	}
	return node->ppChildren[index];
}

static const XL_AstNode* xlang_func_receiver_node(const XL_AstNode* node)
{
	const XL_AstNode* maybe_receiver;

	if ( (node == NULL) || (node->eKind != XL_AST_FUNC_DECL) ) {
		return NULL;
	}
	maybe_receiver = xlang_ast_child(node, 0u);
	if ( (maybe_receiver != NULL) && (maybe_receiver->eKind == XL_AST_PARAM) ) {
		return maybe_receiver;
	}
	return NULL;
}

static const char* xlang_func_extern_name(const XL_AstNode* node)
{
	const XL_AstNode* body;
	const XL_AstNode* receiver;
	const char* extern_name;

	if ( (node == NULL) || (node->eKind != XL_AST_FUNC_DECL) ) {
		return NULL;
	}
	receiver = xlang_func_receiver_node(node);
	extern_name = node->sText2;
	if ( receiver != NULL ) {
		if ( (extern_name == NULL) || (extern_name[0] == '\0') ) {
			extern_name = node->sText;
		}
		if ( (extern_name == NULL) || (extern_name[0] == '\0') ) {
			return NULL;
		}
	} else if ( (extern_name == NULL) || (extern_name[0] == '\0') ) {
		return NULL;
	}
	body = xlang_ast_child(node, 2u);
	if ( (body == NULL) || (body->eKind != XL_AST_BLOCK) || (body->iChildCount != 0u) ) {
		return NULL;
	}
	return extern_name;
}

static const char* xlang_var_extern_name(const XL_AstNode* node)
{
	const XL_AstNode* type_node;
	const XL_AstNode* value_node;

	if ( (node == NULL) || (node->eKind != XL_AST_VAR_DECL) ) {
		return NULL;
	}
	if ( (node->sText2 == NULL) || (node->sText2[0] == '\0') ) {
		return NULL;
	}
	type_node = xlang_ast_child(node, 0u);
	value_node = xlang_ast_child(node, 1u);
	if ( (value_node == NULL) && (type_node != NULL) && (type_node->eKind != XL_AST_TYPE_REF) ) {
		value_node = type_node;
		type_node = NULL;
	}
	if ( (type_node == NULL) || (value_node != NULL) ) {
		return NULL;
	}
	return node->sText2;
}

static int xlang_add_root_symbols_exec(XL_ExecCtx* ctx, const XL_AstNode* node)
{
	const char* extern_name;
	size_t i;

	if ( node == NULL ) {
		return XL_RC_OK;
	}
	extern_name = xlang_func_extern_name(node);
	if ( extern_name == NULL ) {
		extern_name = xlang_var_extern_name(node);
	}
	if ( (extern_name != NULL) && (extern_name[0] != '\0') ) {
		if ( xlExecCtxAddExternSymbol(ctx, extern_name) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	for ( i = 0u; i < node->iChildCount; ++i ) {
		if ( xlang_add_root_symbols_exec(ctx, node->ppChildren[i]) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

static int xlang_add_root_symbols_host(XL_HostContext* ctx, const XL_AstNode* node)
{
	const char* extern_name;
	size_t i;

	if ( node == NULL ) {
		return XL_RC_OK;
	}
	extern_name = xlang_func_extern_name(node);
	if ( extern_name == NULL ) {
		extern_name = xlang_var_extern_name(node);
	}
	if ( (extern_name != NULL) && (extern_name[0] != '\0') ) {
		if ( xlHostContextAddExternSymbol(ctx, extern_name) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	for ( i = 0u; i < node->iChildCount; ++i ) {
		if ( xlang_add_root_symbols_host(ctx, node->ppChildren[i]) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

static int xlang_add_mir_symbols_exec(XL_ExecCtx* ctx, const XL_MirNode* node)
{
	size_t i;

	if ( node == NULL ) {
		return XL_RC_OK;
	}
	if ( (node->sExternName != NULL) && (node->sExternName[0] != '\0') ) {
		if ( xlExecCtxAddExternSymbol(ctx, node->sExternName) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	for ( i = 0u; i < node->iChildCount; ++i ) {
		if ( xlang_add_mir_symbols_exec(ctx, node->ppChildren[i]) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

static int xlang_add_mir_symbols_host(XL_HostContext* ctx, const XL_MirNode* node)
{
	size_t i;

	if ( node == NULL ) {
		return XL_RC_OK;
	}
	if ( (node->sExternName != NULL) && (node->sExternName[0] != '\0') ) {
		if ( xlHostContextAddExternSymbol(ctx, node->sExternName) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	for ( i = 0u; i < node->iChildCount; ++i ) {
		if ( xlang_add_mir_symbols_host(ctx, node->ppChildren[i]) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

static int xlang_module_needs_shared_symbols(const XL_ModuleSummary* module)
{
	return (module != NULL) &&
		!module->bIsRoot &&
		module->bIsNative &&
		(module->sManifestPath != NULL) &&
		(module->sManifestPath[0] != '\0') &&
		(module->sConsumeMode != NULL) &&
		(strcmp(module->sConsumeMode, "shared-bridge") == 0);
}

static int xlang_add_module_shared_symbols_exec(XL_ExecCtx* ctx, const XL_ModuleSummary* module)
{
	XL_XleFunctionInfo* funcs;
	size_t func_count;
	size_t i;

	if ( !xlang_module_needs_shared_symbols(module) ) {
		return XL_RC_OK;
	}
	funcs = NULL;
	func_count = 0u;
	if ( xlXleReadFunctions(module->sManifestPath, module->sKey, &funcs, &func_count) != XL_RC_OK ) {
		return XL_RC_OK;
	}
	for ( i = 0u; i < func_count; ++i ) {
		if ( (funcs[i].sSymbol[0] != '\0') && (xlExecCtxAddExternSymbol(ctx, funcs[i].sSymbol) != XL_RC_OK) ) {
			xlXleFunctionInfoFree(funcs, func_count);
			return XL_RC_FAIL;
		}
	}
	xlXleFunctionInfoFree(funcs, func_count);
	return XL_RC_OK;
}

static int xlang_add_module_shared_symbols_host(XL_HostContext* ctx, const XL_ModuleSummary* module)
{
	XL_XleFunctionInfo* funcs;
	size_t func_count;
	size_t i;

	if ( !xlang_module_needs_shared_symbols(module) ) {
		return XL_RC_OK;
	}
	funcs = NULL;
	func_count = 0u;
	if ( xlXleReadFunctions(module->sManifestPath, module->sKey, &funcs, &func_count) != XL_RC_OK ) {
		return XL_RC_OK;
	}
	for ( i = 0u; i < func_count; ++i ) {
		if ( (funcs[i].sSymbol[0] != '\0') && (xlHostContextAddExternSymbol(ctx, funcs[i].sSymbol) != XL_RC_OK) ) {
			xlXleFunctionInfoFree(funcs, func_count);
			return XL_RC_FAIL;
		}
	}
	xlXleFunctionInfoFree(funcs, func_count);
	return XL_RC_OK;
}

static int xlang_collect_exec_context(XL_ExecCtx* ctx, const XL_ModuleLinkResult* modules, const XL_LowerResult* lower)
{
	size_t i;
	size_t j;

	for ( i = 0u; i < modules->iModuleCount; ++i ) {
		const XL_ModuleSummary* module = &modules->pModules[i];

		if ( !module->bIsRoot &&
			(module->sConsumeMode != NULL) &&
			(module->sArtifactFile != NULL) &&
			((strcmp(module->sConsumeMode, "shared-bridge") == 0) || (strcmp(module->sConsumeMode, "static-bridge") == 0)) &&
			(module->sArtifactFile[0] != '\0') ) {
			if ( xlExecCtxAddArtifact(ctx, module->sArtifactKind, module->sArtifactFile) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
		if ( xlang_add_module_shared_symbols_exec(ctx, module) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iLinkLibCount; ++j ) {
			if ( xlExecCtxAddLinkLibrary(ctx, module->arrLinkLibs[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iLibPathCount; ++j ) {
			if ( xlExecCtxAddLibraryPath(ctx, module->arrLibPaths[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iIncPathCount; ++j ) {
			if ( xlExecCtxAddIncludePath(ctx, module->arrIncPaths[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iLoadLibCount; ++j ) {
			if ( xlExecCtxAddLoadLibrary(ctx, module->arrLoadLibs[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
	}
	if ( xlang_add_root_symbols_exec(ctx, modules->pLinkedRoot) != XL_RC_OK ) return XL_RC_FAIL;
	if ( xlang_add_mir_symbols_exec(ctx, lower->pMirRoot) != XL_RC_OK ) return XL_RC_FAIL;
	return XL_RC_OK;
}

static int xlang_collect_host_context(XL_HostContext* ctx, const XL_ModuleLinkResult* modules, const XL_LowerResult* lower)
{
	size_t i;
	size_t j;

	for ( i = 0u; i < modules->iModuleCount; ++i ) {
		const XL_ModuleSummary* module = &modules->pModules[i];

		if ( !module->bIsRoot &&
			(module->sConsumeMode != NULL) &&
			(module->sArtifactFile != NULL) &&
			((strcmp(module->sConsumeMode, "shared-bridge") == 0) || (strcmp(module->sConsumeMode, "static-bridge") == 0)) &&
			(module->sArtifactFile[0] != '\0') ) {
			if ( xlHostContextAddArtifact(ctx, module->sArtifactKind, module->sArtifactFile) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
		if ( xlang_add_module_shared_symbols_host(ctx, module) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iLinkLibCount; ++j ) {
			if ( xlHostContextAddLinkLibrary(ctx, module->arrLinkLibs[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iLibPathCount; ++j ) {
			if ( xlHostContextAddLibraryPath(ctx, module->arrLibPaths[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iIncPathCount; ++j ) {
			if ( xlHostContextAddIncludePath(ctx, module->arrIncPaths[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
		for ( j = 0u; j < module->iLoadLibCount; ++j ) {
			if ( xlHostContextAddLoadLibrary(ctx, module->arrLoadLibs[j]) != XL_RC_OK ) return XL_RC_FAIL;
		}
	}
	if ( xlang_add_root_symbols_host(ctx, modules->pLinkedRoot) != XL_RC_OK ) return XL_RC_FAIL;
	if ( xlang_add_mir_symbols_host(ctx, lower->pMirRoot) != XL_RC_OK ) return XL_RC_FAIL;
	return XL_RC_OK;
}

static int xlang_apply_runtime_to_exec(xlang_runtime* runtime, XL_ExecCtx* ctx)
{
	size_t i;

	for ( i = 0u; i < runtime->symbol_count; ++i ) {
		if ( xlExecCtxAddExternSymbolAddress(ctx, runtime->symbols[i].name, runtime->symbols[i].address) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->load_library_count; ++i ) {
		if ( xlExecCtxAddLoadLibrary(ctx, runtime->load_libraries[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->link_library_count; ++i ) {
		if ( xlExecCtxAddLinkLibrary(ctx, runtime->link_libraries[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->library_path_count; ++i ) {
		if ( xlExecCtxAddLibraryPath(ctx, runtime->library_paths[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->include_path_count; ++i ) {
		if ( xlExecCtxAddIncludePath(ctx, runtime->include_paths[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	return XL_RC_OK;
}

static int xlang_apply_runtime_to_host(xlang_runtime* runtime, XL_HostContext* ctx)
{
	size_t i;

	for ( i = 0u; i < runtime->symbol_count; ++i ) {
		if ( xlHostContextAddExternSymbolAddress(ctx, runtime->symbols[i].name, runtime->symbols[i].address) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->load_library_count; ++i ) {
		if ( xlHostContextAddLoadLibrary(ctx, runtime->load_libraries[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->link_library_count; ++i ) {
		if ( xlHostContextAddLinkLibrary(ctx, runtime->link_libraries[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->library_path_count; ++i ) {
		if ( xlHostContextAddLibraryPath(ctx, runtime->library_paths[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	for ( i = 0u; i < runtime->include_path_count; ++i ) {
		if ( xlHostContextAddIncludePath(ctx, runtime->include_paths[i].value) != XL_RC_OK ) return XL_RC_FAIL;
	}
	return XL_RC_OK;
}

static void xlang_fill_run_result(xlang_run_result* result, int rc, const XL_ExecCtx* ctx)
{
	if ( result == NULL ) {
		return;
	}
	memset(result, 0, sizeof(*result));
	result->rc = rc;
	if ( ctx == NULL ) {
		return;
	}
	result->exit_code = ctx->iExitCode;
	result->had_runtime_error = ctx->bHadRuntimeError;
	result->had_exception = ctx->bHadException;
	result->exception_kind = ctx->iExceptionKind;
	xlang_copy_text(result->exception_text, sizeof(result->exception_text), ctx->sExceptionText);
	xlang_copy_text(result->exception_file, sizeof(result->exception_file), ctx->sExceptionFile);
	result->exception_line = ctx->iExceptionLine;
	result->exception_column = ctx->iExceptionColumn;
	result->exception_int = ctx->iExceptionInt;
	result->exception_float = ctx->fExceptionFloat;
}

static void xlang_fill_build_result(xlang_build_result* result, int rc, const XL_HostBuildResult* host_result)
{
	if ( result == NULL ) {
		return;
	}
	memset(result, 0, sizeof(*result));
	result->rc = rc;
	if ( host_result == NULL ) {
		return;
	}
	result->had_build_error = host_result->bHadBuildError;
	xlang_copy_text(result->output_file, sizeof(result->output_file), host_result->sOutputFile);
	xlang_copy_text(result->output_kind, sizeof(result->output_kind), host_result->sOutputKind);
}

xlang_runtime* xlang_runtime_create(const xlang_options* options)
{
	xlang_runtime* runtime;

	xrtInit();
	runtime = (xlang_runtime*)calloc(1u, sizeof(*runtime));
	if ( runtime == NULL ) {
		return NULL;
	}
	xlang_copy_text(runtime->release_root, sizeof(runtime->release_root), (options != NULL && options->release_root != NULL) ? options->release_root : "release");
	return runtime;
}

void xlang_runtime_destroy(xlang_runtime* runtime)
{
	free(runtime);
}

const char* xlang_runtime_last_error(const xlang_runtime* runtime)
{
	if ( runtime == NULL ) {
		return "xlang runtime is NULL";
	}
	return runtime->last_error;
}

int xlang_runtime_register_function(xlang_runtime* runtime, const char* name, void* address)
{
	size_t i;

	if ( (runtime == NULL) || (name == NULL) || (name[0] == '\0') || (address == NULL) ) {
		return XLANG_BAD_ARG;
	}
	for ( i = 0u; i < runtime->symbol_count; ++i ) {
		if ( strcmp(runtime->symbols[i].name, name) == 0 ) {
			runtime->symbols[i].address = address;
			return XLANG_OK;
		}
	}
	if ( runtime->symbol_count >= XL_COUNTOF(runtime->symbols) ) {
		xlang_set_error(runtime, "too many registered functions");
		return XLANG_ERROR;
	}
	xlang_copy_text(runtime->symbols[runtime->symbol_count].name, sizeof(runtime->symbols[runtime->symbol_count].name), name);
	runtime->symbols[runtime->symbol_count].address = address;
	runtime->symbol_count += 1u;
	return XLANG_OK;
}

int xlang_runtime_add_library(xlang_runtime* runtime, const char* path)
{
	if ( runtime == NULL ) {
		return XLANG_BAD_ARG;
	}
	return xlang_add_path(runtime->load_libraries, &runtime->load_library_count, XL_COUNTOF(runtime->load_libraries), path);
}

int xlang_runtime_add_link_library(xlang_runtime* runtime, const char* name)
{
	if ( runtime == NULL ) {
		return XLANG_BAD_ARG;
	}
	return xlang_add_path(runtime->link_libraries, &runtime->link_library_count, XL_COUNTOF(runtime->link_libraries), name);
}

int xlang_runtime_add_library_path(xlang_runtime* runtime, const char* path)
{
	if ( runtime == NULL ) {
		return XLANG_BAD_ARG;
	}
	return xlang_add_path(runtime->library_paths, &runtime->library_path_count, XL_COUNTOF(runtime->library_paths), path);
}

int xlang_runtime_add_include_path(xlang_runtime* runtime, const char* path)
{
	if ( runtime == NULL ) {
		return XLANG_BAD_ARG;
	}
	return xlang_add_path(runtime->include_paths, &runtime->include_path_count, XL_COUNTOF(runtime->include_paths), path);
}

int xlang_run_file(xlang_runtime* runtime, const char* file, xlang_run_result* result)
{
	XL_DiagContext diag;
	XL_ModuleLinkResult modules;
	XL_SemResult sem;
	XL_LowerResult lower;
	XL_ExecCtx exec;
	char* code;
	int rc;

	if ( (runtime == NULL) || (file == NULL) || (file[0] == '\0') ) {
		xlang_fill_run_result(result, XLANG_BAD_ARG, NULL);
		return XLANG_BAD_ARG;
	}

	xlang_set_error(runtime, "");
	xlDiagInit(&diag, stdout, stderr);
	xlDiagSetDefaultSourceFile(&diag, file);
	xlModuleLinkResultInit(&modules);
	xlSemResultInit(&sem);
	xlLowerResultInit(&lower);
	xlExecCtxInit(&exec, file, runtime->release_root);
	code = NULL;

	rc = xlModuleLinkFromFile(file, &diag, 0, &modules);
	if ( (rc == XL_RC_OK) && (modules.pLinkedRoot == NULL) ) {
		rc = XL_RC_FAIL;
	}
	if ( rc == XL_RC_OK ) rc = xlSemAnalyzeAst(modules.pLinkedRoot, &diag, 0, &sem);
	if ( rc == XL_RC_OK ) rc = xlLowerAst(modules.pLinkedRoot, &sem, &diag, 0, &lower);
	if ( rc == XL_RC_OK ) rc = xlCgenMir(&lower, &modules, &diag, &code);
	if ( rc == XL_RC_OK ) rc = xlang_collect_exec_context(&exec, &modules, &lower);
	if ( rc == XL_RC_OK ) rc = xlang_apply_runtime_to_exec(runtime, &exec);
	if ( rc == XL_RC_OK ) rc = xlExecRunCode(&exec, &diag, code, NULL);
	if ( rc != XL_RC_OK ) {
		xlang_set_error(runtime, "failed to run xlang file");
	}

	xlang_fill_run_result(result, rc, &exec);
	free(code);
	xlLowerResultFree(&lower);
	xlSemResultFree(&sem);
	xlModuleLinkResultFree(&modules);
	return (rc == XL_RC_OK) ? XLANG_OK : XLANG_ERROR;
}

int xlang_run_text(xlang_runtime* runtime, const char* virtual_path, const char* source, xlang_run_result* result)
{
	char temp_file[XL_PATH_MAX_LEN];
	const char* name;
	FILE* fp;
	int rc;
	static unsigned long counter = 0ul;

	if ( (runtime == NULL) || (source == NULL) ) {
		xlang_fill_run_result(result, XLANG_BAD_ARG, NULL);
		return XLANG_BAD_ARG;
	}
	name = ((virtual_path != NULL) && (virtual_path[0] != '\0')) ? virtual_path : "memory";
	snprintf(temp_file, sizeof(temp_file), "%s\\tmp\\xlang_sdk_%08lx_%08lx.xl", runtime->release_root, (unsigned long)time(NULL), ++counter);
	XL_UNUSED(name);
	xlPathEnsureParentDir(temp_file);
	fp = fopen(temp_file, "wb");
	if ( fp == NULL ) {
		xlang_set_error(runtime, "failed to create temporary xlang source file");
		xlang_fill_run_result(result, XLANG_ERROR, NULL);
		return XLANG_ERROR;
	}
	fwrite(source, 1u, strlen(source), fp);
	fclose(fp);
	rc = xlang_run_file(runtime, temp_file, result);
	remove(temp_file);
	return rc;
}

int xlang_compile_file(xlang_runtime* runtime, const char* file, const char* output_file, xlang_build_result* result)
{
	XL_DiagContext diag;
	XL_ModuleLinkResult modules;
	XL_SemResult sem;
	XL_LowerResult lower;
	XL_HostContext host;
	XL_HostBuildResult host_result;
	char* code;
	int rc;

	if ( (runtime == NULL) || (file == NULL) || (file[0] == '\0') || (output_file == NULL) || (output_file[0] == '\0') ) {
		xlang_fill_build_result(result, XLANG_BAD_ARG, NULL);
		return XLANG_BAD_ARG;
	}

	xlang_set_error(runtime, "");
	xlDiagInit(&diag, stdout, stderr);
	xlDiagSetDefaultSourceFile(&diag, file);
	xlModuleLinkResultInit(&modules);
	xlSemResultInit(&sem);
	xlLowerResultInit(&lower);
	xlHostContextInit(&host, file, runtime->release_root);
	memset(&host_result, 0, sizeof(host_result));
	code = NULL;

	rc = xlModuleLinkFromFile(file, &diag, 0, &modules);
	if ( (rc == XL_RC_OK) && (modules.pLinkedRoot == NULL) ) {
		rc = XL_RC_FAIL;
	}
	if ( rc == XL_RC_OK ) rc = xlSemAnalyzeAst(modules.pLinkedRoot, &diag, 0, &sem);
	if ( rc == XL_RC_OK ) rc = xlLowerAst(modules.pLinkedRoot, &sem, &diag, 0, &lower);
	if ( rc == XL_RC_OK ) rc = xlCgenMir(&lower, &modules, &diag, &code);
	if ( rc == XL_RC_OK ) rc = xlang_collect_host_context(&host, &modules, &lower);
	if ( rc == XL_RC_OK ) rc = xlang_apply_runtime_to_host(runtime, &host);
	if ( rc == XL_RC_OK ) rc = xlHostBuildCText(&host, &diag, file, code, "xl_generated_entry", output_file, NULL, &host_result);
	if ( rc != XL_RC_OK ) {
		xlang_set_error(runtime, "failed to compile xlang file");
	}

	xlang_fill_build_result(result, rc, &host_result);
	free(code);
	xlLowerResultFree(&lower);
	xlSemResultFree(&sem);
	xlModuleLinkResultFree(&modules);
	return (rc == XL_RC_OK) ? XLANG_OK : XLANG_ERROR;
}

int xlang_compile_text(xlang_runtime* runtime, const char* virtual_path, const char* source, const char* output_file, xlang_build_result* result)
{
	char temp_file[XL_PATH_MAX_LEN];
	const char* name;
	FILE* fp;
	int rc;
	static unsigned long counter = 0ul;

	if ( (runtime == NULL) || (source == NULL) || (output_file == NULL) || (output_file[0] == '\0') ) {
		xlang_fill_build_result(result, XLANG_BAD_ARG, NULL);
		return XLANG_BAD_ARG;
	}
	name = ((virtual_path != NULL) && (virtual_path[0] != '\0')) ? virtual_path : "memory";
	snprintf(temp_file, sizeof(temp_file), "%s\\tmp\\xlang_sdk_compile_%08lx_%08lx.xl", runtime->release_root, (unsigned long)time(NULL), ++counter);
	XL_UNUSED(name);
	xlPathEnsureParentDir(temp_file);
	fp = fopen(temp_file, "wb");
	if ( fp == NULL ) {
		xlang_set_error(runtime, "failed to create temporary xlang source file");
		xlang_fill_build_result(result, XLANG_ERROR, NULL);
		return XLANG_ERROR;
	}
	fwrite(source, 1u, strlen(source), fp);
	fclose(fp);
	rc = xlang_compile_file(runtime, temp_file, output_file, result);
	remove(temp_file);
	return rc;
}
