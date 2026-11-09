"""Acceptance of direct XGE render-resource declarations (CPU by default).

Requires independently qualified compiler, dynamic packages and static packages.
Only explicit --graphics-case inputs open a bounded graphics context.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    p.add_argument('--extra-case', type=Path, action='append', default=[],
        help='Additional CPU-only resource fixture or example; same JIT/static checks')
    p.add_argument('--graphics-case', type=Path, action='append', default=[],
        help='Explicit bounded real-graphics fixture or example; requires working display/backend')
    for name in ('compiler', 'dynamic-packages', 'static-packages', 'out', 'objdump'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, compiler, dynamic, static, out, objdump = [v.resolve() for v in
        (a.root, a.compiler, a.dynamic_packages, a.static_packages, a.out, a.objdump)]
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs = {str(path): sha(path) for path in [Path(__file__).resolve(), compiler, objdump]}
    for folder in (compiler.parent, dynamic, static):
        for path in folder.rglob('*'):
            if path.is_file():
                inputs[str(path)] = sha(path)
    cases = [root / 'test/cases' / (name + '.xl') for name in
        ('foreign_xge3_render_resources', 'foreign_xge3_render_oom')]
    cases.append(root / 'lib/bindings/xge/examples/headless_batches.xl')
    cases.extend(path.resolve() for path in a.extra_case)
    cases.extend(path.resolve() for path in a.graphics_case)
    assert len({path.stem for path in cases}) == len(cases), 'Duplicate output basename'
    inputs.update({str(path): sha(path) for path in cases})
    for case in cases:
        for suffix in ('.c', '.h'):
            sidecar = case.with_suffix(suffix)
            if sidecar.is_file():
                inputs[str(sidecar)] = sha(sidecar)
    report = dict(passed=False, graphicsContext=bool(a.graphics_case), gpuAcceptance=bool(a.graphics_case),
        graphicsScope='Bounded OpenGL API/readback; no hardware performance qualification' if a.graphics_case else None,
        shippingDelivery=False, compilerBuild='caller supplied; qualify cohort separately',
        inputs=inputs, rows=[])

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command, packages=None):
        env = os.environ.copy()
        env.pop('XLANG_PACKAGE_PATH', None)
        if packages:
            env['XLANG_PACKAGE_PATH'] = str(packages)
        command = list(map(str, command))
        result = subprocess.run(command, env=env,
            cwd=root / 'release' / ('win_x64' if os.name == 'nt' else 'linux_x64'),
            capture_output=True, timeout=180)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        report['rows'].append(dict(name=name, command=command, packages=str(packages),
            exitCode=result.returncode, logSha256=sha(log)))
        save()
        if result.returncode:
            raise RuntimeError(name + ' failed: ' + str(log))
        return result

    include = ['--inc-path', root, '--inc-path', compiler.parent / 'include']
    for case in cases:
        name = case.stem
        cfile, hfile = out / (name + '.c'), out / (name + '.h')
        run(name + '-emit', [compiler, 'cgen', case, '--emit', cfile,
            '--emit-header', hfile], dynamic)
        result = run(name + '-jit', [compiler, 'run', case, *include], dynamic)
        if name.endswith('_oom'):
            assert b'render hot allocations: 0' in result.stdout
        binary = out / (name + ('.exe' if os.name == 'nt' else ''))
        run(name + '-static-build', [compiler, 'build', case, '-o', binary, *include], static)
        result = run(name + '-static-run', [binary])
        if name.endswith('_oom'):
            assert b'render hot allocations: 0' in result.stdout
        if os.name == 'nt':
            result = run(name + '-imports', [objdump, '-p', binary])
            imports = re.findall(rb'DLL Name:\s*(\S+)', result.stdout)
            assert imports, 'PE import table not found'
            forbidden = {b'xge3_host.dll', b'xge.dll', b'xl.dll', b'xlc.dll', b'libtcc.dll'}
            assert not forbidden.intersection(name.lower() for name in imports)
            report.setdefault('binaries', []).append(dict(path=str(binary), sha256=sha(binary),
                bytes=binary.stat().st_size, imports=[v.decode('ascii') for v in imports]))
    assert all(sha(path) == digest for path, digest in inputs.items()), 'Acceptance input changed'
    report.update(passed=True, inputsUnchanged=True)
    save()
    print(json.dumps(dict(passed=True, rows=len(report['rows']), gpuAcceptance=bool(a.graphics_case))))


if __name__ == '__main__':
    main()
