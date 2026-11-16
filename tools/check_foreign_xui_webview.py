"""Qualify WebView owned events and checked zoom output with an exact SDK.

CPU-only JIT, dynamic/static executables and checked module retirement.
Close events use the real native SDK with a CPU proxy. Browser initialization,
positive browser zoom, GPU rendering and provider unloading are separate gates.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from full_compiler_qualification import verify_full_compiler


def declaration_probes(packages):
    text = (packages / 'xui/src/managed.xl').read_text(encoding='utf-8')
    declarations = [line.strip() for line in text.splitlines()
                    if 'extern "xuiWebViewSetEventOwned"' in line or
                    'extern "xuiWebViewGetZoomFactor"' in line]
    assert len(declarations) == 2
    return ('import xui\n'
            'export "webview_set" func probeSet(xui.WebView receiver)\n'
            ' receiver.setEvent(null)\nend\n'
            'export "webview_zoom" func probeZoom(xui.WebView receiver) as c_double\n'
            ' return receiver.getZoomFactor()\nend\n'), declarations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'packages', 'stage-report',
                 'static-packages', 'static-stage-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--case', choices=('behavior', 'oom'), default='behavior')
    args = parser.parse_args()
    root, compiler, build, packages, stage, static, static_stage, out = [
        p.resolve() for p in (args.root, args.compiler, args.compiler_report,
                             args.packages, args.stage_report, args.static_packages,
                             args.static_stage_report, args.out)]
    metadata = verify_full_compiler(build, compiler)
    win = os.name == 'nt'
    cohort = Path(metadata['source'])
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    base_source = root / 'test/cases/foreign_xui_webview.xl'
    source = base_source if args.case == 'behavior' else base_source.with_name('foreign_xui_webview_oom.xl')
    provider = base_source.with_name('foreign_xui_webview_native.c')
    tools = {name: Path(shutil.which(name)).resolve() for name in ('gcc', 'g++', 'objdump')}
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
             source, provider, compiler, build,
             stage, static_stage, cohort / 'lib/xrt/xrt.h', *tools.values()]
    files.extend(p for p in root.rglob('*') if p.is_file())
    for pkg, receipt in ((packages, stage), (static, static_stage)):
        record = json.loads(receipt.read_text(encoding='utf-8-sig'))
        assert record['complete'] and record.get('passed', record.get('inputsUnchanged'))
        assert all(sha(p) == value for p, value in record['inputs'].items())
        assert all(sha(pkg / p) == value for p, value in record['outputs'].items())
        core = {value for p, value in record['inputs'].items()
                if p.replace('\\', '/').endswith('/lib/xrt/xrt.h')}
        assert core == {sha(cohort / 'lib/xrt/xrt.h')}
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
    env.update(XLANG_PACKAGE_PATH=str(packages), XL6_SOURCE_NATIVE_ROOT=str(cohort),
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
        report['programs'].append(dict(path=str(path), bytes=path.stat().st_size,
                                       sha256=sha(path), staticNative=is_static,
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
        marker = b'XUI_WEBVIEW_CALLBACK_OK' if args.case == 'behavior' else b'XUI_WEBVIEW_OOM_OK'
        run('jit', [compiler, 'run', source, provider, *includes], marker)
        shared_source = out / 'webview-shared.xl'
        shared_source.write_text(source.read_text(encoding='utf-8').replace(
            'func main(...) as int', 'export "webview_check" func check() as int'), encoding='utf-8')
        host = out / 'shared-host.py'
        host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\n'
                        'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\n'
                        'lib.xl_script_module_init.restype=ctypes.c_int\n'
                        'assert lib.xl_script_module_init(None)==1\n'
                        'lib.webview_check.restype=ctypes.c_int\nassert lib.webview_check()==0\n'
                        'lib.xl_script_module_unit_checked.restype=ctypes.c_int\n'
                        'assert lib.xl_script_module_unit_checked()==1\n', encoding='utf-8')
        for kind, selected in (('dynamic', packages), ('static', static)):
            env['XLANG_PACKAGE_PATH'] = str(selected)
            binary = out / (kind + ('.exe' if win else '.out'))
            run(kind + '-exe-build', [compiler, 'build', source, provider, '-o', binary, *includes])
            run(kind + '-exe-run', [binary], marker)
            imports(kind + '-exe', binary, kind == 'static')
            library = out / (kind + ('.dll' if win else '.so'))
            run(kind + '-shared-build', [compiler, 'build', shared_source, provider,
                                         '--shared', '-o', library, *includes])
            run(kind + '-shared-run', [sys.executable, '-B', host, library], marker)
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
