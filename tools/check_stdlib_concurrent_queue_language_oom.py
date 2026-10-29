#!/usr/bin/env python3
"""Actual Queue/Tree language C: complete synchronous caller-thread allocation prefixes.

This does not claim scheduler-wide Future OOM, ownership-graph, or 53-module acceptance.
Independent fixture/optimization lanes continue after failure for batch diagnosis.
"""
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
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt') != bool(args.public_directory):
        parser.error('Require gcc, fresh evidence and Windows source objects / Linux source products')
    cases = [('stdlib_concurrent_queue_spsc_only', False),
             ('stdlib_concurrent_queue_mpsc_only', False),
             ('stdlib_concurrent_queue_mpmc_only', False),
             ('stdlib_concurrent_queue_owned', True),
             ('stdlib_tree_native_projection', True)]
    files = {Path(__file__).resolve(), Path(cc), Path(sys.executable)}
    for folder in ('src', 'lib', 'release/include'):
        files.update(f for f in (root / folder).rglob('*') if f.suffix in ('.c', '.h', '.inc'))
    files.update(root / f'test/cases/{case}.xl' for case, _ in cases)
    files.update(root / f for f in ('test/cases/stdlib_concurrent_queue_language_oom_host.c',
                                  'test/support/module_product_memory.c',
                                  'test/support/module_product_memory.h'))
    objects = []
    cli = root / 'release/xl'
    if args.public_directory:
        public = args.public_directory.resolve()
        for name, fields in [('build.json', ('Inputs', 'Headers', 'Objects')),
                             ('artifact_sources.json', ('Files',))]:
            manifest = public / name
            build = json.loads(manifest.read_text(encoding='utf-8-sig'))
            files.add(manifest)
            for field in fields:
                for row in build[field]:
                    assert sha(row['Path']) == row['Sha256'].lower(), 'Stale source: ' + row['Path']
                    files.add(Path(row['Path']))
            if name == 'build.json':
                assert build['SourceXrt'] and not build['RuntimeOnly']
                cli = Path(build['Driver'])
                assert sha(cli) == build['DriverSha256'].lower()
                # The source factory, consumer and policy-ops identities form
                # one resident island. Never link new methods to old DLL ops.
                objects = [row['Path'] for row in build['Objects']
                           if Path(row['Path']).name != 'xlir_public_source_driver.o']
        dll = root / 'release/xl.dll'
        files.add(dll)
        links = [*objects, str(dll), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        include = public / 'include'
    else:
        assert sys.platform == 'linux'
        files.update(root / ('release/' + f) for f in ('libxl.so', 'libxlc.so'))
        links = ['-Lrelease', '-lxlc', '-lxl', '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-ldl', '-lm']
        include = out / 'include'
    files.add(cli)
    inputs = [{'Path': str(f.resolve()), 'Sha256': sha(f)} for f in sorted(files)]
    out.mkdir(parents=True)
    if args.public_directory:
        shutil.copy2(dll, out / 'xl.dll')
    else:
        private = include / '__xl_runtime'
        private.mkdir(parents=True)
        for folder in (root / 'src/runtime', root / 'release/include'):
            for header in folder.glob('*.h'):
                shutil.copy2(header, private / header.name)
    rows, prefixes, failures = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Full53Acceptance': False,
              'OwnershipGraphAcceptance': False, 'SchedulerWideOomAcceptance': False,
              'Scope': __doc__, 'Platform': sys.platform, 'Inputs': inputs,
              'Rows': rows, 'Prefixes': prefixes, 'Failures': failures}
    def save():
        (out / 'language-oom.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command):
        command = list(map(str, command))
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=root, capture_output=True, timeout=180)
            data, code = result.stdout + result.stderr, result.returncode
        except subprocess.TimeoutExpired as error:
            data, code = (error.stdout or b'') + (error.stderr or b''), 'timeout-180s'
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': code, 'Seconds': time.monotonic() - start})
        save()
        if code != 0:
            raise RuntimeError(name + ': ' + data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I.', '-Irelease',
             '-Irelease/include', '-I' + str(include), '-Wno-unused-function', '-Wno-unused-parameter',
             '-Wno-unused-variable', '-Wno-unused-label', '-Wno-unused-const-variable',
             '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers']
    if not args.public_directory:
        flags += ['-D_GNU_SOURCE']
    def emit(case, target):
        source = root / f'test/cases/{case}.xl'
        return [cli, source, 'cgen-default', target] if args.public_directory else \
            [cli, 'cgen', source, '--emit', target]

    try:
        for level in (0, 2):
            run(f'memory.O{level}.build', [cc, *flags, f'-O{level}', '-c',
                'test/support/module_product_memory.c', '-o', out / f'memory.O{level}.o'])
        for case, user_entry in cases:
            generated = out / (case + '.c')
            try:
                repeated = out / (case + '.repeat.c')
                run(case + '.emit', emit(case, generated))
                run(case + '.emit.repeat', emit(case, repeated))
                assert generated.read_bytes() == repeated.read_bytes(), 'Nondeterministic actual language C'
                code = generated.read_text(encoding='utf-8')
                assert not re.search(r'xlir_special_queue_|xlir_avl_collect', code), 'Legacy producer/queue fallback'
            except Exception as error:
                failures.append({'Case': case, 'Phase': 'emit', 'Failure': str(error)})
                save()
                continue
            for level in (0, 2):
                lane = case + f'.O{level}'
                binary = out / (lane + ('.exe' if os.name == 'nt' else ''))
                try:
                    run(lane + '.build', [cc, *flags, f'-O{level}',
                        *(['-DXL6_PRODUCT_USER_ENTRY'] if user_entry else []), '-include', generated,
                        'test/cases/stdlib_concurrent_queue_language_oom_host.c', out / f'memory.O{level}.o',
                        *links, '-o', binary])
                    assert 'balanced' in run(lane + '.baseline', [binary])
                    for point in range(4097):
                        proof = run(lane + '.prefix.' + str(point), [binary, point])
                        assert 'balanced' in proof
                        if 'hit=0 balanced' in proof:
                            assert point > 0, 'No allocation work in language body'
                            prefixes.append({'Case': case, 'Optimization': level,
                                             'FirstNoHit': point, 'Runs': point + 1})
                            print(lane + ': complete prefix, first no-hit ' + str(point), flush=True)
                            break
                    else:
                        raise AssertionError('Incomplete caller-thread prefix')
                except Exception as error:
                    failures.append({'Case': case, 'Optimization': level, 'Failure': str(error)})
                    print(failures[-1], file=sys.stderr, flush=True)
                save()
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged'], 'Inputs changed during batch'
        report['Complete'] = not failures and len(prefixes) == 2 * len(cases)
    except Exception as error:
        report['Error'] = str(error)
        print(error, file=sys.stderr)
    finally:
        save()
    print(f'Queue/Tree language OOM: {len(rows)} phases, {len(prefixes)} complete prefixes, complete={report["Complete"]}', flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
