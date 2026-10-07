#!/usr/bin/env python3
"""Actual PROJECT language path, strict C/JIT/AOT and complete synchronous OOM prefix.

Generated C and the host use O0/O2. Linked runtime/compiler optimization is
recorded separately. Configured encode/file/sink cover four formats, not nominal Writer APIs.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--fixture', choices=('project', 'readers', 'configured', 'sinks', 'files'), default='project')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists() or (os.name == 'nt' and not args.public_directory):
        parser.error('Require gcc, fresh evidence, and a Windows current-source driver or Linux current products')
    test_root = Path(__file__).resolve().parents[1]
    fixture_names = {'project': 'stdlib_persistence_project_language.xl',
                     'readers': 'stdlib_serialization_read_errors.xl',
                     'configured': 'stdlib_persistence_configured_language.xl',
                     'sinks': 'stdlib_persistence_sink_language.xl',
                     'files': 'stdlib_persistence_file_language.xl'}
    source = test_root / 'test/cases' / fixture_names[args.fixture]
    oom_source = test_root / 'test/cases/stdlib_serialization_read_oom.xl' if args.fixture == 'readers' else source
    oom_host = root / 'test/cases/xlir_module_product_oom_host.c'
    memory = root / 'test/support/module_product_memory.c'
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxlc.so')
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), source, oom_source,
             oom_host, memory, root / 'test/support/module_product_memory.h', product}
    error_names = {'configured': 'stdlib_persistence_configured_errors.xl',
                   'sinks': 'stdlib_persistence_sink_errors.xl', 'files': 'stdlib_persistence_file_errors.xl'}
    error_source = test_root / 'test/cases' / error_names[args.fixture] if args.fixture in error_names else None
    if error_source: files.add(error_source)
    public = None
    if args.public_directory:
        manifest = args.public_directory.resolve() / 'build.json'
        public = json.loads(manifest.read_text(encoding='utf-8-sig'))
        files.add(manifest)
        for row in public['Inputs'] + public['Headers'] + public['Objects']:
            path = Path(row['Path'])
            if digest(path).lower() != row['Sha256'].lower(): parser.error('Stale compiler input: ' + str(path))
            files.add(path)
        package_path = args.public_directory.resolve() / 'artifact_sources.json'
        files.add(package_path)
        for row in json.loads(package_path.read_text(encoding='utf-8-sig'))['Files']:
            path = Path(row['Path'])
            if digest(path).lower() != row['Sha256'].lower(): parser.error('Stale runtime input: ' + str(path))
            files.add(path)
        cli = Path(public['Driver'])
        includes = args.public_directory.resolve() / 'include'
        links = [row['Path'] for row in public['Objects'] if Path(row['Path']).name != 'xlir_public_source_driver.o']
        links += [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        cli = root / 'release/xl'
        includes = out / 'include'
        files.add(root / 'release/libxl.so')
        for directory in ('src', 'lib', 'release/include'):
            files.update(path for path in (root / directory).rglob('*') if path.suffix in ('.c', '.h', '.inc'))
        links = ['-Lrelease', '-lxlc', '-Wl,-rpath,' + str(root / 'release'), '-pthread', '-ldl', '-lm']
    files.add(cli)
    inputs = [{'Path': str(path), 'Sha256': digest(path)} for path in sorted(files)]
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Rows': rows, 'Sweeps': sweeps,
              'Scope': __doc__, 'Sanitized': False, 'PrunedNativeLink': False,
              'RuntimeCompilerOptimization': 'O0' if public else 'See Linux product manifest',
              'OOMScope': 'Complete caller-thread module initialization and retirement prefix, no Future workers'}
    out.mkdir(parents=True)
    run_environment = os.environ.copy()
    if args.fixture == 'files':
        scratch = out / 'files'
        scratch.mkdir()
        run_environment['XL6_PERSIST_FILE_ROOT'] = str(scratch)
        report['FileOutputDirectory'] = str(scratch)
    if os.name == 'nt': shutil.copy2(product, out / 'xl.dll')
    else:
        shutil.copytree(root / 'src/runtime', includes / '__xl_runtime')
        for path in (root / 'release/include').glob('*.h'): shutil.copy2(path, includes / '__xl_runtime' / path.name)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I.', '-Irelease',
             '-Irelease/include', '-Ilib/tcc', '-I' + str(includes)]
    if not public: flags += ['-D_GNU_SOURCE']
    strict = flags + ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-label',
                      '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers']
    def run(name: str, command: list[str], timeout: int = 360) -> str:
        result = subprocess.run(command, cwd=root, env=run_environment, capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'Complete': result.returncode == 0})
        if not name.startswith('oom.point.') or result.returncode: print(f'{name}: {result.returncode}', flush=True)
        if result.returncode: raise RuntimeError(name + ': ' + data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def command(mode: str, target: Path, input_source: Path = source) -> list[str]:
        if public: return [str(cli), str(input_source), mode, str(target)]
        if mode == 'cgen-default': return [str(cli), 'cgen', str(input_source), '--inc-path', str(includes), '--emit', str(target)]
        if mode == 'run': return [str(cli), 'run', str(input_source), '--inc-path', str(includes)]
        return [str(cli), 'build', str(input_source), '--inc-path', str(includes), '-o', str(target)]
    try:
        generated = out / 'project.c'
        run('cgen', command('cgen-default', generated))
        text = generated.read_text(encoding='utf-8')
        if args.fixture in ('project', 'configured', 'sinks', 'files'):
            assert 'xl6PersistenceProjectV1(' in text and 'xlir_persist_plan_2' in text and 'xlir_persist_plan_4' in text
            if args.fixture == 'configured':
                assert 'xl6_serialization_encode.h' in text
                assert all(name + '(' in text for name in
                           ('xl6JsonEncodeV1', 'xl6XlonEncodeV1', 'xl6JsonlEncodeV1', 'xl6XlonlEncodeV1'))
                assert '_stringify_configured(' not in text
            elif args.fixture in ('sinks', 'files'):
                assert 'xl6_serialization_io.h' in text
                suffix = 'WriteSinkV1' if args.fixture == 'sinks' else 'WriteFileV1'
                assert all('xl6' + name + suffix + '(' in text for name in ('Json', 'Xlon', 'Jsonl', 'Xlonl'))
                assert '_write_to_configured(' not in text and '_write_file_configured(' not in text
                assert 'bool projection_ok_' in text
        else:
            assert 'xl6_serialization_read.h' in text and 'xlir_result_xvalue xl6_xrt_json_parse(' in text
            assert 'xlir_result_xvalue xl6_xrt_xlonl_parse(' in text
        assert '_project_t' not in text and '_stringify_t' not in text and '_value_t' not in text
        for level in (0, 2):
            binary, memory_object = out / f'project.O{level}.exe', out / f'memory.O{level}.o'
            run(f'O{level}.memory', [cc, *strict, f'-O{level}', '-c', str(memory), '-o', str(memory_object)])
            # cgen-default emits a standalone module, not public artifact entry
            # wrappers. Preinclude it in the module host; its top-level program
            # executes during init, and normal/failure paths use the same host.
            run(f'O{level}.compile', [cc, *strict, f'-O{level}', '-include', str(generated),
                str(oom_host), str(memory_object), *links, '-o', str(binary)])
            assert 'balanced' in run(f'O{level}.baseline', [str(binary)])
        run('jit', command('run', out / 'jit'))
        run('aot.emit', command('exe', out / 'aot'))
        run('aot.run', [str(out / ('aot.exe' if os.name == 'nt' else 'aot'))])
        if error_source:
            errors_generated = out / 'errors.c'
            run('errors.cgen', command('cgen-default', errors_generated, error_source))
            errors_text = errors_generated.read_text(encoding='utf-8')
            assert '_stringify_configured(' not in errors_text
            if args.fixture in ('sinks', 'files'):
                assert '_write_to_configured(' not in errors_text and '_write_file_configured(' not in errors_text
            for level in (0, 2):
                errors_binary = out / f'errors.O{level}.exe'
                run(f'errors.O{level}.compile', [cc, *strict, f'-O{level}', '-include', str(errors_generated),
                    str(oom_host), str(out / f'memory.O{level}.o'), *links, '-o', str(errors_binary)])
                assert 'balanced' in run(f'errors.O{level}.baseline', [str(errors_binary)])
            run('errors.jit', command('run', out / 'errors.jit', error_source))
            run('errors.aot.emit', command('exe', out / 'errors.aot', error_source))
            run('errors.aot.run', [str(out / ('errors.aot.exe' if os.name == 'nt' else 'errors.aot'))])
        oom_generated = generated
        if oom_source != source:
            oom_generated = out / 'readers.oom.c'
            run('oom.cgen', command('cgen-default', oom_generated, oom_source))
        for level in (0, 2):
            binary = out / f'project.O{level}.exe'
            if oom_source != source:
                binary = out / f'readers.oom.O{level}.exe'
                run(f'oom.O{level}.compile', [cc, *strict, f'-O{level}', '-include', str(oom_generated),
                    str(oom_host), str(out / f'memory.O{level}.o'), *links, '-o', str(binary)])
                assert 'balanced' in run(f'oom.O{level}.baseline', [str(binary)])
            for point in range(4097):
                proof = run(f'oom.point.O{level}.{point}', [str(binary), str(point)])
                assert 'balanced' in proof
                if 'hit=0 balanced' in proof:
                    assert point > 0
                    sweeps.append({'Level': level, 'FirstNoHit': point})
                    print(f'O{level}: complete allocation prefix through {point}', flush=True)
                    break
            else: raise RuntimeError('Incomplete allocation prefix')
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        print(error, file=sys.stderr)
    finally:
        (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
