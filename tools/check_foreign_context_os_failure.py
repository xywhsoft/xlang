"""Cold OS-key rollback in actual context source, using a qualified full XRT.

Only OS allocation/set calls in that source are fault-injected. This does not
qualify complete language callback entry under cold OS TLS failure or unload.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from full_compiler_qualification import verify_full_compiler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    a = parser.parse_args()
    root, compiler, build, out = (v.resolve() for v in
        (a.root, a.compiler, a.compiler_report, a.out))
    metadata = verify_full_compiler(build, compiler)
    snapshot = Path(metadata['source'])
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    win = os.name == 'nt'
    cc = Path(shutil.which('gcc')).resolve()
    case = root / 'test/cases/foreign_context_os_failure.c'
    relative = ('src/runtime/xl6_foreign_context.c', 'src/runtime/xl6_foreign_context.h',
        'src/runtime/xl6_code_lease.h', 'src/runtime/xl6_code_owner.h', 'src/common/xl6_xrt.h')
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
        case, cc, build, snapshot / 'tools/stage_runtime_headers.py']
    for name in relative:
        assert sha(root / name) == sha(snapshot / name), name
        files += [root / name, snapshot / name]
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')
    files += [compiler.parent / name for name in names]
    inputs = {str(p): sha(p) for p in files}
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_')}
    env.update(PYTHONUTF8='1', PYTHONDONTWRITEBYTECODE='1', TMP=str(out), TEMP=str(out), TMPDIR=str(out))
    key = 'PATH' if win else 'LD_LIBRARY_PATH'
    env[key] = str(compiler.parent) + os.pathsep + env.get(key, '')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
        networkTests=False, shippingDelivery=False, runtimeImplementationChanged=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command):
        start = time.monotonic()
        result = subprocess.run(list(map(str, command)), env=env, capture_output=True, timeout=45)
        log = out / (name + '.log')
        data = result.stdout + result.stderr
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=list(map(str, command)), exitCode=result.returncode,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert not result.returncode, log
        return data

    # The private header merge leaves the verified compiler's disk SDK intact.
    sdk = out / 'sdk'
    shutil.copytree(compiler.parent / 'include', sdk / 'include')
    run('private-headers', [sys.executable, '-B', snapshot / 'tools/stage_runtime_headers.py',
        '--root', snapshot, '--out', out / 'generated'])
    shutil.copytree(out / 'generated/include', sdk / 'include', dirs_exist_ok=True)
    inputs.update((str(p), sha(p)) for p in sdk.rglob('*') if p.is_file())
    for opt in ('O0', 'O2'):
        binary = out / (opt + ('.exe' if win else ''))
        flags = ['-std=c11', '-' + opt, '-Wall', '-Wextra', '-Werror', '-I' + str(sdk),
            '-I' + str(sdk / 'include'), '-I' + str(root)]
        if not win:
            flags += ['-D_GNU_SOURCE', '-pthread', '-ldl', '-lm']
        run(opt + '-build', [cc, *flags, case, compiler.parent / names[1], '-o', binary])
        data = run(opt + '-normal', [binary, '0'])
        match = re.search(rb'FOREIGN_CONTEXT_OS_FAILURE_ROLLBACK_OK slots=(\d+)', data)
        assert match
        count = int(match[1])
        assert 0 < count < 32
        for slot in range(1, count + 1):
            data = run(opt + '-fail-' + str(slot), [binary, str(slot)])
            assert match[0] in data
    verify_full_compiler(build, compiler)
    report.update(complete=True, passed=True, slots=count,
        inputsUnchanged=all(sha(p) == value for p, value in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
