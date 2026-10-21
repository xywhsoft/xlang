"""Real managed Proxy parent graph, exact policy population and OOM prefixes.

Only offline ownership/allocation checks and ordinary owned literal loopback.
No system DNS, external endpoints, malformed protocols or security testing.
Not public-language or shipping DLL/SO/VFS acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--compiler', action='append', required=True)
    p.add_argument('--sanitize', action='store_true')
    args = p.parse_args()
    root, out = Path(__file__).resolve().parents[1], args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    compilers = [Path(shutil.which(c) or c).resolve() for c in args.compiler]
    gcc = next(c for c in compilers if c.stem != 'tcc')
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files = {Path(__file__).resolve(), *compilers}
    for area in ('src', 'lib/xrt', 'lib/xruntime', 'release/include', 'test/support'):
        files.update(f for f in (root / area).rglob('*') if f.is_file() and f.suffix in ('.c', '.h', '.inc'))
    inputs = {str(f): sha(f) for f in sorted(files)}
    out.mkdir(parents=True)
    rows, products, failures = [], [], []
    report = dict(Complete=False, Inputs=inputs, Rows=rows, Products=products, Failures=failures, Plans={},
        OrdinaryLiteralLoopback=True, ExternalNetworkTests=False, NetworkSecurityTests=False,
        PublicLanguageApiAccepted=False, ShippingLibraryAcceptance=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def run(name, command, instrumented=False, reject=False):
        command = list(map(str, command))
        invocation, env = command, dict(os.environ)
        if command[0] in map(str, compilers) and Path(command[0]).stem != 'tcc':
            rsp = out / (name + '.rsp')
            rsp.write_text('\n'.join('"' + a.replace('\\', '/') + '"' for a in command[1:]) + '\n', encoding='utf8')
            product(rsp)
            invocation = [command[0], '@' + str(rsp)]
        if instrumented:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log = out / (name + '.log')
        with log.open('wb') as stream:
            code = subprocess.run(invocation, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=180).returncode
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=code, ExpectedRejection=reject,
            Log=str(log), Sha256=sha(log)))
        save()
        print(name, code, flush=True)
        assert (code != 0 if reject else code == 0), (name, str(log))
        return log.read_text(encoding='utf8', errors='replace')

    def flags(cc, mode):
        return [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', *mode, '-I' + str(root),
            '-I' + str(root / 'release'), '-I' + str(root / 'release/include'), '-I' + str(out),
            *(['-D_GNU_SOURCE'] if os.name != 'nt' else [])]

    ext = '.exe' if os.name == 'nt' else ''
    libs = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']
    save()
    try:
        for opt in ('O0', 'O2'):
            plan = out / ('plan.' + opt + ext)
            run('plan.' + opt + '.build', flags(gcc, ['-' + opt]) + ['-Wno-missing-field-initializers',
                '-ffunction-sections', '-fdata-sections', '-fno-jump-tables', '-fno-asynchronous-unwind-tables',
                '-fno-unwind-tables', root / 'test/support/tcp_proxy_plan.c', root / 'src/compile/xl6_compile_unit.c',
                '-Wl,--gc-sections', '-o', plan])
            product(plan)
            run('plan.' + opt + '.run', [plan])
            if opt == 'O0':
                for kind in ('sources', 'modules', 'domains'):
                    text = run('plan.' + kind, [plan, '--' + kind, 'resource'])
                    report['Plans'][kind] = text.splitlines() if kind == 'sources' else text
                    if kind != 'sources':
                        header = out / ('tcp-resource-' + kind + '.h')
                        header.write_text(text, encoding='utf8')
                        product(header)
                sources = report['Plans']['sources']
                assert len(sources) == len(set(sources)) and 'src/runtime/xl6_execution_tcp_dial.c' not in sources
        for index, cc in enumerate(compilers):
            modes = [('O0', ['-O0']), ('O2', ['-O2'])]
            if args.sanitize and cc.stem != 'tcc':
                modes.append(('instrumented', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']))
            for opt, mode in modes:
                name = f'{index}.{opt}'
                binary = out / (name + ext)
                run(name + '.build', flags(cc, mode) + [root / 'test/support/tcp_proxy_resource.c',
                    *[root / s for s in report['Plans']['sources']], *libs, '-o', binary])
                product(binary)
                for backend, argv in (('select', []), ('native', ['native'])):
                    proof = run(name + '.' + backend, [binary, *argv], opt == 'instrumented')
                    assert all(s in proof for s in ('managed TCP resources: graphs=24', 'managed Proxy: graphs=16 budgets=64', 'balanced backend='))
        original = root / 'src/runtime/xl6_ownership_domain_net_proxy_dial_native.c'
        body = original.read_text(encoding='utf8')
        for method, getter in (('NetRuntime', 'xrtNetProxyDialRuntimeOwnershipV1Get'),
                ('ProxyDialDone', 'xrtNetProxyDialFutureDialPolicyV1Get'), ('StreamEvents', 'xrtNetProxyDialStreamOwnershipV1Get')):
            needle = f'xl6OwnershipCatalogAdd{method}V1(catalog,{getter}())'
            assert body.count(needle) == 1
            name = 'missing-' + method.lower()
            mutant, binary = out / (name + '.c'), out / (name + ext)
            mutant.write_text(body.replace(needle, 'true'), encoding='utf8')
            product(mutant)
            selected = [mutant if root / s == original else root / s for s in report['Plans']['sources']]
            run(name + '.build', flags(gcc, ['-O0']) + ['-I' + str(root / 'src/runtime'),
                root / 'test/support/tcp_proxy_resource.c', *selected, *libs, '-o', binary])
            product(binary)
            proof = run(name + '.refused', [binary], reject=True)
            assert 'root&&inner&&done&&events&&producer' in proof
        report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
        report['ProductsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in products)
        assert all(report[key] for key in ('InputsUnchanged', 'LogsUnchanged', 'ProductsUnchanged'))
        report['Complete'] = True
        save()
    except BaseException as error:
        failures.append(repr(error))
        save()
        raise


if __name__ == '__main__':
    main()
