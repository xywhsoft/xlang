"""Independent chunked buffer/framing resident closures, entirely offline.
No sockets, host names, traffic, protocol fuzzing or security tests. No shipping claim.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', action='append', required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--discovery', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    compilers = [Path(shutil.which(name) or name).resolve() for name in args.compiler]
    gcc = next(cc for cc in compilers if cc.stem.lower() != 'tcc')
    plan_source, runtime_source = (root / ('test/support/net_frame_' + name + '.c') for name in ('plan', 'runtime'))
    files = {Path(__file__).resolve(), *compilers, plan_source, runtime_source, root / 'test/support/executor_catalog_plan.c'}
    for folder in ('src', 'lib/xrt', 'lib/xruntime', 'release/include'):
        files.update(path for path in (root / folder).rglob('*') if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs = {str(path): sha(path) for path in sorted(files)}
    out.mkdir(parents=True)
    rows, products, failures, plans = [], [], [], {}
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Inputs=inputs,
                  Rows=rows, Products=products, Failures=failures, Plans=plans,
                  DiscoveryOnly=args.discovery, InstrumentedSelectedClosure=False,
                  NetworkTraffic=False, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  ShippingLibraryAcceptance=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def run(name, command, proof=None, sanitize=False):
        log = out / (name + '.log'); env = dict(os.environ)
        if sanitize:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        command = list(map(str, command))
        with log.open('wb') as stream:
            result = subprocess.run(command, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=240)
        rows.append(dict(Name=name, Command=command, Exit=result.returncode, Log=str(log), Sha256=sha(log)))
        save(); print(name + ': ' + str(result.returncode), flush=True)
        assert result.returncode == 0, str(log)
        text = log.read_text(encoding='utf-8', errors='replace')
        if proof:assert proof in text, str(log)
        return text

    def flags(cc):
        result = [cc, '-std=c11', '-Wall', '-Werror', '-I' + str(root), '-I' + str(root / 'release'),
                  '-I' + str(root / 'release/include'), '-I' + str(out)]
        if cc.stem.lower() != 'tcc':result += ['-Wextra']
        if os.name != 'nt':result += ['-D_GNU_SOURCE']
        return result

    libraries = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']
    extension = '.exe' if os.name == 'nt' else ''
    names = ('buffer-core', 'buffer', 'line-core', 'line', 'length-core', 'length', 'result', 'all')
    options = ('O0',) if args.discovery else ('O0', 'O2')
    save()
    try:
        for opt in options:
            plan = out / ('plan.' + opt + extension)
            run('plan.' + opt + '.build', [*flags(gcc), '-' + opt, '-Wno-missing-field-initializers',
                '-ffunction-sections', '-fdata-sections', '-fno-jump-tables', '-fno-asynchronous-unwind-tables',
                '-fno-unwind-tables', plan_source, root / 'src/compile/xl6_compile_unit.c', '-Wl,--gc-sections', '-o', plan])
            product(plan);run('plan.' + opt + '.run', [plan], 'Frame plans: eight independent closures')
            if opt == 'O0':
                for name in names:
                    sources = run(name + '.sources', [plan, '--sources', name]).splitlines()
                    assert len(sources) == len(set(sources));plans[name] = dict(Sources=sources)
                    for kind in ('modules', 'domains'):
                        content = run(name + '.' + kind, [plan, '--' + kind, name]);assert '_MODULE_ALL' not in content
                        target = out / (name + '.' + kind + '.h');target.write_text(content, encoding='utf-8');product(target)
                    impl = out / (name + '.impl.c')
                    impl.write_text('#include "' + name + '.modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'
                                    '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n', encoding='utf-8');product(impl)
        for index, name in enumerate(names):
            providers = [root / path for path in plans[name]['Sources']]
            for number, cc in enumerate(compilers):
                for opt in options:
                    label = name + '.' + str(number) + '.' + opt;binary = out / (label + extension)
                    try:
                        run(label + '.build', [*flags(cc), '-DTEST_ROOT=' + str(index), '-' + opt,
                            runtime_source, out / (name + '.impl.c'), *providers, *libraries, '-o', binary])
                        product(binary);run(label + '.run', [binary], 'offline lifecycle balanced')
                    except Exception as error:
                        failures.append(dict(Name=label, Error=repr(error)));save()
            if args.sanitize:
                label = name + '.instrumented';binary = out / label
                try:
                    run(label + '.build', [*flags(gcc), '-DTEST_ROOT=' + str(index), '-O1', '-g',
                        '-fno-omit-frame-pointer', '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                        runtime_source, out / (name + '.impl.c'), *providers, *libraries, '-o', binary])
                    product(binary);run(label + '.run', [binary], 'offline lifecycle balanced', sanitize=True)
                except Exception as error:
                    failures.append(dict(Name=label, Error=repr(error)));save()
        report['InstrumentedSelectedClosure'] = args.sanitize
        report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
        report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
        save();assert report['Complete'], failures
    except BaseException as error:
        failures.append(dict(Name='batch', Error=repr(error)));save();raise


if __name__ == '__main__':
    main()
