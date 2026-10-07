#!/usr/bin/env python3
"""Actual shared native/wire Array deep clone, checked IR, C/JIT/AOT and caller-thread OOM."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    fixture_root = Path(__file__).resolve().parents[1]
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt' and not args.public_directory):
        parser.error('Require gcc, fresh evidence and a Windows current-source driver or Linux current products')
    public = None
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc)}
    if args.public_directory:
        manifest = args.public_directory.resolve() / 'build.json'
        public = json.loads(manifest.read_text(encoding='utf-8-sig'))
        cli = Path(public['Driver']); files.add(manifest)
        for row in public['Inputs'] + public['Headers'] + public['Objects']:
            path = Path(row['Path'])
            if digest(path).lower() != row['Sha256'].lower(): parser.error('Stale compiler input: ' + str(path))
            files.add(path)
        staged = args.public_directory.resolve() / 'artifact_sources.json'
        package = json.loads(staged.read_text(encoding='utf-8-sig')); files.add(staged)
        for row in package['Files']:
            path = Path(row['Path'])
            if digest(path).lower() != row['Sha256'].lower(): parser.error('Stale staged source: ' + str(path))
            files.add(path)
    else:
        cli = root / 'release/xl'
        files.update((root / 'release/libxl.so', root / 'build_linux.sh'))
        for directory in ('src', 'lib', 'release/include'):
            files.update(path for path in (root / directory).rglob('*') if path.suffix in ('.c', '.h', '.inc'))
    source = fixture_root / 'test/cases/stdlib_array_clone_oom.xl'
    contract = fixture_root / 'test/cases/xlir_array_clone_contract.c'
    host = root / 'test/cases/stdlib_array_create_host.c'
    oom_host = root / 'test/cases/xlir_module_product_oom_host.c'
    memory_source = root / 'test/support/module_product_memory.c'
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxlc.so')
    files.update((cli, source, contract, host, oom_host, memory_source,
                  root / 'test/support/module_product_memory.h', product))
    inputs = [{'Path': str(path), 'Sha256': digest(path)} for path in sorted(files)]
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Sweeps': sweeps,
              'Scope': __doc__, 'Sanitized': False, 'PrunedGccLink': False,
              'OOMScope': 'Complete synchronous caller-thread module/entry/retirement prefix; no Future workers',
              'WindowsPublicSourceCompiler': bool(public), 'LinuxSharedCompilerProduct': not bool(public)}
    out.mkdir(parents=True)
    if os.name == 'nt': shutil.copy2(product, out / 'xl.dll')
    else:
        runtime_include = out / 'include/__xl_runtime'
        shutil.copytree(root / 'src/runtime', runtime_include)
        for header in (root / 'release/include').glob('*.h'): shutil.copy2(header, runtime_include / header.name)
    includes = args.public_directory.resolve() / 'include' if public else out / 'include'
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I.', '-Irelease',
             '-Irelease/include', '-Ilib/tcc', '-I' + str(includes)]
    if public:
        links = [row['Path'] for row in public['Objects'] if Path(row['Path']).name != 'xlir_public_source_driver.o']
        links += [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        flags += ['-D_GNU_SOURCE']
        links = ['-Lrelease', '-lxlc', '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-ldl', '-lm']
    strict = flags + ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-label',
                      '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers']
    def run(name, command, timeout=180):
        result = subprocess.run(command, cwd=root, capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'Complete': result.returncode == 0})
        if not re.fullmatch(r'oom\.O[02]\.\d+', name) or result.returncode: print(f'{name}: {result.returncode}', flush=True)
        if result.returncode: raise RuntimeError(name + ': ' + data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def command(mode, target):
        if public: return [str(cli), str(source), mode, str(target)]
        if mode == 'ir-lowered': return [str(cli), 'ir', str(source), '--lowered', '--format', 'json', '--emit', str(target)]
        if mode == 'cgen-default': return [str(cli), 'cgen', str(source), '--inc-path', str(includes), '--emit', str(target)]
        if mode == 'run': return [str(cli), 'run', str(source), '--inc-path', str(includes)]
        return [str(cli), 'build', str(source), '--inc-path', str(includes), '-o', str(target)]
    try:
        oracle, generated = out / 'contract.exe', out / 'clone.c'
        run('contract.build', [cc, *flags, '-O0', str(contract), *links, '-o', str(oracle)], 360)
        log = run('contract.run', [str(oracle), str(source), str(generated)])
        match = re.search(r'Array clone: (\d+) checked producers; (\d+) mutations refused before C/header', log)
        assert match and int(match[1]) >= 6 and int(match[2]) == int(match[1]) * 7
        report['Producers'], report['RefusedMutations'] = int(match[1]), int(match[2])
        text = generated.read_text(encoding='utf-8')
        assert 'xl6ValueDeepCloneV1(' in text and '= xrtValueDeepClone(' not in text
        for mode, suffix in (('ir-lowered', '.json'), ('cgen-default', '.repeat.c')):
            run(mode, command(mode, out / ('clone' + suffix)))
        assert generated.read_bytes() == (out / 'clone.repeat.c').read_bytes()
        for level in (0, 2):
            binary = out / f'clone.O{level}.exe'
            run(f'O{level}.compile', [cc, *strict, f'-O{level}', str(generated), str(host), *links, '-o', str(binary)], 360)
            run(f'O{level}.run', [str(binary)])
        run('jit', command('run', out / 'jit'))
        run('aot.emit', command('exe', out / 'aot'))
        run('aot.run', [str(out / ('aot.exe' if os.name == 'nt' else 'aot'))])
        for level in (0, 2):
            binary, memory = out / f'clone.oom.O{level}.exe', out / f'memory.O{level}.o'
            run(f'oom.O{level}.memory', [cc, *strict, f'-O{level}', '-c', str(memory_source), '-o', str(memory)])
            run(f'oom.O{level}.compile', [cc, *strict, f'-O{level}', '-DXL6_PRODUCT_USER_ENTRY',
                '-include', str(generated), str(oom_host), str(memory), *links, '-o', str(binary)], 360)
            assert 'balanced' in run(f'oom.O{level}.baseline', [str(binary)])
            for point in range(4097):
                proof = run(f'oom.O{level}.{point}', [str(binary), str(point)])
                assert 'balanced' in proof
                if 'hit=0 balanced' in proof:
                    assert point > 0
                    sweeps.append({'Level': level, 'FirstNoHit': point})
                    print(f'O{level}: complete allocation prefix, first no-hit {point}', flush=True)
                    break
            else: raise RuntimeError('Incomplete caller-thread allocation prefix')
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error); print(error, file=sys.stderr)
    finally:
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
