"""Ensure actual retained CRL evidence does not impose unused crypto/policy code."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['root', 'out']:
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    out.mkdir(parents=True)
    plan = Path(__file__).resolve().parents[1]/'test/support/tls_revocation_prune_plan.c'
    compiler = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    files = {Path(__file__), plan, compiler, root/'src/compile/xl6_compile_unit.c',
             *[p for p in (root/'src').rglob('*') if p.is_file() and p.suffix in ['.h', '.inc']]}
    report = dict(Complete=False, Inputs={str(p): sha(p) for p in sorted(files)}, Rows=[])
    binary = out/('planner.exe' if os.name == 'nt' else 'planner')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-missing-field-initializers',
        '-O0', '-DXL_STATIC_EMBED', '-ffunction-sections', '-fdata-sections', '-fno-jump-tables',
        '-fno-asynchronous-unwind-tables', '-fno-unwind-tables', '-I'+str(root),
        '-I'+str(root/'release'), '-I'+str(root/'release/include')]
    if os.name != 'nt':
        flags.append('-D_GNU_SOURCE')
    commands = [[compiler, *flags, plan, root/'src/compile/xl6_compile_unit.c', '-Wl,--gc-sections', '-o', binary]]
    commands.extend([binary, '--'+name+'-'+kind] for name in ['check', 'peer'] for kind in ['sources', 'modules'])
    for i, command in enumerate(commands):
        command = list(map(str, command))
        completed = subprocess.run(command, cwd=root, capture_output=True, timeout=90)
        log = out/(str(i)+'.log')
        log.write_bytes(completed.stdout+completed.stderr)
        report['Rows'].append(dict(Command=command, Exit=completed.returncode, Log=str(log), Sha256=sha(log)))
        assert completed.returncode == 0, log
    sources = (out/'1.log').read_text().splitlines()
    modules = (out/'2.log').read_text()
    assert any('xl6_x509_validated_crl_owner.c' in s for s in sources)
    assert not any(Path(s).name in ['xl6_x509_crl_policy.c', 'xl6_x509_crl_set.c',
        'xl6_x509_revocation_update.c'] or 'xl6_tls_' in s for s in sources)
    assert not any(s in modules for s in ['XRT_MODULE_X509_VERIFY_', 'XRT_MODULE_X509_CRL_POLICY', 'XRT_MODULE_TLS_'])
    sources = (out/'3.log').read_text()
    assert 'xl6_tls_peer_policy.c' in sources and 'xl6_tls_revocation' not in sources and 'xl6_x509_revocation' not in sources
    assert 'XRT_MODULE_X509_CRL_POLICY' not in (out/'4.log').read_text()
    report['InputsUnchanged'] = all(sha(Path(p)) == h for p, h in report['Inputs'].items())
    report['Complete'] = report['InputsUnchanged']
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    assert report['Complete']
    print('Five pruning phases passed: empty Check and additional-only Verifier do not select CRL algorithms/policy')


if __name__ == '__main__':
    main()
