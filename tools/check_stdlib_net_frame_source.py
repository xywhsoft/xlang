"""Source buffer/framing contracts, language products, pruning and full OOM prefixes.
Offline values/codecs only; retained Resolver regression uses literal numeric addresses.
No sockets, host names, traffic, protocol fuzzing, security tests or shipping claim.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess

CASES = ('stdlib_net_buffer_values', 'stdlib_net_buffer_limits', 'stdlib_net_frame_values',
         'stdlib_net_frame_limits', 'stdlib_net_frame_owned', 'stdlib_net_frame_defaults',
         'stdlib_net_buffer_only', 'stdlib_net_frame_line_only', 'stdlib_net_frame_length_only',
         'stdlib_net_frame_atomic', 'stdlib_net_resolver_default')


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--negative-only', action='store_true', help='Rerun only semantic/default-construction rejections against an unchanged source build.')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1];out = args.out.resolve();manifest = args.source_build.resolve()
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert not out.exists() and build.get('Complete', True) and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler'] == 1
    assert all(sha(row['Path']) == row['Sha256'].lower() for row in build['Inputs'] + build['Objects'] + build['Headers'])
    driver = Path(build['Driver']);assert sha(driver) == build['DriverSha256'].lower()
    cc = Path(shutil.which('gcc')).resolve();sdk = driver.parent / 'include'
    runtime = root / 'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot']) / 'release/libxl.so'
    contract = root / 'test/cases/xlir_net_frame_contract.c';artifact = root / 'test/support/container_projection_artifact.c'
    memory = root / 'test/support/module_product_memory.c';host = root / 'test/cases/xlir_module_product_oom_host.c'
    atomic = root / 'test/cases/xlir_net_frame_atomic_host.c'
    files = {Path(__file__).resolve(), manifest, driver, cc, runtime, contract, artifact, memory, host, atomic,
             root / 'test/support/module_product_memory.h'}
    files.update(root / ('test/cases/' + case + '.xl') for case in CASES)
    inputs = {str(path): sha(path) for path in files};out.mkdir(parents=True)
    if os.name == 'nt':shutil.copy2(runtime, out / 'xl.dll')
    env = dict(os.environ)
    if os.name != 'nt':env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    rows, products, failures, prefixes, plans, atomic_proofs = [], [], [], [], [], []
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Inputs=inputs,
        SourceBuild=str(manifest), Rows=rows, Products=products, Failures=failures, ClosedPrefixes=prefixes,
        Plans=plans, AtomicBoxing=atomic_proofs, NegativeOnly=args.negative_only, DiscoveryOnly=False, NetworkTraffic=False, ExternalNetworkTests=False,
        NetworkSecurityTests=False, ShippingLibraryAcceptance=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def finish():
        report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items()) and all(sha(row['Path']) == row['Sha256'].lower() for row in build['Inputs'])
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
        report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'], failures

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)));save()

    def run(name, command, rejected=False, eager=False):
        command = list(map(str, command));invocation = command
        if command[0] == str(cc):
            response = out / (name + '.rsp')
            response.write_text('\n'.join('"' + token.replace('\\', '/').replace('"', '\\"') + '"' for token in command[1:]) + '\n', encoding='utf-8')
            invocation = [str(cc), '@' + str(response)];product(response)
        log = out / (name + '.log');options = {}
        if os.name != 'nt':
            import resource
            options['preexec_fn'] = lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        try:
            with log.open('wb') as stream:
                result = subprocess.run(invocation, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=600, **options)
        except subprocess.TimeoutExpired:
            rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=None, TimedOut=True, Log=str(log), Sha256=sha(log)));save();raise
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=result.returncode,
            ExpectedRejection=rejected, ExpectedEagerRegressionFailure=eager, Log=str(log), Sha256=sha(log)))
        save();print(name + ': ' + str(result.returncode), flush=True)
        content = log.read_text(encoding='utf-8', errors='replace')
        if eager:
            assert result.returncode != 0 and 'frame boxing point=1 kind=0 hit=1 result=0 error=6 remaining=0' in content, (name, content)
        else:assert result.returncode == (1 if rejected else 0), (name, str(log))
        return content

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I' + str(root),
             '-I' + str(root / 'release'), '-I' + str(root / 'release/include'), '-I' + str(sdk)]
    if os.name != 'nt':flags += ['-D_GNU_SOURCE']
    compat = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-but-set-variable',
              '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-missing-field-initializers', '-Wno-pointer-sign']
    objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    assert len(objects) == len(build['Objects']) - 1
    libraries = [runtime, *(['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else
                            ['-Wl,--export-dynamic', '-Wl,-rpath,' + str(runtime.parent), '-pthread', '-ldl', '-lm'])]
    suffix = '.exe' if os.name == 'nt' else '';save()
    for case in (() if args.negative_only else CASES):
        try:
            source = root / ('test/cases/' + case + '.xl');generated = out / (case + '.c')
            run(case + '.emit', [driver, source, 'cgen-default', generated]);product(generated)
            run(case + '.run', [driver, source, 'run', out / (case + '.run')])
            code = generated.read_text(encoding='utf-8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Net(?:Buffer|FrameResult|LineFramer|LengthFramer)\w*\([^;]*\)\s*\{', code, re.M)
            assert not any(name in code for name in ('xl6_xrt_net_frame_', 'xl6_xrt_net_framer_', 'xl6_xrt_net_line_framer_', 'xl6_xrt_net_length_framer_'))
            for stage in ('typed', 'lowered'):
                target = out / (case + '.' + stage + '.json');run(case + '.' + stage, [driver, source, 'ir-' + stage, target]);product(target)
            for opt in ('O0', 'O2'):
                obj = out / (case + '.' + opt + '.o');run(case + '.' + opt, [cc, *flags, *compat, '-' + opt, '-c', generated, '-o', obj]);product(obj)
            run(case + '.module', [driver, source, 'module', out / (case + '.module')])
            binary = out / (case + '.aot' + suffix);target = binary.with_suffix('') if os.name == 'nt' else binary
            run(case + '.aot.build', [driver, source, 'exe', target]);product(binary);run(case + '.aot.run', [binary])
            native = Path(str(target) + '.temps') / 'xl6_xrt_impl.c';product(native);assert '_MODULE_ALL' not in native.read_text(encoding='utf-8')
            imports = run(case + '.imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', binary] if os.name == 'nt' else ['readelf', '-d', binary])
            assert not any(name in imports.lower() for name in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'))
        except Exception as error:
            failures.append(dict(Name=case, Error=repr(error)));save()

    negatives = {
        'buffer-pointer': ('auto b = NetBuffer(null)\n', '[sem]', 'create'),
        'buffer-size': ('auto b = NetBuffer()\nb.append("x", "wrong")\n', '[sem]', 'append'),
        'buffer-move': ('auto b = NetBuffer()\nb.appendMove(Buffer())\n', '[sem]', 'appendMove'),
        'buffer-output': ('auto b = NetBuffer()\nb.readInto(b)\n', '[sem]', 'readInto'),
        'buffer-private': ('auto b = NetBuffer()\nauto p = b.handle\n', '[sem]', 'private class member is not accessible'),
        'line-pointer': ('auto b = NetLineFramer(null)\n', '[sem]', 'create'),
        'line-size': ('auto b = NetLineFramer("x", 3, false, 100, "wrong")\n', '[sem]', 'create'),
        'length-mode': ('auto b = NetLengthFramer(0, 4, 0, 4, 100, "wrong")\n', '[sem]', 'create'),
        'line-private-peek': ('auto b = NetLineFramer()\nauto r = b.peekNative()\n', '[sem]', 'unknown class/union field'),
        'line-private-commit': ('auto b = NetLineFramer()\nb.commitNative(b.next(false))\n', '[sem]', 'unknown class/union field'),
        'frame-create': ('auto f = NetFrameResult()\n', '[checked]', 'XLIR default class constructor requires a verified zero-argument layout ABI'),
        'frame-output': ('Buffer b = NetLineFramer().next().bytes()\n', '[sem]', 'target Buffer, value bytes'),
        'native-field-default': ('class C\n NetBuffer value\nend\nauto c = C()\n', '[ir]', 'native resource field needs explicit initialization or nullable storage'),
    }
    for name, (body, stage, reason) in negatives.items():
        try:
            source = out / ('bad.' + name + '.xl');source.write_text(body, encoding='utf-8');product(source)
            generated = out / ('bad.' + name + '.c');log = run('bad.' + name, [driver, source, 'cgen-default', generated], rejected=True)
            assert stage in log and reason in log and not generated.exists() and 'out of memory' not in log.lower(), (name, log)
        except Exception as error:
            failures.append(dict(Name='bad.' + name, Error=repr(error)));save()
    if args.negative_only:
        finish();return
    for opt in ('O0', 'O2'):
        name = contract.stem + '.' + opt
        try:
            obj = out / (name + '.o');binary = out / (name + suffix)
            run(name + '.compile', [cc, *flags, '-' + opt, '-c', contract, '-o', obj]);product(obj)
            run(name + '.link', [cc, *objects, obj, *libraries, '-o', binary]);product(binary)
            assert 'Frame XLIR:' in run(name + '.run', [binary, root / 'test/cases/stdlib_net_frame_owned.xl'])
        except Exception as error:
            failures.append(dict(Name=name, Error=repr(error)));save()
    try:
        planner = out / ('planner' + suffix);run('planner.build', [cc, *flags, artifact, *objects, *libraries, '-o', planner]);product(planner)
        for case in ('stdlib_net_buffer_only', 'stdlib_net_frame_line_only', 'stdlib_net_frame_length_only', 'stdlib_net_frame_values'):
            prefix = out / (case + '.selected');run(case + '.plan', [planner, root / ('test/cases/' + case + '.xl'), prefix])
            plan_file = Path(str(prefix) + '.plan.json');product(plan_file);plan = json.loads(plan_file.read_text())
            assert not any('_MODULE_ALL' in name or name in ('XRT_MODULE_NET_SOCKET', 'XRT_MODULE_NET_ENGINE', 'XRT_MODULE_NET_RESOLVER', 'XRT_MODULE_TLS') for name in plan['NativeModules'])
            if case.endswith('_only'):
                assert not any('xl6_buffer_' in name or name.endswith('xl6_net_buffer_buffer.c') for name in plan['Sources'])
            if case == 'stdlib_net_buffer_only':assert 'XRT_MODULE_NET_FRAME' not in plan['NativeModules']
            if case == 'stdlib_net_frame_line_only':assert 'XRT_MODULE_NET_FRAME_LENGTH' not in plan['NativeModules']
            if case == 'stdlib_net_frame_length_only':assert 'XRT_MODULE_NET_FRAME_LINE' not in plan['NativeModules']
            plans.append(dict(Fixture=case, Sources=plan['Sources'], NativeModules=plan['NativeModules']));save()
        memory_object = out / 'memory.o';run('memory.build', [cc, *flags, '-O0', '-c', memory, '-o', memory_object]);product(memory_object)
        for opt in ('O0', 'O2'):
            binary = out / ('owned.' + opt + suffix)
            run('owned.' + opt + '.build', [cc, *flags, *compat, '-' + opt, '-include', out / 'stdlib_net_frame_owned.c', host, memory_object, *objects, *libraries, '-o', binary]);product(binary)
            proof = run('owned.' + opt + '.prefixes', [binary, '--all']);match = re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced', proof)
            assert match and int(match[1]) == int(match[2]) + 1
            prefixes.append(dict(Optimization=opt, Runs=int(match[1]), FirstNoHit=int(match[2])));save()
            binary = out / ('atomic.' + opt + suffix)
            run('atomic.' + opt + '.build', [cc, *flags, *compat, '-' + opt, '-include', out / 'stdlib_net_frame_atomic.c', atomic, memory_object, *objects, *libraries, '-o', binary]);product(binary)
            proof = run('atomic.' + opt + '.prefixes', [binary]);matches = re.findall(r'kind=(\d+) prefixes=(\d+) first-no-hit=(\d+) input-preserved balanced', proof)
            assert len(matches) == 2 and all(int(n) == int(last) + 1 for kind, n, last in matches)
            atomic_proofs.append(dict(Optimization=opt, Kinds=[dict(Kind=int(k), Runs=int(n), FirstNoHit=int(last)) for k, n, last in matches]));save()
        binary = out / ('eager-regression' + suffix)
        run('eager-regression.build', [cc, *flags, *compat, '-O0', '-DXL6_FRAME_EAGER_REGRESSION', '-Dxl6NetLineFramerNextV1=xl_frame_eager_line',
            '-include', out / 'stdlib_net_frame_atomic.c', atomic, memory_object, *objects, *libraries, '-o', binary]);product(binary)
        run('eager-regression.reject', [binary], eager=True)
    except Exception as error:
        failures.append(dict(Name='pruning-and-owned-prefixes', Error=repr(error)));save()
    finish()


if __name__ == '__main__':
    main()
