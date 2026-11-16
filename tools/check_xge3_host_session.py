"""Public-package explicit host rendering: unique-XRT JIT/dynamic/static/shared.

Real bounded local WGL/EGL contexts and pixels, with system/custom GL resolvers;
no network or hardware benchmark.
The OS-only fixture imports neither XRT nor a compiler. Acyclic shared products
undergo checked cleanup, terminal runtime retirement and actual OS unload.
This gate does not claim native/language cycle collection, cold OS TLS failure
coverage, nested compiler or a shipping SDK/IDE installation.
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


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for n in ('root', 'compiler', 'compiler-report', 'dynamic-packages',
              'static-packages', 'dynamic-report', 'static-report', 'out'):
        p.add_argument('--' + n, type=Path, required=True)
    a = p.parse_args()
    root, compiler, build, dynamic, static, dynamic_report, static_report, out = [
        v.resolve() for v in (a.root, a.compiler, a.compiler_report, a.dynamic_packages,
            a.static_packages, a.dynamic_report, a.static_report, a.out)]
    metadata = verify_full_compiler(build, compiler)
    snapshot = Path(metadata['source'])
    win = os.name == 'nt'
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    tools = {n: Path(shutil.which(n)).resolve() for n in ('gcc', 'ar', 'objdump')}
    source = root / 'test/cases/foreign_xge3_host.xl'
    native = root / 'test/cases/foreign_xge3_host_probe.c'
    header = native.with_suffix('.h')
    host = root / 'test/cases/foreign_xge3_host_shared.py'
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
        compiler, build, source, native, header, host, *tools.values(), dynamic_report, static_report,
        snapshot / 'tools/stage_runtime_headers.py']
    images = ('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')
    files += [compiler.parent / n for n in images]
    for folder, receipt in ((dynamic, dynamic_report), (static, static_report)):
        stage = json.loads(receipt.read_text(encoding='utf8'))
        assert stage['complete'] and not stage['shippingDelivery']
        assert stage['inputs'] and all(sha(n) == h for n, h in stage['inputs'].items())
        assert stage['outputs'] and all(sha(folder / n) == h for n, h in stage['outputs'].items())
        files += [v for v in folder.rglob('*') if v.is_file()]
        for n in ('xge/src/backend.xl', 'xge/include/xge3_backend.h', 'xge/xlang.package.json'):
            assert sha(folder / n) == sha(root / 'lib/bindings' / n), n
    inputs = {str(v.resolve()): sha(v) for v in files}
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(TMP=str(out), TEMP=str(out), TMPDIR=str(out), PYTHONUTF8='1',
        PYTHONDONTWRITEBYTECODE='1', LIBGL_ALWAYS_SOFTWARE='1')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
        programs=[], networkTests=False, shippingDelivery=False, actualGraphics=True,
        fixtureContainsXrt=False, hardwarePerformance=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, marker=None, packages=None, cwd=None):
        execution = dict(env)
        if packages:
            execution['XLANG_PACKAGE_PATH'] = str(packages)
        key = 'PATH' if win else 'LD_LIBRARY_PATH'
        execution[key] = str(compiler.parent) + os.pathsep + str(out) + os.pathsep + \
            str(dynamic / '_xge_native/native') + os.pathsep + execution.get(key, '')
        start = time.monotonic()
        command = list(map(str, command))
        result = subprocess.run(command, cwd=cwd or out, env=execution, capture_output=True, timeout=180)
        log = out / (name + '.log')
        data = result.stdout + result.stderr
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert not result.returncode, log
        if marker:
            assert marker in data, log
        return data

    def inspect(name, binary, fixture=False, statically_linked=False):
        raw = run(name + '-imports', [tools['objdump'], '-p', binary])
        names = re.findall(rb'DLL Name:\s*(\S+)', raw) if win else re.findall(rb'NEEDED\s+(\S+)', raw)
        names = [v.decode().lower() for v in names]
        assert not set(names) & {'xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so', 'libtcc.dll', 'libtcc.so'}
        if fixture or statically_linked:
            assert not any('xge' in n for n in names)
        report['programs'].append(dict(path=str(binary), sha256=sha(binary), imports=names,
            fixture=fixture, staticallyLinked=statically_linked))
        save()

    run('private-headers', [sys.executable, '-B', snapshot / 'tools/stage_runtime_headers.py',
        '--root', snapshot, '--out', out / 'sdk'])
    includes = out / 'include'
    shutil.copytree(compiler.parent / 'include', includes)
    shutil.copytree(out / 'sdk/include', includes, dirs_exist_ok=True)
    inputs.update((str(v), sha(v)) for v in includes.rglob('*') if v.is_file())
    # The CLI's own disk SDK precedes additional include paths. Assemble the
    # exact source-matched private headers beside byte-identical verified
    # binaries, without replacing the immutable build's older disk SDK.
    execution = out / 'runtime'
    execution.mkdir()
    shutil.copytree(includes, execution / 'include')
    for n in images:
        shutil.copy2(compiler.parent / n, execution / n)
    compiler = execution / compiler.name
    verify_full_compiler(build, compiler)
    inputs.update((str(v), sha(v)) for v in execution.rglob('*') if v.is_file())
    common = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-cast-function-type',
        '-ffunction-sections', '-fdata-sections', '-I' + str(root / 'test/cases'),
        '-I' + str(dynamic / '_xge_native/include'), '-I' + str(includes)]
    if not win:
        common += ['-fPIC']
    obj = out / 'probe.o'
    run('fixture-object', [tools['gcc'], *common, '-DXL6_TEST_HOST_PROBE_DLL', '-c', native, '-o', obj])
    archive = out / 'probe.a'
    run('fixture-archive', [tools['ar'], 'rcs', archive, obj])
    fixture = out / ('probe.dll' if win else 'probe.so')
    systems = ['-lopengl32', '-lgdi32', '-luser32'] if win else ['-lEGL', '-pthread']
    run('fixture-shared', [tools['gcc'], '-shared', obj, *systems, '-o', fixture])
    inspect('fixture', fixture, fixture=True)
    inc = ['--inc-path', root, '--inc-path', includes]
    static_fixture = ['--link', archive, '--link', 'opengl32' if win else 'EGL']
    variants = [('gl', False, False), ('gl-retry', False, True)]
    if not win:
        variants += [('gles', True, False), ('gles-retry', True, True)]
    variants += [(name + '-custom', es, defer) for name, es, defer in list(variants)]
    for name, es, defer in variants:
        text = source.read_text(encoding='utf8')
        if es:
            text = text.replace('c_int backend=xge.XGE_GPU_BACKEND_OPENGL33',
                'c_int backend=xge.XGE_GPU_BACKEND_GLES30')
        if defer:
            text = text.replace('bool deferredRelease=false', 'bool deferredRelease=true')
        if name.endswith('-custom'):
            text = text.replace('bool customResolver=false', 'bool customResolver=true')
        case = out / (name + '.xl')
        case.write_text(text, encoding='utf8')
        inputs[str(case)] = sha(case)
        if name in ('gl', 'gl-custom'):
            generated = out / (name + '-emitted.c')
            run(name + '-public-cgen', [compiler, 'cgen', case, '--emit', generated, *inc], packages=dynamic)
            emitted = generated.read_text(encoding='utf8')
            for symbol in ('xgePlatformBackendCreateOwned', 'xgeGraphicsBackendCreateOwned',
                'xgeHostSessionCreateOwned', 'xgeHostSessionBegin', 'xgeHostSessionEnd', 'xgeHostSessionPresent'):
                assert re.search(r'\b' + symbol + r'\(', emitted), symbol
        run(name + '-jit', [compiler, 'run', case, *inc, '--loadlib', fixture],
            b'PUBLIC_HOST_SESSION_REAL_GL_OK', dynamic)
        for label, packages in (('dynamic', dynamic), ('static', static)):
            binary = out / (name + '-' + label + ('.exe' if win else ''))
            native_flags = ['--loadlib', fixture] if label == 'dynamic' else static_fixture
            run(name + '-' + label + '-build', [compiler, 'build', case, *inc,
                '--native-linkage', label, *native_flags, '-o', binary], packages=packages)
            run(name + '-' + label + '-run', [binary], b'PUBLIC_HOST_SESSION_REAL_GL_OK')
            inspect(name + '-' + label, binary, statically_linked=label == 'static')
    # Each shared image runs in its own process/runtime, avoiding cross-binding
    # distinct embedded XRTs to one already-bound hosted engine image.
    for custom in (False, True):
        suffix = '-custom' if custom else ''
        shared_source = out / ('shared-check' + suffix + '.xl')
        text = source.read_text(encoding='utf8').replace('func main(...) as int',
            'export "host_check" func check() as int')
        if custom:
            text = text.replace('bool customResolver=false', 'bool customResolver=true')
        shared_source.write_text(text, encoding='utf8')
        inputs[str(shared_source)] = sha(shared_source)
        for label, packages in (('dynamic', dynamic), ('static', static)):
            name = label + suffix + '-shared'
            library = out / ('host-' + label + suffix + ('.dll' if win else '.so'))
            run(name + '-build', [compiler, 'build', shared_source, *inc,
                '--native-linkage', label, *static_fixture, '--shared', '-o', library], packages=packages)
            run(name + '-run', [sys.executable, '-B', host, library,
                dynamic / '_xge_native/native'], b'HOST_SHARED_PHYSICAL_UNLOAD_OK')
            inspect(name, library, statically_linked=label == 'static')
    verify_full_compiler(build, compiler)
    report.update(complete=True, passed=True, inputsUnchanged=all(sha(v) == h for v, h in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
