"""Exercise imported Proxy Future graphs against xlang's pinned time ABI.

Selected actual amalgamation, not a mock adapter. Literal self-created IPv4
loopback only: no DNS/external host, malformed traffic or security tests.
This prerequisite does not publish managed TCP language APIs or certify VFS.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compiler', action='append', required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--probe', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    fixture = root/'test/support/tcp_proxy_dial_native_future.c'
    header = root/'lib/xrt/xrt.h'
    compilers = [Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    modules = ('NET_PROXY_DIAL_FUTURE','NET_TCP_DIAL_FUTURE','NET_PROXY_SOCKS5',
               'NET_PROXY_HTTP_CONNECT','MEMORY_DEBUG','THREAD_KEY','NET_PORT_SELECT',
               'NET_PORT_IOCP' if os.name=='nt' else 'NET_PORT_EPOLL')
    headers=(root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h')
    peers=(root/'test/support/tcp_proxy_dial_peer.h',root/'test/support/tcp_proxy_socks5_peer.h')
    inputs = {str(path):sha(path) for path in (fixture,*headers,*peers,Path(__file__).resolve(),*compilers)}
    if args.probe:compilers=compilers[:1]
    rows, products, failures = [], [], []
    out.mkdir(parents=True)
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Inputs=inputs,
                  Rows=rows, Products=products, Failures=failures, OrdinaryLiteralLoopback=True,
                  ExternalNetworkTests=False, NetworkSecurityTests=False, ShippingLibraryAcceptance=False,
                  DiscoveryOnly=args.probe,PublicLanguageApiAccepted=False,
                  PinnedDeadlineAbi='uint64 microseconds/xdeadline; no native millisecond ABI import', Scope=__doc__)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, instrumented=False):
        command = list(map(str, command))
        invocation = command
        if command[0] in map(str, compilers) and Path(command[0]).stem != 'tcc':
            rsp = out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+arg.replace('\\', '/')+'"' for arg in command[1:])+'\n', encoding='utf-8')
            invocation = [command[0], '@'+str(rsp)]
            products.append(dict(Path=str(rsp), Sha256=sha(rsp)))
        env = dict(os.environ)
        if instrumented:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log = out/(name+'.log')
        with log.open('wb') as stream:
            result = subprocess.run(invocation, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
        rows.append(dict(Name=name, Command=command, Invocation=invocation, Exit=result.returncode,
                         Log=str(log), Sha256=sha(log)))
        save()
        print(name+': '+str(result.returncode), flush=True)
        assert result.returncode == 0, (name, str(log))

    save()
    for index, cc in enumerate(compilers):
        modes = [('O0', ['-O0']), ('O2', ['-O2'])]
        if args.probe:modes=modes[:1]
        elif args.sanitize and cc.stem != 'tcc':
            modes += [('instrumented', ['-O1', '-g', '-fsanitize=address,undefined',
                                       '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'])]
        for mode, flags in modes:
            variants=[('core.socks',False,False),('core.http',False,True),
                      ('runtime.socks',True,False),('runtime.http',True,True)]
            if args.probe:variants=variants[:2]
            for variant,runtime,http in variants:
                name=f'{index}.{mode}.{variant}'
                try:
                    binary=out/(name+('.exe' if os.name=='nt' else ''))
                    command=[cc,'-Wall','-Wextra','-Werror',*flags,
                        '-I'+str(root/('lib/xruntime' if runtime else 'lib/xrt'))]
                    if cc.stem!='tcc':command+=['-std=c11']
                    if os.name!='nt':command+=['-D_GNU_SOURCE']
                    command+=['-DXRT_MODULE_'+item for item in modules]
                    if runtime:command+=['-DTEST_PROXY_RUNTIME_PRODUCT','-DXRUNTIME_MODULE_ALL']
                    if http:command+=['-DTEST_PROXY_DIAL_HTTP_CONNECT=1']
                    command+=[fixture,*(['-lws2_32'] if os.name=='nt' else ['-pthread','-lm']),'-o',binary]
                    run(name+'.build',command)
                    products.append(dict(Path=str(binary),Sha256=sha(binary)))
                    for backend,argv in (('select',[]),('native',['native'])):
                        run(name+'.'+backend,[binary,*argv],mode=='instrumented')
                        log=(out/(name+'.'+backend+'.log')).read_text(encoding='utf8')
                        assert 'cases=16 snapshots=8 family-refusals=' in log
                        assert 'budgets=10 failures=9 roundtrips=4 literal loopback balanced' in log
                except Exception as error:
                    failures.append(dict(Name=name,Error=repr(error)));save()
    report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(item['Path'])==item['Sha256'] for item in products)
    report['Complete'] = report['ProductsUnchanged'] and not failures and report['InputsUnchanged'] and report['LogsUnchanged']
    save()
    assert report['Complete'], failures


if __name__ == '__main__':
    main()
