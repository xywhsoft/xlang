"""Qualify six Chart queries preserving native flags and pointer outputs.

Deterministic CPU backend, exact C O0/O2 oracles, JIT/dynamic/static AOT and
C/C++ headers. Measures XRT heap operations only, not CPU equivalence.
No window, graphics device, network or release installation is involved.
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
from full_compiler_qualification import verify_full_compiler
from compiler_response import command_with_response
from stage_xge3_replay_contracts import verify_replay_package


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'native-root', 'engine',
                 'static-build', 'dynamic-packages', 'static-packages', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--dynamic-replay-report',type=Path)
    p.add_argument('--static-replay-report',type=Path)
    a = p.parse_args()
    root, compiler, build, native, engine, static_build, dynamic, static, out = (
        v.resolve() for v in (a.root, a.compiler, a.compiler_report, a.native_root,
            a.engine, a.static_build, a.dynamic_packages, a.static_packages, a.out))
    verify_full_compiler(build, compiler)
    win = os.name == 'nt'
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    sdk = compiler.parent / 'include'
    runtime = compiler.parent / ('xlc.dll' if win else 'libxlc.so')
    host_report = json.loads((engine / 'build-report.json').read_text(encoding='utf8'))
    static_report = json.loads((static_build / 'build-report.json').read_text(encoding='utf8'))
    assert host_report['linked'] and host_report['xrt_mode'] == 'host'
    assert static_report['passed'] and static_report['sourceUnchanged']
    assert static_report['xrt_mode'] == 'direct-external'
    image = engine / host_report['runtime_image']
    library = engine / host_report['import_library'] if win else image
    archive = static_build / static_report['archive']
    case = root / 'test/cases/foreign_xui_chart_queries.xl'
    fixture, header = case.with_suffix('.c'), case.with_suffix('.h')
    driver = case.with_name(case.stem + '_native.c')
    host = root / 'test/cases/foreign_decl_module_host.c'
    tools = {name: Path(shutil.which(name)).resolve() for name in ('gcc', 'g++', 'objdump')}
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
        Path(__file__).with_name('compiler_response.py'), case, fixture, header, driver,
        host, compiler, runtime, build, image, library, archive, *tools.values(),
        engine / 'build-report.json', static_build / 'build-report.json',
        engine / 'xge_build_config.h', static_build / 'xge_build_config.h',
        root / 'test/cases/foreign_xui_render_backend.c',
        root / 'test/cases/foreign_xui_render_backend.h',
        root / 'test/cases/foreign_xui_render_adapters.inc',
        native / 'tests/xui/xui_test_proxy.c', native / 'tests/xui/xui_test_proxy.h']
    for folder in (sdk, dynamic, static):
        files.extend(f for f in folder.rglob('*') if f.is_file())
    for packages,replay in ((dynamic,a.dynamic_replay_report),(static,a.static_replay_report)):
        if replay is not None:
            files.extend(verify_replay_package(packages,replay))
            continue
        receipt = packages.with_suffix('.json')
        data = json.loads(receipt.read_text(encoding='utf8'))
        assert data['complete'] and not data['shippingDelivery']
        assert len(data['changes']) == 6
        assert all(sha(packages / n) == h for n, h in data['outputs'].items())
        files.append(receipt)
    inputs = {str(f): sha(f) for f in files}
    out.mkdir(parents=True, exist_ok=False)
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs,
        rows=[], programs=[], fullCompilerBuild=True, shippingDelivery=False,
        networkTests=False, graphicsTests=False)

    def save():
        temporary = out / 'results.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        os.replace(temporary, out / 'results.json')

    def run(name, command, packages=dynamic, marker=None):
        env = {k: v for k, v in os.environ.items()
            if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages), TMP=str(out), TEMP=str(out), TMPDIR=str(out))
        key = 'PATH' if win else 'LD_LIBRARY_PATH'
        env[key] = os.pathsep.join((str(packages / '_xge_native/native'),
            str(engine), str(compiler.parent), env.get(key, '')))
        command = list(map(str, command))
        actual = command_with_response(command, tools['gcc'], out / (name + '.rsp'))
        started = time.monotonic()
        result = subprocess.run(actual, cwd=root, env=env, capture_output=True, timeout=300)
        data = result.stdout + result.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
            seconds=time.monotonic() - started, logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert not result.returncode, log
        if marker:
            assert marker in data, log
        return data

    def check_imports(name, program, host_image):
        data = run(name + '-imports', [tools['objdump'], '-p', program])
        imported = re.findall(rb'DLL Name:\s*(\S+)', data) if win else re.findall(rb'\bNEEDED\s+(\S+)', data)
        assert not {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so',
            b'libtcc.dll', b'libtcc.so'}.intersection(n.lower() for n in imported)
        assert any(b'xge3_host' in n.lower() for n in imported) == host_image
        report['programs'].append(dict(path=str(program), bytes=program.stat().st_size,
            sha256=sha(program), imports=[n.decode() for n in imported]))
        save()

    try:
        includes = ['--inc-path', root, '--inc-path', sdk, '--inc-path', native]
        ci = ['-I' + str(f) for f in (root, sdk, native, native / 'include',
            *sorted(dynamic.glob('*/include')))]
        for kind, folder, link in (('host', engine, library), ('static', static_build, archive)):
            flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-cast-function-type',
                '-DXGE_XRT_HOST=1', '-include', folder / 'xge_build_config.h', *ci,
                '-I' + str(folder), '-I' + str(engine / 'host-xrt')]
            flags += ['-DXGE_TEST_STATIC=1', '-DXL6_XGE_STATIC=1'] if kind == 'static' else (
                ['-DXGE_DLL=1', '-DXUI_DLL=1'] if win else [])
            systems = ['-l' + n for n in static_report['system_libraries']] if kind == 'static' else []
            if not win:
                systems += ['-Wl,-rpath,' + str(engine), '-Wl,-rpath,' + str(runtime.parent),
                    '-ldl', '-lm', '-pthread']
            for opt in ('O0', 'O2'):
                name = 'native-' + kind + '-' + opt
                binary = out / (name + ('.exe' if win else ''))
                run(name + '-build', [tools['gcc'], *flags, '-' + opt, driver,
                    fixture, link, runtime, *systems, '-o', binary])
                run(name + '-run', [binary], marker=b'XUI_CHART_NATIVE_OK')
        generated, public = out / 'generated.c', out / 'public.h'
        run('emit', [compiler, 'cgen', case, '--emit', generated, '--emit-header', public, *includes])
        text = generated.read_text(encoding='utf-8-sig')
        for suffix in ('ViewRange', 'BrushRange', 'Animation', 'SeriesAreaFill', 'SeriesValueRadius', 'SeriesValueColor'):
            assert re.search(r'\.e0\s*=\s*xuiChartGet' + suffix + r'\(', text)
        for standard, tool, language in (('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++'), ('c++17', 'g++', 'c++')):
            run('header-' + standard, [tools[tool], '-std=' + standard,
                '-Wall', '-Wextra', '-Werror', *ci, '-x', language, '-fsyntax-only', public])
        for kind, packages in (('jit', dynamic), ('dynamic', dynamic), ('static', static)):
            if kind == 'jit':
                data = run('jit', [compiler, 'run', case, *includes], packages)
            else:
                binary = out / (kind + ('.exe' if win else ''))
                run(kind + '-build', [compiler, 'build', case, '-o', binary, *includes], packages)
                data = run(kind + '-run', [binary], packages)
                check_imports(kind, binary, kind == 'dynamic')
            assert b'XUI_CHART_QUERIES_OK' in data
            assert data.count(b'calls=6000 alloc=0 free=0 realloc=0') == 2
        verify_full_compiler(build, compiler)
        report.update(complete=True, inputsUnchanged=all(sha(f) == h for f, h in inputs.items()))
        report['passed'] = report['inputsUnchanged']
        assert report['passed']
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
