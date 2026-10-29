"""Resolver declaration/XLIR/language products, exact providers and owned OOM prefixes.
Only offline values and literal IPv4/IPv6 numeric fast paths; no sockets, host names,
external DNS/traffic, security tests or shipping-library acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess


CASES = (
    'stdlib_net_address_list_values', 'stdlib_net_resolver_options',
    'stdlib_net_resolver_lifecycle', 'stdlib_net_resolver_numeric',
    'stdlib_net_resolver_default', 'stdlib_net_resolver_validation',
    'stdlib_net_resolver_owned',
    # Shared execution resource / callback failure normalization regressions.
    'stdlib_net_engine_factory', 'stdlib_executor_errors',
    'stdlib_task_pool_results', 'stdlib_channel_finite_select',
)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--discovery', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = args.source_build.resolve()
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    out = args.out.resolve()
    # Linux includes an explicit terminal flag; the Windows builder publishes
    # its complete manifest only on success, without that field. Both paths
    # still validate every source/object/header and the actual driver below.
    assert not out.exists() and build.get('Complete', True) and build['SourceXrt'] and not build['RuntimeOnly']
    assert build['EnableExeCompiler'] == 1
    assert all(sha(row['Path']) == row['Sha256'].lower()
               for row in build['Inputs'] + build['Objects'] + build['Headers'])
    driver = Path(build['Driver'])
    assert sha(driver) == build['DriverSha256'].lower()
    cc = Path(shutil.which('gcc')).resolve()
    sdk = driver.parent / 'include'
    runtime = root / 'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot']) / 'release/libxl.so'
    contract = root / 'test/cases/xlir_net_resolver_contract.c'
    artifact = root / 'test/support/container_projection_artifact.c'
    memory = root / 'test/support/module_product_memory.c'
    host = root / 'test/cases/xlir_execution_product_oom_host.c'
    contract_input = root / 'test/cases/stdlib_net_resolver_contract.xl'
    files = {Path(__file__).resolve(), manifest, driver, cc, runtime, contract,
             artifact, memory, host, root / 'test/cases/xlir_module_product_oom_host.c',
             root / 'test/support/module_product_memory.h', contract_input}
    files.update(root / ('test/cases/' + case + '.xl') for case in CASES)
    inputs = {str(path): sha(path) for path in files}
    out.mkdir(parents=True)
    if os.name == 'nt':
        shutil.copy2(runtime, out / 'xl.dll')
    env = dict(os.environ)
    if os.name != 'nt':
        env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    rows, products, failures, prefixes, plans = [], [], [], [], []
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Inputs=inputs,
                  SourceBuild=str(manifest), Rows=rows, Products=products, Failures=failures,
                  ClosedPrefixes=prefixes, Plans=plans, DiscoveryOnly=args.discovery,
                  NumericOnly=True, NetworkTraffic=False, ExternalNetworkTests=False,
                  NetworkSecurityTests=False, ShippingLibraryAcceptance=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def run(name, command, rejected=False):
        command = list(map(str, command))
        invocation = command
        if command[0] == str(cc):
            response = out / (name + '.rsp')
            response.write_text('\n'.join('"' + token.replace('\\', '/').replace('"', '\\"') + '"'
                                          for token in command[1:]) + '\n', encoding='utf-8')
            invocation = [str(cc), '@' + str(response)]
            product(response)
        log = out / (name + '.log')
        try:
            with log.open('wb') as stream:
                result = subprocess.run(invocation, cwd=root, env=env, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=600)
        except subprocess.TimeoutExpired:
            rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=None,
                             TimedOut=True, Log=str(log), Sha256=sha(log)))
            save()
            raise
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=result.returncode,
                         ExpectedRejection=rejected, Log=str(log), Sha256=sha(log)))
        save()
        print(name + ': ' + str(result.returncode), flush=True)
        assert result.returncode == (1 if rejected else 0), (name, str(log))
        return log.read_text(encoding='utf-8', errors='replace')

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I' + str(root), '-I' + str(root / 'release'), '-I' + str(root / 'release/include'),
             '-I' + str(sdk)]
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    compat = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
              '-Wno-unused-but-set-variable', '-Wno-unused-label', '-Wno-unused-const-variable',
              '-Wno-missing-field-initializers', '-Wno-pointer-sign']
    objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
               ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    assert len(objects) == len(build['Objects']) - 1
    libraries = [runtime, *(['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
                           if os.name == 'nt' else ['-Wl,--export-dynamic', '-Wl,-rpath,' + str(runtime.parent),
                                                   '-pthread', '-ldl', '-lm'])]
    suffix = '.exe' if os.name == 'nt' else ''
    save()
    for case in CASES:
        try:
            source = root / ('test/cases/' + case + '.xl')
            generated = out / (case + '.c')
            run(case + '.emit', [driver, source, 'cgen-default', generated])
            product(generated)
            run(case + '.run', [driver, source, 'run', out / (case + '.run')])
            if args.discovery:
                continue
            code = generated.read_text(encoding='utf-8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Net(?:Resolver|ResolveOperation|AddressList)\w*\([^;]*\)\s*\{', code, re.M)
            assert not any(name in code for name in ('xl6_xrt_net_address_list_', 'xl6_xrt_net_resolver_stats',
                                                    'xl6_xrt_net_resolve_async', 'xl6_xrt_net_map_addresses'))
            if case == 'stdlib_net_address_list_values':
                assert '(XL6_NativeClassArrayContextV1){{' in code
            if case == 'stdlib_net_resolver_numeric':
                assert re.search(r'=\s*xl6NetResolveOperationWaitV1\(', code)
            for stage in ('typed', 'lowered'):
                target = out / (case + '.' + stage + '.json')
                run(case + '.' + stage, [driver, source, 'ir-' + stage, target])
                product(target)
            for opt in ('O0', 'O2'):
                obj = out / (case + '.' + opt + '.o')
                run(case + '.' + opt, [cc, *flags, *compat, '-' + opt, '-c', generated, '-o', obj])
                product(obj)
            run(case + '.module', [driver, source, 'module', out / (case + '.module')])
            binary = out / (case + '.aot' + suffix)
            target = binary.with_suffix('') if os.name == 'nt' else binary
            run(case + '.aot.build', [driver, source, 'exe', target])
            product(binary)
            run(case + '.aot.run', [binary])
            if case == 'stdlib_net_resolver_default':
                # Startup/exit of one module-owned service used to race a
                # ready Future's native callback tail. The native fixture
                # also holds that tail deterministically with a real callback.
                for repeat in range(4):
                    run(case + '.aot.exit.' + str(repeat), [binary])
            native = Path(str(target) + '.temps') / 'xl6_xrt_impl.c'
            product(native)
            assert '_MODULE_ALL' not in native.read_text(encoding='utf-8')
            imports = run(case + '.imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', binary]
                          if os.name == 'nt' else ['readelf', '-d', binary])
            assert not any(name in imports.lower() for name in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'))
        except Exception as error:
            failures.append(dict(Name=case, Error=repr(error)))
            save()

    if not args.discovery:
        negatives = {
            'list-integer': ('array<int> values = [1]\nauto value = NetAddressList(values)\n', 'create'),
            'list-handle': ('auto value = NetAddressList(null)\n', 'create'),
            'list-private': ('auto value = NetAddressList([NetAddress.loopback(4, 1)])\nauto raw = value.handle\n', 'private class member is not accessible'),
            'options-worker': ('auto value = NetResolverOptions().withWorkers("bad")\n', 'withWorkers'),
            'resolver-handle': ('auto value = NetResolver(1)\n', 'create'),
            'snapshot-create': ('auto value = NetResolverStats()\n', 'create'),
            'operation-create': ('auto value = NetResolveOperation()\n', 'create'),
            'operation-future': ('auto resolver = NetResolver()\nauto op = resolver.query("127.0.0.1")\nfuture<int> value = op.waitAsync()\n', 'future'),
            'operation-port': ('auto resolver = NetResolver()\nauto op = resolver.query("127.0.0.1")\nauto value = op.result("bad")\n', 'result'),
            'resolver-token': ('auto resolver = NetResolver()\nauto value = resolver.resolveAsyncCancel("127.0.0.1", 1)\n', 'resolveAsyncCancel'),
        }
        for name, (body, reason) in negatives.items():
            try:
                source = out / ('bad.' + name + '.xl')
                source.write_text(body, encoding='utf-8')
                product(source)
                generated = out / ('bad.' + name + '.c')
                log = run('bad.' + name, [driver, source, 'cgen-default', generated], rejected=True)
                if name in ('snapshot-create', 'operation-create'):
                    assert '[checked]' in log and 'XLIR default class constructor requires a verified zero-argument layout ABI' in log
                else:
                    assert '[sem]' in log and reason in log, (name, log)
                assert not generated.exists(), (name, log)
                assert 'out of memory' not in log.lower(), (name, log)
            except Exception as error:
                failures.append(dict(Name='bad.' + name, Error=repr(error)))
                save()
        for opt in ('O0', 'O2'):
            name = contract.stem + '.' + opt
            try:
                obj = out / (name + '.o')
                binary = out / (name + suffix)
                run(name + '.compile', [cc, *flags, '-' + opt, '-c', contract, '-o', obj])
                product(obj)
                run(name + '.link', [cc, *objects, obj, *libraries, '-o', binary])
                product(binary)
                assert 'Resolver XLIR:' in run(name + '.run', [binary, contract_input])
            except Exception as error:
                failures.append(dict(Name=name, Error=repr(error)))
                save()
        try:
            planner = out / ('planner' + suffix)
            run('planner.build', [cc, *flags, artifact, *objects, *libraries, '-o', planner])
            product(planner)
            for case in ('stdlib_net_address_list_values', 'stdlib_net_resolver_options',
                         'stdlib_net_resolver_lifecycle', 'stdlib_net_resolver_numeric', 'stdlib_net_resolver_default'):
                prefix = out / (case + '.selected')
                run(case + '.plan', [planner, root / ('test/cases/' + case + '.xl'), prefix])
                plan_file = Path(str(prefix) + '.plan.json')
                product(plan_file)
                plan = json.loads(plan_file.read_text())
                assert not any('_MODULE_ALL' in name for name in plan['NativeModules'])
                assert not any(name in plan['NativeModules'] for name in ('XRT_MODULE_NET_ENGINE', 'XRT_MODULE_TLS'))
                assert not any(name.endswith('xl6_future_callback_queue.c') for name in plan['Sources'])
                if case in ('stdlib_net_address_list_values', 'stdlib_net_resolver_options'):
                    assert not any('xl6_net_resolve_operation' in name or 'xl6_execution_' in name for name in plan['Sources'])
                plans.append(dict(Fixture=case, Sources=plan['Sources'], NativeModules=plan['NativeModules']))
                save()
            memory_object = out / 'memory.o'
            run('memory.build', [cc, *flags, '-O0', '-c', memory, '-o', memory_object])
            product(memory_object)
            generated = out / 'stdlib_net_resolver_owned.c'
            for opt in ('O0', 'O2'):
                binary = out / ('owned.' + opt + suffix)
                run('owned.' + opt + '.build', [cc, *flags, *compat, '-' + opt, '-include', generated,
                                               host, memory_object, *objects, *libraries, '-o', binary])
                product(binary)
                proof = run('owned.' + opt + '.prefixes', [binary, '--all'])
                match = re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced', proof)
                assert match and int(match[1]) == int(match[2]) + 1
                prefixes.append(dict(Optimization=opt, Runs=int(match[1]), FirstNoHit=int(match[2])))
                save()
        except Exception as error:
            failures.append(dict(Name='pruning-and-owned-prefixes', Error=repr(error)))
            save()
    report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items()) and all(
        sha(row['Path']) == row['Sha256'].lower() for row in build['Inputs'])
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
    report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
    save()
    assert report['Complete'], failures


if __name__ == '__main__':
    main()
