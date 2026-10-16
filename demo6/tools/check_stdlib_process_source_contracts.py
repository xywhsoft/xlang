"""White-box Process source contracts, separate from genuine product gates.

Build and run the live/File/Future hosts against one native implementation.
Resolve resident dependencies from actual undefined symbols and the canonical
artifact registry, not a second handwritten source list. Keep private mapper
tests on the real V4 frame protocol. This is not DLL/SO or full53 acceptance.
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
from stage_stdlib_process_observer import stage


EMBEDDED = {
    'live': {'process_command', 'process_live', 'process_stream', 'process_status'},
    'file': {'process_command', 'process_terminal_config', 'process_file'},
    'future': {'process_command', 'process_live', 'process_future'},
}
WITNESSES = {
    'live': rb'reader/writer OOM (\d+)/(\d+) failures \+ 2 first no-hit, balanced',
    'file': rb'OS error prefix (\d+) failures \+ 1 first no-hit, heap/handles balanced',
    'future': rb'mapper/pending/terminal OOM (\d+)/(\d+)/(\d+) failures \+ 3 first no-hit, waiter retired, balanced',
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--family', choices=tuple(EMBEDDED), action='append')
    parser.add_argument('--sanitize', action='store_true', help='Linux ASan/UBSan O1 source lane; not shipping acceptance')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    families = args.family or list(EMBEDDED)
    assert len(set(families)) == len(families)
    cc, nm = [shutil.which(n) for n in ('gcc', 'nm')]
    assert cc and nm
    assert not args.sanitize or os.name != 'nt'
    levels = ('O1',) if args.sanitize else ('O0', 'O2')
    task = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(root / 'tools'))
    from check_xlir_resident_catalog import declarations
    ids, entries = declarations(root)
    owners = dict(entries)
    assert len(owners) == len(entries)
    registry = root / 'src/compile/xl6_compile_unit.c'
    body = registry.read_text(encoding='utf-8-sig').split('xl6ArtifactComponents[] = {', 1)[1].split('\n};', 1)[0]
    rows = re.findall(r'\{"((?:src|tools)/[^"\n]+)",([^{}]*?)\{([^{}]*)\}[^{}]*\}', body, re.S)
    assert len(rows) == len(ids)
    components = {name: dict(Source=source, Dependencies=re.findall(r'R\((\w+)\)', deps))
                  for name, (source, deps, _) in zip(ids, rows)}
    assert all(d in components for c in components.values() for d in c['Dependencies'])
    sources = {f: task / 'test/cases' / ('xlir_process_' + f + '_host.c') for f in families}
    child_source = task / 'test/cases/xlir_process_child.c'
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), Path(nm), registry,
             root / 'tools/check_xlir_resident_catalog.py', root / 'tools/xlir_public_build_inputs.py',
             task / 'tools/stage_stdlib_process_observer.py', task / 'test/support/process_product_observer.h',
             root / 'tools/xrt_single_header_impl.c', root / 'lib/xrt/xrt.h', root / 'lib/xruntime/xruntime.h',
             root / 'release/include/xruntime_decl.h', root / 'src/api/xl_foreign_error.c',
             child_source, *sources.values()}
    files.update(p for p in (root / 'src').rglob('*') if p.suffix in ('.h', '.inc'))
    files.update((root / 'src/runtime').glob('*.c'))
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    out.mkdir()
    objects_dir = out / 'objects'
    objects_dir.mkdir()
    observer = stage(root, out / 'observer')
    environment = {k: v for k, v in os.environ.items()
                   if not k.upper().startswith('XL6_') and k not in
                   ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I' + str(root), '-I' + str(root / 'release'), '-I' + str(root / 'release/include'),
             '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
             '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
             '-Wno-missing-field-initializers', '-Wno-pointer-sign', '-Wno-format-truncation']
    windows = os.name == 'nt'
    systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if windows else ['-pthread', '-ldl', '-lm']
    if not windows:
        flags += ['-D_GNU_SOURCE']
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie']
        systems += ['-fsanitize=address,undefined', '-no-pie']
        environment.setdefault('ASAN_OPTIONS', 'detect_leaks=1:halt_on_error=1')
        environment.setdefault('UBSAN_OPTIONS', 'halt_on_error=1:print_stacktrace=1')
    host_flags = [*flags, '-DXL6_PROCESS_SOURCE_CONTRACTS', '-I' + str(out / 'observer'),
                  '-include', str(task / 'test/support/process_product_observer.h')]
    records, commands, controls, failures = [], [], [], []
    cache = {}
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=commands,
                  Proofs=records, Controls=controls, Failures=failures, Scope=__doc__,
                  ExplicitSubset=bool(args.family), SourceContractAcceptance=False,
                  OptimizationLevels=levels, Sanitizers=bool(args.sanitize), Observer=observer,
                  NativeImplementationCompiledOncePerLevel=True,
                  ShippingAcceptance=False, Full53Acceptance=False)

    def run(name, argv, reject=False, compile_rejection=False, timeout=180):
        argv = list(map(str, argv))
        started = time.monotonic()
        try:
            result = subprocess.run(argv, cwd=root, env=environment, capture_output=True, timeout=timeout)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out / (name + '.log')
        log.write_bytes(data)
        commands.append(dict(Name=name, Command=argv, Exit=code, Log=str(log), LogSha256=sha(log),
                             Seconds=time.monotonic() - started, ExpectedRejection=reject,
                             CompileRejection=compile_rejection))
        assert (code not in (0, 'timeout')) if reject else code == 0, (name, data.decode(errors='replace')[-6000:])
        return data, code

    def closure(obj, family, tag):
        text, _ = run(tag + '.undefined', [nm, '-u', obj])
        undefined = set(s.decode() for s in re.findall(rb'\bU\s+(\w+)\s*$', text, re.M))
        unknown = {s for s in undefined if s.startswith('xl6') and s not in owners}
        assert not unknown, ('Unregistered resident symbol', unknown)
        selected, pending = set(), [owners[s] for s in undefined if s in owners]
        roots = sorted(set(pending))
        while pending:
            name = pending.pop()
            if name not in selected:
                selected.add(name)
                pending += components[name]['Dependencies']
        embedded = {'src/api/xl_foreign_error.c', 'src/runtime/xl6_native_handle.c'}
        embedded.update('src/runtime/xl6_' + name + '.c' for name in EMBEDDED[family])
        return roots, sorted({components[n]['Source'] for n in selected} - embedded)

    def resident_objects(paths, opt):
        result = []
        for path in paths:
            key = path, opt
            if key not in cache:
                obj = objects_dir / (opt + '.' + Path(path).stem + '.o')
                run(opt + '.unit.' + Path(path).stem,
                    [cc, *flags, '-include', root / 'release/include/xruntime_decl.h', '-' + opt,
                     '-c', root / path, '-o', obj])
                cache[key] = obj
            result.append(cache[key])
        return result

    try:
        child = out / ('child.exe' if windows else 'child')
        child_flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                       *(['-municode'] if windows else ['-D_GNU_SOURCE'])]
        run('child.build', [cc, *child_flags, child_source, '-o', child])
        native_objects = {}
        for opt in levels:
            native_obj = objects_dir / ('native.' + opt + '.o')
            run('native.' + opt + '.compile', [cc, *flags, '-' + opt, '-c',
                                              root / 'tools/xrt_single_header_impl.c', '-o', native_obj],
                timeout=600 if args.sanitize else 240)
            native_objects[opt] = native_obj
            for family, source in sources.items():
                tag = family + '.' + opt
                try:
                    obj = objects_dir / (tag + '.o')
                    run(tag + '.compile', [cc, *host_flags, '-' + opt, '-c', source, '-o', obj])
                    roots, paths = closure(obj, family, tag)
                    units = resident_objects(paths, opt)
                    binary = out / (tag + ('.exe' if windows else ''))
                    run(tag + '.link', [cc, obj, native_obj, *units, *systems, '-o', binary])
                    data, _ = run(tag + '.run', [binary, child])
                    found = re.findall(WITNESSES[family], data)
                    assert len(found) == 1
                    groups = found[0] if isinstance(found[0], tuple) else (found[0],)
                    counts = list(map(int, groups))
                    assert all(n > 0 for n in counts)
                    records.append(dict(Family=family, Optimization=opt, ResidentRoots=roots,
                                        ResidentSources=paths, CompletePrefixes=counts,
                                        Image=dict(Path=str(binary), Sha256=sha(binary))))
                    print(data.decode(errors='replace'), end='', flush=True)
                except Exception as error:
                    failures.append(dict(Name=tag, Error=repr(error)))
        if 'future' in families:
            source = sources['future']
            original = source.read_text(encoding='utf-8-sig')
            needle = 'processReady(&input,promise,NULL,frame);'
            assert original.count(needle) == 3
            last_level = levels[-1]
            positive = next(p for p in records if p['Family'] == 'future' and p['Optimization'] == last_level)
            for name, replacement in (
                    ('old-arity', 'processReady(&input,promise,NULL);'),
                    ('null-frame', 'fputs("NULL_FRAME_CONTROL\\n",stderr);processReady(&input,promise,NULL,NULL);')):
                modified = out / (name + '.c')
                modified.write_text(original.replace(needle, replacement, 1), encoding='utf-8')
                obj = objects_dir / (name + '.o')
                if name == 'old-arity':
                    data, _ = run(name + '.compile', [cc, *host_flags, '-' + last_level, '-c', modified, '-o', obj],
                                  reject=True, compile_rejection=True)
                    assert b'too few arguments' in data and not obj.exists()
                    controls.append(dict(Name=name, CompileRejected=True))
                else:
                    run(name + '.compile', [cc, *host_flags, '-' + last_level, '-c', modified, '-o', obj])
                    binary = out / (name + ('.exe' if windows else ''))
                    run(name + '.link', [cc, obj, native_objects[last_level],
                                        *resident_objects(positive['ResidentSources'], last_level),
                                        *systems, '-o', binary])
                    data, code = run(name + '.run', [binary, child], reject=True)
                    assert code == (3 if windows else -6) and b'NULL_FRAME_CONTROL' in data
                    controls.append(dict(Name=name, Compiled=True, NullFrameAborted=True))
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        report['NativeObjects'] = [dict(Optimization=o, Path=str(p), Sha256=sha(p)) for o, p in native_objects.items()]
        report['SourceContractAcceptance'] = (len(records) == len(levels) * len(families) and
                                              len(controls) == (2 if 'future' in families else 0) and not failures)
        report['Complete'] = report['InputsUnchanged'] and report['SourceContractAcceptance']
    except Exception as error:
        failures.append(dict(Name='gate', Error=repr(error)))
    report['PhaseCount'] = len(commands)
    report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'], Proofs=len(records), Failures=failures)), flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
