"""Build the standalone analysis core directly from shared xlang source assets.

No compiler/SDK files are copied or changed. This target owns compiler XRT and
must not be linked into the XGE-owned IDE. It does not replace the shipped LS.
"""
from __future__ import annotations

import argparse
from native_raw_body_overlay import stdlib as derive_raw_stdlib,verify_domains
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from xrt_process_tree_overlay import derive as derive_process_runtime
from native_context_overlay import derive as derive_context_runtime
from native_cancellation_overlay import ir_cancellation, semantic_cancellation
from native_ir_memo_overlay import derive as derive_ir_memo
from native_ir_scan_overlay import derive as derive_ir_scans
from native_type_source_overlay import derive as derive_type_sources
from native_initializer_overlay import derive as derive_initializer_index
from native_parser_ownership_overlay import derive as derive_parser_ownership

FRONTEND_SOURCES = (
    "src/lexer/xl6_token.c", "src/lexer/xl6_lexer_api.c", "src/lexer/xl6_lexer_gen.c",
    "src/parser/xl6_parser.c", "src/parser/xl6_parser_api.c", "src/ast/xl6_ast.c",
    "src/diag/xl6_diag.c", "src/common/xl6_output.c", "src/common/xl6_console.c",
    "src/sem/xl6_completion.c", "src/compile/xl6_source_report.c",
    "src/compile/xl6_imports.c", "src/compile/xl6_source_overlay.c", "src/compile/xl6_build_inputs.c",
    "src/package/xl6_package.c", "src/stdlib/xl6_stdlib.c", "src/stdlib/xl6_stdlib_data.c",
    "src/type/xl6_type_model.c", "src/sem/xl6_protocol.c", "src/sem/xl6_type_registry.c",
    "src/sem/xl6_sem_docs.c", "src/sem/xl6_sem_field_attrs.c", "src/sem/xl6_sem.c", "src/abi/xl6_callable.c",
    "src/sem/xl6_sem_index.c", "src/ir/xl6_ir.c", "src/ir/xl6_ir_class_identity.c",
)
SERVER_SOURCES = (
    "src/protocol/analysis_context_wire.c", "src/protocol/signature_help.c", "src/services/analysis_catalog.c",
    "src/protocol/lsp_wire.c", "src/services/language_check.c", "src/services/language_snapshot.c", "src/services/diagnostic_store.c",
    "src/services/semantic_definition.c", "src/services/project_references.c", "src/services/semantic_completion.c", "src/services/c_bridge_map.c",
    "src/services/toolchain.c", "src/platform/file_rename.c", "src/platform/file_io.c",
    "src/platform/text_file.c", "src/platform/process.c", "src/platform/path.c", "src/editor/coordinates.c",
    "src/server/xlang_ls.c",
)
CORE_TESTS = ("analysis_context_fault_test", "analysis_context_resolution_test", "analysis_input_view_test", "analysis_context_test", "analysis_check_test", "analysis_check_session_test", "analysis_incremental_check_test", "analysis_semantic_memo_test", "analysis_check_worker_test", "analysis_index_test", "analysis_index_limit_test", "analysis_ir_scan_test", "analysis_stdlib_scan_test", "analysis_overload_scan_test", "analysis_completion_candidates_test", "analysis_ast_clone_test", "analysis_semantic_test", "analysis_completion_session_test", "analysis_folding_test", "analysis_worker_test", "analysis_session_test", "analysis_incremental_syntax_test", "analysis_cursor_syntax_test", "analysis_text_test", "document_sync_test")
HOST_TESTS = ("signature_help_test", "analysis_catalog_test", "language_context_test", "semantic_lookup_test", "analysis_index_view_test", "language_input_guard_test", "diagnostic_store_test", "language_definition_proof_test", "snapshot_report_test")
CLIENT_TESTS = ("diagnostic_dependencies_test", "diagnostic_notice_test", "diagnostic_stream_test", "reference_results_test")
CLIENT_TEST_SOURCES = ("src/services/diagnostic_dependencies.c", "src/services/diagnostic_notice.c", "src/services/diagnostic_stream.c", "src/services/reference_results.c")
HOST_TESTS += CLIENT_TESTS
HOST_TESTS += ("diagnostic_reverse_fault_test",)
NATIVE_INDEX_BYTES = 32 * 1024 * 1024
CORE_TESTS = ("analysis_header_syntax_test", "analysis_semantic_read_values_test", "analysis_semantic_reads_test", "analysis_ir_entity_test", "analysis_entity_versions_test", "analysis_entity_binding_test", "analysis_entity_store_test", "analysis_module_graph_test", "analysis_input_topology_test", "analysis_module_store_test", "analysis_process_tree_test", *CORE_TESTS)
CORE_TESTS = ("diagnostic_paths_test", *CORE_TESTS)
CORE_TESTS = ("analysis_entry_cache_test", *CORE_TESTS)
CORE_TESTS = ("analysis_import_names_test", *CORE_TESTS)
CORE_TESTS = ("analysis_index_storage_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ir_regions_test", *CORE_TESTS)
CORE_TESTS = ("analysis_syntax_storage_test", *CORE_TESTS)
CORE_TESTS = ("analysis_cursor_memo_test", *CORE_TESTS)
CORE_TESTS = ("analysis_call_candidates_test", *CORE_TESTS)
CORE_TESTS = ("analysis_cursor_source_memo_test", *CORE_TESTS)
CORE_TESTS = ("analysis_completion_annotations_test", *CORE_TESTS)
CORE_TESTS = ("analysis_dependency_capture_test", "analysis_ast_tree_test", "analysis_ast_leaf_failure_test", "analysis_ast_leaf_test", "analysis_ast_storage_test", *CORE_TESTS)
CORE_TESTS = ("analysis_completion_body_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ir_plan_direct_test", "analysis_check_materialize_test", "analysis_ast_materialize_test", *CORE_TESTS)
CORE_TESTS = ("analysis_call_context_test", *CORE_TESTS)
CORE_TESTS = ("analysis_signature_test", *CORE_TESTS)
CORE_TESTS = ("analysis_input_usage_test", *CORE_TESTS)
CORE_TESTS = ("analysis_dependency_graph_test", *CORE_TESTS)
CORE_TESTS = ("analysis_worker_cache_test", *CORE_TESTS)
CORE_TESTS = ("analysis_worker_cache_fault_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ast_intern_test", *CORE_TESTS)
CORE_TESTS = ("analysis_frontend_cancel_test", *CORE_TESTS)
CORE_TESTS = ("analysis_dependency_session_test", "analysis_dependency_worker_test", *CORE_TESTS)
CORE_TESTS = ("analysis_overlay_owner_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ir_memo_test", "analysis_projection_test", *CORE_TESTS)
CORE_TESTS = ("analysis_completion_body_failure_test", *CORE_TESTS)
CORE_TESTS = ("analysis_semantic_proof_test", *CORE_TESTS)
CORE_TESTS = ("analysis_semantic_headers_test", *CORE_TESTS)
CORE_TESTS = ("semantic_coordinates_test", *CORE_TESTS)
CORE_TESTS = ("analysis_function_unit_test", "analysis_function_unit_fault_test", *CORE_TESTS)
CORE_TESTS = ("analysis_completion_unit_test", "analysis_completion_unit_fault_test", *CORE_TESTS)
CORE_TESTS = ("analysis_completion_pool_fault_test", *CORE_TESTS)
CORE_TESTS = ("analysis_topology_clone_fault_test", *CORE_TESTS)
CORE_TESTS = ("analysis_raw_signature_test", "analysis_raw_native_test", "analysis_raw_body_projection_test", *CORE_TESTS)
CORE_TESTS = ("analysis_typed_module_test", "analysis_typed_module_fault_test", "analysis_ir_persistent_test", "analysis_ir_persistent_fault_test", *CORE_TESTS)
CORE_TESTS = ("analysis_current_loader_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ir_callsite_test", *CORE_TESTS)
CORE_TESTS = ("analysis_current_provider_test", *CORE_TESTS)
CORE_TESTS = ("analysis_selection_test", "analysis_selection_projection_test", *CORE_TESTS)
CORE_TESTS = ("analysis_semantic_read_front_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ir_sparse_decode_test", *CORE_TESTS)
CORE_TESTS = ("analysis_typed_identity_test", *CORE_TESTS)
CORE_TESTS = ("analysis_semantic_final_input_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ir_input_test", *CORE_TESTS)
CORE_TESTS = ("analysis_semantic_alias_reads_test", *CORE_TESTS)
CORE_TESTS = ("analysis_multifunction_syntax_test", "analysis_multifunction_fault_test", *CORE_TESTS)
CORE_TESTS = ("analysis_type_constructor_test", *CORE_TESTS)
CORE_TESTS = ("analysis_ir_derived_test", *CORE_TESTS)
HOST_TESTS += ("navigation_cache_test", "semantic_reference_shard_test", "project_references_test", "project_reference_budget_test", "reference_path_index_test")
HOST_TESTS += ("path_identity_batch_test", "reference_capture_test")
HOST_TESTS += ("project_reference_parallel_test",)
HOST_TESTS += ("project_reference_owner_test", "project_reference_owner_fault_test")


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(64 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def prepare_native_index(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    from native_index_overlay import index
    return index(frontend, output)


def prepare_native_ir(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Index iteration candidates without changing shared cleanup/verify rules."""
    original = frontend / 'src/ir/xl6_ir.c'
    body = original.read_bytes()
    from native_current_ir_overlay import derive as current_ir,PHASE_IR_DIGESTS
    if hashlib.sha256(body).hexdigest() in PHASE_IR_DIGESTS:
        # Admit the corrected current IR only with its complete audited
        # AST/parser/index/type/XRT cohort. Other service guards are separate.
        return current_ir(frontend,output,phases=True)
    if hashlib.sha256(body).hexdigest() != '6022b348ecb5ec0dc79eac881a4e51cb6dc227daa210f730bcdf27a191aad2d0':
        raise ValueError('shared IR changed; audit cleanup/plan/call candidates, scope-proof purity and phase mutation boundaries')
    if sha(frontend / 'src/ir/xl6_ir.h') != 'ec58343edcf43d97aba95b94bc3fb3d3a163ec4fc593ae0b064a09c3eac4ff3a':
        raise ValueError('shared IR layout changed; audit raw node ownership and exact type-prefix fields')
    type_model = frontend / 'src/type/xl6_type_model.c'
    type_sha = 'fdf35faa5dc2366a0625317d70cc82dbe8031762fcb7c81439e99a3a8de2a051'
    if sha(type_model) != type_sha:
        raise ValueError('shared alias/type model changed; audit raw IR environment region independence')
    type_ids = frontend / 'src/type/xl6_type_id.h'
    type_ids_sha = 'ae843f553ba8e4feda829a829c05fa5f524abafa2fc75946293f3abea012aca0'
    if sha(type_ids) != type_ids_sha:
        raise ValueError('shared symbol/type identity domains changed; audit tagged lexical symbol slots and exact fact types')
    source = body.decode('utf-8').replace('\r\n', '\n')
    source,scan_seams = derive_ir_scans(source)
    compiled = output / 'frontend/xl6_ir.c'
    source = ir_cancellation(source)
    source = derive_initializer_index(source)
    source = derive_ir_memo(source)
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        change='bounded nominal-declaration candidates, cleanup/plan/call-origin indexes and per-verification scope proofs; shared rules retained',
        rawFunctionRegions='lexical-relative keys with zero sentinel;current AST region/target rebinding;exact fact-type trees',
        initializerLookup='phase-local lazy nondecreasing declaration proof;lower-bound first match;original unordered/absent-scope fallback;no allocation or cached ABI results',
        callOriginVerification='existing phase-local sorted origin/physical-index candidates; preserve prior-index limit, original predicates and first diagnostics; original linear fallback',
        auditedDependencies={str(type_model):type_sha,str(type_ids):type_ids_sha},
        lexicalSymbols='source/range-proven per-function declaration slots;separate entity/global NAME identities;tagged raw nodes/dependencies/writes bind current original IDs;fact types and type-pool prefix exact',
        cancellation='cooperative node/phase/verify points; partial child lists and owned incoming counts unwind through audited cleanup',
        auditedSeams=scan_seams, indexBytes=16 * 1024 * 1024, planIndexBytes=8 * 1024 * 1024,
        scopeProofBytes=1024 * 1024, callIndexBytes=8 * 1024 * 1024, typeLookupBytes=8 * 1024 * 1024,
        regionIndexBytes=8 * 1024 * 1024)


def prepare_native_sem(frontend: Path, output: Path, *, _current_unit_probe=False) -> tuple[Path, Path, dict]:
    """Keep function rules in their original TU, add opt-in immutable memos."""
    original = frontend / 'src/sem/xl6_sem.c'
    body = original.read_bytes()
    cohort_dependencies = {}
    if _current_unit_probe:
        from native_index_overlay import profile
        profile(frontend)
    # Reuse depends on the checker's side effects and cross-declaration reads,
    # not just stable patch locations. Require an explicit audit on upgrades.
    audited = 'd4f19af32f1732126913b99717a6b1b2d38d92f343b60828786b62d2a7d8183a'
    current = sha(original)=='407ca4e51cbc7889b535cbe591f7f823f59131cd179aa285073c034c6796e0b0'
    if current:
        # Normal admission binds the audited reads, declaration effects and
        # registry records to their whole current include/SDK cohort. Private
        # historical component probes retain their separately recorded assets.
        if not _current_unit_probe:
            from native_current_semantic_profile import profile
            cohort_dependencies = profile(frontend)
        audited = '407ca4e51cbc7889b535cbe591f7f823f59131cd179aa285073c034c6796e0b0'
    if hashlib.sha256(body).hexdigest() != audited:
        raise ValueError('shared semantic checker changed; audit memo context/identity/side-effect assumptions')
    registry = frontend / 'src/sem/xl6_type_registry.c'
    registry_sha = 'a0c37403f28d8a29ac4589e1dd78344a0d5fa2c8051ba2ead0209b9e0a228583'
    if current: registry_sha = 'f8be2409dc6188091d831a68df657b64b6911d07b197736796d74ce2379260ba'
    if sha(registry) != registry_sha:
        raise ValueError('shared type registry changed; audit generic identity reservation replay')
    registry_header = frontend / 'src/sem/xl6_type_registry.h'
    registry_header_sha = 'f5967cce13c1d6051cf0c5c716e422e2865c69c5fd3451b39cbad95e642cd8a3'
    if current: registry_header_sha = '27f0415459b060075b8e18a677e9c05d7460412ef5d908a86dc10d519f30ccd1'
    if sha(registry_header) != registry_header_sha:
        raise ValueError('shared type registry fields changed; audit complete observed type records')
    ast = frontend / 'src/ast/xl6_ast.c'
    ast_sha = 'adf07bc08b6e6949fe1bb6095e373d130df205535e9e2fa1662d2cc7b4c65035'
    if current: ast_sha = 'b8bbafaed7b469232b73f6256282607f4644eb605fb1aa7a636bf123143cf026'
    if sha(ast) != ast_sha:
        raise ValueError('shared AST source/range ownership changed; audit synthetic type origins')
    source = body.decode('utf-8').replace('\r\n', '\n')
    begin = ('    for ( i = 0; i < root->childCount; ++i ) {\n'
             '        XL6_AstNode* child = root->children[i];\n'
             '        if ( child->kind == XL6_AST_FUNC || child->kind == XL6_AST_ASYNC_FUNC ) {')
    changes = {
        'static XL6_SymbolId xl6SemReserveSymbolId(\n': 'static XL6_SymbolId xlsSemReserveSymbolIdOriginal(\n',
        '} XL6_SemContext;': '    int xlsSignature;\n} XL6_SemContext;\n#include "analysis/frontend_context.h"\n#include "analysis/frontend_sem.inc"\n#include "analysis/signature_internal.h"\nstatic int xlsSemSignatureCaptureCall(XL6_SemContext*,XL6_AstNode*);\nstatic int xlsSemSignatureOffer(XL6_SemContext*,const XL6_AstNode*,const XL6_AstNode*,const char*);',
        'static int xl6SemCheckFunc(XL6_SemContext* ctx, XL6_AstNode* node)\n{':
            'static int xlsSemCheckFuncOriginal(XL6_SemContext* ctx, XL6_AstNode* node)\n{',
        begin: '    xlsSemMemoBegin(&ctx);\n' + begin,
    }
    for old, new in changes.items():
        if source.count(old) != 1:
            raise ValueError('shared semantic memo seam changed; audit: ' + old)
        source = source.replace(old, new)
    # Observe actual global lookup results and every root-function lookup family.
    # Original predicates/order/mutations stay in their original bodies.
    old='static XL6_Symbol* xl6ScopeFindLocal(XL6_Scope* scope, const char* name)\n{'
    if source.count(old)!=1:raise ValueError('semantic global-name observation seam changed')
    source=source.replace(old,old.replace('xl6ScopeFindLocal','xlsSemScopeFindLocalOriginal'))
    read_hooks={'xl6SemFindFunctionValueFunc':'name','xl6SemHasFunctionNamed':'name',
        'xl6SemFindExplicitGenericFunc':'name',
        'xl6SemResolveFutureNamedCallbackType':'callbackExpr ? callbackExpr->text : NULL'}
    for name,arg in read_hooks.items():
        import re
        pattern=r'(static [^;{}]*\b'+name+r'\([^;{}]*\)\n\{)'
        found=list(re.finditer(pattern,source))
        if len(found)!=1:raise ValueError('semantic declaration read hook changed: '+name)
        at=found[0].end();source=source[:at]+'\n    xlsSemReadsName(ctx,'+arg+');'+source[at:]
    old='static int xl6SemCheckExportSymbolUnique(XL6_SemContext* ctx, XL6_AstNode* node)\n{'
    if source.count(old)!=1:raise ValueError('semantic export visibility observation seam changed')
    source=source.replace(old,old+'\n    xlsSemReadsUnknown(ctx);')
    type_queries = 71 if current else 62
    if source.count('xl6TypeRegistryFind(')!=type_queries:
        raise ValueError('semantic type lookup calls changed; audit declaration read coverage')
    source=source.replace('xl6TypeRegistryFind(', 'xlsSemReadTypeFind(')
    if current:
        # This consumer reads kind and an alias declaration target only. All
        # other callers retain complete live TypeInfo observations.
        alias_start=source.index('static const XL6_AstNode* xl6SemAliasTargetType(')
        alias_end=source.index('\nstatic ',alias_start+1)
        alias=source[alias_start:alias_end]
        lookup='xlsSemReadTypeFind(&ctx->types, typeNode->text)'
        if alias.count(lookup)!=1:
            raise ValueError('native alias target reads changed; audit projected dependencies')
        line='    typeInfo = '+lookup+';'
        if alias.count(line)!=1:
            raise ValueError('native alias target statement changed')
        projected=('#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS\n'
                   '    typeInfo = xlsSemReadAliasFind(&ctx->types, typeNode->text);\n'
                   '#else\n'+line+'\n#endif')
        source=source[:alias_start]+alias.replace(line,projected,1)+source[alias_end:]
    start = source.index('static int xl6SemCheckOverloadConflicts(XL6_SemContext* ctx)\n{')
    end = source.index('\nstatic int xl6SemIsExternCDecl(', start)
    conflict = source[start:end]
    conflict_changes = {
        '    for ( i = 0; i < ctx->root->childCount; ++i ) {':
            '    xls_sem_overload_scan scan=xlsSemOverloadBegin(ctx->root,NULL);\n'
            '    for ( i = 0; i < ctx->root->childCount; ++i ) {',
        '        for ( j = i + 1; j < ctx->root->childCount; ++j ) {':
            '        for ( j = xlsSemOverloadNext(&scan,i,ctx->root->childCount);\n'
            '              j < ctx->root->childCount; j = xlsSemOverloadNext(&scan,j,ctx->root->childCount) ) {',
        '    return ok;': '    xlsSemOverloadEnd(&scan);\n    return ok;',
    }
    for old, new in conflict_changes.items():
        if conflict.count(old) != 1:
            raise ValueError('shared overload candidate seam changed; audit: ' + old)
        conflict = conflict.replace(old, new)
    source = source[:start] + '#include "analysis/frontend_sem_overload.inc"\n' + conflict + source[end:]
    start = source.index('static void xl6SemCompletionNames(XL6_SemContext* ctx)\n{')
    end = source.index('\nstatic void xl6SemCompletionMembers(', start)
    completion = source[start:end]
    completion_changes = {
        ' XL6_Scope* scope;XL6_CompletionQuery* q=ctx->completionQuery;size_t i,j;':
            ' XL6_Scope* scope;XL6_CompletionQuery* q=ctx->completionQuery;size_t i,j;\n'
            ' int namespaceQuery=xlsFrontendNamespaceActive();\n'
            ' xmap heads={0};xls_sem_overload_scan scan=xlsSemOverloadBegin(ctx->root,&heads);\n'
            ' xls_type_alias_scope aliasScope;xlsSemCompletionTypesBegin(&aliasScope,ctx->root);',
        '   if (!xl6SemCompletionSymbolAllowed(ctx,sym)) continue;':
            '   if (!xl6SemCompletionSymbolAllowed(ctx,sym)) continue;\n'
            '   if (namespaceQuery&&!xlsFrontendNamespaceCount(sym->name)) continue;',
        '  if (xl6ScopeFind(ctx->scope,info->name)) continue;':
            '  if (xl6ScopeFind(ctx->scope,info->name)) continue;\n'
            '  if (namespaceQuery&&!xlsFrontendNamespaceCount(info->name)) continue;',
        '    for (j=0;j<ctx->root->childCount&&!q->error;++j) {':
            '    for (j=xlsSemOverloadFirst(&scan,&heads,sym->name,ctx->root->childCount);\n'
            '         j<ctx->root->childCount&&!q->error;j=xlsSemOverloadNext(&scan,j,ctx->root->childCount)) {',
        '  xl6CompletionAdd(q,info->name,"type",info->decl?info->decl->semanticSymbolId:0,info->decl,xl6SemCompletionType(ctx,type));xl6AstFree(type);\n }\n}':
            '  xl6CompletionAdd(q,info->name,"type",info->decl?info->decl->semanticSymbolId:0,info->decl,xl6SemCompletionType(ctx,type));xl6AstFree(type);\n }\n'
            ' xlsFrontendTypeAliasEnd(&aliasScope);\n'
            ' xrtMapUnit(&heads);xlsSemOverloadEnd(&scan);\n}',
    }
    for old, new in completion_changes.items():
        if completion.count(old) != 1:
            raise ValueError('shared completion candidate seam changed; audit: ' + old)
        completion = completion.replace(old, new)
    source = source[:start] + completion + source[end:]
    # Only candidate iteration changes. Function names/root membership stay
    # fixed during the audited root-body pass; signatures remain live reads.
    resolver = 'static XL6_AstNode* xl6SemResolveCallFuncEx('
    if source.count(resolver) != 1:
        raise ValueError('shared call resolver include seam changed; audit')
    source = source.replace(resolver, '#include "analysis/frontend_sem_calls.inc"\n' + resolver)
    for name in ['xl6SemResolveCallFuncEx', 'xl6SemResolveExplicitGenericCallFunc']:
        start = source.index('static XL6_AstNode* ' + name + '(')
        end = source.index('\nstatic ', start + 1)
        call_body = source[start:end]
        old = '    for ( i = 0; i < ctx->root->childCount; ++i ) {'
        if call_body.count(old) != 1:
            raise ValueError('shared call candidate loop changed; audit: ' + name)
        call_body = call_body.replace(old, '    for ( i = xlsSemCallFirst(ctx,name); i < ctx->root->childCount; i = xlsSemCallNext(ctx,i) ) {')
        source = source[:start] + call_body + source[end:]
    for old, new in {
        '    xlsSemMemoBegin(&ctx);': '    xls_sem_call_scope callScope;\n    xlsSemCallsBegin(&callScope,&ctx);\n    xlsSemMemoBegin(&ctx);',
        '    xl6SemAnnotateTypeIdentities(&ctx, root);': '    xlsSemCallsEnd(&callScope);\n    xl6SemAnnotateTypeIdentities(&ctx, root);',
    }.items():
        if source.count(old) != 1:
            raise ValueError('shared call candidate lifetime seam changed; audit: ' + old)
        source = source.replace(old, new)
    # Signature mode is explicitly bound to this exact query. Ordinary checks
    # and completion queries keep the original expression/member paths.
    for old,new in {
        '    ctx.completionQuery = query;': '    ctx.completionQuery = query;\n    ctx.xlsSignature = xlsSignatureScopeCurrent(query)!=NULL;',
        'static int xl6SemCheckExprRaw(XL6_SemContext* ctx, XL6_AstNode* node)\n{': 'static int xl6SemCheckExprRaw(XL6_SemContext* ctx, XL6_AstNode* node)\n{\n    if(ctx->xlsSignature && node && node->kind==XL6_AST_CALL && xlsSemSignatureCaptureCall(ctx,node))return 0;',
        'static int xl6SemCaptureCompletion(XL6_SemContext* ctx,XL6_AstNode* node)\n{': '#include "analysis/frontend_sem_signature.inc"\n\nstatic int xl6SemCaptureCompletion(XL6_SemContext* ctx,XL6_AstNode* node)\n{',
    }.items():
        if source.count(old)!=1:raise ValueError('shared signature seam changed: '+old)
        source=source.replace(old,new)
    start=source.index('static void xl6SemCompletionMembers(')
    end=source.index('\n#include "analysis/frontend_sem_signature.inc"',start)
    members=source[start:end]
    for old,new in {
        ' XL6_CompletionQuery* q=ctx->completionQuery;': ' XL6_CompletionQuery* q=ctx->completionQuery;\n xls_signature_scope* signature=ctx->xlsSignature ? xlsSignatureScopeCurrent(q) : NULL;',
        '   name=field?member->text:xl6SemMemberName(member);if (!name) continue;': '   name=field?member->text:xl6SemMemberName(member);if (!name) continue;\n   if(signature && signature->targetName && strcmp(name,signature->targetName))continue;',
        '   xl6CompletionAdd(q,name,field?"field":"method",member->semanticSymbolId,selection,xl6SemCompletionType(ctx,type));xl6AstFree(type);': '   if(signature)xlsSemSignatureOffer(ctx,member,type,name);\n   else xl6CompletionAdd(q,name,field?"field":"method",member->semanticSymbolId,selection,xl6SemCompletionType(ctx,type));\n   xl6AstFree(type);',
    }.items():
        if members.count(old)!=1:raise ValueError('shared signature member seam changed: '+old)
        members=members.replace(old,new)
    source=source[:start]+members+source[end:]
    # Final annotation rules stay byte-for-byte in the audited TU. Extract
    # their original invocation block once so eager and deferred paths call
    # exactly the same helpers with the same plan-region initialization.
    start = source.index('    xl6SemAnnotateTypeIdentities(&ctx, root);')
    end = source.index('    xl6SemPopScope(&ctx, &globalScope);', start)
    tail = source[start:end]
    annotation = 'xl6SemAnnotateExecutionPlans(&ctx, root, &planState);' if current else 'xl6SemAnnotateExecutionPlans(root, &planState);'
    if tail.count(annotation) != 1:
        raise ValueError('shared final semantic annotation block changed; audit')
    source = source[:start] + '    xlsSemFinishAnnotations(&ctx, root);\n' + source[end:]
    seam = 'int xl6SemCheckModuleWithQuery('
    if source.count(seam) != 1:
        raise ValueError('shared deferred annotation include seam changed; audit')
    helper = ('static void xlsSemFinalizeAnnotations(XL6_SemContext* ctx, XL6_AstNode* root)\n{\n' +
              tail.replace('&ctx', 'ctx') + '}\n#include "analysis/frontend_sem_tail.inc"\n\n')
    source = source.replace(seam, helper + seam)
    compiled = output / 'frontend/xl6_sem.c'
    effect_overlay = None
    if current:
        from native_semantic_effects_overlay import derive
        source,effect_overlay=derive(frontend,output,source)
    from native_type_constructor_overlay import derive as derive_type_constructor
    source,type_constructor=derive_type_constructor(frontend,output,source,current)
    source = semantic_cancellation(source)
    source = derive_type_sources(source)
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        change='bounded ordered conflict/completion and root-body call candidates,namespace/alias filters,original full-context and observed-declaration memos,independent immutable cursor/signature function-body views and lazy IR annotations;complete source/coordinate/symbol admission precedes views,original bodies/annotations materialize atomically on demand;ordinary/fresh eager unchanged',
        cancellation='cooperative module/function/statement/expression points; ordinary scope/type/call-index cleanup retained',
        symbolAllocations='observe original allocator unchanged;source/range-proven function interval,immutable origin,current scalar/type-fact staging;predeclared IDs exact,foreign dynamic/type/nonfunction boundaries decline',
        auditedSeams=len(changes)+len(conflict_changes)+len(completion_changes)+16, memoBytes=64 * 1024 * 1024,
        deferredRegistryBytes=8 * 1024 * 1024,
        deferredBodyViewBytes=64 * 1024 * 1024,
        overloadCandidateBytes=8 * 1024 * 1024,
        syntheticTypeOrigins='eight explicit at-node constructors retain original AST source/ranges;original type rules unchanged',
        observedTypeQueries=type_queries, projectedAliasQueries=1 if current else 0,
        typeConstructor=type_constructor,
        currentUnitProbe=bool(current and _current_unit_probe), declarationEffects=effect_overlay,
        semanticCohort='current-2084f8ac' if cohort_dependencies else None,
        auditedDependencies={**cohort_dependencies, str(registry): registry_sha, str(registry_header):registry_header_sha, str(ast): ast_sha})


def prepare_native_completion(frontend: Path, output: Path, namespaced: Path, *, _current_unit_probe=False) -> tuple[Path, Path, dict]:
    """Observe original emitter inputs without changing the filtered response."""
    original = frontend / 'src/sem/xl6_completion.c'
    digest = '384d4cf57a648900ee1b9d93df1895ba22526639f3393702891027c94053c94c'
    namespace_sha = 'de10164dcde8a4f4a0c3044012005850b64ea1d3c0d6d2570b3eecd9541cc658'
    current_emitter=sha(original)=='d56f80b8b3ca93e9ec4009a56ee9235a86b66674f2c2ea174a0f5db36fdef2bf'
    cohort_dependencies={}
    if current_emitter and not _current_unit_probe:
        from native_current_context_profile import profile
        cohort_dependencies=profile(frontend)
    if _current_unit_probe or current_emitter:
        from native_context_overlay import completion_source
        # Coherent parser/AST/XRT/emitter and current path matcher are checked
        # by the same standalone composer; its output must be byte-exact.
        _,verified,_=completion_source(frontend)
        if sha(original)=='d56f80b8b3ca93e9ec4009a56ee9235a86b66674f2c2ea174a0f5db36fdef2bf':
            digest='d56f80b8b3ca93e9ec4009a56ee9235a86b66674f2c2ea174a0f5db36fdef2bf'
            namespace_sha='5c7a7cd4c89938549d2a3da6a19621a7ba89ee875855def5b955c384da594bac'
        if hashlib.sha256(verified.encode('utf8')).hexdigest()!=namespace_sha:
            raise ValueError('completion namespace composition changed; audit')
    if sha(original) != digest:
        raise ValueError('shared completion emitter changed; audit prefix-only filtering, ordering, limits and canonical ownership')
    if sha(namespaced) != namespace_sha:
        raise ValueError('namespace completion adapter changed; audit complete projected labels before prefix observation')
    source = namespaced.read_text(encoding='utf-8').replace('\r\n', '\n')
    seam = ('static int xlsCompletionAddOriginal(XL6_CompletionQuery* q,const char* name,const char* kind,'
            'XL6_SymbolId symbolId,const XL6_AstNode* selection,char* canonical)')
    if source.count(seam) != 1:
        raise ValueError('shared completion emitter observation seam changed')
    start = source.index(seam)
    append = source.index(' length=strlen(name);bytes=', start)
    end = source.index('\n}\n', append) + 2
    declarations = ' XL6_CompletionItem *item,*items;size_t length,bytes,capacity;\n'
    guard = ' if (!name||!*name||!q||q->error) {xrtFree(canonical);return 0;}\n'
    private_filter = 'strchr(name,\'.\')||strncmp(name,"__xl6_",6)==0'
    prefix_filter = 'q->prefix&&strncmp(name,q->prefix,strlen(q->prefix))'
    original_prefix = (seam + '\n{\n' + declarations + guard +
        ' /* Resolver-private and qualified names require the import alias contract. */\n' +
        ' if (' + private_filter + '||(' + prefix_filter + ')) {xrtFree(canonical);return 1;}\n')
    if source[start:append] != original_prefix:
        raise ValueError('shared completion filtering/ownership body changed; audit before composing pool')
    # The appender's byte/item limits, growth, names and canonical ownership
    # remain the exact original body. Observe only names that pass its private
    # filter. Optional records copy text into an XRT buffer; the native prefix
    # filter's canonical buffers still become reusable immediately.
    original_append = source[append:end]
    appender = (seam.replace('xlsCompletionAddOriginal', 'xlsCompletionAppendNative') +
        '\n{\n' + declarations + original_append + '\n')
    # The optional collector has independent XRT buffer ownership. Keep every
    # original limit, growth step and item field. Strings use offsets until
    # final publication,with extra optional-budget checks. The filtered native
    # appender remains exact.
    if original_append.count('xrtStrDupN(name,length)') != 1 or original_append.count('xrtFree(canonical);') != 3:
        raise ValueError('completion appender ownership operations changed')
    pool_body = original_append.replace('XL6_CompletionItem', 'xls_completion_pool_record')
    pool_body = pool_body.replace('xrtStrDupN(name,length)', 'xlsCompletionPoolAppend(owner,name,length)')
    pool_body = pool_body.replace('xrtFree(canonical);', '(void)canonical;')
    pool_body = pool_body.replace('capacity=q->capacity?q->capacity*2:64;items=',
        'capacity=q->capacity?q->capacity*2:64;'
        'if(!xlsCompletionPoolRecordFits(owner,capacity)){q->error="completion pool budget exceeded";return 0;}items=')
    pool_body = pool_body.replace('item->canonical=canonical;',
        'item->canonical=canonical?xlsCompletionPoolAppend(owner,canonical,strlen(canonical)):0;'
        'if(canonical&&!item->canonical){q->error="cannot copy completion type";return 0;}')
    pool_appender = (seam.replace('xlsCompletionAddOriginal', 'xlsCompletionAppendPool')
        .replace('XL6_CompletionQuery* q,', 'xls_completion_pool_scope* owner,') +
        '\n{\n xls_completion_pool_build* q=&owner->build;\n' +
        declarations.replace('XL6_CompletionItem', 'xls_completion_pool_record') + pool_body + '\n')
    observe = 'xlsCompletionPoolObserve(q,name,kind,symbolId,selection,canonical,'
    filtered = (seam + '\n{\n' + guard +
        ' if (' + private_filter + ') {xrtFree(canonical);return 1;}\n' +
        ' if (' + prefix_filter + ') {' + observe + '1);return 1;}\n' +
        ' ' + observe + '0);\n' +
        ' return xlsCompletionAppendNative(q,name,kind,symbolId,selection,canonical);\n}\n')
    source = source[:start] + '#include "analysis/frontend_completion_pool.inc"\n' + appender + pool_appender + filtered + source[end:]
    compiled = output / 'native_completion/xl6_completion.c'
    compiled.parent.mkdir(parents=True, exist_ok=True)
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=digest,
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        composedFrom=str(namespaced), composedFromSha256=namespace_sha,
        auditedDependencies=cohort_dependencies,
        originalAppenderSha256=hashlib.sha256(original_append.encode('utf-8')).hexdigest(),
        poolAppenderSha256=hashlib.sha256(pool_body.encode('utf-8')).hexdigest(),
        change='original namespace/private filters and exact native appender;optional builder preserves original limits/growth/fields with XRT buffer offsets/final pointer publication and immediate original canonical recycling;existing context scope slot')


def prepare_native_stdlib(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Reject only absent spellings; positive decisions keep the shared rules."""
    original = frontend / 'src/stdlib/xl6_stdlib.c'
    body = original.read_bytes()
    digest=hashlib.sha256(body).hexdigest()
    current=digest=='e45f88b2e1dba9bad5cf4caab928eccf6d04cf0dcc9c7b694141609cc7e0ef10'
    audited={}
    if current:
        from native_current_stdlib_profile import profile
        audited=profile(frontend)
    elif digest != 'bf863afd1bdef637388e6e4198813484f90e4eb8f1399580d75304e70a110919':
        raise ValueError('shared stdlib changed; audit scope producers/name lifetimes/free paths and exact-text discovery predicates')
    source = verify_domains(body.decode('utf-8').replace('\r\n', '\n'),frontend=frontend,current=current)
    changes = {
        '#include <stdio.h>': '#include <stdio.h>\n#include "analysis/frontend_stdlib.inc"',
        '    size_t capacity;\n} XL6_StdlibNameScope;':
            '    size_t capacity;\n    struct xls_stdlib_scope_index* xlsIndex;\n    int xlsIndexDisabled;\n} XL6_StdlibNameScope;\n#include "analysis/frontend_stdlib_scope.inc"',
        'static void xl6StdlibScopeFree(XL6_StdlibNameScope* scope)\n{':
            'static void xlsStdlibScopeFreeOriginal(XL6_StdlibNameScope* scope)\n{',
        'static int xl6StdlibScopeContains(const XL6_StdlibNameScope* scope, const char* name)\n{':
            'static int xlsStdlibScopeContainsOriginal(const XL6_StdlibNameScope* scope, const char* name)\n{',
        'static int xl6StdlibScopeAdd(XL6_StdlibNameScope* scope, const char* name)\n{':
            'static int xlsStdlibScopeAddOriginal(XL6_StdlibNameScope* scope, const char* name)\n{',
        'static int xl6StdlibNodeUsesTypeName(const XL6_AstNode* node, const char* typeName)\n{':
            'static int xlsStdlibTypeOriginal(const XL6_AstNode* node, const char* typeName)\n{',
        'static int xl6StdlibNodeUsesMemberName(const XL6_AstNode* node, const char* memberName)\n{':
            'static int xlsStdlibMemberOriginal(const XL6_AstNode* node, const char* memberName)\n{',
        'int xl6StdlibExpandModule(XL6_AstNode* root, XL6_DiagList* diags)\n{':
            'static int xlsStdlibExpandOriginal(XL6_AstNode* root, XL6_DiagList* diags)\n{',
        '    xl6StdlibApplyDeps(used);\n    needsCoreClass =':
            '    xl6StdlibApplyDeps(used);\n    xlsStdlibScanBegin(root);\n    needsCoreClass =',
        '    ok = builder.ok && xl6StdlibMergeGeneratedDecls(root, builder.data, diags);':
            '    xlsStdlibScanEnd();\n    ok = builder.ok && xl6StdlibMergeGeneratedDecls(root, builder.data, diags);',
    }
    if current:
        changes.pop('    xl6StdlibApplyDeps(used);\n    needsCoreClass =')
        changes['    xl6StdlibApplyDeps(used);\n    /* Resolve the whole declaration island']='    xl6StdlibApplyDeps(used);\n    xlsStdlibScanBegin(root);\n    /* Resolve the whole declaration island'
    for old, new in changes.items():
        if source.count(old) != 1:
            raise ValueError('shared stdlib scan seam changed; audit: ' + old)
        source = source.replace(old, new)
    source=derive_raw_stdlib(source)
    compiled = output / 'frontend/xl6_stdlib.c'
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled), auditedSeams=len(changes),
        auditedDependencies=audited,stdlibCohort='current-2084f8ac' if current else 'legacy',
        change='bounded private scope-name candidates and exact spelling absence filter; original matching/parent rules retained',
        indexBytes=8 * 1024 * 1024)


def dependencies(body: str) -> list[Path]:
    """Read a GCC -MM rule with a fixed, drive-independent -MT target."""
    body = body.replace("\r\n", "\n").replace("\\\n", "").split(":", 1)[1]
    words, current, at = [], [], 0
    while at < len(body):
        char = body[at]
        if char == "\\" and at + 1 < len(body) and body[at + 1] in " \t\\#:":
            at += 1
            current.append(body[at])
        elif char.isspace():
            if current:
                words.append("".join(current))
                current = []
        else:
            current.append(char)
        at += 1
    if current:
        words.append("".join(current))
    return [Path(word).resolve() for word in words]


def prepare_native_types(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    from native_index_overlay import types
    return types(frontend, output)


def prepare_native_parser(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    from native_signature_overlay import parser
    return parser(frontend, output)


def prepare_native_lexer(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    from native_signature_overlay import lexer
    return lexer(frontend, output)


def prepare_native_ast(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    from native_ast_overlay import derive
    return derive(frontend, output)


def prepare_current_loader_oracles(frontend: Path, output: Path):
    """Namespace independent unmodified loader/report/package reference units."""
    if sha(frontend/'src/compile/xl6_source_overlay.c')!='7e2e3c2335dd8e785d4f6205ee677272777c20472f9c36458c5b618cccde52c9':return [],None
    import re
    from native_current_context_profile import profile
    profile(frontend)
    headers=['src/compile/xl6_source_report.h','src/compile/xl6_source_overlay.h','src/compile/xl6_imports.h','src/package/xl6_package.h','src/package/xl6_compiled_package.h']
    names=sorted(set(re.findall(r'\b(xl6(?:SourceReport|SourceOverlay|ImportDeps|Imports|Package|CompiledPackage)\w*)\s*\(', '\n'.join((frontend/p).read_text(encoding='utf8') for p in headers))))
    folder=output/'loader_oracles';folder.mkdir()
    aliases=folder/'loader_oracle_names.h';aliases.write_text(''.join('#define '+n+' xlsReference'+n.removeprefix('xl6')+'\n' for n in names),encoding='utf8',newline='\n')
    sources=[]
    for rel in ['src/compile/xl6_source_report.c','src/compile/xl6_source_overlay.c','src/compile/xl6_imports.c','src/package/xl6_package.c']:
        source=folder/('reference_'+Path(rel).name);source.write_bytes((frontend/rel).read_bytes());sources.append(source)
    return sources,aliases


def prepare_reference_owner_fixture(ide: Path, output: Path) -> tuple[Path, Path, list[dict]]:
    """Private phase fences and one constructor substitution; normal host unchanged."""
    source = ide / 'src/services/project_references.c'
    snapshot = ide / 'src/services/language_snapshot.c'
    body = source.read_text('utf8')
    definition = 'static int locations(xarray* rows,reference_output* out)'
    marker = 'static xtaskoutcome execute('
    if body.count(definition) != 1 or body.count(marker) != 1:
        raise ValueError('reference owner publication fixture seam changed')
    folder = output / 'owner_fixtures'
    folder.mkdir()
    coordinator = folder / 'project_references.c'
    body = body.replace(definition, 'static int ownerLocationsOriginal(xarray* rows,reference_output* out)')
    body = body.replace(marker, 'static int locations(xarray* rows,reference_output* out){int okay=ownerLocationsOriginal(rows,out);if(okay)ownerFence(6);return okay;}\n' + marker)
    coordinator.write_text(body, encoding='utf8', newline='\n')
    alternate = folder / 'owner_snapshot_fault.c'
    alternate.write_text('#define xideSemanticReferenceShardCreate ownerFaultShardCreate\n#include "services/language_snapshot.c"\n', encoding='utf8', newline='\n')
    return coordinator, alternate, [
        dict(source=str(source), sourceSha256=sha(source), compiledSource=str(coordinator),
             compiledSha256=sha(coordinator), fixtureOnly=True,
             change='original Location serializer body; fixture-only post-serialization publication fence'),
        dict(source=str(snapshot), sourceSha256=sha(snapshot), compiledSource=str(alternate),
             compiledSha256=sha(alternate), fixtureOnly=True,
             change='include original snapshot body with one fixture-only shard constructor substitution')]


def main() -> None:
    ide = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend-root", type=Path, default=ide.parent)
    parser.add_argument("--output", type=Path, required=True, help="fresh build directory")
    parser.add_argument("--cc", default=shutil.which("gcc") or "E:/software/w64devkit/bin/gcc.exe")
    parser.add_argument("--server", action="store_true", help="also build the external language-server host")
    parser.add_argument("--skip-tests", action="store_true", help="build only, without compiling or running core fixtures")
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--cflag", action="append", default=[],
                        help="additional compiler option; repeat --cflag=-option as needed")
    args = parser.parse_args()
    tests = CORE_TESTS + (HOST_TESTS if args.server else ())
    frontend, output = args.frontend_root.resolve(), args.output.resolve()
    cc = Path(args.cc).resolve()
    if not cc.is_file():
        parser.error(f"C compiler does not exist: {cc}")
    sources = [(frontend / source, False) for source in FRONTEND_SOURCES]
    sources += [(ide / "src/analysis/runtime.c", False)]
    sources += [(ide / "src/protocol/input_state.c", True)]
    sources += [(ide / "src/protocol/diagnostic_paths.c", True)]
    sources += [(ide / path, True) for path in (
    "src/analysis/frontend_cancel.c", "src/analysis/frontend_ir_regions.c", "src/analysis/frontend_symbols.c", "src/analysis/text.c", "src/analysis/context.c", "src/analysis/frontend_context.c", "src/analysis/frontend_raw_body.c", "src/analysis/frontend_entry_cache.c", "src/analysis/module_store.c", "src/analysis/entity_store.c", "src/analysis/module_graph.c", "src/analysis/input_view.c", "src/analysis/input_topology.c", "src/analysis/dependency_graph.c", "src/analysis/ast_storage.c", "src/analysis/ast_materialize.c", "src/analysis/ast_clone.c", "src/analysis/ast_intern.c", "src/analysis/function_unit.c", "src/analysis/typed_module.c", "src/analysis/syntax.c", "src/analysis/session.c", "src/analysis/worker.c", "src/analysis/projection.c", "src/analysis/folding.c", "src/analysis/selection.c", "src/analysis/semantic.c", "src/analysis/signature.c", "src/analysis/signature_session.c", "src/analysis/check.c", "src/analysis/input_batch.c", "src/analysis/frontend_overlay.c", "src/analysis/check_session.c", "src/analysis/dependency_session.c", "src/analysis/completion_session.c",
    )]
    if not args.skip_tests:
        sources += [(ide / "src/protocol/document_sync.c", True)]
        # These private fixtures include the exact generated native TUs; use
        # their native warning profile. Their objects never enter core/server.
        sources += [(ide / "tests" / (name + ".c"), name not in
                     ("analysis_semantic_read_front_test", "analysis_ir_sparse_decode_test",
                      "analysis_semantic_final_input_test", "analysis_ir_input_test",
                      "analysis_semantic_alias_reads_test", "analysis_type_constructor_test", "analysis_ir_derived_test")) for name in tests]
    if args.server:
        if os.name != "nt":
            parser.error("the current host still uses Windows transport/process adapters; the standalone core is separate")
        sources += [(ide / path, True) for path in SERVER_SOURCES]
        sources += [(ide / path, True) for path in CLIENT_TEST_SOURCES]
    if any(not path.is_file() for path, _ in sources):
        parser.error("frontend root must contain the shared xlang lexer, parser, AST and XRT assets")
    output.mkdir(parents=True, exist_ok=False)
    loader_references,loader_aliases=(prepare_current_loader_oracles(frontend,output) if not args.skip_tests else ([],None))
    sources += [(p,False) for p in loader_references]
    # Keep the pinned compiler assets immutable. This checked build overlay
    # makes the existing private serializer budget configurable; no grammar,
    # fact collection or index algorithm is duplicated in the IDE.
    try:
        owner_coordinator, owner_snapshot, owner_overlays = (prepare_reference_owner_fixture(ide, output)
            if args.server and not args.skip_tests else (None, None, []))
        original_index, generated_index, index_overlay = prepare_native_index(frontend, output)
        original_ir, generated_ir, ir_overlay = prepare_native_ir(frontend, output)
        original_sem, generated_sem, sem_overlay = prepare_native_sem(frontend, output)
        original_stdlib, generated_stdlib, stdlib_overlay = prepare_native_stdlib(frontend, output)
        original_types, generated_types, types_overlay = prepare_native_types(frontend, output)
        original_ast, generated_ast, ast_overlay = prepare_native_ast(frontend, output)
        original_parser, generated_parser, parser_overlay = prepare_native_parser(frontend, output)
        original_lexer, generated_lexer, lexer_overlay = prepare_native_lexer(frontend, output)
        original_stack, generated_stack, stack_overlay = derive_parser_ownership(frontend, output)
        context_overlays = derive_context_runtime(frontend, output)
        namespaced_completion = next(Path(row['compiledSource']) for row in context_overlays
                                     if Path(row['source']).name == 'xl6_completion.c')
        original_completion, generated_completion, completion_overlay = prepare_native_completion(frontend, output, namespaced_completion)
        runtime_overlay = derive_process_runtime(frontend / 'lib/xrt/xrt.h', output / 'native_xrt/xrt.h')
    except ValueError as error:
        parser.error(str(error))
    if not args.skip_tests and ir_overlay.get('irDependencyCohort')=='current-ed78373d':
        # Original backend builds populated targets for the provider verifier
        # fixture only; the analysis service still uses its existing raw IR.
        sources.append((frontend/'src/ir/xl6_ir_lower.c',False))
    raw_owner_fixture=None
    if not args.skip_tests:
        raw_owner_fixture=output/'frontend/syntax_raw_fixture.c'
        raw_owner_fixture.write_text('#define XLS_RAW_BODY_VERIFY_OWNERS 1\n#include "'+(ide/'src/analysis/syntax.c').as_posix()+'"\n',encoding='utf8',newline='\n')
        sources.append((raw_owner_fixture,True))
    if owner_snapshot:
        sources.append((owner_snapshot, True))
    sources = [(generated_index if path == original_index else path, owned) for path, owned in sources]
    sources = [(generated_ir if path == original_ir else path, owned) for path, owned in sources]
    sources = [(generated_sem if path == original_sem else path, owned) for path, owned in sources]
    sources = [(generated_completion if path == original_completion else path, owned) for path, owned in sources]
    sources = [(generated_stdlib if path == original_stdlib else path, owned) for path, owned in sources]
    sources = [(generated_types if path == original_types else path, owned) for path, owned in sources]
    sources = [(generated_ast if path == original_ast else path, owned) for path, owned in sources]
    sources = [(generated_parser if path == original_parser else path, owned) for path, owned in sources]
    sources = [(generated_lexer if path == original_lexer else path, owned) for path, owned in sources]
    sources = [(generated_stack if path == original_stack else path, owned) for path, owned in sources]
    scoped_sources = {Path(row['source']): Path(row['compiledSource']) for row in context_overlays}
    sources = [(scoped_sources.get(path, path), owned) for path, owned in sources]
    env = dict(os.environ)
    env["PATH"] = str(cc.parent) + os.pathsep + env.get("PATH", "")
    common = ["-std=c11", "-ffunction-sections", "-fdata-sections", "-DXRT_MODULE_ALL=", "-DXLS_COMPILER_XRT=1",
              "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
              "-I" + str(ide / "src"), "-I" + str(output / "frontend"), "-I" + str(frontend),
              "-I" + str(frontend / "src/ir"),
              "-I" + str(frontend / "release"), "-I" + str(frontend / "release/include"),
              "-I" + str(output / 'native_xrt'), "-I" + str(frontend / "lib/xrt"),
              "-include", str(output / 'native_xrt/xrt.h')]
    # MODULE_ALL also selects memory-debug pointer validation. Its release
    # path scans heap ownership when freeing large JSON trees. Select the
    # library's existing release policy consistently in EVERY translation unit;
    # Debug keeps the diagnostic allocator. This does not change analysis or
    # source/provenance validation and does not touch the XGE-owned GUI runtime.
    common += ["-O2", "-DXRT_EXCLUDE_MEMORY_DEBUG="] if args.configuration == "Release" else ["-O0", "-g"]
    if ir_overlay.get('irDependencyCohort')=='current-ed78373d':common+=['-DXLS_NATIVE_IR_PROVIDER_FIELDS=1']
    index_budget_flag = "-DXL6_INDEX_BYTES=" + str(NATIVE_INDEX_BYTES) + "u"
    common += [index_budget_flag]
    common += args.cflag
    strict = ["-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter"]
    shared = ["-Wall", "-Wno-pointer-sign", "-Wno-unused-function", "-Wno-unused-variable",
              "-Wno-unused-but-set-variable", "-Wno-format-truncation", "-Wno-dangling-pointer"]
    libraries = ["-lm"]
    if os.name == "nt":
        libraries += ["-lws2_32", "-lwinhttp", "-liphlpapi", "-lgdi32", "-luser32", "-lshell32",
                      "-lole32", "-lwinmm", "-lavrt", "-lversion", "-lbcrypt", "-ladvapi32"]
    else:
        common += ["-D_GNU_SOURCE=1"]
        libraries += ["-pthread", "-ldl"]
    commands, inputs, outputs, runs, imports = [], {str(Path(__file__).resolve()): sha(Path(__file__)),
        str(original_index): index_overlay["sourceSha256"], str(original_ir): ir_overlay["sourceSha256"],
        str(original_sem): sem_overlay["sourceSha256"], str(original_stdlib): stdlib_overlay["sourceSha256"],
        str(original_completion): completion_overlay["sourceSha256"],
        str(original_types): types_overlay["sourceSha256"]}, {}, [], {}
    source_overlays = [index_overlay, ir_overlay, sem_overlay, completion_overlay, stdlib_overlay, types_overlay, ast_overlay, parser_overlay, lexer_overlay, stack_overlay, *context_overlays, *owner_overlays]
    inputs.update({row['source']: row['sourceSha256'] for row in owner_overlays})
    inputs[str(Path(__file__).with_name('native_cancellation_overlay.py').resolve())] = sha(Path(__file__).with_name('native_cancellation_overlay.py'))
    inputs[str(Path(__file__).with_name('native_ir_memo_overlay.py').resolve())] = sha(Path(__file__).with_name('native_ir_memo_overlay.py'))
    inputs[str(Path(__file__).with_name('native_ir_scan_overlay.py').resolve())] = sha(Path(__file__).with_name('native_ir_scan_overlay.py'))
    inputs[str(Path(__file__).with_name('native_current_ir_overlay.py').resolve())] = sha(Path(__file__).with_name('native_current_ir_overlay.py'))
    inputs[str(Path(__file__).with_name('native_ir_derived_overlay.py').resolve())] = sha(Path(__file__).with_name('native_ir_derived_overlay.py'))
    inputs[str(Path(__file__).with_name('native_current_ir_profile.py').resolve())] = sha(Path(__file__).with_name('native_current_ir_profile.py'))
    inputs[str(Path(__file__).with_name('native_current_retirement_profile.py').resolve())] = sha(Path(__file__).with_name('native_current_retirement_profile.py'))
    for name in ('native_current_semantic_profile.py','native_semantic_effects_overlay.py','native_current_stdlib_profile.py','native_current_context_profile.py','native_type_constructor_overlay.py'):
        controller=Path(__file__).with_name(name).resolve()
        inputs[str(controller)]=sha(controller)
    for overlay in (ir_overlay,sem_overlay,stdlib_overlay,completion_overlay,*context_overlays):
        inputs.update(overlay.get('auditedDependencies',{}))
    if sem_overlay.get('declarationEffects'):
        inputs.update(sem_overlay['declarationEffects']['auditedDependencies'])
    inputs[str(Path(__file__).with_name('native_type_source_overlay.py').resolve())] = sha(Path(__file__).with_name('native_type_source_overlay.py'))
    inputs[str(Path(__file__).with_name('native_raw_body_overlay.py').resolve())] = sha(Path(__file__).with_name('native_raw_body_overlay.py'))
    inputs[str(Path(__file__).with_name('native_ast_overlay.py').resolve())] = sha(Path(__file__).with_name('native_ast_overlay.py'))
    inputs[str(Path(__file__).with_name('native_initializer_overlay.py').resolve())] = sha(Path(__file__).with_name('native_initializer_overlay.py'))
    inputs[str(Path(__file__).with_name('native_parser_ownership_overlay.py').resolve())] = sha(Path(__file__).with_name('native_parser_ownership_overlay.py'))
    inputs[str(Path(__file__).with_name('native_signature_overlay.py').resolve())] = sha(Path(__file__).with_name('native_signature_overlay.py'))
    inputs[str(Path(__file__).with_name('native_index_overlay.py').resolve())] = sha(Path(__file__).with_name('native_index_overlay.py'))
    inputs.update({stack_overlay['source']:stack_overlay['sourceSha256'], stack_overlay['grammarSource']:stack_overlay['grammarSha256']})
    inputs[str(original_parser)] = parser_overlay['sourceSha256']
    inputs[str(original_lexer)] = lexer_overlay['sourceSha256']
    inputs.update({ast_overlay['source']:ast_overlay['sourceSha256'], ast_overlay['headerSource']:ast_overlay['headerSourceSha256'], ast_overlay['runtimeSource']:ast_overlay['runtimeSourceSha256']})
    inputs.update({row["source"]: row["sourceSha256"] for row in context_overlays})
    inputs[completion_overlay['composedFrom']] = completion_overlay['composedFromSha256']
    source_overlays.append(runtime_overlay)
    inputs[runtime_overlay['source']] = runtime_overlay['sourceSha256']
    inputs.update(runtime_overlay['sourceMaterials'])
    runtime_profiles = []
    ast_consumers = []
    started = time.monotonic()
    status = "failed"

    def run(command: list[str], name: str, timeout: int = 180) -> subprocess.CompletedProcess:
        commands.append(command)
        result = subprocess.run(command, env=env, cwd=ide, capture_output=True, timeout=timeout)
        (output / (name + ".log")).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{name} failed ({result.returncode}); see {output / (name + '.log')}")
        return result

    def unit_flags(source: Path, owned: bool) -> list[str]:
        flags = common + (strict if owned else shared)
        if source in loader_references:flags+=['-include',str(loader_aliases)]
        if source==ide/'tests/analysis_current_loader_test.c':flags+=['-DXLS_CURRENT_LOADER_DIRECTORY="'+(output/'loader-fixtures').as_posix()+'"']
        if source == ide / 'tests/project_reference_owner_fault_test.c':
            if not owner_coordinator:
                raise ValueError('missing owner publication fixture target')
            flags += ['-DXIDE_OWNER_REFERENCE_FIXTURE="' + owner_coordinator.as_posix() + '"']
        return flags

    try:
        probe_sources = [ide / "src/analysis/runtime.c", frontend / "src/ast/xl6_ast.c"]
        if args.server:
            probe_sources.append(ide / "src/server/xlang_ls.c")
        for source in probe_sources:
            result = run([str(cc), *common, "-dM", "-E", str(source)], source.stem + "-runtime-features")
            features = {line.split()[1] for line in result.stdout.decode("utf-8").splitlines()
                        if line.startswith("#define ") and len(line.split()) > 1}
            debug_memory = "XRT_FEATURE_MEMORY_DEBUG" in features
            if debug_memory != (args.configuration == "Debug") or not {"XRT_MODULE_ALL", "XRT_FEATURE_VALUE"} <= features:
                raise RuntimeError("inconsistent native XRT runtime profile: " + str(source))
            runtime_profiles.append(dict(source=str(source), memoryDebug=debug_memory,
                macroSha256=hashlib.sha256(result.stdout).hexdigest()))
        for source, owned in sources:
            flags = unit_flags(source, owned)
            result = run([str(cc), *flags, "-MM", "-MT", "xlsObject", str(source)], source.stem + "-deps")
            unit_dependencies = dependencies(result.stdout.decode("utf-8"))
            ast_headers = {path for path in unit_dependencies if path.name == 'xl6_ast.h'}
            if ast_headers:
                if ast_headers != {Path(ast_overlay['compiledHeader']).resolve()}:
                    raise RuntimeError('mixed native AST ownership layouts: ' + str(source))
                ast_consumers.append(dict(source=str(source),header=str(next(iter(ast_headers)))))
            for path in unit_dependencies:
                if path.name == 'xl6_ast.h' and path != Path(ast_overlay['compiledHeader']).resolve():
                    raise RuntimeError('mixed native AST ownership layouts: ' + str(path))
                digest = sha(path)
                if str(path) in inputs and inputs[str(path)] != digest:
                    raise RuntimeError(f"source changed during dependency discovery: {path}")
                inputs[str(path)] = digest
        inputs[str(cc)] = sha(cc)
        objects = {}
        for source, owned in sources:
            target = output / (source.stem + ".o")
            run([str(cc), *unit_flags(source, owned), "-c", str(source), "-o", str(target)], source.stem)
            objects[source.stem] = target
        ar = cc.with_name("ar.exe" if os.name == "nt" else "ar")
        core = output / "xlang-analysis-core.a"
        core_names = {Path(source).stem for source in FRONTEND_SOURCES} | {"runtime", "frontend_cancel", "frontend_ir_regions","frontend_symbols","frontend_entry_cache","module_store","entity_store","module_graph","input_view", "input_topology", "dependency_graph", "text", "context", "frontend_context", "frontend_raw_body", "ast_storage", "ast_materialize", "ast_clone", "ast_intern", "function_unit", "typed_module", "syntax", "session", "worker", "projection", "folding", "selection", "semantic", "signature", "signature_session", "check", "input_batch", "frontend_overlay", "check_session", "dependency_session", "completion_session", "input_state", "diagnostic_paths"}
        members = [target for name, target in objects.items() if name in core_names]
        run([str(ar), "rcs", str(core), *map(str, members)], "archive")
        limited_index = output / "xl6_sem_index_limit.o"
        if not args.skip_tests:
            limited_flags = [flag for flag in common if flag != index_budget_flag]
            run([str(cc), *limited_flags, "-DXL6_INDEX_BYTES=1024u", *shared, "-c", str(generated_index),
                 "-o", str(limited_index)], "index-limit-fixture")
        suffix = ".exe" if os.name == "nt" else ""
        for name in (() if args.skip_tests else tests):
            program = output / (name + suffix)
            extra = [str(objects["document_sync"])] if name in ("document_sync_test", "diagnostic_stream_test") else []
            if name=='analysis_current_provider_test' and ir_overlay.get('irDependencyCohort')=='current-ed78373d':extra+=[str(objects['xl6_ir_lower'])]
            if name=='analysis_current_loader_test' and loader_references:
                extra += [str(objects[p.stem]) for p in loader_references]+['-Wl,--wrap=xrtMalloc']
            if name=='analysis_ir_callsite_test':extra+=['-Wl,--wrap=xrtArrayReserve','-Wl,--wrap=xrtArrayResize']
            if name=='analysis_type_constructor_test':
                extra += ['-Wl,--wrap='+function for function in ('xrtCalloc','xrtStrDupN','xrtStrDup')]
            if name=='analysis_ir_derived_test':extra+=['-Wl,--wrap='+function for function in ('xrtCalloc','xrtArrayInit','xrtArrayResize','xrtArrayPush','xrtStrBufReserve')]
            if name=='navigation_cache_test':extra+=['-Wl,--wrap='+function for function in ('xrtCalloc','xrtStrDup','xlsSemanticCheckRetain','malloc','calloc','free','xrtMalloc','xrtRealloc','xrtArrayInit','xrtArrayReserve','xrtArrayResize','xrtStrBufAppend','xlsTextCreate','xlsSemanticIndexPointFact')]
            if name=='analysis_selection_test':extra+=['-Wl,--wrap=xrtCalloc','-Wl,--wrap=xrtArrayInit','-Wl,--wrap=xrtArrayPush']
            if name == "analysis_index_limit_test":
                extra.append(str(limited_index))
            if name in ("analysis_completion_body_test", "analysis_completion_body_failure_test", "analysis_semantic_proof_test", "analysis_signature_test"):
                extra.append("-Wl,--wrap=xlsRawBodyMemoAdmit")
            if name == "analysis_syntax_storage_test":
                extra += ["-Wl,--wrap=xrtCalloc","-Wl,--wrap=xrtStrDupN"]
            if name == "analysis_raw_body_projection_test":
                extra += [str(objects['syntax_raw_fixture']),'-Wl,--wrap=xlsAstMaterialize','-Wl,--wrap=xl6AstFree']
            if name == "analysis_completion_pool_fault_test":
                extra += ["-Wl,--wrap=" + function for function in (
                    "xrtStrDup", "xrtStrDupN", "xrtRealloc", "xrtFree", "xrtStrBufAppend", "xrtStrBufFree", "xrtMalloc")]
            if name == "analysis_typed_module_test":
                extra += ["-Wl,--wrap=xl6AstFree", "-Wl,--wrap=xlsTypedModuleTake"]
            if name == "analysis_typed_identity_test":
                extra += ["-Wl,--wrap=xrtCalloc", "-Wl,--wrap=xrtFree", "-Wl,--wrap=xl6AstFree"]
            if name == "analysis_semantic_final_input_test":
                extra += ["-Wl,--wrap=xrtCalloc", "-Wl,--wrap=xrtFree",
                          "-Wl,--wrap=xrtStrBufReserve", "-Wl,--wrap=xrtStrBufFree"]
            if name == "analysis_ir_input_test":
                extra += ["-Wl,--wrap=" + function for function in (
                    "xlsFrontendIrResultBuildTypedChecked", "xrtCalloc", "xrtFree", "xrtArrayResize",
                    "xrtArrayUnit", "xrtStrBufReserve", "xrtStrBufFree", "xrtMapSet")]
            if name == "analysis_semantic_alias_reads_test":
                extra += ["-Wl,--wrap=" + function for function in (
                    "xrtStrDup", "xrtStrBufReserve", "xrtArrayPush", "xrtArrayResize")]
            if name in ("analysis_ir_persistent_fault_test", "analysis_typed_module_fault_test"):
                extra += ["-Wl,--wrap=" + function for function in (
                    "xrtMalloc", "xrtCalloc", "xrtRealloc", "xrtFree", "xrtStrBufReserve", "xrtStrBufFree",
                    "xrtArrayPush", "xrtArrayResize", "xrtArrayReserve", "xrtArrayUnit", "xrtMapSet")]
                if name == "analysis_typed_module_fault_test":extra.append("-Wl,--wrap=xl6AstFree")
            if name in HOST_TESTS:
                extra += [str(objects[Path(source).stem]) for source in SERVER_SOURCES
                          if Path(source).stem != "xlang_ls" and
                          not (name in ("snapshot_report_test", "reference_capture_test", "project_reference_owner_fault_test") and Path(source).stem == "language_snapshot") and
                          not (name == "path_identity_batch_test" and Path(source).stem == "path") and
                          not (name in ("semantic_lookup_test", "semantic_reference_shard_test") and Path(source).stem == "semantic_definition") and
                          not (name in ("project_reference_budget_test", "reference_path_index_test", "project_reference_parallel_test", "project_reference_owner_fault_test") and Path(source).stem == "project_references")]
            if name == "project_reference_owner_fault_test":
                extra.append(str(objects['owner_snapshot_fault']))
            if name in CLIENT_TESTS:
                # The stream fixture privately owns its actor for deterministic
                # gates. Other client fixtures need only the pure decoders.
                extra += [str(objects[Path(source).stem]) for source in CLIENT_TEST_SOURCES
                          if Path(source).stem != "diagnostic_stream" and
                          not (name == "reference_results_test" and Path(source).stem == "reference_results")]
            if name in ("analysis_dependency_capture_test", "analysis_dependency_session_test", "analysis_dependency_worker_test"):
                # Observe real original semantic/IR entry calls in this fixture
                # only;the dependency snapshot must invoke neither stage.
                extra += ["-Wl,--wrap=xl6SemCheckModuleWithIndex",
                          "-Wl,--wrap=xl6IrBuildModuleFromAst"]
            if name == "analysis_topology_clone_fault_test":
                extra += ["-Wl,--wrap=" + function for function in (
                    "xrtMalloc", "xrtCalloc", "xrtFree", "xrtArrayInit",
                    "xrtArrayReserve", "xrtArrayPush", "xrtArrayUnit")]
            run([str(cc), str(objects[name]), *extra, str(core), "-Wl,--gc-sections", *libraries, "-o", str(program)], name + "-link")
            result = run([str(program)], name + "-run", 180 if name == "analysis_ir_persistent_fault_test" else 120 if name == "analysis_ir_scan_test" else 30)
            if b"passed:" not in result.stdout:
                raise RuntimeError(f"missing test completion receipt: {name}")
            runs.append(dict(name=name,stdout=result.stdout.decode("utf-8")))
            outputs[program.name] = sha(program)
        if args.server:
            program = output / ("xlang-ls" + suffix)
            server_objects = [str(objects[Path(source).stem]) for source in SERVER_SOURCES]
            run([str(cc), *server_objects, str(core), "-Wl,--gc-sections", *libraries, "-o", str(program)], "server-link")
            outputs[program.name] = sha(program)
        if os.name == "nt":
            for name in outputs:
                result = run([str(cc.with_name("objdump.exe")), "-p", str(output / name)], name + "-imports")
                names = [line.split("DLL Name:",1)[1].strip() for line in result.stdout.decode().splitlines() if "DLL Name:" in line]
                if not names or any(name.lower() in ("xge.dll", "xlc.dll") for name in names):
                    raise RuntimeError("standalone runtime ownership failed: " + name)
                imports[name] = names
        if any(sha(Path(path)) != digest for path, digest in inputs.items()):
            raise RuntimeError("source or toolchain changed during the build")
        outputs[core.name] = sha(core)
        status = "passed"
    finally:
        report = dict(schema=1,status=status,scope="standalone native C/XRT shared analysis with private AST ownership; full LSP/performance/delivery goal remains incomplete",
                      frontendRoot=str(frontend),compiler=str(cc),compilerSha256=sha(cc),inputs=inputs,outputs=outputs,
                      configuration=args.configuration,serverBuilt=args.server,testsBuilt=not args.skip_tests,
                      commands=commands,runs=runs,imports=imports,elapsedSeconds=time.monotonic()-started)
        report["sourceOverlays"] = source_overlays
        report["runtimeProfiles"] = runtime_profiles
        report["privateAstConsumers"] = ast_consumers
        (output / "build-result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(dict(status=status,report=str(output / "build-result.json")),ensure_ascii=False))


if __name__ == "__main__":
    main()
