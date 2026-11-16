"""Actual nested compiler, owned host callbacks and GL procedure-name views.

The standalone outer program embeds its compiler. Its runtime work directory
contains the explicit OS fixture header only, with source packages supplied
separately. No compiler/runtime DLL/DSO may be imported by that program.
Real bounded local WGL/EGL graphics; no network or hardware benchmark.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'packages', 'stage-report',
                 'public-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    a = parser.parse_args()
    root, compiler, build, packages, stage, public, out = (p.resolve() for p in
        (a.root, a.compiler, a.compiler_report, a.packages, a.stage_report, a.public_report, a.out))
    metadata = verify_full_compiler(build, compiler)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    win = os.name == 'nt'
    receipt = json.loads(stage.read_text(encoding='utf8'))
    assert receipt['complete'] and not receipt['shippingDelivery']
    assert all(sha(p) == h for p, h in receipt['inputs'].items())
    assert all(sha(packages / n) == h for n, h in receipt['outputs'].items())
    parent = json.loads(public.read_text(encoding='utf8'))
    assert parent['complete'] and parent['passed'] and parent['inputsUnchanged']
    assert all(sha(p) == h for p, h in parent['inputs'].items())
    fixture = public.parent / ('probe.dll' if win else 'probe.so')
    identity = next(p for p in parent['programs'] if p['fixture'])
    assert sha(fixture) == identity['sha256']
    assert not any('xge' in p or p in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so')
                   for p in identity['imports'])
    source = root / 'test/cases/foreign_xge3_host_nested.xl'
    inner = root / 'test/cases/foreign_xge3_host.xl'
    header = root / 'test/cases/foreign_xge3_host_probe.h'
    objdump = Path(shutil.which('objdump')).resolve()
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
        source, inner, header, fixture, public, stage, build, objdump,
        *(compiler.parent / n for n in names),
        *(p for p in packages.rglob('*') if p.is_file())]
    inputs = {str(p): sha(p) for p in files}
    out.mkdir(parents=True, exist_ok=False)
    work = out / 'work'
    (work / 'test/cases').mkdir(parents=True)
    shutil.copy2(header, work / 'test/cases' / header.name)
    selected = out / 'packages'
    shutil.copytree(packages, selected)
    probe = selected / 'host_probe'
    (probe / 'native').mkdir(parents=True)
    shutil.copy2(fixture, probe / 'native' / fixture.name)
    (probe / 'probe.xl').write_text('export const int fixtureAbi=1\n', encoding='utf8')
    (probe / 'xlang.package.json').write_text(json.dumps(dict(format='xlang-package-1',
        name='host_probe', version='1.0.0', abiVersion='1', kind='xlang-extlib',
        artifact=dict(kind='source', cSources=[], xlSources=['probe.xl']),
        loadlibs=['native/' + fixture.name]), indent=2) + '\n', encoding='utf8')
    inputs.update((str(p), sha(p)) for p in [*(p for p in work.rglob('*') if p.is_file()),
        *(p for p in selected.rglob('*') if p.is_file())])
    env = {k: v for k, v in os.environ.items()
           if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(PYTHONUTF8='1', PYTHONDONTWRITEBYTECODE='1', LIBGL_ALWAYS_SOFTWARE='1',
        XLANG_PACKAGE_PATH=str(selected), XLANG_HOST_NESTED_ROOT=str(work),
        TMP=str(out), TEMP=str(out), TMPDIR=str(out))
    key = 'PATH' if win else 'LD_LIBRARY_PATH'
    standalone_path = str(selected / '_xge_native/native') + os.pathsep + env.get(key, '')
    env[key] = str(compiler.parent) + os.pathsep + str(selected / '_xge_native/native') + \
        os.pathsep + env.get(key, '')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
        xrtRevision=metadata['xrtRevision'], actualGraphics=True,
        shippingDelivery=False, networkTests=False, importedCompiler=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, marker=None, standalone=False):
        command = list(map(str, command))
        start = time.monotonic()
        execution = dict(env)
        if standalone:
            execution[key] = standalone_path
        r = subprocess.run(command, env=execution, cwd=work, capture_output=True, timeout=180)
        data = r.stdout + r.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=r.returncode,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, r.returncode, flush=True)
        assert not r.returncode and (not marker or marker in data), log
        if marker:
            assert data.count(b'PUBLIC_HOST_SESSION_REAL_GL_OK') == 2, log
        return data

    try:
        binary = out / ('nested-host.exe' if win else 'nested-host')
        run('outer-aot-build', [compiler, 'build', source, '-o', binary])
        raw = run('outer-aot-imports', [objdump, '-p', binary])
        imports = re.findall(rb'DLL Name:\s*(\S+)', raw) if win else re.findall(rb'NEEDED\s+(\S+)', raw)
        imports = [p.decode().lower() for p in imports]
        assert not any('xge' in p or p in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so',
            'libtcc.dll', 'libtcc.so') for p in imports)
        report.update(imports=imports, programSha256=sha(binary))
        for backend in (('gl',) if win else ('gl', 'gles')):
            case = out / (backend + '-inner.xl')
            text = 'import host_probe\n' + inner.read_text(encoding='utf8').replace(
                'bool customResolver=false', 'bool customResolver=true')
            if backend == 'gles':
                text = text.replace('c_int backend=xge.XGE_GPU_BACKEND_OPENGL33',
                    'c_int backend=xge.XGE_GPU_BACKEND_GLES30')
            case.write_text(text, encoding='utf8')
            inputs[str(case)] = sha(case)
            env['XLANG_HOST_NESTED_CASE'] = str(case)
            for mode, command in (('jit', [compiler, 'run', source]), ('aot', [binary])):
                temporary = out / (backend + '-' + mode + '-tmp')
                temporary.mkdir()
                env['XLANG_HOST_NESTED_TEMP'] = str(temporary)
                run(backend + '-' + mode, command, b'PUBLIC_HOST_SESSION_NESTED_OK',
                    standalone=mode == 'aot')
        verify_full_compiler(build, compiler)
        report.update(complete=True, passed=True,
            inputsUnchanged=all(sha(p) == h for p, h in inputs.items()))
        assert report['inputsUnchanged']
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
