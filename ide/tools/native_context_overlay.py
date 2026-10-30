"""Audit and adapt shared resolution inputs without copying its search rules."""
from pathlib import Path
import hashlib
from native_raw_body_overlay import imports as derive_raw_imports


def _completion_namespace(source: str) -> str:
    old='int xl6CompletionAdd(XL6_CompletionQuery* q,'
    marker='void xl6CompletionFree(XL6_CompletionQuery* q)\n{'
    if source.count(old)!=1 or source.count(marker)!=1:
        raise ValueError('shared completion label seam changed')
    source=source.replace(old,'static int xlsCompletionAddOriginal(XL6_CompletionQuery* q,',1)
    wrapper="""int xl6CompletionAdd(XL6_CompletionQuery* q,const char* name,const char* kind,XL6_SymbolId symbolId,const XL6_AstNode* selection,char* canonical)
{
    if (!q || q->error || !name || !*name || !xlsFrontendNamespaceActive())
        return xlsCompletionAddOriginal(q,name,kind,symbolId,selection,canonical);
    size_t count=xlsFrontendNamespaceCount(name);
    if (!count) { xrtFree(canonical); return 1; }
    for (size_t i=0;i<count;++i) {
        char* owned=i+1==count ? canonical : canonical ? xrtStrDup(canonical) : NULL;
        if (canonical && !owned) { q->error="cannot retain namespace completion signature"; xrtFree(canonical); return 0; }
        if (!xlsCompletionAddOriginal(q,xlsFrontendNamespaceName(name,i),kind,symbolId,selection,owned)) {
            if (i+1<count) xrtFree(canonical);
            return 0;
        }
    }
    return 1;
}

"""
    return source.replace(marker,wrapper+marker,1)


def completion_source(frontend: Path) -> tuple[Path,str,dict]:
    """Verify and compose only the emitter without producing output files."""
    from native_signature_overlay import profile
    audited=profile(frontend)
    original=frontend/'src/sem/xl6_completion.c'
    if audited[str(original)]=='d56f80b8b3ca93e9ec4009a56ee9235a86b66674f2c2ea174a0f5db36fdef2bf':
        path=frontend/'src/common/xl6_path_identity.h'
        digest='48fd7456ad348c974da20aefa87f32862daeab664a341cfa144e2817d03c2683'
        if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError('shared completion path identity changed; audit failure and equality contracts')
        audited[str(path)]=digest
    source='#include "analysis/frontend_context.h"\n'+original.read_text(encoding='utf8')
    return original,_completion_namespace(source),audited


def completion(frontend: Path, output: Path) -> tuple[Path,Path,dict]:
    """Compose only the emitter; does not admit loader/report/package assets."""
    original,source,audited=completion_source(frontend)
    compiled=output/'frontend/xl6_completion.c'
    compiled.parent.mkdir(parents=True,exist_ok=True)
    compiled.write_text(source,encoding='utf8',newline='\n')
    return original,compiled,dict(source=str(original),sourceSha256=audited[str(original)],
        compiledSource=str(compiled),compiledSha256=hashlib.sha256(compiled.read_bytes()).hexdigest(),
        auditedDependencies=audited,auditedSeams=3,scope='completion namespace projection only;resolution inputs remain separately guarded')


def derive(frontend: Path, output: Path) -> list[dict]:
    sources = {
        "src/compile/xl6_source_report.c": "e5ada493079927e46fade888ab983883bbcc4a72897acae096a441a7924f771b",
        "src/compile/xl6_imports.c": "f8e571ffa7f48b2ddc55b20e7d73de782604024d3930e2bb27fd68dc55b16182",
        "src/sem/xl6_completion.c": "384d4cf57a648900ee1b9d93df1895ba22526639f3393702891027c94053c94c",
        "src/package/xl6_package.c": "628681072acdd939c85b112ccd19af647b9084b9e460db6850d8140c18c5e881",
        "src/compile/xl6_source_overlay.c": "cfe3ed6985af841504c13b62deeb24e552086ed8a67c1f34e46847f21cf281cf",
    }
    rows = []
    for relative, expected in sources.items():
        original = frontend / relative
        raw = original.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if digest != expected:
            raise ValueError("shared resolution changed; audit scoped input/validation and original search rules: " + str(original))
        source = raw.decode("utf-8").replace("\r\n", "\n")
        source = '#include "analysis/frontend_context.h"\n' + source
        if original.stem == "xl6_source_overlay":
            source = '#include "analysis/input_topology.h"\n' + source
            source = '#include "analysis/frontend_entry_cache.h"\n' + source
            source = '#include "analysis/frontend_input_usage.h"\n' + source
            old = 'xl6ParseSourceWithCompletion('
            if source.count(old) != 2:
                raise ValueError("shared source overlay parse seams changed")
            source = source.replace(old, 'xlsFrontendEntryParse(')
            # Observe the exact original Unicode/path matcher and resolver;
            # no second path/search rule set. Resource failures conservatively
            # disable dependency reuse, without changing ordinary lookup results.
            for old, new in {
                '    if (na <= 0 || nb <= 0) return 0;':
                    '    if (na <= 0 || nb <= 0) return xlsFrontendInputUsageFailure(NULL);',
                '        equal = CompareStringOrdinal(wa, -1, wb, -1, TRUE) == CSTR_EQUAL;\n    xrtFree(wa);':
                    '        equal = CompareStringOrdinal(wa, -1, wb, -1, TRUE) == CSTR_EQUAL;\n    else xlsFrontendInputUsageFailure(NULL);\n    xrtFree(wa);',
                '    if (!absolute) return NULL;':
                    '    if (!absolute) { xlsFrontendInputUsageFailure(overlay); return NULL; }',
                '    return index < overlay->count ? &overlay->items[index] : NULL;':
                    '    if (index < overlay->count) xlsFrontendInputUsageRead(overlay,index);\n    return index < overlay->count ? &overlay->items[index] : NULL;',
            }.items():
                if source.count(old) != 1:
                    raise ValueError("shared overlay read/failure observation seam changed: " + old)
                source = source.replace(old, new, 1)
            old = '    index = xl6SourceOverlayIndex(overlay, absolute);\n    xrtFree(absolute);\n    if (index < overlay->count) xlsFrontendInputUsageRead(overlay,index);'
            if source.count(old) != 1:
                raise ValueError('original overlay lookup topology seam changed')
            source = source.replace(old, '    xlsInputTopologyLookup(overlay,absolute);\n' + old, 1)
            seams = 9
        elif original.stem == "xl6_source_report":
            old = '(char*)xrtEnvGet((str)"XLANG_PACKAGE_PATH")'
            if source.count(old) != 3 or source.count("xrtPathAbs(path)") != 3:
                raise ValueError("shared report resolution/validation seams changed")
            source = source.replace(old, "xlsFrontendContextPackagePath()")
            source = source.replace("xrtPathAbs(path)", "xlsFrontendContextAbsolute(path)")
            seams = 7
        elif original.stem == "xl6_package":
            old = "static int xl6PackageTryManifest(XL6_PackageResolveResult* result, const char* path, XL6_PackageResolveKind kind)\n{"
            marker = "static int xl6PackageLooksLikePath(const char* target)\n{"
            if source.count(old) != 1 or source.count(marker) != 1:
                raise ValueError("shared package candidate seam changed")
            source = source.replace(old, old.replace("xl6PackageTryManifest(", "xlsPackageTryManifestOriginal("), 1)
            wrapper = """static int xl6PackageTryManifest(XL6_PackageResolveResult* result, const char* path, XL6_PackageResolveKind kind)
{
    if (!path || !*path || !xlsFrontendContextCurrent()) return xlsPackageTryManifestOriginal(result,path,kind);
    char* absolute = xlsFrontendContextAbsolute(path);
    if (!absolute) return 0;
    int okay = xlsPackageTryManifestOriginal(result,absolute,kind);
    xrtFree(absolute); return okay;
}

"""
            source = source.replace(marker, wrapper + marker, 1)
            seams = 3
        elif original.stem == "xl6_imports":
            source = '#include "analysis/frontend_module_graph.h"\n' + source
            for old, new in {
                '        activeSourcePushed = 1;':
                    '        activeSourcePushed = 1;\n        xlsModuleGraphImport(deps->activeSources.items,deps->activeSources.count);',
                '    if ( activeSourcePushed ) {\n        xl6ImportsStringListPop(&deps->activeSources);':
                    '    if ( activeSourcePushed ) {\n        xlsModuleGraphExpanded(deps->activeSources.items,deps->activeSources.count,ok);\n        xl6ImportsStringListPop(&deps->activeSources);',
            }.items():
                if source.count(old) != 1:
                    raise ValueError('original import module dependency observation seam changed: ' + old)
                source = source.replace(old,new,1)
            source = '#include "src/sem/xl6_completion.h"\n' + source
            source = '#include "analysis/frontend_import_names.h"\n' + source
            shape = '    size_t count;\n} XL6_ImportNameMap;'
            if source.count(shape) != 1:
                raise ValueError("shared append-only import name map changed")
            source = source.replace(shape, '    size_t count;\n    xls_import_names_index* xlsIndex;\n} XL6_ImportNameMap;', 1)
            for name in ['Init', 'Free', 'Find', 'RewriteDotted']:
                start = ('static void ' if name in ['Init', 'Free'] else 'static const char* ' if name == 'Find' else 'static char* ') + 'xl6ImportsNameMap' + name + '('
                if source.count(start) != 1:
                    raise ValueError("shared import name scan seam changed: " + name)
                source = source.replace(start, start.replace('xl6ImportsNameMap'+name, 'xlsImportsNameMap'+name+'Original'), 1)
            marker_names = 'static int xl6ImportsNameMapAdd('
            if source.count(marker_names) != 1:
                raise ValueError("shared import name append seam changed")
            source = source.replace(marker_names, '#include "analysis/frontend_import_names.inc"\n' + marker_names, 1)
            for name in ['ModuleHasDeclName', 'ModuleHasNaturalDeclName', 'PrepareSourceModule']:
                start = 'static int xl6Imports' + name + '('
                if source.count(start) != 1:
                    raise ValueError("shared import preparation seam changed: " + name)
                replacement = start.replace('xl6Imports'+name, 'xlsImports'+name+'Original')
                if name == 'ModuleHasDeclName':
                    replacement = 'static int xl6ImportsModuleHasDeclName(const XL6_AstNode*,const char*);\n' + replacement
                source = source.replace(start, replacement, 1)
            marker_decls = 'static char* xl6ImportsImportedPublicNameForInternal('
            setter = '            xl6ImportsSetDeclName(child, internalName);'
            if source.count(marker_decls) != 1 or source.count(setter) != 1:
                raise ValueError("shared preparation root/rename observer seam changed")
            source = source.replace(marker_decls, '#include "analysis/frontend_import_decls.inc"\n' + marker_decls, 1)
            source = source.replace(setter, '            xlsImportsSetIndexedDeclName(moduleRoot, i, child, internalName);', 1)
            marker = 'static int xl6ImportsTryRewriteQualifiedNamespaceRef(\n'
            old = '    symbol = xl6ImportsFindSymbol(symbols, ns, parts[count - 1]);'
            if source.count(marker) != 1 or source.count(old) != 1:
                raise ValueError("shared qualified namespace cursor seam changed")
            source = source.replace(marker, '#include "analysis/frontend_namespace.inc"\n' + marker, 1)
            source = source.replace(old,
                '    if (!nsInfo->isPrivate && xlsImportsNamespaceCursor(node)) {\n'
                '        int okay=xlsImportsCaptureNamespace(node,symbols,ns); xrtFree(ns); return okay;\n'
                '    }\n' + old, 1)
            source=derive_raw_imports(source)
            seams = 19
        else:
            source = _completion_namespace(source)
            seams = 3
        compiled = output / "frontend" / original.name
        compiled.parent.mkdir(parents=True, exist_ok=True)
        compiled.write_text(source, encoding="utf-8", newline="\n")
        rows.append(dict(source=str(original), sourceSha256=digest,
                         compiledSource=str(compiled), compiledSha256=hashlib.sha256(compiled.read_bytes()).hexdigest(),
                         auditedSeams=seams, change=("bounded pre-import entry AST reuse,allocation-free usedmask andoptional persistent original hit/miss overlay topology;failed matching disablesproof;source reporting,path rules and parser fallback retained" if original.stem == "xl6_source_overlay" else "request-owned directory/package inputs; original resolution/validation rules retained; default calls unchanged")))
    return rows
