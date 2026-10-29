"""Retained managed creator identity for three thread-confined resources.

Use only actual runtime/compiler DLL/SO APIs, O0/O2 consumer hosts, one resource
per real managed creator. OS TLS exit callbacks witness XRT entry return before
metadata inspection. Never dereference a retired thread or force OS-ID reuse.
Check correct-owner operations, later wrong-owner rejection, remote destruction,
ambient errors, creator pin/release, worker-local constructor OOM and full heap
ledgers. Optional old genuine products must reproduce missing identity pins.
This is not OS-join, external host-thread ID reuse or full53 acceptance.
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


FAMILIES = ('text-iterator', 'unicode-validator', 'buffer-iterator')
GUARDS = (3, 5, 4)
WITNESS = re.compile(rb'resource-identity family=(\d+) point=(\d+) hit=([01]) creator-pinned=([01]) '
                     rb'expected=([01]) guards=(\d+) native-entry-returned=1 caller-isolated=1 balanced=1')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--baseline-root', type=Path)
    parser.add_argument('--baseline-build-report', type=Path)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    assert bool(args.baseline_root) == bool(args.baseline_build_report)
    windows = os.name == 'nt'
    cc, nm, inspector = [shutil.which(n) for n in ('gcc', 'nm', 'objdump' if windows else 'readelf')]
    assert cc and nm and inspector
    task = Path(__file__).resolve().parents[1]
    host = task / 'test/cases/stdlib_resource_thread_identity_product_host.c'
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), Path(nm), Path(inspector), host}
    candidates = {}
    for label, selected, receipt in (
            ('current', root, args.build_report),
            ('baseline', args.baseline_root, args.baseline_build_report)):
        if not selected:
            continue
        selected, receipt = selected.resolve(), receipt.resolve()
        report = json.loads(receipt.read_text(encoding='utf-8-sig'))
        assert report['Complete'] and report['InputsUnchanged'] and report['Exit'] == 0
        products = [selected / 'release' / n for n in
                    (('xl.dll', 'xlc.dll') if windows else ('libxl.so', 'libxlc.so'))]
        recorded = {Path(r['Path']).resolve(): r['Sha256'] for r in report['Products']}
        assert all(recorded.get(p) == sha(p) for p in products)
        files.add(receipt)
        for row in report['Inputs'] + report['Products']:
            assert sha(row['Path']) == row['Sha256'], row['Path']
            files.add(Path(row['Path']))
        candidates[label] = products
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    out.mkdir()
    rows, proofs, controls, failures = [], [], [], []
    result = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
                  Proofs=proofs, Controls=controls, Failures=failures, Scope=__doc__,
                  BaselineRequested=bool(args.baseline_root), ExplicitSubset=False,
                  CompilerSourceObjects=False, RuntimeImplementationObjects=False,
                  ProviderOverrides=False, ForcedOsIdReuse=False, OsThreadJoinAcceptance=False,
                  RetainedManagedCreatorIdentityAcceptance=False, Full53Acceptance=False)
    environment = {k: v for k, v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
                   ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    if not windows:
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(root), '-I' + str(root / 'release/include')]
    if not windows:
        flags += ['-D_GNU_SOURCE']

    def run(name, argv, cwd):
        argv = list(map(str, argv))
        started = time.monotonic()
        try:
            process = subprocess.run(argv, cwd=cwd, env=environment, capture_output=True, timeout=60)
            code, data = process.returncode, process.stdout + process.stderr
        except subprocess.TimeoutExpired as error:
            code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out / (name + '.log')
        log.write_bytes(data)
        rows.append(dict(Name=name, Command=argv, Exit=code, Log=str(log), LogSha256=sha(log),
                         Seconds=time.monotonic() - started))
        return code, data

    def require(name, argv, cwd):
        code, data = run(name, argv, cwd)
        assert code == 0, (name, code, data.decode(errors='replace')[-6000:])
        return data

    def observe(name, binary, directory, family, budget, baseline):
        args = [binary, family] + ([] if budget is None else [budget])
        code, data = run(name, args, directory)
        expected_code = 2 if baseline else 0
        assert code == expected_code, (name, code, data.decode(errors='replace')[-6000:])
        found = WITNESS.findall(data)
        assert len(found) == 1, (name, data)
        actual, point, hit, pinned, expected, guards = map(int, found[0])
        assert actual == family and point == (2**64 - 1 if budget is None else budget)
        assert expected == 1 - hit and guards == (0 if hit else GUARDS[family])
        assert pinned == (0 if baseline else expected)
        if baseline:
            assert not hit and b'managed creator identity was not retained' in data
        return bool(hit)

    try:
        objects = {}
        for opt in ('O0', 'O2'):
            obj = out / ('consumer.' + opt + '.o')
            require(opt + '.compile', [cc, *flags, '-' + opt, '-c', host, '-o', obj], out)
            definitions = require(opt + '.definitions', [nm, '--defined-only', obj], out)
            assert not re.search(rb'\b[TDB]\s+(?:xrt|xl6)\w+\r?$', definitions, re.M), 'Host defines a runtime provider'
            objects[opt] = obj
        for label, products in candidates.items():
            for product in products:
                directory = out / (label + '.' + product.stem)
                (directory / 'release').mkdir(parents=True)
                shutil.copy2(product, directory / 'release' / product.name)
                if windows:
                    shutil.copy2(product, directory / product.name)
                libs = [directory / 'release' / product.name] if windows else [
                    '-L' + str(directory / 'release'), '-l' + product.stem[3:], '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']
                for opt, obj in objects.items():
                    name = label + '.' + product.stem + '.' + opt
                    binary = directory / (opt + ('.exe' if windows else ''))
                    try:
                        require(name + '.link', [cc, obj, *libs, '-o', binary], directory)
                        imports = require(name + '.imports', [inspector, '-p' if windows else '-d', binary], directory)
                        names = re.findall(rb'DLL Name:\s*([^\s]+)', imports) if windows else re.findall(rb'Shared library:\s*\[([^\]]+)\]', imports)
                        actual = {n.decode().lower() for n in names} & {p.name.lower() for p in products}
                        assert actual == {product.name.lower()}, actual
                    except Exception as error:
                        failures.append(dict(Name=name + '.link', Error=repr(error)))
                        continue
                    for family in range(3):
                        tag = name + '.' + FAMILIES[family]
                        try:
                            assert not observe(tag + '.baseline', binary, directory, family, None, label == 'baseline')
                            if label == 'baseline':
                                controls.append(dict(Name=tag, Product=product.name, Optimization=opt, Family=family,
                                                     ActualOldProduct=True, MissingCreatorPinRejected=True, CleanupBalanced=True))
                                continue
                            for point in range(32):
                                if not observe(tag + '.point.' + str(point), binary, directory, family, point, False):
                                    break
                            else:
                                raise AssertionError('Constructor allocation prefix incomplete')
                            assert point > 0
                            assert not observe(tag + '.confirmed', binary, directory, family, point + 1, False)
                            proofs.append(dict(Name=tag, Product=product.name, Optimization=opt, Family=family,
                                               AllocationFailures=point, FirstNoHit=point, Confirmation=point + 1,
                                               WrongCreatorGuards=GUARDS[family], NativeEntryReturned=True,
                                               IdentityPinReleased=True, CallerIsolated=True, HeapBalanced=True,
                                               Image=dict(Path=str(binary), Sha256=sha(binary))))
                            print(tag + ': complete ' + str(point), flush=True)
                        except Exception as error:
                            failures.append(dict(Name=tag, Error=repr(error)))
        result['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        result['RetainedManagedCreatorIdentityAcceptance'] = len(proofs) == 12 and not failures
        result['ActualOldProductsRejected'] = len(controls) == 12 if args.baseline_root else False
        result['Complete'] = (result['InputsUnchanged'] and result['RetainedManagedCreatorIdentityAcceptance'] and
                              (not args.baseline_root or result['ActualOldProductsRejected']))
    except Exception as error:
        failures.append(dict(Name='gate', Error=repr(error)))
    result['PhaseCount'] = len(rows)
    result['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out / 'results.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(Complete=result['Complete'], Proofs=len(proofs), Controls=len(controls), Failures=failures)), flush=True)
    return 0 if result['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
