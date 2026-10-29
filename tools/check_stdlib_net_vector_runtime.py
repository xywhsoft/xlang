"""Real pruned vector/batch providers, independent API and fixture closure roots.
Only literal own loopback IPv4/IPv6 ordinary I/O and offline lifecycle checks.
No external network, ICMP/probes, multicast, or network-security testing.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', action='append', required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--discovery', action='store_true', help='One O0 compile/run per closure; collect failures, not final acceptance')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    compilers = [Path(shutil.which(item) or item).resolve() for item in args.compiler]
    gcc = next(cc for cc in compilers if cc.stem.lower() != 'tcc')
    plan_source = root / 'test/support/net_vector_batch_plan.c'
    runtime_source = root / 'test/support/net_vector_batch_runtime.c'
    files = {Path(__file__).resolve(), *compilers, plan_source, runtime_source, root / 'test/support/executor_catalog_plan.c'}
    for directory in ('src', 'lib/xrt', 'lib/xruntime', 'release/include'):
        files.update(path for path in (root / directory).rglob('*') if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs = {str(path): sha(path) for path in sorted(files)}
    out.mkdir(parents=True)
    rows, products, failures = [], [], []
    plans = {}
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Inputs=inputs,
                  Rows=rows, Products=products, Failures=failures, Plans=plans,
                  DiscoveryOnly=args.discovery, InstrumentedSelectedClosure=False,
                  OwnLoopbackOnly=True, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  MulticastTraffic=False, ShippingLibraryAcceptance=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def run(name, command, proof=None, sanitize=False):
        log = out / (name + '.log')
        env = dict(os.environ)
        if sanitize:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        command = list(map(str, command))
        start = time.monotonic()
        with log.open('wb') as stream:
            result = subprocess.run(command, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        rows.append(dict(Name=name, Command=command, Exit=result.returncode, Log=str(log),
                         Sha256=sha(log), Seconds=time.monotonic() - start))
        save()
        print(name + ': ' + str(result.returncode), flush=True)
        assert result.returncode == 0, str(log)
        text = log.read_text(encoding='utf-8', errors='replace')
        if proof:
            assert proof in text, str(log)
        return text

    def flags(cc):
        result = [cc, '-std=c11', '-Wall', '-Werror', '-I' + str(root), '-I' + str(root / 'release'),
                  '-I' + str(root / 'release/include'), '-I' + str(out)]
        if cc.stem.lower() != 'tcc':
            result += ['-Wextra']
        if os.name != 'nt':
            result += ['-D_GNU_SOURCE']
        return result

    libraries = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']
    extension = '.exe' if os.name == 'nt' else ''
    names = ('progress', 'packet', 'text', 'bytes', 'buffer', 'receive', 'batch', 'all')
    options = ('O0',) if args.discovery else ('O0', 'O2')
    save()
    try:
        for opt in options:
            plan = out / ('plan.' + opt + extension)
            run('plan.' + opt + '.build', [*flags(gcc), '-' + opt, '-Wno-missing-field-initializers',
                '-ffunction-sections', '-fdata-sections', '-fno-jump-tables', '-fno-asynchronous-unwind-tables',
                '-fno-unwind-tables', plan_source, root / 'src/compile/xl6_compile_unit.c', '-Wl,--gc-sections', '-o', plan])
            product(plan)
            run('plan.' + opt + '.run', [plan], 'Vector/batch plan: eight independent API closures')
            if opt == 'O0':
                for name in names:
                    sources = run(name + '.sources', [plan, '--sources', name]).splitlines()
                    api = run(name + '.api-sources', [plan, '--api-sources', name]).splitlines()
                    assert len(sources) == len(set(sources)) and set(api).issubset(set(sources))
                    plans[name] = dict(ApiSources=api, TestSources=sources)
                    for kind in ('modules', 'domains'):
                        text = run(name + '.' + kind, [plan, '--' + kind, name])
                        assert '_MODULE_ALL' not in text
                        target = out / (name + '.' + kind + '.h')
                        target.write_text(text, encoding='utf-8')
                        product(target)
                    impl = out / (name + '.impl.c')
                    impl.write_text('#include "' + name + '.modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'
                                    '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n', encoding='utf-8')
                    product(impl)
        for index, name in enumerate(names):
            directory = out / name
            directory.mkdir()
            domain = directory / 'net-vector-domains.h'
            shutil.copy2(out / (name + '.domains.h'), domain)
            product(domain)
            providers = [root / path for path in plans[name]['TestSources']]
            for number, cc in enumerate(compilers):
                for opt in options:
                    label = name + '.' + str(number) + '.' + opt
                    binary = directory / (str(number) + '.' + opt + extension)
                    try:
                        run(label + '.build', [*flags(cc), '-I' + str(directory), '-DTEST_ROOT=' + str(index),
                            '-' + opt, runtime_source, out / (name + '.impl.c'), *providers, *libraries, '-o', binary])
                        product(binary)
                        run(label + '.run', [binary], 'Vector/batch runtime: exact values/owning graph')
                    except Exception as error:
                        failures.append(dict(Name=label, Error=repr(error)))
                        save()
            if args.sanitize:
                label = name + '.instrumented'
                binary = directory / 'instrumented'
                try:
                    run(label + '.build', [*flags(gcc), '-I' + str(directory), '-DTEST_ROOT=' + str(index),
                        '-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                        runtime_source, out / (name + '.impl.c'), *providers, *libraries, '-o', binary])
                    product(binary)
                    run(label + '.run', [binary], 'Vector/batch runtime: exact values/owning graph', True)
                except Exception as error:
                    failures.append(dict(Name=label, Error=repr(error)))
                    save()
        report['InstrumentedSelectedClosure'] = args.sanitize
        report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
        report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
        save()
        assert report['Complete'], failures
    except BaseException as error:
        failures.append(dict(Name='batch', Error=repr(error)))
        save()
        raise


if __name__ == '__main__':
    main()
