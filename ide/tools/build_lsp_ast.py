"""Build/test the guarded native AST substrate without admitting a new frontend.

Use a committed frontend snapshot. Parser, semantic and IR guards in the full
server build remain independent; this target does not publish a server or IDE.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

from build_lsp_core import dependencies
from native_ast_overlay import derive


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main(syntax=False,semantic=False):
    ide = Path(__file__).resolve().parents[1]
    syntax = syntax or semantic
    scope = ('Original current semantic checker and guarded typed-index/type assets; semantic memo/IR admission remains closed.'
             if semantic else 'Guarded original lexer/parser and editing syntax; semantic/IR admission remains closed.'
             if syntax else __doc__)
    parser = argparse.ArgumentParser(description=scope)
    parser.add_argument('--frontend-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', default=shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe')
    args = parser.parse_args()
    frontend, output, cc = args.frontend_root.resolve(strict=True), args.output.resolve(), Path(args.cc).resolve(strict=True)
    output.mkdir(parents=True, exist_ok=False)
    commands, runs, outputs, inputs, consumers = [], [], {}, {}, []
    report = dict(status='running', accepted=False, scope=scope, commands=commands,
                  runs=runs, outputs=outputs, inputs=inputs, astConsumers=consumers)
    started = time.monotonic()

    def publish():
        (output/'build-result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8', newline='\n')

    env = dict(os.environ)
    env['PATH'] = str(cc.parent)+os.pathsep+env.get('PATH', '')

    def run(argv, name, timeout=240):
        commands.append(argv)
        try:
            result = subprocess.run(argv, cwd=ide, env=env, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            (output/(name+'.log')).write_bytes((error.stdout or b'')+(error.stderr or b''))
            raise
        (output/(name+'.log')).write_bytes(result.stdout+result.stderr)
        publish()
        if result.returncode:
            raise RuntimeError(f'{name} failed ({result.returncode}); see {output/(name+".log")}')
        return result

    try:
        _, ast, overlay = derive(frontend, output)
        report['sourceOverlay'] = overlay
        tests = ['analysis_ast_intern_test', 'analysis_ast_leaf_test',
                 'analysis_ast_tree_test', 'analysis_ast_leaf_failure_test']
        if overlay['astPackageFields']:
            tests += ['analysis_ast_package_test', 'analysis_ast_package_fault_test']
        report['tests'] = tests
        core_sources = [ide/'src/analysis/runtime.c', ast, *[
            ide/'src/analysis'/name for name in
            ('ast_storage.c', 'ast_clone.c', 'ast_intern.c', 'ast_materialize.c')]]
        adapters = [Path(__file__).with_name('native_ast_overlay.py'),
                    Path(__file__).with_name('native_raw_body_overlay.py')]
        shared_sources = [ast, ide/'src/analysis/runtime.c']
        references = []
        fixture_support = []
        if syntax:
            from native_signature_overlay import parser as derive_parser, lexer as derive_lexer
            from native_parser_ownership_overlay import derive as derive_ownership
            overlays = [derive_parser(frontend, output), derive_lexer(frontend, output),
                        derive_ownership(frontend, output)]
            report['parserOverlays'] = [row[2] for row in overlays]
            shared_sources += [row[1] for row in overlays]+[frontend/name for name in
                ('src/lexer/xl6_token.c', 'src/lexer/xl6_lexer_api.c',
                 'src/diag/xl6_diag.c', 'src/common/xl6_output.c',
                 'src/common/xl6_console.c', 'src/sem/xl6_completion.c')]
            core_sources += shared_sources[2:]+[ide/'src/analysis/text.c', ide/'src/analysis/syntax.c']
            tests += ['analysis_cursor_syntax_test', 'analysis_syntax_storage_test',
                      'analysis_call_context_test', 'analysis_parser_profile_test']
            adapters += [Path(__file__).with_name('native_signature_overlay.py'),
                         Path(__file__).with_name('native_parser_ownership_overlay.py'),
                         Path(__file__).with_name('build_lsp_syntax.py')]
            # Independent original API and Lemon tables/actions. No projection
            # adapter or discarded-stack cleanup in these reference objects.
            reference = output/'reference'
            reference.mkdir()
            for rel in ('src/parser/xl6_parser_api.c', 'src/parser/xl6_parser.c'):
                target = reference/('reference_'+Path(rel).name)
                target.write_bytes((frontend/rel).read_bytes())
                references.append(target)
            report['referenceSources'] = {str(p): str(frontend/'src/parser'/p.name.removeprefix('reference_'))
                                          for p in references}
            fixture_support = [ide/'tests/analysis_syntax_link_fence.c']
            for p in [frontend/'src/parser/xl6_parser.y', *[row[0] for row in overlays]]:
                inputs[str(p)] = sha(p)
        if semantic:
            from native_index_overlay import index as derive_index, types as derive_types
            index_assets = [derive_index(frontend, output), derive_types(frontend, output)]
            report['indexOverlays'] = [row[2] for row in index_assets]
            semantic_sources = [frontend/name for name in
                ('src/sem/xl6_sem.c', 'src/sem/xl6_type_registry.c', 'src/sem/xl6_protocol.c',
                 'src/sem/xl6_sem_docs.c', 'src/sem/xl6_sem_field_attrs.c',
                 'src/stdlib/xl6_stdlib.c', 'src/stdlib/xl6_stdlib_data.c', 'src/abi/xl6_callable.c')]
            shared_sources += [row[1] for row in index_assets]+semantic_sources
            core_sources += [row[1] for row in index_assets]+semantic_sources+[ide/'src/analysis'/name
                for name in ('context.c','entity_store.c','module_store.c','module_graph.c','frontend_cancel.c')]
            tests += ['analysis_native_index_profile_test','analysis_entity_package_key_test']
            adapters += [Path(__file__).with_name('native_index_overlay.py'),
                         Path(__file__).with_name('build_lsp_semantic.py')]
            original_index = frontend/'src/sem/xl6_sem_index.c'
            reference_index = output/'reference/reference_xl6_sem_index.c'
            reference_index.write_bytes(original_index.read_bytes())
            references.append(reference_index)
            report['referenceSources'][str(reference_index)] = str(original_index)
            report['originalSemanticSources'] = {str(p):sha(p) for p in semantic_sources}
            for row in index_assets:
                inputs.update(row[2]['auditedDependencies'])
                inputs[str(row[0])] = sha(row[0])
            # A fixture-only private copy of the real serializer. Namespace
            # its defined public exports, leaving imported dependencies real.
            probe_inputs = [index_assets[0][1], *[ide/'src/analysis'/name for name in
                ('frontend_index.inc','frontend_entities.inc','frontend_entity_versions.inc')]]
            definitions = '\n'.join(p.read_text('utf8') for p in probe_inputs)
            public = sorted(set(re.findall(r'(?m)^(?!static\b)(?:const\s+)?[A-Za-z_]\w*[ \t*]+(xls\w+|xl6Sem(?:IndexJson|CBridgeMapJson))\s*\([^;{}]*\)\s*\{',definitions)))
            if len(public)<26 or not {'xlsFrontendIndexCreate','xl6SemCBridgeMapJson'}<=set(public):
                raise RuntimeError('index probe export discovery changed')
            probe = output/'frontend/xls_index_package_probe.h'
            probe.write_text(''.join('#define '+name+' xlsProbe_'+name+'\n' for name in public)+
                '#include "'+index_assets[0][1].as_posix()+'"\n'+
                ''.join('#undef '+name+'\n' for name in public),encoding='utf8',newline='\n')
            report['privateIndexProbe'] = dict(header=str(probe),namespacedExports=public)
        sources = core_sources+references+fixture_support+[ide/'tests'/(name+'.c') for name in tests]
        common = ['-std=c11', '-O2', '-ffunction-sections', '-fdata-sections',
                  '-DXRT_MODULE_ALL=', '-DXLS_COMPILER_XRT=1', '-DXRT_EXCLUDE_MEMORY_DEBUG=',
                  '-finput-charset=UTF-8', '-fexec-charset=UTF-8',
                  '-I'+str(output/'frontend'), '-I'+str(ide/'src'), '-I'+str(frontend),
                  '-I'+str(frontend/'lib/xrt'), '-include', str(frontend/'lib/xrt/xrt.h')]
        if os.name != 'nt':
            common += ['-D_GNU_SOURCE=1']
        if semantic:
            common += ['-DXL6_INDEX_BYTES=33554432u']
        strict = ['-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter']
        shared = ['-Wall', '-Wno-pointer-sign', '-Wno-unused-function', '-Wno-unused-variable',
                  '-Wno-unused-but-set-variable', '-Wno-format-truncation', '-Wno-dangling-pointer']
        reference_flags = ['-D'+name+'=xlsReference'+name.removeprefix('xl6') for name in
            ('xl6ParseSource', 'xl6ParseSourceWithCompletion', 'xl6ParseFile',
             'xl6ParseResultInit', 'xl6ParseResultFree', 'xl6ParseAlloc', 'xl6Parse', 'xl6ParseFree')]
        reference_flags += ['-D'+name+'=xlsReference'+name.removeprefix('xl6') for name in
            ('xl6ParseTrace', 'xl6ParseInit', 'xl6ParseFinalize', 'xl6ParseStackPeak',
             'xl6ParseCoverage', 'xl6ParseFallback')]
        reference_flags += ['-D'+name+'=xlsReference'+name.removeprefix('xl6') for name in
            ('xl6SemIndexJson','xl6SemCBridgeMapJson')]
        def flags(source):
            preset = common
            if source.stem=='reference_xl6_sem_index':
                # Original JSON keeps its own literal 8 MiB budget; only the
                # guarded native collector takes the configured typed budget.
                preset = [flag for flag in common if not flag.startswith('-DXL6_INDEX_BYTES=')]
            return preset+(shared if source in shared_sources or source in references else strict)+(
                reference_flags if source in references else [])
        profiles = []
        report['runtimeProfiles'] = profiles
        for source in (ide/'src/analysis/runtime.c', ast, ide/'src/analysis/ast_clone.c'):
            result = run([str(cc), *flags(source), '-dM', '-E', str(source)], source.stem+'-runtime-features')
            macros = {line.split()[1] for line in result.stdout.decode('utf8').splitlines()
                      if line.startswith('#define ') and len(line.split()) > 1}
            package_fields = 'XLS_NATIVE_AST_PACKAGE_FIELDS' in macros
            if 'XRT_FEATURE_MEMORY_DEBUG' in macros or not {'XRT_MODULE_ALL', 'XRT_FEATURE_VALUE'} <= macros:
                raise RuntimeError('inconsistent native Release XRT profile: '+str(source))
            if source != ide/'src/analysis/runtime.c' and package_fields != overlay['astPackageFields']:
                raise RuntimeError('inconsistent native AST field schema: '+str(source))
            profiles.append(dict(source=str(source), memoryDebug=False, astPackageFields=package_fields,
                                 macroSha256=hashlib.sha256(result.stdout).hexdigest()))
        for p in (Path(__file__), *adapters,
                  Path(__file__).with_name('build_lsp_core.py'), frontend/'src/ast/xl6_ast.c',
                  frontend/'src/ast/xl6_ast.h', cc):
            inputs[str(p)] = sha(p)
        for source in sources:
            result = run([str(cc), *flags(source), '-MM', '-MT', 'xlsObject', str(source)], source.stem+'-deps')
            closure = dependencies(result.stdout.decode('utf8'))
            ast_headers = {p for p in closure if p.name == 'xl6_ast.h'}
            if ast_headers:
                if ast_headers != {Path(overlay['compiledHeader']).resolve()}:
                    raise RuntimeError('mixed AST ownership layout: '+str(source))
                consumers.append(str(source))
            for p in closure:
                digest = sha(p)
                if str(p) in inputs and inputs[str(p)] != digest:
                    raise RuntimeError('source changed during discovery: '+str(p))
                inputs[str(p)] = digest
        objects = {}
        for source in sources:
            target = output/(source.stem+'.o')
            run([str(cc), *flags(source), '-c', str(source), '-o', str(target)], source.stem)
            objects[source.stem] = target
        ar = cc.with_name('ar.exe' if os.name == 'nt' else 'ar')
        archive = output/('xlang-semantic-substrate.a' if semantic else 'xlang-syntax-core.a' if syntax else 'xlang-ast-core.a')
        run([str(ar), 'rcs', str(archive), *[str(objects[p.stem]) for p in core_sources]], 'archive')
        libraries = ['-lm']
        if os.name == 'nt':
            libraries += ['-lws2_32', '-lwinhttp', '-liphlpapi', '-lgdi32', '-luser32',
                          '-lshell32', '-lole32', '-lwinmm', '-lavrt', '-lversion', '-lbcrypt', '-ladvapi32']
        else:
            libraries += ['-pthread', '-ldl']
        for name in tests:
            program = output/(name+('.exe' if os.name == 'nt' else ''))
            extra = [str(objects[p.stem]) for p in fixture_support]
            if name=='analysis_syntax_storage_test':
                extra += ['-Wl,--wrap=xrtCalloc', '-Wl,--wrap=xrtStrDupN']
            if name=='analysis_parser_profile_test':
                extra += [str(objects[p.stem]) for p in references if p.stem!='reference_xl6_sem_index']
            if name=='analysis_native_index_profile_test':
                extra += [str(objects['reference_xl6_sem_index'])]
            run([str(cc), str(objects[name]), *extra, str(archive), '-Wl,--gc-sections', *libraries, '-o', str(program)], name+'-link')
            run([str(program)], name+'-run')
            runs.append(dict(name=name, status='passed', program=str(program)))
            outputs[program.name] = sha(program)
        if any(sha(p) != h for p, h in inputs.items()):
            raise RuntimeError('source or compiler changed during build')
        outputs[archive.name] = sha(archive)
        report['status'] = 'passed'
        print(json.dumps(dict(status='passed', packageFields=overlay['astPackageFields'],
                             fixtures=len(runs), report=str(output/'build-result.json'))), flush=True)
    except BaseException as error:
        report.update(status='failed', error=repr(error))
        raise
    finally:
        report['durationSeconds'] = time.monotonic()-started
        publish()


if __name__ == '__main__':
    main()
