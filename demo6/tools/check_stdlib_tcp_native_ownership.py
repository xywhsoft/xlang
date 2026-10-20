"""Exercise the imported native TCP graph against xlang's pinned time ABI.

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
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    fixture = root/'test/support/tcp_native_ownership.c'
    header = root/'lib/xrt/xrt.h'
    compilers = [Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    features = ('ATOMIC', 'QUEUE', 'QUEUE_MPSC', 'MEMORY_DEBUG', 'TIME', 'WAIT', 'THREAD',
                'SYNC', 'MUTEX', 'COND', 'CANCEL', 'FUTURE', 'NET', 'NET_SOCKET', 'NET_BUFFER',
                'NET_PORT', 'NET_PORT_SELECT', 'NET_ENGINE', 'NET_TCP', 'NET_TCP_FUTURE')
    features += ('NET_PORT_IOCP',) if os.name == 'nt' else ('NET_PORT_EPOLL',)
    inputs = {str(path): sha(path) for path in (fixture, header, Path(__file__).resolve(), *compilers)}
    rows, products, failures = [], [], []
    out.mkdir(parents=True)
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False, Inputs=inputs,
                  Rows=rows, Products=products, Failures=failures, OrdinaryLiteralLoopback=True,
                  ExternalNetworkTests=False, NetworkSecurityTests=False, ShippingLibraryAcceptance=False,
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
        if args.sanitize and cc.stem != 'tcc':
            modes += [('instrumented', ['-O1', '-g', '-fsanitize=address,undefined',
                                       '-fno-omit-frame-pointer', '-fno-pie', '-no-pie'])]
        for mode, flags in modes:
            name = f'{index}.{mode}'
            try:
                binary = out/(name+('.exe' if os.name == 'nt' else ''))
                command = [cc, '-Wall', '-Wextra', '-Werror', *flags, '-I'+str(header.parent)]
                if cc.stem != 'tcc':
                    command += ['-std=c11']
                if os.name != 'nt':
                    command += ['-D_GNU_SOURCE']
                command += ['-DXRT_FEATURE_'+feature for feature in features]
                command += [fixture, *(['-lws2_32'] if os.name == 'nt' else ['-pthread', '-lm']), '-o', binary]
                run(name+'.build', command)
                products.append(dict(Path=str(binary), Sha256=sha(binary)))
                run(name+'.select', [binary], mode == 'instrumented')
                run(name+'.native', [binary, 'native'], mode == 'instrumented')
            except Exception as error:
                failures.append(dict(Name=name, Error=repr(error)))
                save()
    report['InputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
    report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
    report['Complete'] = not failures and report['InputsUnchanged'] and report['LogsUnchanged']
    save()
    assert report['Complete'], failures


if __name__ == '__main__':
    main()
