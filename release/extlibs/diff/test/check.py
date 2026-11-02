"""Bounded C/XLang gate with independent results and exact input identities."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[4]
PKG = ROOT / 'release/extlibs/diff'
WINDOWS = os.name == 'nt'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--skip-aot', action='store_true')
    args = parser.parse_args()
    base = ROOT / 'build/test'
    base.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='diff-gate-', dir=base))
    env = dict(os.environ, TEMP=str(work), TMP=str(work), TMPDIR=str(work))
    env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    cc = shutil.which(os.environ.get('CC', 'gcc'))
    xl = ROOT / ('release/xl.exe' if WINDOWS else 'release/xl')
    phases, inputs, after = [], {}, {}
    report = {'status': 'failed', 'work': str(work), 'skipAot': args.skip_aot,
              'phases': phases, 'inputsBefore': inputs, 'inputsAfter': after}

    def run(name, command, expected=0):
        command = list(map(str, command))
        prefix = work / f'{len(phases):02d}'
        start = time.monotonic()
        with prefix.with_suffix('.stdout').open('wb') as out, prefix.with_suffix('.stderr').open('wb') as err:
            process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=out, stderr=err,
                                       start_new_session=not WINDOWS,
                                       creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if WINDOWS else 0)
            try:
                code = process.wait(timeout=150)
            except subprocess.TimeoutExpired:
                if WINDOWS:
                    subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
                else:
                    os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=15)
                code = 'timeout'
        row = {'name': name, 'command': command, 'exit': code, 'expected': expected,
               'seconds': time.monotonic()-start, 'stdout': str(prefix.with_suffix('.stdout')),
               'stderr': str(prefix.with_suffix('.stderr'))}
        phases.append(row)
        text = prefix.with_suffix('.stdout').read_text(encoding='utf8')
        if code != expected:
            raise RuntimeError(f'{name}: exit {code}, expected {expected}; see {row["stderr"]}')
        print(f'[diff] {name}: PASS (exit {code})', flush=True)
        return text.replace('\r\n', '\n')

    def same(name, actual, golden):
        if actual != golden:
            raise AssertionError(name + ': output differs from fixed expected bytes')

    def binary(name):
        return work / (name + ('.exe' if WINDOWS else ''))

    try:
        if not cc:
            raise RuntimeError('GCC is required on PATH (or set CC)')
        cc1 = Path(subprocess.check_output([cc, '-print-prog-name=cc1'], text=True, timeout=10).strip())
        if not cc1.is_file():
            raise RuntimeError('GCC cc1 identity is unavailable')
        paths = set(PKG.rglob('*')) | set((ROOT / 'release/include').rglob('*'))
        paths.update([Path(cc), cc1.resolve(), Path(sys.executable), xl,
                      ROOT / ('release/xl.dll' if WINDOWS else 'release/libxl.so'),
                      ROOT / ('release/xlc.dll' if WINDOWS else 'release/libxlc.so'),
                      ROOT / 'lib/xrt/xrt.h', ROOT / 'lib/xruntime/xruntime.h',
                      ROOT / 'test/cases/extlib_diff.xl', ROOT / 'release/examples/extlibs/diff.xl'])
        inputs.update({str(p): sha(p) for p in sorted(paths) if p.is_file()})
        report['compilerVersion'] = run('compiler identity', [cc, '--version'])
        manifest = json.loads((PKG / 'xlang.package.json').read_text(encoding='utf8'))
        modules = ['-D' + v for v in manifest['artifact']['xrtModules']]
        flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt']
        if not WINDOWS:
            flags = ['-D_GNU_SOURCE', *flags, '-g', '-fsanitize=address,undefined',
                     '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', '-pthread']
        links = ['-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm'] if WINDOWS else ['-lm', '-ldl']
        run('manifest macro compile', [cc, *flags, *modules, '-fsyntax-only', PKG/'diff_native.c'])
        runtime = work / 'runtime.o'
        run('selected test runtime', [cc, *flags, '-c', PKG/'test/runtime.c', '-o', runtime])
        native = work / 'native.o'
        run('production native object', [cc, *flags, '-c', PKG/'diff_native.c', '-o', native])

        def build(name, source, embedded=False, extra=()):
            command = [cc, *flags, *extra, source]
            if not embedded:
                command.append(native)
            run(name + ' GCC build', [*command, runtime, *links, '-o', binary(name)])
            return binary(name)

        # 旧用例仍执行；不以新性能夹具替代格式、越界和线程边界。
        for name in ('test_diff', 'test_unterminated', 'test_threads'):
            exe = build(name, PKG/f'test/{name}.c', extra=['-DNDEBUG'] if name == 'test_unterminated' else [])
            run(name + ' GCC', [exe])
            run(name + ' TCC JIT', [xl, 'run', PKG/f'test/{name}.c', PKG/'diff_native.c'])
        corpus = work / 'corpus'
        run('independent Python corpus', [sys.executable, '-B', PKG/'test/oracle.py', corpus])
        report['corpus'] = json.loads((corpus/'corpus.json').read_text(encoding='utf8'))
        oracle = build('oracle', PKG/'test/oracle_c.c', embedded=True)
        run('full matrix oracle', [oracle, corpus/'corpus.bin'])
        collision = build('collision', PKG/'test/oracle_c.c', embedded=True, extra=['-DXDIFF_FORCE_COLLISION'])
        run('constant hash oracle', [collision, corpus/'corpus.bin'])
        for name in ('wrong-output', 'wrong-stats', 'truncated'):
            run('oracle rejects ' + name, [oracle, corpus/(name + '.bin')], expected=1)
        plan = build('plan', PKG/'test/suffix_plan_c.c', embedded=True)
        run('suffix shape and actual OOM', [plan])
        run('suffix TCC JIT', [xl, 'run', PKG/'test/suffix_plan_c.c'])
        run('oracle TCC JIT', [xl, 'run', PKG/'test/oracle_c.c', '--', corpus/'corpus.bin'])
        for source in ('test/cases/extlib_diff.xl', 'release/extlibs/diff/test/boundaries.xl',
                       'release/extlibs/diff/test/owned_results.xl'):
            run(source + ' JIT', [xl, 'run', ROOT/source])
        generated = work / 'owned.c'
        run('whole-call ownership cgen', [xl, 'cgen', PKG/'test/owned_probe.xl', '--emit', generated])
        code = generated.read_text(encoding='utf8')
        for symbol in ('xdiff_lines', 'xdiff_stats'):
            if len(re.findall(r'\b\w+\.Data\s*=\s*' + symbol + r'\s*\(', code)) != 1:
                raise AssertionError('Result must be adopted exactly once: ' + symbol)
        if 'xl6CStringCopyV1' in code or re.search(r'=\s*xlir_string_copy\s*\(', code):
            raise AssertionError('Unexpected input/result body copy')
        prototypes = re.findall(r'(?m)^extern\s+[^\n;{}]*\bxdiff_(?:lines|stats)\s*\([^;\n]*\);', code)
        if len(prototypes) != 2:
            raise AssertionError('Missing generated native prototypes')
        abi = work / 'abi.c'
        abi.write_text('#include "' + (PKG/'include/diff_native.h').as_posix() + '"\n' + '\n'.join(prototypes), encoding='utf8')
        run('two actual generated C prototypes', [cc, *flags, '-fsyntax-only', abi])
        wrong = work / 'wrong-abi.c'
        wrong.write_text(abi.read_text(encoding='utf8') + '\nint xdiff_lines(const char*,const char*);\n', encoding='utf8')
        run('wrong C prototype negative control', [cc, *flags, '-fsyntax-only', wrong], expected=1)

        examples = [(PKG/'examples/diff_c.c', ROOT/'release/examples/extlibs/diff.xl',
                     (PKG/'test/diff.expected.txt').read_text(encoding='utf8'), 'diff'),
                    (PKG/'examples/review_c.c', PKG/'examples/review.xl',
                     (PKG/'examples/review.expected.txt').read_text(encoding='utf8'), 'review')]
        for c, xlang, golden, name in examples:
            exe = build(name, c)
            same(name + ' GCC', run(name + ' GCC', [exe]), golden)
            same(name + ' C JIT', run(name + ' C JIT', [xl, 'run', c]), golden)
            same(name + ' XL JIT', run(name + ' XL JIT', [xl, 'run', xlang]), golden)
        if not args.skip_aot:
            sources = [(ROOT/'test/cases/extlib_diff.xl', None),
                       (PKG/'test/owned_results.xl', None), (PKG/'test/boundaries.xl', 'diff boundaries: PASS\n'),
                       (PKG/'test/suffix_plan_c.c', None), (PKG/'test/oracle_c.c', None)]
            for c, xlang, golden, _ in examples:
                sources += [(c, golden), (xlang, golden)]
            for i, (source, golden) in enumerate(sources):
                exe = binary('aot-' + str(i))
                run('AOT build ' + str(i), [xl, 'build', source, '-o', exe])
                command = [exe, corpus/'corpus.bin'] if source.name == 'oracle_c.c' else [exe]
                output = run('AOT run ' + str(i), command)
                if golden is not None:
                    same('AOT ' + str(i), output, golden)
        report['status'] = 'passed'
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        if inputs:
            after.update({path: sha(path) if Path(path).is_file() else None for path in inputs})
            if inputs != after:
                report['status'] = 'failed'
                report['inputDrift'] = [p for p in inputs if inputs[p] != after[p]]
        # 成功也保留命令、真实退出码和指纹；不删除失败现场或测试证明。
        (work/'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
        print('diff gate report:', work/'results.json', flush=True)
        if report['status'] != 'passed' and 'error' not in report:
            raise RuntimeError('Validation inputs changed during the gate')


if __name__ == '__main__':
    main()
