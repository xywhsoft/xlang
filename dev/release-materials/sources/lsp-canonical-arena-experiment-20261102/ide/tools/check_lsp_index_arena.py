"""Reject actual C omissions in a passed native index-arena build.

Recompile the real owner or private collector fixture with one changed include.
The normal archive, compiler inputs and existing assertion bodies stay intact.
This is ownership/fixture validation, not performance or GUI admission.
"""
from pathlib import Path
import argparse, json, os, subprocess
from build_lsp_lifecycle_profile import sha, verify_build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, output = args.build.resolve(strict=True), args.output.resolve()
    report = build / 'build-result.json'
    normal = json.loads(report.read_text('utf8'))
    verify_build(normal, build)
    output.mkdir(parents=True, exist_ok=False)
    temporary = output / 'compiler-temp'
    temporary.mkdir()
    ide = Path(__file__).resolve().parents[1]
    record = dict(status='running', accepted=False, scope=__doc__,
                  normalBuild=str(build), commands=[], cases=[],
                  inputs={str(p): sha(p) for p in [Path(__file__).resolve(),
                          Path(__file__).with_name('build_lsp_lifecycle_profile.py'), report]})
    env = dict(os.environ, TMP=str(temporary), TEMP=str(temporary))
    compiler = Path(normal['compiler'])
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')

    def save():
        (output / 'result.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf8')

    def run(command, folder, name, expected=0):
        record['commands'].append(command)
        save()
        result = subprocess.run(command, cwd=folder, env=env, capture_output=True, timeout=180)
        (folder / (name + '.log')).write_bytes(result.stdout + result.stderr)
        if result.returncode != expected:
            raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: ' +
                               (result.stdout + result.stderr).decode('utf8', 'replace')[-2000:])
        return result

    def compile_command(stem):
        return next(c for c in normal['commands'] if '-c' in c and
                    Path(c[c.index('-c') + 1]).stem == stem)

    variants = [
        ('omit-buffer-free', 'xl6_sem_index', 'frontend_index.inc',
         'xrtStrBufFree(&index->canonical);', '(void)index->canonical;'),
        ('omit-pointer-seal', 'xl6_sem_index', 'frontend_index.inc',
         'b->facts[i].canonical=index->canonical.Data+b->facts[i].canonicalOffset;',
         'b->facts[i].canonical=NULL;'),
        ('omit-string-terminator', 'xl6_type_model', 'frontend_type_arena.inc',
         'if(okay){arena->Data[arena->Size++]=0;arena->Data[arena->Size]=0;}',
         'if(okay){(void)arena;}'),
        ('omit-scope-restore', 'analysis_index_storage_test', 'frontend_index.inc',
         'xlsTypedCanonicalArena=previousArena;', '(void)previousArena;'),
    ]
    try:
        for label, stem, include, old, new in variants:
            folder = output / label
            shadow = folder / 'include' / 'analysis' / include
            shadow.parent.mkdir(parents=True)
            source = ide / 'src' / 'analysis' / include
            body = source.read_text('utf8')
            if body.count(old) != 1:
                raise ValueError('omission seam changed: ' + old)
            shadow.write_text(body.replace(old, new), encoding='utf8', newline='\n')
            record['inputs'][str(source)] = sha(source)
            command = compile_command(stem)
            obj = folder / (stem + '.o')
            run([command[0], '-I' + str(folder / 'include'),
                 *command[1:command.index('-c')], '-c', command[command.index('-c') + 1],
                 '-o', str(obj)], folder, 'compile')
            normal_core = build / 'xlang-analysis-core.a'
            core = normal_core
            fixture = 'analysis_index_storage_test' if stem.endswith('_test') else 'analysis_index_arena_test'
            if not stem.endswith('_test'):
                core = folder / 'core.a'
                core.write_bytes(normal_core.read_bytes())
                run([str(compiler.with_name('ar.exe' if os.name == 'nt' else 'ar')),
                     'rcs', str(core), str(obj)], folder, 'archive')
            suffix = '.exe' if os.name == 'nt' else ''
            normal_exe = build / (fixture + suffix)
            link = next(c for c in normal['commands'] if '-o' in c and str(normal_exe) in c)
            exe = folder / ('variant' + suffix)
            substitutions = {str(normal_exe): str(exe), str(normal_core): str(core)}
            if stem.endswith('_test'):
                substitutions[str(build / (stem + '.o'))] = str(obj)
            run([substitutions.get(x, x) for x in link], folder, 'link')
            result = run([str(exe)], folder, 'run', expected=1)
            record['cases'].append(dict(name=label, actualCompiledOmissionRejected=True,
                                        exit=result.returncode,
                                        stdout=result.stdout.decode('utf8', 'replace'),
                                        stderr=result.stderr.decode('utf8', 'replace')))
            save()
            print(label + ': rejected', flush=True)
        verify_build(normal, build)
        for path, digest in record['inputs'].items():
            if sha(Path(path)) != digest:
                raise ValueError('omission input changed: ' + path)
        record['status'] = 'passed'
    except BaseException as error:
        record.update(status='failed', error=repr(error))
        raise
    finally:
        record['outputs'] = {str(p): sha(p) for p in output.rglob('*')
                             if p.is_file() and p.name != 'result.json'}
        save()


if __name__ == '__main__':
    main()
