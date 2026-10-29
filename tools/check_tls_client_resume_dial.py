"""Companion actual-source resume binding check through literal-IP TLS Dial.

Consumes the pinned valid stream fixture from check_tls_client_resume_source.
Only two ordinary connections to a self-created 127.0.0.1 listener; no external
DNS, public endpoint, malformed material or network-security test.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, time
from xlir_public_build_inputs import verify_public_build


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build', 'fixture', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    out.mkdir(parents=True)
    build, pinned, _ = verify_public_build(root, args.build.resolve().parent)
    assert build['Complete'] and build['InputsUnchanged'] and build['Optimization'] == 'O2'
    original = args.fixture.resolve()
    text = original.read_text(encoding='utf8')
    replacements = [
        ('async func connect(NetEngine engine,TlsServer server,',
         'async func connect(NetEngine engine,NetResolver resolver,TlsServer server,'),
        ('auto client=TlsStream(engine,server.local(),context,"localhost")',
         'auto client=await TlsStream.dialAsync(engine,resolver,"127.0.0.1",server.local().port,context,'
         'TlsDialOptions().withServerName("localhost").withVerifyName("localhost").withServerNameFromHost(false))'),
        ('engine.start()', 'auto resolver=NetResolver()\nengine.start()'),
        ('await connect(engine,server,resumeClient(),true)',
         'await connect(engine,resolver,server,resumeClient(),true)'),
        ('await connect(engine,server,resumeClient().withoutResume(),false)',
         'await connect(engine,resolver,server,resumeClient().withoutResume(),false)'),
        ('server.close()\nengine.close()',
         'server.close()\nresolver.close()\nassert resolver.wait(10000)\nengine.close()'),
        ('TLS client resume transport:', 'TLS client resume literal-IP dial:')]
    for before, after in replacements:
        assert text.count(before) == 1, before
        text = text.replace(before, after)
    inputs = out / 'inputs'
    inputs.mkdir()
    fixture = inputs / 'tls.client.resume.dial.xl'
    fixture.write_text(text, encoding='utf8')
    cc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    driver = Path(build['Driver'])
    runtime = root / 'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot']) / 'release/libxl.so'
    env = dict(os.environ)
    if os.name == 'nt':
        shutil.copy2(runtime, out / 'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
    pinned.update((original, fixture, cc, runtime, Path(__file__).resolve()))
    result = dict(Complete=False, Inputs={str(q): sha(q) for q in sorted(pinned)}, Rows=[],
                  DialResumeBindingAccepted=False, ShippingAccepted=False,
                  OrdinaryLiteralLoopback=True, ExternalNetworkTests=False,
                  NetworkSecurityTests=False, MalformedCryptoTests=False)

    def save():
        (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

    def run(name, command):
        command = list(map(str, command))
        log = out / (name + '.log')
        start = time.monotonic()
        with log.open('wb') as stream:
            code = subprocess.run(command, cwd=root, env=env, stdout=stream,
                                  stderr=subprocess.STDOUT, timeout=240).returncode
        result['Rows'].append(dict(Name=name, Command=command, Exit=code, Log=str(log),
                                   Sha256=sha(log), Seconds=time.monotonic() - start))
        save()
        print(name, code, flush=True)
        assert code == 0, str(log)
        return log.read_text(encoding='utf8', errors='replace')

    save()
    generated = out / 'tls.client.resume.dial.c'
    run('emit', [driver, fixture, 'cgen-default', generated])
    assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6TlsClientContext\w*\([^;]*\)\s*\{',
                         generated.read_text(encoding='utf8'), re.M)
    for stage in ('typed', 'lowered'):
        run(stage, [driver, fixture, 'ir-' + stage, out / (stage + '.json')])
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fmax-errors=4', '-DXL_STATIC_EMBED',
             '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
             '-Wno-unused-label', '-Wno-unused-but-set-variable', '-Wno-unused-const-variable',
             '-Wno-missing-field-initializers', '-I' + str(root), '-I' + str(root / 'release'),
             '-I' + str(root / 'release/include'), '-I' + str(driver.parent / 'include')]
    if os.name != 'nt':
        flags.append('-D_GNU_SOURCE')
    for opt in ('O0', 'O2'):
        run('strict.' + opt, [cc, *flags, '-' + opt, '-c', generated, '-o', out / (opt + '.o')])
    for mode in ('run', 'module'):
        assert 'literal-IP dial:' in run(mode, [driver, fixture, mode, out / mode])
    binary = out / ('dial.aot.exe' if os.name == 'nt' else 'dial.aot')
    target = binary.with_suffix('') if os.name == 'nt' else binary
    run('aot.build', [driver, fixture, 'exe', target])
    assert 'literal-IP dial:' in run('aot.run', [binary])
    imports = run('aot.imports', ['E:/software/w64devkit/bin/objdump.exe', '-p', binary]
                  if os.name == 'nt' else ['readelf', '-d', binary])
    assert not any(q in imports.lower() for q in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so'))
    bundle = (Path(str(target) + '.temps') / 'xl6_xrt_impl.c').read_text(encoding='utf8')
    assert 'XRT_MODULE_TLS_CLIENT_RESUME' in bundle and '_MODULE_ALL' not in bundle
    result['InputsUnchanged'] = all(sha(q) == h for q, h in result['Inputs'].items())
    result['DialResumeBindingAccepted'] = result['InputsUnchanged']
    result['Complete'] = result['DialResumeBindingAccepted']
    result['Stages'] = len(result['Rows'])
    result['Products'] = [dict(Path=str(q), Sha256=sha(q)) for q in out.rglob('*')
                          if q.is_file() and q.name != 'results.json']
    save()
    assert result['Complete']


if __name__ == '__main__':
    main()
