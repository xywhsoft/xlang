"""Normal five-group composition on real, source-free shipping products.

Fixed offline key/certificates and literal self-owned loopback only. This is
not a network/crypto security campaign or complete five-group release proof.
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
from gen_tcc_vfs_resources import load_resources


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists(), 'Preserve previous evidence; use a fresh directory'
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    controls = Path(__file__).resolve().parents[1]
    windows = os.name == 'nt'
    cc = Path(shutil.which('gcc')).resolve()
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so')
    original = [root / 'release' / name for name in names]
    recorded = {Path(row['Path']).resolve(): row['Sha256'] for row in build['Products']}
    assert all(recorded.get(path) == sha(path) for path in original)

    out.mkdir(parents=True)
    isolated, work, inputs = (out / name for name in ('isolated', 'empty-work', 'inputs'))
    for path in (isolated, work, inputs):
        path.mkdir()
    for path in original:
        shutil.copy2(path, isolated / path.name)
    source = controls / 'test/cases/stdlib_five_groups_joint.xl'
    host_source = controls / 'test/cases/xlir_five_groups_joint_host.c'
    drain_source = controls / 'test/cases/xlir_runtime_drain_threads.c'
    copied = inputs / source.name
    shutil.copy2(source, copied)
    host = isolated / ('joint-host.exe' if windows else 'joint-host')
    manifest = root / 'tools' / ('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    directory = root / ('build/tmp' if windows else 'build/linux')
    dist = root / 'res/tcc/win64/x86_64' if windows else directory / 'tcc-dist'
    resources = load_resources(manifest, root, dist, directory / 'xrt.o', directory)
    declarations = [row for row in resources if row.virtual.startswith('include/__xl_runtime/')]
    sdk = out / 'strict-sdk'
    for row in declarations:
        destination = sdk / Path(row.virtual).relative_to('include')
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(row.source, destination)
    pinned = {Path(__file__).resolve(), Path(sys.executable), cc, args.build_report.resolve(),
              Path(load_resources.__code__.co_filename).resolve(), source, host_source, drain_source,
              manifest, *original, *[row.source for row in declarations]}
    report = dict(Complete=False, Inputs={str(q): sha(q) for q in sorted(pinned)},
                  Rows=[], Failures=[], ActualShippingProducts=True, SourceObjectOverrides=False,
                  FullFiveGroupsAccepted=False, Scope=__doc__)
    environment = {k: v for k, v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
                   ('CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH', 'LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT')}
    environment.update(TMPDIR=str(out), TEMP=str(out), TMP=str(out))

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, timeout=300):
        command = list(map(str, command))
        log = out / (name + '.log')
        started = time.monotonic()
        with log.open('wb') as stream:
            code = subprocess.run(command, cwd=work, env=environment, stdout=stream,
                                  stderr=subprocess.STDOUT, timeout=timeout).returncode
        report['Rows'].append(dict(Name=name, Command=command, Exit=code, Expected=0,
                                   Log=str(log), Sha256=sha(log), Seconds=time.monotonic() - started))
        save()
        print(name, code, flush=True)
        assert code == 0, str(log)
        return log.read_text(encoding='utf8', errors='replace')

    def normal_flow(output, count):
        assert output.count('five-groups-joint=ok') == count
        assert output.count('five-groups-joint: select') == count
        native = 'iocp' if windows else 'epoll'
        assert output.count('five-groups-joint: ' + native) == count
        assert 'five-groups-joint: ' + ('epoll' if windows else 'iocp') not in output

    try:
        for optimization in ('O0', 'O2'):
            drain = isolated / ('drain-' + optimization + ('.exe' if windows else ''))
            run('drain-build-' + optimization,
                [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-' + optimization,
                 '-I' + str(root / 'release/include'), drain_source, isolated / names[2], isolated / names[1],
                 *([] if windows else ['-pthread', '-ldl', '-lm', '-Wl,-rpath,$ORIGIN']), '-o', drain])
            output = run('drain-run-' + optimization, [drain])
            assert output.count('drain threads: phase=') == 3 and output.count('wait=1/1 released=1') == 3
        report['DrainThreadPhasesPerOptimization'] = 3
        run('public-host-build', [cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                                 '-I' + str(root / 'release/include'), host_source,
                                 isolated / names[2], *([] if windows else ['-Wl,-rpath,$ORIGIN']), '-o', host])
        output = run('public-host-audit', [host, copied, out], timeout=600)
        for name in ('compile', 'typed', 'lowered', 'cgen', 'aot-build', 'jit', 'module', 'checked-retirement'):
            assert output.count('joint-host: ' + name + '\n') == 1, name
        normal_flow(output, 2)
        generated = out / 'joint.c'
        code = generated.read_text(encoding='utf8')
        assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+(?:xl6Tls|xl6TaskPool|xl6Nist|xl6ChannelDirect)\w*\([^;]*\)\s*\{', code, re.M)
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
                 '-Wno-unused-parameter', '-Wno-unused-label', '-Wno-unused-but-set-variable',
                 '-Wno-unused-const-variable', '-Wno-missing-field-initializers', '-I' + str(sdk)]
        for optimization in ('O0', 'O2'):
            run('strict-' + optimization, [cc, *flags, '-' + optimization, '-fsyntax-only', generated])
        binary = out / 'joint.exe'
        relocated = out / 'standalone'
        relocated.mkdir()
        independent = relocated / binary.name
        shutil.copy2(binary, independent)
        normal_flow(run('aot-relocated', [independent]), 1)
        imports = run('aot-imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', independent]
                      if windows else ['readelf', '-d', independent])
        assert not any(name in imports.lower() for name in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'))
        normal_flow(run('cli', [isolated / names[0], 'run', copied]), 1)
        assert not any((isolated / folder).exists() for folder in ('src', 'lib', 'res', 'include'))
        assert all(sha(isolated / path.name) == sha(path) for path in original)
        report['BackendFlows'] = dict(select=4, **{('iocp' if windows else 'epoll'): 4})
        report['PublicHostPhases'] = 8
    except Exception as error:
        report['Failures'].append(repr(error))
    finally:
        report['InputsUnchanged'] = all(sha(q) == h for q, h in report['Inputs'].items())
        report['Complete'] = report['InputsUnchanged'] and not report['Failures']
        report['Products'] = [dict(Path=str(q), Sha256=sha(q)) for q in out.rglob('*')
                              if q.is_file() and q.name != 'results.json']
        save()
    assert report['Complete'], report['Failures']


if __name__ == '__main__':
    main()
