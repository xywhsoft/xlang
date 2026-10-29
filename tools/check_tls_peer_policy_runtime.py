"""Offline typed TLS peer predicates, snapshots, code leases and OOM rollback.
Real resident closure/profile and default trust, no network/security tests.
This is not public language/JIT/AOT or full TLS shipping acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time
from tls_verifier_fixtures import fixtures
from runtime_sources import sources as shipping_sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--tcc', type=Path)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists(), 'retain prior acceptance evidence; use a fresh output directory'
    out.mkdir(parents=True)
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    gcc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc = (args.tcc or Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe')).resolve()
    assert fixtures(root, out) == ['rsa', 'p256', 'p384', 'ed25519']
    context = root/'src/runtime/xl6_tls_verifier_context.c'
    assert shipping_sources(root).count(context.resolve()) == 1
    for name in ('compiler_dll.rsp.in', 'runtime_dll.rsp.in'):
        template = (root/'tools'/name).read_text(encoding='utf-8-sig').replace('\\', '/')
        assert template.count('src/runtime/xl6_tls_verifier_context.c') == 1
    plan = root/'test/support/tls_peer_policy_plan.c'
    support = root/'test/support/tls_peer_policy_runtime.c'
    paths = {Path(__file__), gcc, tcc, plan, support, root/'src/compile/xl6_compile_unit.c',
             root/'build_dll.bat', root/'tools/runtime_sources.py',
             *[root/'tools'/n for n in ('runtime_dll.rsp.in', 'compiler_dll.rsp.in',
                 'tls_verifier_fixtures.py', 'x509_path_fixtures.py', 'check_key_format_wire.py')],
             *[root/'test/support'/('crypto_'+n+'_runtime.c') for n in ('rsa', 'nist', 'ed25519', 'xdh')],
             *[p for p in (root/'src').rglob('*') if p.is_file() and p.suffix in ('.h', '.inc')],
             *[root/p for p in ('lib/xrt/xrt.h', 'lib/xruntime/xruntime.h',
                 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h')],
             *[p for p in out.iterdir() if p.is_file()]}
    report = dict(Complete=False, Inputs={str(p): sha(p) for p in sorted(paths)}, Rows=[], Failures=[],
                  NetworkTraffic=False, SecurityTests=False, PublicLanguageCallbacksAccepted=False,
                  FullTlsAccepted=False, FullGoalComplete=False)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')

    def run(name, command, instrumented=False):
        command = list(map(str, command))
        executed = command
        if command[0] in (str(gcc), str(tcc)):
            rsp = out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+p.replace('\\', '/').replace('"', '\\"')+'"'
                                     for p in command[1:])+'\n', encoding='utf8')
            executed = [command[0], '@'+str(rsp)]
        log = out/(name+'.log')
        env, start = dict(os.environ), time.monotonic()
        if instrumented:
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:
            code = subprocess.run(executed, cwd=root, env=env, stdout=stream,
                                  stderr=subprocess.STDOUT, timeout=300).returncode
        report['Rows'].append(dict(Name=name, Command=command, Exit=code, Log=str(log),
                                   Sha256=sha(log), Seconds=time.monotonic()-start))
        save()
        print(name, code, flush=True)
        assert code == 0, str(log)
        return log.read_text(errors='replace')

    flags = ['-std=c11', '-DXL_STATIC_EMBED', '-I'+str(root), '-I'+str(root/'release'),
             '-I'+str(root/'release/include'), '-I'+str(out)]+([] if os.name == 'nt' else ['-D_GNU_SOURCE'])
    planner = out/('planner.exe' if os.name == 'nt' else 'planner')
    run('planner.build', [gcc, *flags, '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers',
        '-O0', '-ffunction-sections', '-fdata-sections', '-fno-jump-tables', '-fno-asynchronous-unwind-tables',
        '-fno-unwind-tables', plan, root/'src/compile/xl6_compile_unit.c', '-Wl,--gc-sections', '-o', planner])
    sources = [root/p for p in run('planner.sources', [planner, '--sources']).splitlines()]
    modules = run('planner.modules', [planner, '--modules'])
    assert 'XRT_MODULE_TLS_VERIFY' in modules and not any(name in modules for name in
        ('_MODULE_ALL', 'XRT_MODULE_TLS_CLIENT', 'XRT_MODULE_TLS_SERVER', 'XRT_MODULE_TLS_SESSION', 'XRT_MODULE_NET_SOCKET'))
    assert sources.count(context) == 1
    report['Closure'] = dict(Sources=list(map(str, sources)), Modules=modules)
    report['Inputs'].update({str(p): sha(p) for p in sources})
    impl = out/'native.c'
    impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+
        ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if 'XRUNTIME_MODULE_' in modules
         else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'), encoding='utf8')
    libs = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']
    modes = [('gcc-O0', gcc, ['-O0']), ('gcc-O2', gcc, ['-O2']), ('tcc', tcc, [])]
    if os.name != 'nt':
        modes.append(('asan', gcc, ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']))
    for name, cc, options in modes:
        try:
            binary = out/(name+('.exe' if os.name == 'nt' else ''))
            run(name+'.build', [cc, *flags, *options,
                *(['-Wall', '-Wextra', '-Werror', '-fmax-errors=5'] if cc == gcc else []),
                support, impl, *sources, *libs, '-o', binary])
            assert 'TLS peer policy: default trust, bool predicates' in run(name+'.run', [binary], name == 'asan')
        except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
            report['Failures'].append(dict(Name=name, Error=repr(error)))
            save()
    report['InputsUnchanged'] = all(sha(p) == value for p, value in report['Inputs'].items())
    report['Products'] = {str(p): sha(p) for p in out.iterdir() if p.is_file() and p.name != 'results.json'}
    report['Complete'] = not report['Failures'] and report['InputsUnchanged']
    save()
    assert report['Complete'], report['Failures']


if __name__ == '__main__':
    main()
