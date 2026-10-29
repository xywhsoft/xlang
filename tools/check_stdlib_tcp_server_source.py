"""Current-source aggregate Server JIT/module, independent AOT and sealed IR proof.
Only valid owned literal loopback, normal file I/O and offline compiler tests.
No external network, system DNS, malformed protocols or security tests. Not
shipping DLL/SO/VFS or complete-network acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--native-root', type=Path)
    parser.add_argument('--profile',choices=('server','owned-sends','owned-receives','owned-ref-batches','chain-move'),default='server')
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
    cases=tuple('stdlib_tcp_server_'+name for name in ('options','core','sync','async','stops','poll'))+('stdlib_tcp_managed_async','stdlib_tcp_data_buffer_async_only')
    negatives=tuple('bad_stdlib_tcp_server_'+name for name in ('mode','options','future'))
    if args.profile=='owned-sends':
        cases=('stdlib_net_bytes_value','stdlib_net_bytes_buffer','stdlib_tcp_send_ref_move',
            'stdlib_tcp_server_poll','stdlib_tcp_managed_async')
        negatives=('bad_stdlib_net_bytes_pointer','bad_stdlib_tcp_send_ref_bytes')
    if args.profile=='owned-receives':
        cases=tuple('stdlib_tcp_recv_owned_'+name for name in ('sync','async','stops'))+(
            'stdlib_tcp_managed_async','stdlib_tcp_server_async','stdlib_tcp_data_receive_async')
        negatives=('bad_stdlib_tcp_recv_owned_future',)
    if args.profile=='owned-ref-batches':
        cases=('stdlib_tcp_send_refs','stdlib_tcp_data_vector','stdlib_tcp_send_ref_move','stdlib_tcp_recv_owned_async')
        negatives=('bad_stdlib_tcp_send_refs_buffer',)
    if args.profile=='chain-move':
        cases=('stdlib_net_buffer_ref','stdlib_tcp_send_chain_plain','stdlib_tcp_send_chain_move',
            'stdlib_tcp_chain_relay_async','stdlib_tcp_data_vector','stdlib_tcp_send_refs')
        negatives=('bad_stdlib_tcp_chain_move_size','bad_stdlib_net_buffer_ref_buffer')
    contract = root / 'test/cases/xlir_tcp_server_contract.c'
    poll_contract = root/'test/cases/xlir_tcp_server_poll_contract.c'
    owned_contract = root/'test/cases/xlir_tcp_send_owned_contract.c'
    receive_contract = root/'test/cases/xlir_tcp_recv_owned_contract.c'
    refs_contract = root/'test/cases/xlir_tcp_send_refs_contract.c'
    chain_contract = root/'test/cases/xlir_tcp_chain_move_contract.c'
    inputs = {str(path): sha(path) for path in (Path(__file__).resolve(), manifest, driver, cc, contract, poll_contract, owned_contract, receive_contract, refs_contract, chain_contract,
        root/'test/cases/xlir_tcp_managed_contract.c',root/'test/cases/stdlib_tcp_data_receive_async.xl',
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
            transport = case not in ('stdlib_tcp_server_options','stdlib_net_buffer_ref') and not case.startswith('stdlib_net_bytes_')
            asynchronous = case.endswith(('_async', '_async_only', '_stops'))
            assert data['moduleExecutionOwnershipAbi'] == int(transport)
            assert data['moduleContinuationAbi'] == (2 if asynchronous else 0)
            assert data['moduleTaskPoolAbi'] == 'none'
            assert bool(data['moduleDeadlineOwnershipAbi']) == (case in ('stdlib_tcp_server_async','stdlib_tcp_server_stops',
                'stdlib_tcp_recv_owned_async','stdlib_tcp_recv_owned_stops'))
            text = generated.read_text()
            assert ('static XL6_ExecutionSourceV1* xlir_execution_source;' in text) == transport
            assert not any(symbol in text for symbol in ('xl6NetworkService', 'xl6_xrt_tcp', 'xlir_async_pool',
                'xrtNetProxyDialAsync(', 'xrtNetEngineCreate(', 'xrtCancelWatch('))
            if not asynchronous:
                assert 'xlir_continuation_executor' not in text
            if case=='stdlib_tcp_server_core':
                assert 'xl6TcpServerCreateV1(' in text and 'xl6TcpServerAcceptStartV1(' not in text
                assert 'xl6OwnershipDomainNetServerFutureNativeV1Get(),' not in text
            if case.startswith('stdlib_net_bytes_'):
                assert 'xl6NetBytes' in text and 'xl6_xrt_net_bytes_' not in text
                assert 'xl6NetBytesHandlePolicyV1Get()' in text
                assert 'xl6OwnershipDomainExecutionNetEngineV1Get(),' not in text
            if case=='stdlib_tcp_send_ref_move':
                assert 'xl6TcpStreamSendRefV1(' in text and 'xl6TcpStreamSendMoveV1(' in text
            if case=='stdlib_tcp_send_refs':
                assert 'xl6TcpStreamSendRefsV1(' in text
            if case=='stdlib_net_buffer_ref':assert 'xl6NetBufferAppendRefV1(' in text
            if case.startswith('stdlib_tcp_send_chain_'):assert 'xl6TcpStreamSendChainMoveV1(' in text
            if case.startswith('stdlib_tcp_recv_owned_'):
                assert 'xl6TcpStreamRecvOwned' in text and 'xl6NetBytesHandlePolicyV1Get()' in text
            if case=='stdlib_tcp_server_poll':
                assert 'xl6TcpServerTryAcceptV1(' in text
                assert 'xl6OwnershipDomainNetServerFutureNativeV1Get(),' not in text
                assert 'xl6TcpServerAcceptV1(' not in text and 'xl6TcpServerAcceptStartV1(' not in text
            if case=='stdlib_tcp_server_options':
                assert 'xl6OwnershipDomainExecutionTcpServerV1Get(),' not in text
            if case=='stdlib_tcp_server_async':
                assert all('xl6TcpServerAcceptStart'+suffix+'V1(' in text for suffix in ('','Cancel','Until','UntilCancel'))
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
            # The artifact linker appends the actual private catalog to the
            # final module, not the original Cgen-only source or XRT TU.
            assembled=native.parent/'module.c';product(assembled)
            assembled_text=assembled.read_text()
            if case.startswith(('stdlib_net_bytes_','stdlib_tcp_recv_owned_')) or case in ('stdlib_tcp_send_ref_move','stdlib_tcp_send_refs'):
                assert 'xl6OwnershipDomainNetBytesV1Get(),' in assembled_text
                assert 'xl6OwnershipDomainNetBufferNativeV1Get(),' in assembled_text
            if case.startswith('stdlib_net_bytes_'):
                assert 'xl6OwnershipDomainExecutionNetEngineV1Get(),' not in assembled_text
            if case=='stdlib_tcp_recv_owned_sync':
                assert 'xl6TcpStreamRecvOwnedStartV1(' not in assembled_text
                assert 'xl6OwnershipDomainFutureCallbackQueueV1Get(),' not in assembled_text
            if case=='stdlib_tcp_send_refs':
                assert 'xl6OwnershipDomainFutureCallbackQueueV1Get(),' not in assembled_text
                assert 'xl6OwnershipDomainDeadlineServiceV1Get(),' not in assembled_text
            if case=='stdlib_net_buffer_ref':
                assert 'xl6OwnershipDomainNetBytesV1Get(),' in assembled_text
                assert 'xl6OwnershipDomainExecutionNetEngineV1Get(),' not in assembled_text
            if case=='stdlib_tcp_send_chain_plain':
                assert 'xl6OwnershipDomainNetBytesV1Get(),' not in assembled_text
            if case.startswith('stdlib_tcp_send_chain_'):
                assert 'xl6OwnershipDomainFutureCallbackQueueV1Get(),' not in assembled_text
                assert 'xl6OwnershipDomainDeadlineServiceV1Get(),' not in assembled_text
            if case in ('stdlib_tcp_server_options','stdlib_tcp_server_core','stdlib_tcp_server_poll'):
                assert 'xl6OwnershipDomainNetServerFutureNativeV1Get(),' not in assembled_text
            native_text = native.read_text()
            assert '_MODULE_ALL' not in native_text
            for module in ('TLS', 'TASK_POOL', 'COROUTINE_SCHEDULER', 'NET_TCP_DIAL_FUTURE'):
                assert '#define XRT_MODULE_' + module + '\n' not in native_text, module
            if case in ('stdlib_tcp_server_options','stdlib_tcp_server_core'):
                assert all('#define XRT_MODULE_'+module+'\n' not in native_text for module in ('NET_TCP_SERVER_FUTURE','NET_TCP_FUTURE'))
                assert 'xl6_future_callback_queue.c' not in native_text
            if case=='stdlib_tcp_server_poll':
                assert '#define XRT_MODULE_NET_TCP_SERVER_FUTURE\n' not in native_text
                assert 'xl6_future_callback_queue.c' not in native_text and 'xl6_tcp_server_sync.c' not in native_text
            if case.startswith('stdlib_net_bytes_'):
                assert all('#define XRT_MODULE_'+module+'\n' not in native_text for module in ('NET_ENGINE','NET_TCP','NET_TCP_FUTURE'))
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
        contracts=(('contract',contract,[root/'test/cases/stdlib_tcp_server_async.xl',
            root/'test/cases/stdlib_tcp_data_receive_async.xl'],'TCP XLIR:'),
            ('poll-contract',poll_contract,[root/'test/cases/stdlib_tcp_server_poll.xl'],'TCP optional owner:'))
        if args.profile=='owned-sends':
            contracts=(('owned-contract',owned_contract,[root/'test/cases/stdlib_tcp_send_ref_move.xl',
                root/'test/cases/stdlib_tcp_managed_async.xl'],'TCP owned sends:'),)
        if args.profile=='owned-receives':
            contracts=(('receive-contract',receive_contract,[root/'test/cases/stdlib_tcp_recv_owned_sync.xl',
                root/'test/cases/stdlib_tcp_recv_owned_async.xl'],'TCP owned receives:'),)
        if args.profile=='owned-ref-batches':
            contracts=(('refs-contract',refs_contract,[root/'test/cases/stdlib_tcp_send_refs.xl',
                root/'test/cases/stdlib_tcp_recv_owned_async.xl'],'TCP immutable batch:'),)
        if args.profile=='chain-move':
            contracts=(('chain-contract',chain_contract,[root/'test/cases/stdlib_tcp_send_chain_move.xl',
                root/'test/cases/stdlib_tcp_chain_relay_async.xl'],'TCP chain take:'),)
        for (label,source,arguments,marker),opt in ((item,opt) for item in contracts for opt in ('O0','O2')):
            name=label+'.'+opt
            obj = out / (name + '.o')
            run(name + '.build', [cc, *flags, '-Wno-missing-field-initializers', '-' + opt,
                '-c', source, '-o', obj])
            product(obj)
            binary = out / (name + ('.exe' if os.name == 'nt' else ''))
            links = [*objects, obj]
            if os.name == 'nt':
                links += [root / 'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
                if not (out / 'xl.dll').exists():
                    shutil.copy2(root / 'release/xl.dll', out / 'xl.dll')
                    product(out / 'xl.dll')
            else:
                lib = args.native_root.resolve() / 'release'
                links += ['-L' + str(lib), '-lxl', '-Wl,-rpath,' + str(lib), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
            rsp = out / (name + '.rsp')
            rsp.write_text('\n'.join('"' + str(arg).replace('\\', '/') + '"' for arg in [*links, '-o', binary]) + '\n', encoding='utf8')
            product(rsp)
            run(name + '.link', [cc, '@' + str(rsp)])
            product(binary)
            proof = run(name + '.run', [binary,*arguments])
            assert marker in proof
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
