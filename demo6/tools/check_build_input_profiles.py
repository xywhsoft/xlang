"""Verify real shared-library and static-library build receipts on Windows."""
from pathlib import Path
import argparse, ctypes, hashlib, json, shutil, subprocess, uuid

ap = argparse.ArgumentParser()
ap.add_argument('--compiler-root', type=Path, required=True)
ap.add_argument('--output-root', type=Path, required=True)
a = ap.parse_args()
root = a.compiler_root.resolve()
work = a.output_root.resolve() / ('build-input-profiles-' + str(uuid.uuid4()))
work.mkdir(parents=True)
shutil.copy2(__file__, work / Path(__file__).name)
checks, calls = [], []


def check(label, okay):
    checks.append({'label': label, 'passed': bool(okay)})
    if not okay:
        raise AssertionError(label)


def call(name, argv, expected=0):
    result = subprocess.run([str(v) for v in argv], cwd=work, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=90)
    (work / (name + '.log')).write_bytes(result.stdout)
    calls.append({'name': name, 'args': [str(v) for v in argv], 'exitCode': result.returncode})
    check(name, result.returncode == expected)


def receipt(name):
    value = json.loads((work / name).read_text(encoding='utf8'))
    check(name + ' complete', value['schema'] == 1 and value['complete'] is True and
          value['scope'] == 'xlang-native-build-disk-inputs')
    for row in [*value['files'], value['artifact']]:
        p = Path(row['path'])
        check('digest ' + p.name, p.is_absolute() and p.is_file() and
              p.stat().st_size == row['size'] and hashlib.sha256(p.read_bytes()).hexdigest() == row['sha256'])
    check(name + ' missing candidates', all(not Path(p).exists() for p in value['nativeMissing']))
    return value


try:
    xl = root / 'release/xl.exe'
    (work / 'value.h').write_text('#define VALUE 13\n', encoding='utf8')
    (work / 'shared.c').write_text('#include "value.h"\n__declspec(dllexport) int receipt_value(void) {return VALUE;}\n', encoding='utf8')
    call('shared-build', [xl, 'build', 'shared.c', '--shared', '-o', 'profile.dll', '--input-report', 'shared.json'])
    value = receipt('shared.json')
    paths = {Path(row['path']).resolve() for row in value['files']}
    check('shared source and header observed', {work / 'shared.c', work / 'value.h'} <= paths)
    dll = ctypes.CDLL(str(work / 'profile.dll'))
    dll.receipt_value.argtypes, dll.receipt_value.restype = [], ctypes.c_int
    check('actual shared output', dll.receipt_value() == 13)

    (work / 'static.c').write_text('int receipt_static(void) {return 17;}\n', encoding='utf8')
    call('static-object', [root / 'build/tmp/tcc_bootstrap.exe', '-I' + str(root / 'lib/tcc/include'), '-c', 'static.c', '-o', 'static.o'])
    call('static-archive', ['ar', 'rcs', 'libreceipt.a', 'static.o'])
    (work / 'main.c').write_text('extern int receipt_static(void); int main(void) {return receipt_static();}\n', encoding='utf8')
    call('static-link-build', [xl, 'build', 'main.c', '--link', work / 'libreceipt.a', '-o', 'static.exe', '--input-report', 'static.json'])
    value = receipt('static.json')
    check('actual archive bytes observed', work / 'libreceipt.a' in {Path(row['path']).resolve() for row in value['files']})
    call('actual static output', [work / 'static.exe'], expected=17)
except Exception as e:
    error = str(e)
else:
    error = None
report = {'status': 'passed' if error is None else 'failed', 'error': error,
          'checks': checks, 'calls': calls, 'compilerSha256': hashlib.sha256((root / 'release/xl.exe').read_bytes()).hexdigest()}
(work / 'evidence.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
print(work / 'evidence.json', report['status'], len(checks))
raise SystemExit(0 if error is None else 1)
