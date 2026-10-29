"""Vector/batch language ABI, ownership, pruning and own-loopback source products.
Only literal self-owned IPv4/IPv6 endpoints, plus offline value/IR/OOM checks.
No external peers, DNS, induced network errors, probes or security tests.
This is source acceptance, not shipping-library or complete-network acceptance.
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
    'stdlib_net_batch_values', 'stdlib_net_batch_owned',
    'stdlib_net_batch_packet_only', 'stdlib_net_batch_progress_only',
    'stdlib_net_socket_vector', 'stdlib_net_socket_batch',
    'stdlib_net_socket_udp', 'stdlib_net_socket_tcp',
    'cgen_channel', 'cgen_channel_string',
    'stdlib_channel_finite_select', 'stdlib_channel_finite_dynamic',
)


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--discovery', action='store_true')
    parser.add_argument('--only-negatives', action='store_true', help='Recheck semantic rejection assertions without rerunning unchanged executable products')
    args = parser.parse_args()
    assert not (args.discovery and args.only_negatives)
    root = Path(__file__).resolve().parents[1]
    manifest = args.source_build.resolve()
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    out = args.out.resolve()
    assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly']
    assert build['EnableExeCompiler'] == 1
    assert all(sha(row['Path']) == row['Sha256'].lower()
               for row in build['Inputs'] + build['Objects'] + build['Headers'])
    driver = Path(build['Driver'])
    assert sha(driver) == build['DriverSha256'].lower()
    cc = Path(shutil.which('gcc')).resolve()
    sdk = driver.parent / 'include'
    runtime = root / 'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot']) / 'release/libxl.so'
    contract = root / 'test/cases/xlir_net_vector_batch_contract.c'
    legacy = root / 'test/cases/xlir_net_socket_contract.c'
    datagram = root / 'test/cases/xlir_net_datagram_contract.c'
    artifact = root / 'test/support/container_projection_artifact.c'
    memory = root / 'test/support/module_product_memory.c'
    host = root / 'test/cases/xlir_module_product_oom_host.c'
    files = {Path(__file__).resolve(), manifest, driver, cc, runtime, contract, legacy, datagram,
             artifact, memory, host, root / 'test/cases/stdlib_net_socket_datagram.xl'}
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
                  NegativeOnly=args.only_negatives,
                  OwnLoopbackOnly=True, ExternalNetworkTests=False, NetworkSecurityTests=False,
                  ShippingLibraryAcceptance=False, CompleteNetworkAcceptance=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def finish():
        report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items()) and all(
            sha(row['Path']) == row['Sha256'].lower() for row in build['Inputs'])
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
        report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
        save()
        assert report['Complete'], failures

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
    for case in (() if args.only_negatives else CASES):
        try:
            source = root / ('test/cases/' + case + '.xl')
            generated = out / (case + '.c')
            run(case + '.emit', [driver, source, 'cgen-default', generated])
            product(generated)
            run(case + '.run', [driver, source, 'run', out / (case + '.run')])
            if args.discovery:
                continue
            code = generated.read_text(encoding='utf-8')
            assert 'XL6_ChannelCaseArrayContextV1' not in code and 'XL6_ChannelResultFieldContextV1' not in code
            if case == 'stdlib_net_socket_vector':
                for family in ('Text', 'Bytes', 'Buffer'):
                    for verb in ('SendVector', 'SendVectorTo', 'SendMessageVector', 'SendConnectedMessageVector'):
                        assert re.search(r'=\s*xl6NetSocket' + verb + family + r'V1\(', code)
                assert '(XL6_NativeClassArrayContextV1){{' in code
            if case == 'stdlib_net_batch_values':
                assert '(XL6_NativeClassFieldContextV1){' in code
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
        prefix = ('auto socket = NetSocket(NetSocketKind.datagram, 4)\n'
                  'auto progress = NetBatchProgress()\n')
        negatives = {
            'vector-integer': 'array<int> data = [1]\nauto a, b = socket.sendVector(data)\n',
            'vector-dynamic': 'array<var> data = ["text"]\nauto a, b = socket.sendVector(data)\n',
            'vector-nullable': 'array<string?> data = [null]\nauto a, b = socket.sendVector(data)\n',
            'vector-size': 'array<bytes> data = [b"x"]\nauto a, b = socket.sendVector(data, "bad")\n',
            'receive-text': 'array<string> data = ["x"]\nauto a, b = socket.receiveVectorInto(data)\n',
            'receive-info': ('array<Buffer> data = [Buffer()]\n'
                             'auto a, b = socket.receiveFromVectorInto(data, NetDatagramControl())\n'),
            'batch-text': 'array<string> data = ["x"]\nauto a, b = socket.sendBatch(data, progress)\n',
            'batch-progress': ('array<NetDatagramPacket> data = [NetDatagramPacket()]\n'
                               'auto a, b = socket.sendBatch(data, Buffer())\n'),
            'batch-receive-buffer': ('array<Buffer> data = [Buffer()]\n'
                                     'auto a, b = socket.receiveBatch(data, progress)\n'),
            'batch-limit': ('array<NetDatagramPacket> data = [NetDatagramPacket()]\n'
                            'auto a, b = socket.receiveBatch(data, progress, "bad")\n'),
            'packet-input': 'auto packet = NetDatagramPacket([1, 2])\n',
            'packet-result': 'NetAddress value = NetDatagramPacket().shareBuffer()\n',
        }
        for name, body in negatives.items():
            try:
                source = out / ('bad.' + name + '.xl')
                source.write_text(prefix + body, encoding='utf-8')
                product(source)
                generated = out / ('bad.' + name + '.c')
                log = run('bad.' + name, [driver, source, 'cgen-default', generated], rejected=True)
                method = ('sendVector' if name.startswith('vector-') else
                          'receiveVectorInto' if name == 'receive-text' else
                          'receiveFromVectorInto' if name == 'receive-info' else
                          'receiveBatch' if name in ('batch-receive-buffer', 'batch-limit') else 'sendBatch')
                reason = ('没有匹配的 create 重载' if name == 'packet-input' else
                          'target NetAddress, value Buffer' if name == 'packet-result' else
                          "no matching callable class member 'NetSocket." + method + "'")
                assert '[sem]' in log and reason in log and not generated.exists(), (name, log)
                assert 'out of memory' not in log.lower(), (name, log)
            except Exception as error:
                failures.append(dict(Name='bad.' + name, Error=repr(error)))
                save()
        if args.only_negatives:
            finish()
            return
        contracts = ((contract, 'stdlib_net_socket_vector', 'Vector/batch XLIR:'),
                     (legacy, 'stdlib_net_socket_vector', 'Socket XLIR:'),
                     (datagram, 'stdlib_net_socket_datagram', 'Datagram XLIR:'))
        for unit, case, expected in contracts:
            for opt in ('O0', 'O2'):
                name = unit.stem + '.' + opt
                try:
                    obj = out / (name + '.o')
                    binary = out / (name + suffix)
                    run(name + '.compile', [cc, *flags, '-' + opt, '-c', unit, '-o', obj])
                    product(obj)
                    run(name + '.link', [cc, *objects, obj, *libraries, '-o', binary])
                    product(binary)
                    assert expected in run(name + '.run', [binary, root / ('test/cases/' + case + '.xl')])
                except Exception as error:
                    failures.append(dict(Name=name, Error=repr(error)))
                    save()
        try:
            planner = out / ('planner' + suffix)
            run('planner.build', [cc, *flags, artifact, *objects, *libraries, '-o', planner])
            product(planner)
            for case in ('stdlib_net_batch_packet_only', 'stdlib_net_batch_progress_only'):
                prefix = out / (case + '.selected')
                run(case + '.plan', [planner, root / ('test/cases/' + case + '.xl'), prefix])
                plan_file = Path(str(prefix) + '.plan.json')
                product(plan_file)
                plan = json.loads(plan_file.read_text())
                assert not any('_MODULE_ALL' in name for name in plan['NativeModules'])
                assert not any(name in plan['NativeModules'] for name in ('XRT_MODULE_NET_SOCKET', 'XRT_MODULE_TLS'))
                assert not any('net_socket_' in name for name in plan['Sources'])
                if case.endswith('progress_only'):
                    assert not any('xl6_buffer_' in name or 'xl6_net_datagram_packet' in name for name in plan['Sources'])
                else:
                    assert not any(name.endswith('xl6_net_datagram_packet_info.c') for name in plan['Sources'])
                plans.append(dict(Fixture=case, Sources=plan['Sources'], NativeModules=plan['NativeModules']))
                save()
            memory_object = out / 'memory.o'
            run('memory.build', [cc, *flags, '-O0', '-c', memory, '-o', memory_object])
            product(memory_object)
            generated = out / 'stdlib_net_batch_owned.c'
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
    finish()


if __name__ == '__main__':
    main()
