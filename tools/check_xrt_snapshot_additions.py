"""Compile/dispatch the three additive XRT APIs through the actual C toolchain.

Only in-process error slots, numeric address parsing and NULL-session argument
checking execute. No sockets, DNS, certificates or TLS handshakes are used.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
from full_compiler_qualification import verify_full_compiler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    a = parser.parse_args()
    root, compiler, build, out = (p.resolve() for p in (a.root, a.compiler, a.compiler_report, a.out))
    metadata = verify_full_compiler(build, compiler)
    snapshot = Path(metadata['source'])
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    source = root / 'test/cases/xrt_snapshot_additions.c'
    win = os.name == 'nt'
    objdump = Path(shutil.which('objdump')).resolve()
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'), source,
        objdump, build, snapshot / 'lib/xrt/xrt.h', snapshot / 'lib/xrt/xrt.snapshot.json',
        snapshot / 'lib/xrt/upstream.json', compiler.parent / 'include/xrt_decl.h',
        snapshot / 'src/api/xl_xrt_symbols.inc', snapshot / 'src/compile/xl6_artifact_native_symbols.inc',
        *(compiler.parent / n for n in names)]
    inputs = {str(p): sha(p) for p in files}
    core = json.loads((snapshot / 'lib/xrt/xrt.snapshot.json').read_text(encoding='utf-8-sig'))
    assert sha(compiler.parent / 'include/xrt_decl.h') == core['declarations']['sha256'].lower()
    symbols = (snapshot / 'src/api/xl_xrt_symbols.inc').read_text(encoding='utf-8-sig')
    for name in ('xrtNetAddrTryParse', 'xrtTlsClientResumeCapacity', 'xrtTrySwapErrorTake'):
        assert 'XL_XRT_SYMBOL(' + name + ')' in symbols
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_')}
    env.update(TMP=str(out), TEMP=str(out), TMPDIR=str(out), PYTHONUTF8='1')
    key = 'PATH' if win else 'LD_LIBRARY_PATH'
    env[key] = str(compiler.parent) + os.pathsep + env.get(key, '')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
        networkTests=False, shippingDelivery=False, xrtRevision=metadata['xrtRevision'])

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, marker=None):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, env=env, cwd=out, capture_output=True, timeout=120)
        data = result.stdout + result.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert not result.returncode and (not marker or marker in data), log
        return data

    run('c-jit', [compiler, 'run', source], b'XRT_SNAPSHOT_ADDITIONS_OK')
    binary = out / ('additions.exe' if win else 'additions')
    run('c-aot-build', [compiler, 'build', source, '-o', binary])
    run('c-aot-run', [binary], b'XRT_SNAPSHOT_ADDITIONS_OK')
    raw = run('c-aot-imports', [objdump, '-p', binary])
    imports = re.findall(rb'DLL Name:\s*(\S+)', raw) if win else re.findall(rb'NEEDED\s+(\S+)', raw)
    imports = [n.decode().lower() for n in imports]
    assert not set(imports) & {'xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so', 'libtcc.dll', 'libtcc.so'}
    verify_full_compiler(build, compiler)
    report.update(complete=True, passed=True, imports=imports, programSha256=sha(binary),
        inputsUnchanged=all(sha(p) == value for p, value in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
