"""Qualify shared FlowGraph declarations with a full compiler and exact SDK.

CPU-only JIT, dynamic/static executables and checked module retirement.
This does not install a release or qualify Workflow, UI or GPU operations.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from full_compiler_qualification import verify_full_compiler
from check_xge3_bindings import foreign_method_declaration, split
from stage_xge3_replay_contracts import verify_replay_package


def declaration_probes(packages):
    lines = ['import xui']
    methods = []
    for path in (packages / 'xui/src/flow_graph.xl',):
        owner = None
        for line in path.read_text(encoding='utf-8').splitlines():
            cls = re.match(r'export extern "[^"]+" class (\w+)', line)
            if cls:
                owner = cls[1]
                continue
            if line == 'end':
                owner = None
                continue
            method = foreign_method_declaration(line)
            if not method or (not owner and not line.startswith('export ')):
                continue
            native, kind, name, parameters = method
            params = ['xui.' + owner + ' receiver'] if owner else []
            args = []
            locals = []
            for i, parameter in enumerate(split(parameters)):
                ty = parameter.split('=', 1)[0].strip().rsplit(' ', 1)[0]
                ty = re.sub(r'\b(FlowGraph|FlowViewport)\b', r'xui.\1', ty)
                arg = 'arg' + str(i)
                if ty.endswith('?'):
                    locals.append(ty + ' ' + arg + '=null')
                elif ty.startswith('array<'):
                    locals.append(ty + ' ' + arg + '=[]')
                else:
                    params.append(ty + ' ' + arg)
                args.append(arg)
            expression = ('receiver.' if owner else 'xui.') + name
            if kind != 'get':
                expression += '(' + ','.join(args) + ')'
            methods.append(dict(native=native, owner=owner, name=name, kind=kind))
            lines += ['export "flow_probe_' + str(len(methods)) + '" func probe' +
                      str(len(methods)) + '(' + ','.join(params) + ')',
                      *(' ' + declaration for declaration in locals),
                      ' ' + expression, 'end']
    return '\n'.join(lines) + '\n', methods


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'packages', 'stage-report',
                 'static-packages', 'static-stage-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--case', choices=('behavior', 'oom', 'snapshot'), default='behavior')
    args = parser.parse_args()
    root, compiler, build, packages, stage, static, static_stage, out = [
        p.resolve() for p in (args.root, args.compiler, args.compiler_report,
                             args.packages, args.stage_report, args.static_packages,
                             args.static_stage_report, args.out)]
    metadata = verify_full_compiler(build, compiler)
    win = os.name == 'nt'
    cohort = Path(metadata['source'])
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    base_source = root / 'test/cases/foreign_xui_flow_graph.xl'
    source = base_source if args.case == 'behavior' else base_source.with_name(
        'foreign_xui_flow_graph_' + args.case + '.xl')
    provider = base_source.with_name('foreign_xui_flow_graph_native.c')
    tools = {name: Path(shutil.which(name)).resolve() for name in ('gcc', 'g++', 'objdump')}
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
             Path(__file__).with_name('check_xge3_bindings.py'),
             source, provider, base_source.with_suffix('.h'), compiler, build,
             stage, static_stage, cohort / 'lib/xrt/xrt.h', *tools.values()]
    for pkg, receipt in ((packages, stage), (static, static_stage)):
        files.extend(verify_replay_package(pkg,receipt))
        files.extend(p for p in pkg.rglob('*') if p.is_file())
    sdk = compiler.parent / 'include'
    files.extend(p for p in sdk.rglob('*') if p.is_file())
    files.extend(compiler.parent / n for n in
                 (('xl.dll', 'xlc.dll') if win else ('libxl.so', 'libxlc.so')))
    inputs = {str(p): sha(p) for p in files}
    out.mkdir(parents=True, exist_ok=False)
    report = dict(complete=False, passed=False, scope=__doc__,
                  fullCompilerBuild=True, shippingDelivery=False,
                  uiGpuNetworkTests=False, case=args.case, inputs=inputs, rows=[], programs=[])
    env = {k: v for k, v in os.environ.items()
           if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(XLANG_PACKAGE_PATH=str(packages),
               TMP=str(out), TEMP=str(out))
    native = packages / '_xge_native/native'
    if win:
        for path in native.glob('*.dll'):
            shutil.copy2(path, out / path.name)
    else:
        env['LD_LIBRARY_PATH'] = ':'.join((str(out), str(native),
                                          str(compiler.parent), env.get('LD_LIBRARY_PATH', '')))

    def save():
        temporary = out / 'results.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        os.replace(temporary, out / 'results.json')

    def run(name, command, marker=None):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=300)
        log = out / (name + '.log')
        data = result.stdout + result.stderr
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
                                   seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert result.returncode == 0, log
        if marker:
            assert marker in data, log
        return data

    def imports(name, path, is_static):
        data = run(name + '-imports', [tools['objdump'], '-p', path])
        names = re.findall(rb'DLL Name:\s*(\S+)', data) if win else re.findall(rb'\bNEEDED\s+(\S+)', data)
        forbidden = {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so', b'libtcc.dll', b'libtcc.so'}
        if is_static:
            forbidden.update((b'xge3_host.dll', b'libxge3_host.so'))
        assert not forbidden.intersection(n.lower() for n in names), names
        reserve=None
        if win:
            raw=path.read_bytes();offset=struct.unpack_from('<I',raw,0x3c)[0]+24
            assert struct.unpack_from('<H',raw,offset)[0]==0x20b
            reserve=struct.unpack_from('<Q',raw,offset+72)[0]
            assert reserve==1024*1024
        report['programs'].append(dict(path=str(path), bytes=path.stat().st_size,
                                       sha256=sha(path), staticNative=is_static,
                                       peStackReserve=reserve,
                                       imports=[n.decode() for n in names]))
        save()

    try:
        includes = ['--inc-path', root, '--inc-path', sdk]
        if args.case == 'behavior':
            probe = out / 'all.xl'
            text, methods = declaration_probes(packages)
            probe.write_text(text, encoding='utf-8')
            report['declarations'] = methods
            generated, header = out / 'all.c', out / 'all.h'
            run('all-emit', [compiler, 'cgen', probe, '--emit', generated,
                              '--emit-header', header, *includes])
            flags = ['-DXL_STATIC_EMBED', '-Wall', '-Wextra', '-Werror',
                     '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
                     '-Wno-unused-but-set-variable', '-Wno-unused-const-variable',
                     '-I' + str(packages / '_xge_native/include'), '-I' + str(sdk)]
            run('all-C11', [tools['gcc'], '-std=c11', *flags, '-fsyntax-only', generated])
            for standard, cc, lang in (('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++'),
                                       ('c++17', 'g++', 'c++')):
                run('header-' + standard, [tools[cc], '-std=' + standard, *flags,
                                           '-x', lang, '-fsyntax-only', header])
        marker = {'behavior': b'XUI_SHARED_FLOW_MODEL_OK', 'oom': b'XUI_SHARED_FLOW_OOM_OK',
                  'snapshot': b'XUI_FLOW_SNAPSHOT_OK'}[args.case]
        run('jit', [compiler, 'run', source, provider, *includes], marker)
        shared_source = out / 'flow-shared.xl'
        shared_source.write_text(source.read_text(encoding='utf-8').replace(
            'func main(...) as int', 'export "flow_model_check" func check() as int'), encoding='utf-8')
        host = out / 'shared-host.py'
        host.write_text('import ctypes,sys,os,_ctypes\nlib=ctypes.CDLL(sys.argv[1],**({"winmode":0} if os.name=="nt" else {}))\n'
                        'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\n'
                        'lib.xl_script_module_init.restype=ctypes.c_int\n'
                        'assert lib.xl_script_module_init(None)==1\n'
                        'lib.flow_model_check.restype=ctypes.c_int\nassert lib.flow_model_check()==0\n'
                        'lib.xl_script_module_unit_checked.restype=ctypes.c_int\n'
                        'assert lib.xl_script_module_unit_checked()==1\n'
                        'if os.name=="nt":\n lib.xl_script_runtime_retire_checked.restype=ctypes.c_int\n assert lib.xl_script_runtime_retire_checked()==1\n'
                        '(_ctypes.FreeLibrary if os.name=="nt" else _ctypes.dlclose)(lib._handle)\nprint("FLOW_SHARED_UNLOADED")\n', encoding='utf-8')
        for kind, selected in (('dynamic', packages), ('static', static)):
            env['XLANG_PACKAGE_PATH'] = str(selected)
            binary = out / (kind + ('.exe' if win else '.out'))
            run(kind + '-exe-build', [compiler, 'build', source, provider, '-o', binary, *includes])
            run(kind + '-exe-run', [binary], marker)
            imports(kind + '-exe', binary, kind == 'static')
            library = out / (kind + ('.dll' if win else '.so'))
            run(kind + '-shared-build', [compiler, 'build', shared_source, provider,
                                         '--shared', '-o', library, *includes])
            data=run(kind + '-shared-run', [sys.executable, '-B', host, library], marker)
            assert b'FLOW_SHARED_UNLOADED' in data
            imports(kind + '-shared', library, kind == 'static')
        report.update(complete=True, passed=True,
                      inputsUnchanged=all(sha(p) == value for p, value in inputs.items()))
        assert report['inputsUnchanged']
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
