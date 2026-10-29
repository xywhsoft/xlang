"""Managed Proxy current-source JIT, module, pruned AOT and sealed XLIR proof.

Only valid SOCKS5/HTTP exchanges over owned literal loopback. No external
network, system DNS, malformed protocol or security tests. This is not fresh
shipping DLL/SO/VFS acceptance or acceptance of the remaining network groups.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--native-root', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    manifest = args.source_build.resolve()
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    assert not out.exists()
    # Windows publishes build.json only after a successful, input-checked
    # link; Linux additionally journals explicit completion flags.
    if 'Complete' in build:
        assert build['Complete'] and build['InputsUnchanged']
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler'] == 1
    frozen = build['Inputs'] + build['Objects'] + build['Headers']
    assert all(sha(row['Path']) == row['Sha256'].lower() for row in frozen)
    driver = Path(build['Driver'])
    assert sha(driver) == build['DriverSha256'].lower()
    cc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    cases = tuple('stdlib_tcp_proxy_' + name for name in ('options', 'async', 'sync', 'cancel', 'sync_closed'))
    negatives = tuple('bad_stdlib_tcp_proxy_' + name for name in ('type', 'options', 'token', 'deadline', 'private'))
    contract = root / 'test/cases/xlir_tcp_managed_contract.c'
    inputs = {str(path): sha(path) for path in (Path(__file__).resolve(), manifest, driver, cc, contract,
        *[root / ('test/cases/' + name + '.xl') for name in cases + negatives])}
    env = dict(os.environ)
    if os.name != 'nt':
        assert args.native_root
        env['XL6_SOURCE_NATIVE_ROOT'] = str(args.native_root.resolve())
    out.mkdir(parents=True)
    rows, products, failures = [], [], []
    report = dict(Complete=False, Inputs=inputs, Rows=rows, Products=products, Failures=failures,
        SourceBuild=str(manifest), SourceProductAcceptance=True, ShippingLibraryAcceptance=False,
        OrdinaryLiteralLoopback=True, ExternalNetworkTests=False, NetworkSecurityTests=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def run(name, command, reject=False):
        command = list(map(str, command))
        log = out / (name + '.log')
        started = time.monotonic()
        with log.open('wb') as stream:
            result = subprocess.run(command, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
        rows.append(dict(Name=name, Command=command, Exit=result.returncode, ExpectedRejection=reject,
            Log=str(log), Sha256=sha(log), Seconds=time.monotonic() - started))
        save()
        print(name, result.returncode, flush=True)
        assert result.returncode == (1 if reject else 0), (name, str(log))
        return log.read_text(encoding='utf8', errors='replace')

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(root), '-I' + str(root / 'release'),
        '-I' + str(root / 'release/include'), '-I' + str(driver.parent / 'include')]
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    generated_compat = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
        '-Wno-unused-but-set-variable', '-Wno-unused-label', '-Wno-missing-field-initializers']
    save()
    try:
        for case in cases:
            source = root / ('test/cases/' + case + '.xl')
            generated, ir = out / (case + '.c'), out / (case + '.json')
            run(case + '.emit', [driver, source, 'cgen-default', generated])
            product(generated)
            run(case + '.ir', [driver, source, 'ir-lowered', ir])
            product(ir)
            data = json.loads(ir.read_text())
            transport = not case.endswith('_options')
            asynchronous = case.endswith(('_async', '_sync', '_cancel'))  # sync fixture owns an async peer
            assert data['moduleExecutionOwnershipAbi'] == int(transport)
            assert data['moduleContinuationAbi'] == (2 if asynchronous else 0)
            assert data['moduleTaskPoolAbi'] == 'none'
            assert bool(data['moduleDeadlineOwnershipAbi']) == case.endswith(('_async', '_cancel'))
            text = generated.read_text()
            assert ('static XL6_ExecutionSourceV1* xlir_execution_source;' in text) == transport
            assert not any(symbol in text for symbol in ('xl6NetworkService', 'xl6_xrt_tcp', 'xlir_async_pool',
                'xrtNetProxyDialAsync(', 'xrtNetEngineCreate(', 'xrtCancelWatch('))
            if not asynchronous:
                assert 'xlir_continuation_executor' not in text
            if case.endswith('_async'):
                assert all('xl6TcpProxyDialStart' + kind + default + 'V1(' in text
                    for kind in ('', 'Cancel', 'Until', 'UntilCancel') for default in ('', 'Default'))
            if case.endswith('_sync'):
                assert all('xl6TcpProxyDial' + kind + default + 'V1(' in text
                    for kind in ('', 'Cancel', 'Until', 'UntilCancel') for default in ('', 'Default'))
            for opt in ('O0', 'O2'):
                obj = out / (case + '.' + opt + '.o')
                run(case + '.' + opt, [cc, *flags, *generated_compat, '-' + opt, '-c', generated, '-o', obj])
                product(obj)
            for mode in ('run', 'module'):
                run(case + '.' + mode, [driver, source, mode, out / (case + '.' + mode)])
            prefix = out / (case + '.aot')
            exe = Path(str(prefix) + ('.exe' if os.name == 'nt' else '.out'))
            argument = prefix if os.name == 'nt' else exe
            run(case + '.aot.build', [driver, source, 'exe', argument])
            product(exe)
            run(case + '.aot.run', [exe])
            native = Path(str(argument) + '.temps') / 'xl6_xrt_impl.c'
            product(native)
            native_text = native.read_text()
            assert '_MODULE_ALL' not in native_text
            for module in ('TLS', 'TASK_POOL', 'COROUTINE_SCHEDULER', 'NET_TCP_DIAL_FUTURE'):
                assert '#define XRT_MODULE_' + module + '\n' not in native_text, module
            imports = run(case + '.imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', exe]
                if os.name == 'nt' else ['readelf', '-d', exe])
            assert not any(name in imports.lower() for name in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'))
        for case in negatives:
            output = out / (case + '.c')
            diagnostic = run(case + '.refused', [driver, root / ('test/cases/' + case + '.xl'),
                'cgen-default', output], True)
            assert '[sem]' in diagnostic and not output.exists()
        objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
            ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
        assert len(objects) == len(build['Objects']) - 1
        for opt in ('O0', 'O2'):
            obj = out / ('contract.' + opt + '.o')
            run('contract.' + opt + '.build', [cc, *flags, '-Wno-missing-field-initializers', '-' + opt,
                '-c', contract, '-o', obj])
            product(obj)
            binary = out / ('contract.' + opt + ('.exe' if os.name == 'nt' else ''))
            links = [*objects, obj]
            if os.name == 'nt':
                links += [root / 'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
                if not (out / 'xl.dll').exists():
                    shutil.copy2(root / 'release/xl.dll', out / 'xl.dll')
                    product(out / 'xl.dll')
            else:
                lib = args.native_root.resolve() / 'release'
                links += ['-L' + str(lib), '-lxl', '-Wl,-rpath,' + str(lib), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
            rsp = out / ('contract.' + opt + '.rsp')
            rsp.write_text('\n'.join('"' + str(arg).replace('\\', '/') + '"' for arg in [*links, '-o', binary]) + '\n', encoding='utf8')
            product(rsp)
            run('contract.' + opt + '.link', [cc, '@' + str(rsp)])
            product(binary)
            proof = run('contract.' + opt + '.run', [binary, root / 'test/cases/stdlib_tcp_proxy_sync.xl',
                root / 'test/cases/stdlib_tcp_proxy_async.xl'])
            assert 'TCP XLIR:' in proof and 'before C/header output' in proof
        report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items()) and all(
            sha(row['Path']) == row['Sha256'].lower() for row in frozen)
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
