#!/usr/bin/env python3
"""Actual PROJECT language path, strict C/JIT/AOT and complete synchronous OOM prefix.

Generated C and the host use O0/O2. Linked runtime/compiler optimization is
recorded separately. Configured encode/file/sink cover four formats; Writer
contracts cover JsonWriter and XlonWriter, not additional line Writer classes.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

def digest(path: Path) -> str:
    hashed = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            hashed.update(chunk)
    return hashed.hexdigest()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--public-directory', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--fixture', choices=('project', 'readers', 'configured', 'sinks', 'files', 'writers', 'visitors', 'nominal', 'pretty', 'restore', 'restore-domains', 'restore-construction', 'native-identity'), default='project')
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
                     'files': 'stdlib_persistence_file_language.xl',
                     'writers': 'stdlib_persistence_writer_language.xl',
                     'visitors': 'stdlib_serialization_visit_language.xl',
                     'nominal': 'stdlib_persistence_nominal_language.xl',
                     'pretty': 'stdlib_persistence_pretty_language.xl',
                     'restore': 'stdlib_persistence_restore_language.xl',
                     'restore-domains': 'stdlib_persistence_restore_domains.xl',
                     'restore-construction': 'stdlib_persistence_restore_construction.xl',
                     'native-identity': 'stdlib_native_sequence_identity.xl'}
    source = test_root / 'test/cases' / fixture_names[args.fixture]
    oom_source = test_root / 'test/cases/stdlib_serialization_read_oom.xl' if args.fixture == 'readers' else source
    oom_host = test_root / 'test/cases/xlir_module_product_oom_host.c'
    memory = root / 'test/support/module_product_memory.c'
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxlc.so')
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), source, oom_source,
             oom_host, memory, root / 'test/support/module_product_memory.h', product}
    error_names = {'configured': 'stdlib_persistence_configured_errors.xl',
                   'sinks': 'stdlib_persistence_sink_errors.xl', 'files': 'stdlib_persistence_file_errors.xl',
                   'writers': 'stdlib_persistence_writer_errors.xl',
                   'visitors': 'stdlib_serialization_visit_errors.xl',
                   'nominal': 'stdlib_persistence_nominal_errors.xl',
                   'pretty': 'stdlib_persistence_pretty_errors.xl',
                   'restore': 'stdlib_persistence_restore_errors.xl',
                   'restore-domains': 'stdlib_persistence_restore_errors.xl',
                   'restore-construction': 'stdlib_persistence_restore_construction_errors.xl'}
    error_source = test_root / 'test/cases' / error_names[args.fixture] if args.fixture in error_names else None
    if error_source: files.add(error_source)
    status_names = {'visitors': 'xlir_serialization_visit_status_host.c',
                    'pretty': 'xlir_persistence_pretty_status_host.c',
                    'restore': 'xlir_persistence_restore_status_host.c',
                    'restore-domains': 'xlir_persistence_restore_status_host.c',
                    'restore-construction': 'xlir_persistence_restore_status_host.c'}
    status_host = test_root / 'test/cases' / status_names[args.fixture] if args.fixture in status_names else None
    if status_host: files.add(status_host)
    invalid_source = test_root / 'test/cases/stdlib_persistence_nominal_nonderived_invalid.xl' if args.fixture == 'nominal' else None
    if invalid_source: files.add(invalid_source)
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
    if args.fixture in ('files', 'nominal', 'pretty', 'restore'):
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
    def run(name: str, command: list[str], timeout: int = 360, expected: int = 0) -> str:
        result = subprocess.run(command, cwd=root, env=run_environment, capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'ExpectedExit': expected, 'Complete': result.returncode == expected})
        if not name.startswith('oom.point.') or result.returncode: print(f'{name}: {result.returncode}', flush=True)
        if result.returncode != expected: raise RuntimeError(name + ': ' + data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    def command(mode: str, target: Path, input_source: Path = source) -> list[str]:
        if public: return [str(cli), str(input_source), mode, str(target)]
        if mode == 'cgen-default': return [str(cli), 'cgen', str(input_source), '--inc-path', str(includes), '--emit', str(target)]
        if mode == 'run': return [str(cli), 'run', str(input_source), '--inc-path', str(includes)]
        return [str(cli), 'build', str(input_source), '--inc-path', str(includes), '-o', str(target)]
    try:
        if invalid_source:
            rejected = run('invalid.schema', command('cgen-default', out / 'invalid.c', invalid_source), expected=1)
            assert 'persistence' in rejected.lower() or '持久化' in rejected or 'verified admission ABI' in rejected
        generated = out / 'project.c'
        run('cgen', command('cgen-default', generated))
        text = generated.read_text(encoding='utf-8')
        if args.fixture in ('project', 'configured', 'sinks', 'files', 'writers', 'nominal', 'pretty'):
            assert 'xl6PersistenceProjectV1(' in text and 'xlir_persist_plan_2' in text and 'xlir_persist_plan_4' in text
            if args.fixture == 'nominal':
                assert 'xl6XlonEncodeV1(' in text and 'xl6XlonWriteFileV1(' in text
                assert 'xl6_xrt_xlon_stringify_options(' not in text and 'xl6_xrt_xlon_write_options(' not in text
            elif args.fixture == 'pretty':
                assert 'xrtJsonStringify(' in text and 'xl6JsonWriteFilePrettyV1(' in text
            elif args.fixture == 'configured':
                assert 'xl6_serialization_encode.h' in text
                assert all(name + '(' in text for name in
                           ('xl6JsonEncodeV1', 'xl6XlonEncodeV1', 'xl6JsonlEncodeV1', 'xl6XlonlEncodeV1'))
                assert '_stringify_configured(' not in text
            elif args.fixture == 'writers':
                assert 'xl6_serialization_writer.h' in text
                assert 'xl6_json_writer.h' in text and 'xl6_xlon_writer.h' in text
                assert 'finishNative' not in text
                assert all(name + '(' in text for name in ('xl6JsonWriterValueV1', 'xl6JsonWriterNameV1',
                           'xl6XlonWriterValueV1', 'xl6XlonWriterNameV1', 'xl6XlonWriterIndexV1'))
                assert 'bool projection_ok_' in text
            elif args.fixture in ('sinks', 'files'):
                assert 'xl6_serialization_io.h' in text
                suffix = 'WriteSinkV1' if args.fixture == 'sinks' else 'WriteFileV1'
                assert all('xl6' + name + suffix + '(' in text for name in ('Json', 'Xlon', 'Jsonl', 'Xlonl'))
                assert '_write_to_configured(' not in text and '_write_file_configured(' not in text
                assert 'bool projection_ok_' in text
        elif args.fixture == 'visitors':
            assert 'xl6_serialization_callback.h' in text
            assert 'xl6_serialization_visit_finish(' in text
            assert 'xl6_xrt_json_visit_configured(' in text and 'xl6_xrt_xlon_visit_configured(' in text
            assert 'static XL6_Serialization_Visit_Result xl6_serialization_callback_finish(' not in text
        elif args.fixture in ('restore', 'restore-domains', 'restore-construction'):
            assert 'xlir_persist_json_parse_t' in text and 'xlir_persist_xlon_parse_t' in text
            assert 'xrtValueObjectGet(source, (xstrview){' in text
            assert 'xl6SerializeRestoreReadFinishV1(' in text and 'bool projection_ok_' in text
            assert 'xl6PersistenceRestoreScalarV1(' in text and 'xl6_persistence_restore.h' in text
            if args.fixture in ('restore-domains', 'restore-construction'):
                assert 'xl6PersistenceRestoreArrayTakeV1(' in text and 'xl6SequencePrimitiveCreateV1(' in text
                assert 'for (index = 0; index < count;' not in text
                factories = re.findall(r'static xvalue\* xlir_persist_(json|xlon)_default_t(\d+)\(void\)\s*\{([^}]+)\}', text)
                assert len(factories) == (2 if args.fixture == 'restore-domains' else 6)
                assert {row[0] for row in factories} == {'json', 'xlon'}
                symbols = {}
                formats = {}
                for format_name, type_id, body in factories:
                    assert 'outcome.ok ? outcome.value : NULL' in body
                    assert 'xrtValueObjectSetNew' not in body and '->Create(' not in body
                    call = re.search(r'outcome = (xlir_fn_\d+)\(\);', body)
                    assert call
                    assert type_id not in symbols or symbols[type_id] == call[1]
                    symbols[type_id] = call[1]
                    formats.setdefault(type_id, set()).add(format_name)
                assert all(formats_for_type == {'json', 'xlon'} for formats_for_type in formats.values())
        elif args.fixture == 'native-identity':
            assert 'xl6SequencePrimitiveCreateV1(' in text and 'xl6SequenceNestedCreateTakeV2(' in text
            assert 'xl6SequenceReferenceCreateV1(' in text and 'xl6StackEqualsV1(' in text
            assert 'xl6_persistence_restore.h' in text and 'xlir_class_derive_equal_t' in text
            assert 'xl6_xrt_array_create(' not in text and 'xl6_xrt_array_equals(' not in text
        else:
            assert 'xl6_serialization_read.h' in text and 'xlir_result_xvalue xl6_xrt_json_parse(' in text
            assert 'xlir_result_xvalue xl6_xrt_xlonl_parse(' in text
        assert '_project_t' not in text and '_stringify_t' not in text
        # Verified callable adapters are not per-class persistence emitters.
        assert '_value_t' not in text.replace('xlir_language_function_value_t', 'callable-adapter-')
        for level in (0, 2):
            binary, memory_object = out / f'project.O{level}.exe', out / f'memory.O{level}.o'
            run(f'O{level}.memory', [cc, *strict, f'-O{level}', '-c', str(memory), '-o', str(memory_object)])
            # cgen-default emits a standalone module, not public artifact entry
            # wrappers. Preinclude it in the module host; its top-level program
            # executes during init, and normal/failure paths use the same host.
            run(f'O{level}.compile', [cc, *strict, f'-O{level}', '-include', str(generated),
                str(oom_host), str(memory_object), *links, '-o', str(binary)])
            assert 'balanced' in run(f'O{level}.baseline', [str(binary)])
            if status_host:
                status_binary = out / f'status.O{level}.exe'
                run(f'O{level}.status.compile', [cc, *strict, f'-O{level}', '-include', str(generated),
                    str(status_host), str(memory_object), *links, '-o', str(status_binary)])
                assert 'ownership balanced' in run(f'O{level}.status.run', [str(status_binary)])
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
