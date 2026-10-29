"""Full queued-root worker completion population against genuine resident DLL/SO products.
The shipping CLI emits C and lowered IR; the observer does not rebuild compiler
objects or native providers. Preserve 3978 executions, 90 real worker result
allocation faults, 972 discarded Futures and five complete allocation prefixes.
The allocation-entry observer redirects generated-C calls only; worker TLS and
all runtime allocation/retirement remain in the selected, unmodified product.
Module logical quiescence is not an OS join or a module-unmap proof.
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


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    windows = os.name == 'nt'
    cc, inspector = shutil.which('gcc'), shutil.which('objdump' if windows else 'readelf')
    assert cc and inspector
    task = root
    sys.path.insert(0, str(root / 'tools'))
    from gen_tcc_vfs_resources import load_resources

    name = 'xlir_async_worker_completion'
    fixture, host, runtime = [task / 'test/cases' / (name + suffix)
                              for suffix in ('.xl', '_host.c', '_runtime.c')]
    helpers = [task / 'test/cases' / h for h in ('xlir_launch_test.h', 'xlir_owner_fault_runtime.h')]
    root_native=task/'test/cases/xlir_async_root_launch_native.c'
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    products = [root / 'release' / n for n in
                (('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so'))]
    recorded = {Path(row['Path']).resolve(): row['Sha256'] for row in build['Products']}
    assert all(recorded.get(p) == sha(p) for p in products)
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), Path(inspector),
             args.build_report.resolve(), fixture, host, runtime, root_native, *helpers}
    for row in build['Inputs'] + build['Products']:
        assert sha(row['Path']) == row['Sha256'], row['Path']
        files.add(Path(row['Path']))
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]

    out.mkdir(parents=True)
    isolated, sdk, temporary = out / 'host', out / 'sdk', out / 'tmp'
    release = isolated / 'release'
    release.mkdir(parents=True)
    sdk.mkdir()
    temporary.mkdir()
    for product in products:
        shutil.copy2(product, release / product.name)
        if windows:
            shutil.copy2(product, isolated / product.name)
    source = isolated / fixture.name
    shutil.copy2(fixture, source)
    for header in (root / 'release/include').glob('*.h'):
        shutil.copy2(header, sdk / header.name)
    build_dir = root / ('build/tmp' if windows else 'build/linux')
    manifest = root / 'tools' / ('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    dist = root / 'res/tcc/win64/x86_64' if windows else build_dir / 'tcc-dist'
    for resource in load_resources(manifest, root, dist, build_dir / 'xrt.o', build_dir):
        if resource.virtual.startswith('include/__xl_runtime/'):
            target = sdk / Path(resource.virtual).relative_to('include')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(resource.source, target)
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
           ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    env.update(TMPDIR=str(temporary), TEMP=str(temporary), TMP=str(temporary))
    rows, proofs, controls, failures, root_proofs = [], [], [], [], []
    report = dict(Complete=False, InputsUnchanged=False, Scope=__doc__, Inputs=inputs, Rows=rows,
                  Proofs=proofs, Counterexamples=controls, Failures=failures, ShippingArtifact=True,
                  RootLaunchProofs=root_proofs,
                  RuntimeInstrumented=False, GeneratedAllocationEntryObserver=True,
                  ProviderOverrides=False, CompilerSourceObjects=False, NativeProductCompletionAcceptance=False,
                  IrMutationAcceptance=False, WorkerWideOomAcceptance=False, OsThreadJoinAcceptance=False,
                  ModuleUnmapAcceptance=False, Full53Acceptance=False)

    def run(label, command, reject=False):
        command = list(map(str, command))
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=isolated, env=env, capture_output=True, timeout=180)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out / (label + '.log')
        log.write_bytes(data)
        rows.append(dict(Name=label, Command=command, Exit=code, ExpectedRejection=reject,
                         Seconds=time.monotonic() - start, Log=str(log), LogSha256=sha(log)))
        print(label + ': ' + str(code), flush=True)
        assert (code not in (0, 'timeout')) if reject else code == 0, (label, data[-6000:])
        return data

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I' + str(sdk),
             '-I' + str(root), '-I' + str(task / 'test/cases')]
    if not windows:
        flags += ['-D_GNU_SOURCE']
    generated_compat = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
                        '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                        '-Wno-missing-field-initializers', '-Wno-pointer-sign']

    def libraries(product):
        return [release / product.name] if windows else ['-L' + str(release), '-l' + product.stem[3:],
                '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']

    try:
        cli = release / products[0].name
        # The same basename keeps the public header guard byte-identical.
        for folder in (out / 'generated', out / 'repeat'):
            folder.mkdir()
            run(folder.name + '.cgen', [cli, 'cgen', source, '--emit', folder / 'worker.c',
                                       '--emit-header', folder / 'worker.h'])
            run(folder.name + '.ir', [cli, 'ir', source, '--lowered', '--format', 'json',
                                     '--emit', folder / 'worker.json'])
        generated = out / 'generated'
        for suffix in ('c', 'h', 'json'):
            assert sha(generated / ('worker.' + suffix)) == sha(out / 'repeat' / ('worker.' + suffix))
        ir = json.loads((generated / 'worker.json').read_text())
        workers = [t for t in ir['callTargets'] if t['asyncCompletionAbiName'] == 'retire-before-publish']
        assert len(workers) == ir['asyncMachineCount'] == 6 and ir['asyncSuspensionCount'] == 0
        assert all(m['executionAbi'] == 3 and m['suspensionCount'] == 0 for m in ir['asyncMachines'])
        generated_text = (generated / 'worker.c').read_text()
        assert 'xl6ContinuationFrameLaunchV1(frame,' in generated_text and 'xlir_async_pool' not in generated_text
        assert [t['asyncResultAbiName'] for t in workers] == [
            'copy-i64', 'void', 'owned-string', 'retained-xvalue', 'retained-closure', 'copy-i64']
        for worker in workers:
            assert worker['asyncDispatch'] == 1 and worker['parameterCount'] == 3 and worker['asyncMachine'] < 6
            assert worker['workerErrorAbiName'] == 'may-raise'
            assert worker['asyncCaptureAbis'] == ['retain-xvalue', 'retain-xvalue', 'copy-value']
        binding = generated / 'worker.h.bind.h'
        binding.write_text(''.join('#define WORKER_COMPLETION_' + str(i) + ' xlir_async_start_' +
                                   str(worker['symbol']) + '\n' for i, worker in enumerate(workers)))
        definitions = ['-DXLIR_WORKER_COMPLETION_SOURCE="' + (generated / 'worker.c').as_posix() + '"',
                       '-DXLIR_WORKER_COMPLETION_BINDING="' + binding.as_posix() + '"']
        for product in products[1:]:
            for opt in ('O0', 'O2'):
                tag = product.stem + '.' + opt
                binary = isolated / (tag + ('.exe' if windows else ''))
                try:
                    root_binary=isolated/(tag+'.root'+('.exe' if windows else ''))
                    run(tag+'.root.build',[cc,*flags,'-'+opt,root_native,*libraries(product),'-o',root_binary])
                    root_output=run(tag+'.root.run',[root_binary])
                    prefix=re.search(rb'queued root launch: (\d+) complete allocation prefixes',root_output)
                    assert prefix and b'32 real single-worker barriers' in root_output and b'exact memory balance' in root_output
                    root_proofs.append(dict(Product=product.name,HostOptimization=opt,Barriers=32,PrefixRuns=int(prefix[1])))
                    run(tag + '.build', [cc, *flags, *generated_compat, '-' + opt, *definitions,
                                        host, runtime, *libraries(product), '-o', binary])
                    imports = run(tag + '.imports', [inspector, '-p' if windows else '-d', binary])
                    deps = re.findall(rb'DLL Name:\s*([^\s]+)', imports) if windows else re.findall(
                        rb'Shared library:\s*\[([^\]]+)\]', imports)
                    selected = {d.decode().lower() for d in deps} & {p.name.lower() for p in products[1:]}
                    assert selected == {product.name.lower()}, selected
                    output = run(tag + '.run', [binary])
                    assert b'3978 executions, 90 real worker result allocation failures' in output
                    assert b'972 discarded Futures' in output and b'balanced allocations' in output
                    prefix = re.search(rb'worker allocation prefixes: 5 complete lanes, (\d+) runs, (\d+) real worker TLS failures', output)
                    assert prefix and int(prefix[1]) == int(prefix[2]) + 5
                    proofs.append(dict(Product=product.name, HostOptimization=opt, Executions=3978,
                                       ResultAllocationFaults=90, DiscardedFutures=972,
                                       PrefixRuns=int(prefix[1]), PrefixFaults=int(prefix[2]),
                                       CompletePrefixLanes=5, FullOriginalPopulation=True))
                except Exception as error:
                    failures.append(dict(Name=tag, Error=repr(error)))
        # Counterexamples prove that both propagated callback failures and actual
        # allocator refusal are observed. They are test-only source mutations,
        # never product patches or substitutes for positive completion proofs.
        bad_host = out / 'missing-body-failure.c'
        text = host.read_text()
        assert text.count('xl6ErrorScopePropagateFailure(error);') == 2
        bad_host.write_text(text.replace('xl6ErrorScopePropagateFailure(error);',
                                        'xrtErrorFree(error.error);', 1))
        bad_runtime = out / 'missing-result-fault.c'
        text = runtime.read_text()
        assert text.count('assert(xrtMemDebugFailAfter(0));') == 1
        bad_runtime.write_text(text.replace('assert(xrtMemDebugFailAfter(0));',
                                           'assert(xrtMemDebugFailAfter(1));'))
        for label, sources in (('missing-body-failure', [bad_host, runtime]),
                               ('missing-result-fault', [host, bad_runtime])):
            for product in products[1:]:
                tag = label + '.' + product.stem
                binary = isolated / (tag + ('.exe' if windows else ''))
                try:
                    run(tag + '.build', [cc, *flags, *generated_compat, '-O2', *definitions,
                                        *sources, *libraries(product), '-o', binary])
                    output = run(tag + '.run', [binary], reject=True)
                    assert re.search(rb'assertion.*failed|assert.*failed', output, re.I), output[-4000:]
                    if label == 'missing-body-failure':assert b'worker outcome' in output,output[-4000:]
                    else:assert re.search(rb'value\s*==\s*NULL',output),output[-4000:]
                    controls.append(dict(Name=label, Product=product.name, AssertionRejected=True))
                except Exception as error:
                    failures.append(dict(Name=tag, Error=repr(error)))
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        assert all(sha(release / p.name) == sha(p) for p in products)
        report['NativeProductCompletionAcceptance'] = len(proofs) == len(controls) == len(root_proofs) == 4 and not failures
        report['Complete'] = report['InputsUnchanged'] and report['NativeProductCompletionAcceptance']
    except Exception as error:
        failures.append(dict(Name='gate', Error=repr(error)))
    report['PhaseCount'] = len(rows)
    report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'], Proofs=len(proofs), Failures=failures)), flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
