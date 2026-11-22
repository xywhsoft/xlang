"""Exercise native exit-result allocation and cancellation fallback.

Rebuild the actual IR translation unit with one private include override.
The installed service and all runtime libraries remain untouched. Normal
production contains no fault branches or substitute allocator.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from build_lsp_core import dependencies


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    args = parser.parse_args()
    out, runtime = args.output.resolve(), args.runtime.resolve()
    out.mkdir(parents=True, exist_ok=False)
    base = json.loads(args.build.read_text(encoding='utf-8-sig'))
    assert base['status'] == 'passed'
    ir = next(c for c in base['commands'] if '-c' in c and Path(c[c.index('-o')+1]).name == 'xl6_ir.o')
    if '-DXLS_EXPERIMENTAL_IR_EXIT_REUSE=1' not in ir:
        parser.error('exit reuse is disabled in the release profile; supply a fresh opt-in build made with --cflag=-DXLS_EXPERIMENTAL_IR_EXIT_REUSE=1')
    receipt = dict(status='failed', commands=[], inputs={}, outputs={},
        scope='Private clone/capture/key allocator failures and cancellation inside both key paths')
    env = dict(os.environ, TMP=str(out), TEMP=str(out))
    alias = runtime / '.ir-exit-fault-test.exe'
    owned = None

    def run(label, command):
        receipt['commands'].append(command)
        process = subprocess.run(command, cwd=out, env=env, capture_output=True, timeout=300)
        (out / (label + '.log')).write_bytes(process.stdout + process.stderr)
        assert process.returncode == 0, (label, (process.stdout + process.stderr).decode(errors='replace')[-2400:])
        return process

    def compile_unit(label, command, source, extra):
        flags = [command[0], *extra, *command[1:command.index('-c')]]
        deps = run(label + '-deps', flags + ['-MM', '-MT', 'object', str(source)])
        receipt['inputs'].update({str(p): sha(p) for p in dependencies(deps.stdout.decode())})
        run(label + '-compile', flags + ['-c', str(source), '-o', str(out / (label + '.o'))])

    try:
        for path, digest in base['inputs'].items():
            assert sha(path) == digest, path
        override_test = base.get('testOverrides', {}).get('analysis_ir_exit_test', {})
        test = override_test.get('compile') or next(c for c in base['commands'] if '-c' in c and Path(c[c.index('-o')+1]).name == 'analysis_ir_exit_test.o')
        original = next(Path(p) for p in base['inputs'] if Path(p).name == 'frontend_ir_exit.inc')
        source = original.read_text(encoding='utf8')
        assert source.count('xrtMalloc(') == 3 and source.count('xrtCalloc(') == 1
        source = source.replace('xrtMalloc(', 'xlsExitFaultMalloc(').replace('xrtCalloc(', 'xlsExitFaultCalloc(')
        source = source.replace('exitInputKey(p,m)', 'xlsExitFaultInputKey(p,m)')
        seam = '#include "analysis/frontend_ir_exit_key.inc"'
        assert source.count(seam) == 1
        source = source.replace(seam, seam + '\nstatic int xlsExitFaultInputKey(xls_exit_pending* p,const XL6_IrModule* m)'
            '{xlsExitTestInKey=1;int okay=exitInputKey(p,m);xlsExitTestInKey=0;return okay;}')
        source = ('extern _Thread_local int xlsExitTestInKey;\nextern void* xlsExitFaultMalloc(size_t);\n'
                  'extern void* xlsExitFaultCalloc(size_t,size_t);\n') + source
        override = out / 'overlay/analysis/frontend_ir_exit.inc'
        override.parent.mkdir(parents=True)
        override.write_text(source, encoding='utf8', newline='\n')
        original_test = Path(test[test.index('-c')+1])
        driver = original_test.read_text(encoding='utf8')
        entry = 'int main(void){if(!xlsFrontendIrExitAvailable())'
        assert driver.count(entry) == 1
        driver = driver.replace(entry, 'static int qaMain(void){if(!xlsFrontendIrExitAvailable())', 1)
        fault = Path(__file__).resolve().parents[1] / 'tests/analysis_ir_exit_fault.inc'
        driver += '\n#include "' + fault.as_posix() + '"\n'
        driver_path = out / 'fault-driver.c'
        driver_path.write_text(driver, encoding='utf8', newline='\n')
        compile_unit('xl6_ir', ir, Path(ir[ir.index('-c')+1]), ['-I'+str(out/'overlay')])
        compile_unit('test', test, driver_path, ['-I'+str(original_test.parent)])
        core = args.build.resolve().parent / 'xlang-analysis-core.a'
        assert sha(core) == base['outputs'][core.name]
        target_core = out / core.name
        shutil.copy2(core, target_core)
        run('archive', [str(Path(ir[0]).with_name('ar.exe')), 'r', str(target_core), str(out/'xl6_ir.o')])
        link = override_test.get('link') or next(c for c in base['commands'] if '-c' not in c and '-o' in c and
                    Path(c[c.index('-o')+1]).name == 'analysis_ir_exit_test.exe')
        original_object = Path(test[test.index('-o')+1]).resolve()
        link = [str(out/'test.o') if v.endswith('.o') and Path(v).resolve() == original_object else
                str(target_core) if v.endswith('xlang-analysis-core.a') else v for v in link]
        link[link.index('-o')+1] = str(out/'test.exe')
        run('link', link + ['-Wl,--wrap=xrtStrBufReserve'])
        assert not alias.exists()
        shutil.copy2(out/'test.exe', alias)
        owned = sha(alias)
        process = run('run', [str(alias)])
        receipt['stdout'] = process.stdout.decode(errors='replace')
        for path, digest in receipt['inputs'].items():
            assert sha(path) == digest, path
        for path, digest in base['inputs'].items():
            assert sha(path) == digest, path
        receipt['status'] = 'passed'
        print(json.dumps(dict(status='passed', result=str(out/'result.json'))))
    finally:
        if owned is not None:
            assert sha(alias) == owned
            alias.unlink()
        receipt['outputs'] = {p.name: sha(p) for p in out.iterdir() if p.suffix in ('.exe', '.o', '.a')}
        receipt['inputs'][str(Path(__file__).resolve())] = sha(__file__)
        (out/'result.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf8', newline='\n')


if __name__ == '__main__':
    main()
