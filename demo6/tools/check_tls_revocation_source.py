"""Actual-source TLS CRL facade: typed/lowered IR, mechanical C, JIT/module/AOT.
Normal offline trust/CRLs only. Not shipping or whole-TLS acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, time
from tls_revocation_fixtures import language_fixtures


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['root', 'build', 'out']:
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--fixtures', type=Path)
    parser.add_argument('--preflight-only', action='store_true')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    out.mkdir(parents=True)
    build = json.loads(args.build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:
        assert sha(row['Path']) == row['Sha256'].lower(), row['Path']
    driver = Path(build['Driver'])
    assert sha(driver) == build['DriverSha256'].lower()
    fixture_dir = args.fixtures.resolve() if args.fixtures else out/'fixtures'
    if args.fixtures:
        cases = [fixture_dir/('tls.revocation.'+name+'.xl') for name in
                 ['rsa', 'p256', 'p384', 'ed25519', 'unknown', 'lenient', 'throw', 'cycle',
                  'partial', 'partitions', 'delta', 'indirect']]
        empty = fixture_dir/'tls.revocation.empty.xl'
    else:
        fixture_dir.mkdir()
        cases, empty = language_fixtures(root, fixture_dir)
    cases.append(empty)
    env = dict(os.environ)
    if os.name == 'nt':
        shutil.copy2(root/'release/xl.dll', out/'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    compiler = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    contract = Path(__file__).resolve().parents[1]/'test/cases/xlir_tls_revocation_contract.c'
    negatives = {
        'request_raw': 'auto bad = TlsRevocationRequest(point(null))\n',
        'request_write': 'func bad(TlsRevocationRequest request)\n request.verifiedAt = time(0)\nend\n',
        'policy_arity': 'auto bad = TlsRevocationPolicy(func() as X509RevocationCheck?\n return null\nend)\n',
        'policy_result': 'auto bad = TlsRevocationPolicy(func(TlsRevocationRequest request) as bool\n return true\nend)\n'}
    negative_paths = []
    for name, text in negatives.items():
        path = out/('reject_'+name+'.xl')
        path.write_text(text, encoding='utf8')
        negative_paths.append(path)
    files = {Path(__file__), args.build.resolve(), compiler, contract, *cases, *negative_paths,
             *[p for p in fixture_dir.rglob('*') if p.is_file()]}
    files.update(Path(__file__).parent/name for name in ['tls_revocation_fixtures.py', 'tls_verifier_fixtures.py',
        'x509_path_fixtures.py', 'check_key_format_wire.py'])
    report = dict(Complete=False, SourceBuild=str(args.build), Inputs={str(p): sha(p) for p in sorted(files)},
                  Rows=[], Failures=[], NetworkTraffic=False, SecurityTests=False,
                  ShippingAccepted=False, FullTlsAccepted=False)

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')

    def run(name, command, expected=0):
        command = list(map(str, command))
        executed = command
        if Path(command[0]) == compiler:
            rsp = out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+s.replace('\\', '/').replace('"', '\\"')+'"' for s in command[1:])+'\n', encoding='utf8')
            executed = [command[0], '@'+str(rsp)]
        log, start = out/(name+'.log'), time.monotonic()
        with log.open('wb') as stream:
            code = subprocess.run(executed, cwd=root, env=env, stdout=stream,
                                  stderr=subprocess.STDOUT, timeout=900).returncode
        report['Rows'].append(dict(Name=name, Command=command, Exit=code, Expected=expected,
                                  Log=str(log), Sha256=sha(log), Seconds=time.monotonic()-start))
        save()
        print(name, code, flush=True)
        assert code == expected, str(log)
        return log.read_text(encoding='utf8', errors='replace')

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fmax-errors=4', '-DXL_STATIC_EMBED',
        '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-label',
        '-Wno-unused-but-set-variable', '-Wno-unused-const-variable', '-Wno-missing-field-initializers',
        '-I'+str(root), '-I'+str(root/'release'), '-I'+str(root/'release/include'), '-I'+str(driver.parent/'include'),
        *(['-D_GNU_SOURCE'] if os.name != 'nt' else [])]
    for case in cases:
        name = case.stem
        try:
            generated = out/(name+'.c')
            run(name+'.emit', [driver, case, 'cgen-default', generated])
            text = generated.read_text(encoding='utf8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Tls\w+\([^;]*\)\s*\{', text, re.M)
            if case != empty:
                assert 'XL6_NativeClassCallbackContextV1' in text and 'xl6TlsRevocationPolicyCreateV1(' in text
                if name.split('.')[-1] in ['rsa', 'p256', 'p384', 'ed25519', 'cycle']:
                    assert 'xl6TlsVerifierCreateCallbackRevocationV1(' in text
            for stage in ['typed', 'lowered']:
                run(name+'.'+stage, [driver, case, 'ir-'+stage, out/(name+'.'+stage+'.json')])
            for opt in ['O0', 'O2']:
                run(name+'.strict.'+opt, [compiler, *flags, '-'+opt, '-c', generated, '-o', out/(name+'.'+opt+'.o')])
        except Exception as error:
            report['Failures'].append(dict(Name=name, Error=repr(error)))
            save()
    for path in negative_paths:
        try:
            text = run(path.stem, [driver, path, 'cgen-default', out/(path.stem+'.c')], 1)
            assert 'without diagnostic' not in text and not (out/(path.stem+'.c')).exists()
        except Exception as error:
            report['Failures'].append(dict(Name=path.stem, Error=repr(error)))
            save()
    if not report['Failures'] and not args.preflight_only:
        for case in cases:
            name = case.stem
            try:
                for mode in ['run', 'module']:
                    run(name+'.'+mode, [driver, case, mode, out/(name+'.'+mode)])
                binary = out/(name+'.aot'+('.exe' if os.name == 'nt' else ''))
                target = binary.with_suffix('') if os.name == 'nt' else binary
                run(name+'.aot.build', [driver, case, 'exe', target])
                run(name+'.aot.run', [binary])
                imports = run(name+'.aot.imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', binary] if os.name == 'nt' else ['readelf', '-d', binary])
                assert not any(s in imports.lower() for s in ['xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'])
                bundle = (Path(str(target)+'.temps')/'xl6_xrt_impl.c').read_text(encoding='utf8')
                assert '_MODULE_ALL' not in bundle
                if case == empty:
                    assert not any(s in bundle for s in ['xl6_tls_revocation', 'xl6_native_class_callback', 'XRT_MODULE_TLS_'])
            except Exception as error:
                report['Failures'].append(dict(Name=name, Error=repr(error)))
                save()
        objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
                   ['xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c']]
        runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'
        libraries = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else [
            '-pthread', '-ldl', '-lm', '-Wl,--export-dynamic', '-Wl,-rpath,'+str(runtime.parent)]
        for opt in ['O0', 'O2']:
            try:
                label = 'contract.'+opt
                obj, binary = out/(label+'.o'), out/(label+('.exe' if os.name == 'nt' else ''))
                run(label+'.compile', [compiler, *flags, '-'+opt, '-c', contract, '-o', obj])
                run(label+'.link', [compiler, *objects, obj, runtime, *libraries, '-o', binary])
                assert '14 refusals and 28 empty-output gates' in run(label+'.run', [binary, cases[1]])
            except Exception as error:
                report['Failures'].append(dict(Name='contract.'+opt, Error=repr(error)))
                save()
    report['InputsUnchanged'] = all(sha(p) == h for p, h in report['Inputs'].items())
    report['Products'] = {str(p): sha(p) for p in out.rglob('*') if p.is_file() and p.name != 'results.json'}
    report['Stages'] = len(report['Rows'])
    report['PreflightOnly'] = args.preflight_only
    report['Complete'] = report['InputsUnchanged'] and not report['Failures']
    save()
    assert report['Complete'], report['Failures']


if __name__ == '__main__':
    main()
