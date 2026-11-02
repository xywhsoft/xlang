"""Reject actual native collector ownership/provenance omissions.

Use the complete matching build and its original per-function lifetime test.
Only one private include changes per mutant; public/native layouts, all
other inputs and the compiler options stay fixed. This establishes correctness
boundaries, not ordinary edit locality, performance or release admission.
"""
from pathlib import Path
import argparse, ctypes, json, os, subprocess
from build_lsp_lifecycle_profile import verify_build, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    normal = json.loads((build / 'build-result.json').read_text())
    verify_build(normal, build)
    assert normal['status'] == 'passed' and normal['configuration'] == 'Release' and normal['testsBuilt']
    root = Path(__file__).resolve().parents[2]
    source = root / 'ide/src/analysis/frontend_function_index.inc'
    original = source.read_text(encoding='utf8')
    reference = build / 'frontend/xl6_sem_index.c'
    compile_command = next(c for c in normal['commands'] if '-c' in c and str(reference) in c)
    link_command = next(c for c in normal['commands'] if '-o' in c and Path(c[c.index('-o') + 1]).name == 'analysis_function_index_asset_test.exe')
    protected = {**normal['inputs'], str(build / 'build-result.json'): sha(build / 'build-result.json'), str(Path(__file__).resolve()): sha(__file__)}
    for name, digest in normal['outputs'].items():
        protected[str(Path(name) if Path(name).is_absolute() else build / name)] = digest
    report = dict(status='running', accepted=False, scope=__doc__, protected=protected, commands=[], cases=[])

    def save():
        (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8', newline='\n')

    def run(name, command, assertion=None):
        rejected = assertion is not None
        result = subprocess.run(command, cwd=out, capture_output=True, timeout=90)
        stdout, stderr = out / (name + '.stdout.log'), out / (name + '.stderr.log')
        stdout.write_bytes(result.stdout)
        stderr.write_bytes(result.stderr)
        row = dict(name=name, command=command, exit=result.returncode, stdout=str(stdout), stdoutSha256=sha(stdout), stderr=str(stderr), stderrSha256=sha(stderr), expected='nonzero' if rejected else 'zero')
        report['commands'].append(row)
        save()
        assert (result.returncode != 0) == rejected, (name, result.returncode, result.stderr[-2000:])
        if rejected:
            assert result.returncode == 1 and b'analysis_semantic_memo_test:' in result.stderr and assertion.encode() in result.stderr, (name, 'expected a deterministic lifetime/value assertion', result.stderr[-2000:])

    variants = [
        ('borrow-report-path', 'memcpy(text,original->path,length);target->path=text;text+=length;',
         'memcpy(text,original->path,length);target->path=original->path;text+=length;',
         'Source catalog must own path bytes before the old report retires.', 'own.path!=original.path'),
        ('omit-range-admission', '            if(f->canonical)canonical+=strlen(f->canonical)+1;',
         '            f->validRanges=0;\n            if(f->canonical)canonical+=strlen(f->canonical)+1;',
         'Detached facts must preserve the original report-validated ranges.', 'factEqual(a,b)'),
        ('retain-parent-directory', '    xrtFree(storage->rows);storage->rows=NULL;',
         '    /* Fault: the complete index directory outlives its producer. */',
         'A retained function must not pin the complete index directory.', 'remainingBuffers==1'),
        ('omit-function-owner', '        asset->function=function;asset->canonicalBytes=canonical;',
         '        xlsTypedFunctionRelease(function);asset->function=NULL;asset->canonicalBytes=canonical;',
         'A detached segment must retain the original native function owner.', 'asset'),
    ]
    previous_mode = ctypes.windll.kernel32.SetErrorMode(0x8003) if os.name == 'nt' else None
    try:
        run('normal-control', [str(build / 'analysis_function_index_asset_test.exe')])
        report['cases'].append(dict(name='normal-control', status='passed'))
        for name, seam, replacement, contract, assertion in variants:
            assert original.count(seam) == 1, name
            folder = out / name
            changed = folder / 'analysis/frontend_function_index.inc'
            changed.parent.mkdir(parents=True)
            changed.write_text(original.replace(seam, replacement), encoding='utf8', newline='\n')
            command = list(compile_command)
            command.insert(1, '-I' + str(folder))
            obj = folder / 'xl6_sem_index.o'
            for flag in ['-MF', '-MT', '-o']:
                if flag in command:
                    command[command.index(flag) + 1] = str(folder / ('xl6_sem_index.d' if flag == '-MF' else 'xl6_sem_index.o'))
            run(name + '-compile', command)
            command = list(link_command)
            command[1:2] = [str(build / 'analysis_function_index_asset_test.o'), str(obj)]
            exe = folder / 'fault.exe'
            command[command.index('-o') + 1] = str(exe)
            run(name + '-link', command)
            run(name + '-run', [str(exe)], assertion)
            report['cases'].append(dict(name=name, status='rejected', contract=contract, source=str(changed), sha256=sha(changed)))
        verify_build(normal, build)
        for path, digest in protected.items():
            assert sha(path) == digest, path
        report['status'] = 'passed'
    except BaseException as error:
        report.update(status='failed', error=repr(error))
        raise
    finally:
        if previous_mode is not None:
            ctypes.windll.kernel32.SetErrorMode(previous_mode)
        save()
    print(json.dumps(dict(status=report['status'], cases=len(report['cases']), commands=len(report['commands']), report=str(out / 'result.json'))))


if __name__ == '__main__':
    main()
