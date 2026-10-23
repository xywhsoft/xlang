"""Build the standalone analysis core directly from shared xlang source assets.

No compiler/SDK files are copied or changed. This target owns compiler XRT and
must not be linked into the XGE-owned IDE. It does not replace the shipped LS.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from xrt_process_tree_overlay import derive as derive_process_runtime
from native_context_overlay import derive as derive_context_runtime

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
    "src/protocol/analysis_context_wire.c", "src/services/analysis_catalog.c",
    "src/protocol/lsp_wire.c", "src/services/language_check.c", "src/services/language_snapshot.c", "src/services/diagnostic_store.c",
    "src/services/semantic_definition.c", "src/services/semantic_completion.c", "src/services/c_bridge_map.c",
    "src/services/toolchain.c", "src/platform/file_rename.c", "src/platform/file_io.c",
    "src/platform/text_file.c", "src/platform/process.c", "src/platform/path.c", "src/editor/coordinates.c",
    "src/server/xlang_ls.c",
)
CORE_TESTS = ("analysis_context_fault_test", "analysis_context_resolution_test", "analysis_input_view_test", "analysis_context_test", "analysis_check_test", "analysis_check_session_test", "analysis_incremental_check_test", "analysis_semantic_memo_test", "analysis_check_worker_test", "analysis_index_test", "analysis_index_limit_test", "analysis_ir_scan_test", "analysis_stdlib_scan_test", "analysis_overload_scan_test", "analysis_completion_candidates_test", "analysis_ast_clone_test", "analysis_semantic_test", "analysis_completion_session_test", "analysis_folding_test", "analysis_worker_test", "analysis_session_test", "analysis_incremental_syntax_test", "analysis_cursor_syntax_test", "analysis_text_test", "document_sync_test")
HOST_TESTS = ("analysis_catalog_test", "language_context_test", "semantic_lookup_test", "analysis_index_view_test", "language_input_guard_test", "diagnostic_store_test", "language_definition_proof_test", "snapshot_report_test")
CLIENT_TESTS = ("diagnostic_dependencies_test", "diagnostic_notice_test", "diagnostic_stream_test")
CLIENT_TEST_SOURCES = ("src/services/diagnostic_dependencies.c", "src/services/diagnostic_notice.c", "src/services/diagnostic_stream.c")
HOST_TESTS += CLIENT_TESTS
NATIVE_INDEX_BYTES = 32 * 1024 * 1024
CORE_TESTS = ("analysis_process_tree_test", *CORE_TESTS)
CORE_TESTS = ("diagnostic_paths_test", *CORE_TESTS)
CORE_TESTS = ("analysis_entry_cache_test", *CORE_TESTS)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare_native_index(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Expose the shared collector and parameterize its limit, keeping upstream immutable."""
    original = frontend / "src/sem/xl6_sem_index.c"
    body = original.read_bytes()
    if hashlib.sha256(body).hexdigest() != '215ffbdfe8cf2a0d54b36c76a96f5c0ff603020b7aee6868b9e819bca70b1c54':
        raise ValueError('shared index changed; audit typed arena/JSON budget separation and unchanged collector rules')
    type_model = frontend / 'src/type/xl6_type_model.c'
    type_sha = 'fdf35faa5dc2366a0625317d70cc82dbe8031762fcb7c81439e99a3a8de2a051'
    if sha(type_model) != type_sha:
        raise ValueError('shared canonical builder changed; audit minimum capacity and geometric allocation charge')
    source = body.decode("utf-8").replace("\r\n", "\n")
    definition = "#define XL6_INDEX_BYTES (8u * 1024u * 1024u)"
    if source.count(definition) != 1:
        raise ValueError("shared index budget declaration changed; audit its native build overlay")
    for old, new in {
        '} XL6_IndexBuild;': '} XL6_IndexBuild;\n#include "analysis/frontend_index_budget.inc"',
        'if (!xl6IndexBudget(b, 1536)) return 0;': 'if (!xlsIndexRecordBudget(b)) return 0;',
    }.items():
        if source.count(old) != 1:
            raise ValueError('shared typed-index allocation seam changed: ' + old)
        source = source.replace(old, new)
    generated_root = output / "frontend"
    generated_root.mkdir()
    compiled = generated_root / "xl6_sem_index.c"
    compiled.write_bytes((source.replace(definition,
        "#ifndef XL6_INDEX_BYTES\n" + definition + "\n#endif") +
        '\n#include "analysis/frontend_index.inc"\n').encode("utf-8"))
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        change="preserve shared collector; separate precharged typed arenas from JSON record allowance", indexBytes=NATIVE_INDEX_BYTES,
        auditedSeams=4, nativeRecordBytes=64, legacyJsonRecordBytes=1536,
        auditedDependencies={str(type_model): type_sha})


def prepare_native_ir(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Index iteration candidates without changing shared cleanup/verify rules."""
    original = frontend / 'src/ir/xl6_ir.c'
    body = original.read_bytes()
    if hashlib.sha256(body).hexdigest() != '6022b348ecb5ec0dc79eac881a4e51cb6dc227daa210f730bcdf27a191aad2d0':
        raise ValueError('shared IR changed; audit cleanup/plan/call candidates, scope-proof purity and phase mutation boundaries')
    source = body.decode('utf-8').replace('\r\n', '\n')
    changes = {
        '#include <wchar.h>': '#include <wchar.h>\n#include "analysis/frontend_ir.inc"',
        'static const XL6_AstNode* xl6IrFindTypeDecl(const XL6_AstNode* root, const XL6_AstNode* type)\n{':
            'static const XL6_AstNode* xlsIrFindTypeDeclOriginal(const XL6_AstNode* root, const XL6_AstNode* type)\n{',
        'int xl6IrBuildModuleFromAst(XL6_IrModule* module, const XL6_AstNode* root, XL6_DiagList* diags)\n{':
            'static int xlsIrBuildModuleOriginal(XL6_IrModule* module, const XL6_AstNode* root, XL6_DiagList* diags)\n{',
        'int xl6IrRefreshExitPlans(XL6_IrModule* module)\n{': 'static int xlsIrRefreshExitPlansOriginal(XL6_IrModule* module)\n{',
        'int xl6IrVerifyModule(const XL6_IrModule* module, XL6_DiagList* diags)\n{': 'static int xlsIrVerifyModuleOriginal(const XL6_IrModule* module, XL6_DiagList* diags)\n{',
        'static int xl6IrBuildImplicitConversionAbis(XL6_IrModule* module)\n{':
            'static int xlsIrBuildImplicitConversionAbisOriginal(XL6_IrModule* module)\n{',
        'XL6_IrNodeId origin = (XL6_IrNodeId)(node - module->nodes + 1);\n'
        '    for (i = 0; i < module->callSiteCount; ++i) {\n'
        '        const XL6_IrCallSite* site = &module->callSites[i];\n'
        '        const XL6_IrNode *declaration, *parameters = NULL, *receiver;':
            'XL6_IrNodeId origin = (XL6_IrNodeId)(node - module->nodes + 1);\n'
            '    for (i = xlsIrFirstCallCandidate(module, origin); i < module->callSiteCount;\n'
            '         i = xlsIrNextCallCandidate(module, origin, i)) {\n'
            '        const XL6_IrCallSite* site = &module->callSites[i];\n'
            '        const XL6_IrNode *declaration, *parameters = NULL, *receiver;',
        'static int xl6IrVerifyScopeExitActions(\n    const XL6_IrModule* module, const XL6_IrExitPlan* plan)\n{':
            'static int xlsIrVerifyScopeExitActionsOriginal(\n    const XL6_IrModule* module, const XL6_IrExitPlan* plan)\n{',
        'for ( i = afterScope ? scope->childCount + 1 : (size_t)plan->origin - 1; i > 0; --i ) {':
            'for ( i = afterScope ? scope->childCount + 1 : xlsIrPreviousCandidate(module, owner, plan->origin, routed); i > 0;\n'
            '          i = afterScope ? i - 1 : xlsIrPreviousCandidate(module, owner, (XL6_IrNodeId)i, routed) ) {',
        'for (child = afterScope ? block->childCount + 1 : (size_t)plan->origin - 1; child > 0; --child) {':
            'for (child = afterScope ? block->childCount + 1 : xlsIrPreviousCandidate(module, owner, plan->origin, routed); child > 0;\n'
            '         child = afterScope ? child - 1 : xlsIrPreviousCandidate(module, owner, (XL6_IrNodeId)child, routed)) {',
        'for ( j = 0; j < module->exitPlanCount; ++j ) {\n'
        '            if ( module->exitPlans[j].origin == (XL6_IrNodeId)(i + 1) ) {':
            'for ( j = xlsIrFirstPlanCandidate(module, (XL6_IrNodeId)(i + 1), module->exitPlanCount);\n'
            '              j < module->exitPlanCount;\n'
            '              j = xlsIrNextPlanCandidate(module, (XL6_IrNodeId)(i + 1), j, module->exitPlanCount) ) {\n'
            '            if ( module->exitPlans[j].origin == (XL6_IrNodeId)(i + 1) ) {',
        'for (previous = 0; previous < j; ++previous)\n'
        '                        if (module->exitPlans[previous].origin == module->exitPlans[j].origin &&':
            'for (previous = xlsIrFirstPlanCandidate(module, (XL6_IrNodeId)(i + 1), j); previous < j;\n'
            '                         previous = xlsIrNextPlanCandidate(module, (XL6_IrNodeId)(i + 1), previous, j))\n'
            '                        if (module->exitPlans[previous].origin == module->exitPlans[j].origin &&',
    }
    for old, new in changes.items():
        if source.count(old) != 1:
            raise ValueError('shared IR scan seam changed; audit its native overlay: ' + old)
        source = source.replace(old, new)
    compiled = output / 'frontend/xl6_ir.c'
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        change='bounded nominal-declaration candidates, cleanup/plan/call-origin indexes and per-verification scope proofs; shared rules retained',
        auditedSeams=len(changes), indexBytes=16 * 1024 * 1024, planIndexBytes=8 * 1024 * 1024,
        scopeProofBytes=1024 * 1024, callIndexBytes=8 * 1024 * 1024, typeLookupBytes=8 * 1024 * 1024)


def prepare_native_sem(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Keep function rules in their original TU, add opt-in immutable memos."""
    original = frontend / 'src/sem/xl6_sem.c'
    body = original.read_bytes()
    # Reuse depends on the checker's side effects and cross-declaration reads,
    # not just stable patch locations. Require an explicit audit on upgrades.
    audited = 'd4f19af32f1732126913b99717a6b1b2d38d92f343b60828786b62d2a7d8183a'
    if hashlib.sha256(body).hexdigest() != audited:
        raise ValueError('shared semantic checker changed; audit memo context/identity/side-effect assumptions')
    registry = frontend / 'src/sem/xl6_type_registry.c'
    registry_sha = 'a0c37403f28d8a29ac4589e1dd78344a0d5fa2c8051ba2ead0209b9e0a228583'
    if sha(registry) != registry_sha:
        raise ValueError('shared type registry changed; audit generic identity reservation replay')
    source = body.decode('utf-8').replace('\r\n', '\n')
    begin = ('    for ( i = 0; i < root->childCount; ++i ) {\n'
             '        XL6_AstNode* child = root->children[i];\n'
             '        if ( child->kind == XL6_AST_FUNC || child->kind == XL6_AST_ASYNC_FUNC ) {')
    changes = {
        '} XL6_SemContext;': '} XL6_SemContext;\n#include "analysis/frontend_context.h"\n#include "analysis/frontend_sem.inc"',
        'static int xl6SemCheckFunc(XL6_SemContext* ctx, XL6_AstNode* node)\n{':
            'static int xlsSemCheckFuncOriginal(XL6_SemContext* ctx, XL6_AstNode* node)\n{',
        begin: '    xlsSemMemoBegin(&ctx);\n' + begin,
    }
    for old, new in changes.items():
        if source.count(old) != 1:
            raise ValueError('shared semantic memo seam changed; audit: ' + old)
        source = source.replace(old, new)
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
    compiled = output / 'frontend/xl6_sem.c'
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        change='bounded ordered conflict/completion candidates with public namespace prefilter, read-only name-query alias scope and opt-in exact-context function memos; original rules and final annotations retained',
        auditedSeams=len(changes)+len(conflict_changes)+len(completion_changes)+1, memoBytes=64 * 1024 * 1024,
        overloadCandidateBytes=8 * 1024 * 1024,
        auditedDependencies={str(registry): registry_sha})


def prepare_native_stdlib(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Reject only absent spellings; positive decisions keep the shared rules."""
    original = frontend / 'src/stdlib/xl6_stdlib.c'
    body = original.read_bytes()
    if hashlib.sha256(body).hexdigest() != 'bf863afd1bdef637388e6e4198813484f90e4eb8f1399580d75304e70a110919':
        raise ValueError('shared stdlib changed; audit scope producers/name lifetimes/free paths and exact-text discovery predicates')
    source = body.decode('utf-8').replace('\r\n', '\n')
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
    for old, new in changes.items():
        if source.count(old) != 1:
            raise ValueError('shared stdlib scan seam changed; audit: ' + old)
        source = source.replace(old, new)
    compiled = output / 'frontend/xl6_stdlib.c'
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled), auditedSeams=len(changes),
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
    """Filter immutable-scope alias candidates without changing alias rules."""
    original = frontend / 'src/type/xl6_type_model.c'
    body = original.read_bytes()
    if hashlib.sha256(body).hexdigest() != 'fdf35faa5dc2366a0625317d70cc82dbe8031762fcb7c81439e99a3a8de2a051':
        raise ValueError('shared type model changed; audit alias predicate, lookup and immutable scope contract')
    source = body.decode('utf-8').replace('\r\n', '\n')
    old = 'static const XL6_AstNode* xl6TypeFindAlias(const XL6_AstNode* root, const char* name)\n{'
    if source.count(old) != 1:
        raise ValueError('shared alias lookup seam changed')
    source = source.replace(old, '#include "analysis/frontend_types.inc"\n'
        'static const XL6_AstNode* xlsTypeFindAliasOriginal(const XL6_AstNode* root, const char* name)\n{')
    compiled = output / 'frontend/xl6_type_model.c'
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled), auditedSeams=1,
        change='ordered alias candidates in explicit immutable IR scope; original alias rules retained',
        pointerBudgetBytes=8 * 1024 * 1024)


def main() -> None:
    ide = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frontend-root", type=Path, default=ide.parent / "demo6")
    parser.add_argument("--output", type=Path, required=True, help="fresh build directory")
    parser.add_argument("--cc", default=shutil.which("gcc") or "E:/software/w64devkit/bin/gcc.exe")
    parser.add_argument("--server", action="store_true", help="also build the external language-server host")
    parser.add_argument("--skip-tests", action="store_true", help="build only, without compiling or running core fixtures")
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
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
        "src/analysis/text.c", "src/analysis/context.c", "src/analysis/frontend_context.c", "src/analysis/frontend_entry_cache.c", "src/analysis/input_view.c", "src/analysis/ast_clone.c", "src/analysis/syntax.c", "src/analysis/session.c", "src/analysis/worker.c", "src/analysis/folding.c", "src/analysis/semantic.c", "src/analysis/check.c", "src/analysis/input_batch.c", "src/analysis/check_session.c", "src/analysis/completion_session.c",
    )]
    if not args.skip_tests:
        sources += [(ide / "src/protocol/document_sync.c", True)]
        sources += [(ide / "tests" / (name + ".c"), True) for name in tests]
    if args.server:
        if os.name != "nt":
            parser.error("the current host still uses Windows transport/process adapters; the standalone core is separate")
        sources += [(ide / path, True) for path in SERVER_SOURCES]
        sources += [(ide / path, True) for path in CLIENT_TEST_SOURCES]
    if any(not path.is_file() for path, _ in sources):
        parser.error("frontend root must contain the shared xlang lexer, parser, AST and XRT assets")
    output.mkdir(parents=True, exist_ok=False)
    # Keep the pinned compiler assets immutable. This checked build overlay
    # makes the existing private serializer budget configurable; no grammar,
    # fact collection or index algorithm is duplicated in the IDE.
    try:
        original_index, generated_index, index_overlay = prepare_native_index(frontend, output)
        original_ir, generated_ir, ir_overlay = prepare_native_ir(frontend, output)
        original_sem, generated_sem, sem_overlay = prepare_native_sem(frontend, output)
        original_stdlib, generated_stdlib, stdlib_overlay = prepare_native_stdlib(frontend, output)
        original_types, generated_types, types_overlay = prepare_native_types(frontend, output)
        context_overlays = derive_context_runtime(frontend, output)
        runtime_overlay = derive_process_runtime(frontend / 'lib/xrt/xrt.h', output / 'native_xrt/xrt.h')
    except ValueError as error:
        parser.error(str(error))
    sources = [(generated_index if path == original_index else path, owned) for path, owned in sources]
    sources = [(generated_ir if path == original_ir else path, owned) for path, owned in sources]
    sources = [(generated_sem if path == original_sem else path, owned) for path, owned in sources]
    sources = [(generated_stdlib if path == original_stdlib else path, owned) for path, owned in sources]
    sources = [(generated_types if path == original_types else path, owned) for path, owned in sources]
    scoped_sources = {Path(row['source']): Path(row['compiledSource']) for row in context_overlays}
    sources = [(scoped_sources.get(path, path), owned) for path, owned in sources]
    env = dict(os.environ)
    env["PATH"] = str(cc.parent) + os.pathsep + env.get("PATH", "")
    common = ["-std=c11", "-ffunction-sections", "-fdata-sections", "-DXRT_MODULE_ALL=", "-DXLS_COMPILER_XRT=1",
              "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
              "-I" + str(ide / "src"), "-I" + str(frontend),
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
    index_budget_flag = "-DXL6_INDEX_BYTES=" + str(NATIVE_INDEX_BYTES) + "u"
    common += [index_budget_flag]
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
        str(original_types): types_overlay["sourceSha256"]}, {}, [], {}
    source_overlays = [index_overlay, ir_overlay, sem_overlay, stdlib_overlay, types_overlay, *context_overlays]
    inputs.update({row["source"]: row["sourceSha256"] for row in context_overlays})
    source_overlays.append(runtime_overlay)
    inputs[runtime_overlay['source']] = runtime_overlay['sourceSha256']
    inputs.update(runtime_overlay['sourceMaterials'])
    runtime_profiles = []
    started = time.monotonic()
    status = "failed"

    def run(command: list[str], name: str, timeout: int = 180) -> subprocess.CompletedProcess:
        commands.append(command)
        result = subprocess.run(command, env=env, cwd=ide, capture_output=True, timeout=timeout)
        (output / (name + ".log")).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"{name} failed ({result.returncode}); see {output / (name + '.log')}")
        return result

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
            flags = common + (strict if owned else shared)
            result = run([str(cc), *flags, "-MM", "-MT", "xlsObject", str(source)], source.stem + "-deps")
            for path in dependencies(result.stdout.decode("utf-8")):
                digest = sha(path)
                if str(path) in inputs and inputs[str(path)] != digest:
                    raise RuntimeError(f"source changed during dependency discovery: {path}")
                inputs[str(path)] = digest
        inputs[str(cc)] = sha(cc)
        objects = {}
        for source, owned in sources:
            target = output / (source.stem + ".o")
            run([str(cc), *common, *(strict if owned else shared), "-c", str(source), "-o", str(target)], source.stem)
            objects[source.stem] = target
        ar = cc.with_name("ar.exe" if os.name == "nt" else "ar")
        core = output / "xlang-analysis-core.a"
        core_names = {Path(source).stem for source in FRONTEND_SOURCES} | {"runtime", "text", "context", "frontend_context", "ast_clone", "syntax", "session", "worker", "folding", "semantic", "check", "input_batch", "check_session", "completion_session", "input_state", "diagnostic_paths"}
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
            if name == "analysis_index_limit_test":
                extra.append(str(limited_index))
            if name in HOST_TESTS:
                extra += [str(objects[Path(source).stem]) for source in SERVER_SOURCES
                          if Path(source).stem != "xlang_ls" and
                          not (name == "snapshot_report_test" and Path(source).stem == "language_snapshot") and
                          not (name == "semantic_lookup_test" and Path(source).stem == "semantic_definition")]
            if name in CLIENT_TESTS:
                # The stream fixture privately owns its actor for deterministic
                # gates. Other client fixtures need only the pure decoders.
                extra += [str(objects[Path(source).stem]) for source in CLIENT_TEST_SOURCES
                          if Path(source).stem != "diagnostic_stream"]
            run([str(cc), str(objects[name]), *extra, str(core), "-Wl,--gc-sections", *libraries, "-o", str(program)], name + "-link")
            result = run([str(program)], name + "-run", 30)
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
        report = dict(schema=1,status=status,scope="standalone syntax and native semantic core; not shipped LSP or semantic incrementality",
                      frontendRoot=str(frontend),compiler=str(cc),compilerSha256=sha(cc),inputs=inputs,outputs=outputs,
                      configuration=args.configuration,serverBuilt=args.server,testsBuilt=not args.skip_tests,
                      commands=commands,runs=runs,imports=imports,elapsedSeconds=time.monotonic()-started)
        report["sourceOverlays"] = source_overlays
        report["runtimeProfiles"] = runtime_profiles
        (output / "build-result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(dict(status=status,report=str(output / "build-result.json")),ensure_ascii=False))


if __name__ == "__main__":
    main()
