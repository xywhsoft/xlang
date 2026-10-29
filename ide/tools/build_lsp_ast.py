"""Build/test the guarded native AST substrate without admitting a new frontend.

Use a committed frontend snapshot. Parser, semantic and IR guards in the full
server build remain independent; this target does not publish a server or IDE.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time

from build_lsp_core import dependencies
from native_ast_overlay import derive


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    ide = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', default=shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe')
    args = parser.parse_args()
    frontend, output, cc = args.frontend_root.resolve(strict=True), args.output.resolve(), Path(args.cc).resolve(strict=True)
    output.mkdir(parents=True, exist_ok=False)
    commands, runs, outputs, inputs, consumers = [], [], {}, {}, []
    report = dict(status='running', accepted=False, scope=__doc__, commands=commands,
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
        sources = core_sources+[ide/'tests'/(name+'.c') for name in tests]
        common = ['-std=c11', '-O2', '-ffunction-sections', '-fdata-sections',
                  '-DXRT_MODULE_ALL=', '-DXLS_COMPILER_XRT=1', '-DXRT_EXCLUDE_MEMORY_DEBUG=',
                  '-finput-charset=UTF-8', '-fexec-charset=UTF-8',
                  '-I'+str(output/'frontend'), '-I'+str(ide/'src'), '-I'+str(frontend),
                  '-I'+str(frontend/'lib/xrt'), '-include', str(frontend/'lib/xrt/xrt.h')]
        if os.name != 'nt':
            common += ['-D_GNU_SOURCE=1']
        strict = ['-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter']
        shared = ['-Wall', '-Wno-pointer-sign', '-Wno-unused-function', '-Wno-unused-variable',
                  '-Wno-unused-but-set-variable', '-Wno-format-truncation', '-Wno-dangling-pointer']
        flags = lambda source: common+(shared if source in (ast, ide/'src/analysis/runtime.c') else strict)
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
        for p in (Path(__file__), Path(__file__).with_name('native_ast_overlay.py'),
                  Path(__file__).with_name('native_raw_body_overlay.py'),
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
        archive = output/'xlang-ast-core.a'
        run([str(ar), 'rcs', str(archive), *[str(objects[p.stem]) for p in core_sources]], 'archive')
        libraries = ['-lm']
        if os.name == 'nt':
            libraries += ['-lws2_32', '-lwinhttp', '-liphlpapi', '-lgdi32', '-luser32',
                          '-lshell32', '-lole32', '-lwinmm', '-lavrt', '-lversion', '-lbcrypt', '-ladvapi32']
        else:
            libraries += ['-pthread', '-ldl']
        for name in tests:
            program = output/(name+('.exe' if os.name == 'nt' else ''))
            run([str(cc), str(objects[name]), str(archive), '-Wl,--gc-sections', *libraries, '-o', str(program)], name+'-link')
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
