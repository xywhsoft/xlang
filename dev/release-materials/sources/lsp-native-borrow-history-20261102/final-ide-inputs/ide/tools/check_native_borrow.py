"""Exercise ordinary native reuse guards and actual isolated allocation refusals.

Matching full cohort only. Original language rules and full AST/index/IR oracles
stay fixed. Mutants are never linked into the server or root release. Evidence
of correctness/fallback is separate from performance, GUI and release admission.
"""
from pathlib import Path
import argparse, json, subprocess
from build_lsp_lifecycle_profile import verify_build, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, out = args.build.resolve(strict=True), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = Path(__file__).resolve().parents[2]
    normal = json.loads((build / 'build-result.json').read_text())
    verify_build(normal, build)
    test = 'analysis_native_borrow_test'
    protected = {**normal['inputs'], str(build / 'build-result.json'): sha(build / 'build-result.json'), str(Path(__file__).resolve()): sha(__file__)}
    for name, digest in normal['outputs'].items():
        protected[str(Path(name) if Path(name).is_absolute() else build / name)] = digest
    record = dict(status='running', accepted=False, scope=__doc__, protected=protected, commands=[], cases=[])

    def save():
        (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf8', newline='\n')

    def run(name, command, assertion=None, refusals=False):
        result = subprocess.run(command, cwd=out, capture_output=True, timeout=180)
        stdout, stderr = out / (name + '.stdout.log'), out / (name + '.stderr.log')
        stdout.write_bytes(result.stdout)
        stderr.write_bytes(result.stderr)
        record['commands'].append(dict(name=name, command=command, exit=result.returncode, stdout=str(stdout), stdoutSha256=sha(stdout),
                                       stderr=str(stderr), stderrSha256=sha(stderr), expected='assertion' if assertion else 'zero'))
        save()
        if assertion:
            choices = [assertion] if isinstance(assertion, str) else assertion
            assert result.returncode == 1 and any(value.encode() in result.stderr for value in choices), (name, result.returncode, result.stderr[-2000:])
        else:
            assert not result.returncode, (name, result.returncode, result.stderr[-2000:])
        if refusals:
            assert b'observed native allocation refusals:' in result.stdout, (name, 'actual producer refusal missing')

    def compile(source, reference, name, include=None):
        command = list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c))
        command[command.index(str(reference))] = str(source)
        if include:
            command.insert(1, '-I' + str(include))
        for flag in ['-MF', '-MT', '-o']:
            if flag in command:
                command[command.index(flag) + 1] = str(out / (name + ('.d' if flag == '-MF' else '.o')))
        run(name + '-compile', command)
        return out / (name + '.o')

    def link(name, objects, extra=()):
        command = list(next(c for c in normal['commands'] if '-o' in c and Path(c[c.index('-o') + 1]).name == test + '.exe'))
        command[1:2] = [str(p) for p in objects]
        exe = out / (name + '.exe')
        command[command.index('-o') + 1] = str(exe)
        command += list(extra)
        run(name + '-link', command)
        return exe

    def changed(name, filename, body):
        folder = out / name
        path = folder / ('analysis/' + filename if filename.endswith('.inc') else filename)
        path.parent.mkdir(parents=True)
        path.write_text(body, encoding='utf8', newline='\n')
        protected[str(path)] = sha(path)
        reference = build / 'frontend/xl6_sem.c' if filename.endswith('.inc') else root / 'ide/src/analysis' / filename
        source = reference if filename.endswith('.inc') else path
        obj = compile(source, reference, name, folder if filename.endswith('.inc') else None)
        return obj, path

    try:
        run('normal-control', [str(build / (test + '.exe'))])
        record['cases'].append(dict(name='normal-control', status='passed'))
        for name, filename, seam, replacement, assertion in [
            ('omit-exact-region-span', 'frontend_sem_final.inc',
             'entry->finalPredictedStart!=old->finalRegionStart || entry->finalPredictedEnd!=old->finalRegionEnd ||',
             '0 ||', ['!unexpectedPlacement', 'sameAst']),
            ('omit-c-owned-write-certificate', 'frontend_sem_native.inc',
             'node->kind==XL6_AST_NATIVE_EXTERN_SPECIAL || xl6COwnedBaseName(node->text)',
             'node->kind==XL6_AST_NATIVE_EXTERN_SPECIAL || 0', 'work.readonlyDeclined>=1'),
            ('omit-pool-backing-charge', 'ast_size.c',
             'if(size->backing && node->xlsStorage)', 'if(0 && size->backing && node->xlsStorage)', 'known>=pool'),
            ('omit-private-eager-retry', 'check.c',
             'if(retry && !attempt){retried=1;continue;}', 'if(0 && retry && !attempt){retried=1;continue;}',
             'result && xlsSemanticCheckInfo(result).accepted'),
        ]:
            source = root / 'ide/src/analysis' / filename
            body = source.read_text(encoding='utf8')
            assert body.count(seam) == 1, name
            obj, path = changed(name, filename, body.replace(seam, replacement))
            exe = link(name, [build / (test + '.o'), obj])
            run(name + '-run', [str(exe)], assertion)
            record['cases'].append(dict(name=name, status='rejected', source=str(path), sha256=sha(path), assertion=assertion))
        for name, filename, function in [
            ('refuse-composite-module', 'typed_module.c', 'xrtCalloc'),
            ('refuse-composite-vector', 'typed_module.c', 'xrtCalloc'),
            ('refuse-native-owner-ledger', 'frontend_sem_native.inc', 'xrtMapSet'),
            ('refuse-pool-backing-ledger', 'ast_size.c', 'xrtMapSet'),
        ]:
            source = root / 'ide/src/analysis' / filename
            body = source.read_text(encoding='utf8')
            if function == 'xrtCalloc':
                start = body.index('xls_typed_module* xlsTypedModuleTakeBorrowed(')
                prefix, tail = body[:start], body[start:]
                if name == 'refuse-composite-module':
                    seam = '    xls_typed_module* module=xrtCalloc(1,sizeof(*module));if(!module)return NULL;'
                    replacement = '    xlsNativeTestAllocation=1;\n    xls_typed_module* module=xrtCalloc(1,sizeof(*module));\n    xlsNativeTestAllocation=0;if(!module)return NULL;'
                else:
                    seam = '    module->borrowed=xrtCalloc(count,sizeof(*module->borrowed));'
                    replacement = '    xlsNativeTestAllocation=1;\n' + seam + '\n    xlsNativeTestAllocation=0;'
                assert tail.count(seam) == 1
                body = '_Thread_local int xlsNativeTestAllocation;\n' + prefix + tail.replace(seam, replacement)
                prototype = 'void* __real_xrtCalloc(size_t,size_t);'
                wrapper = 'void* __wrap_xrtCalloc(size_t count,size_t bytes){if(xlsNativeTestAllocation){++denied;return NULL;}return __real_xrtCalloc(count,bytes);}'
            else:
                seam = '!xrtMapSet(&memo->nativeOwners,key,&yes)' if filename.endswith('.inc') else '!xrtMapSet(&size->pools,key,&yes)'
                replacement = '!xlsNativeTestSet(&memo->nativeOwners,key,&yes)' if filename.endswith('.inc') else '!xlsNativeTestSet(&size->pools,key,&yes)'
                assert body.count(seam) == 1
                helper = '''
_Thread_local int xlsNativeTestAllocation;
static bool xlsNativeTestSet(xmap* map,xbytesview key,const void* value)
{xlsNativeTestAllocation=1;bool okay=xrtMapSet(map,key,value);xlsNativeTestAllocation=0;return okay;}
'''
                body = body.replace(seam, replacement)
                if filename.endswith('.inc'):
                    body = helper + body
                else:
                    seam = '#include "analysis/ast_size.h"'
                    assert body.count(seam) == 1
                    body = body.replace(seam, seam + helper)
                prototype = 'bool __real_xrtMapSet(xmap*,xbytesview,const void*);'
                wrapper = 'bool __wrap_xrtMapSet(xmap* map,xbytesview key,const void* value){if(xlsNativeTestAllocation){++denied;return false;}return __real_xrtMapSet(map,key,value);}'
            obj, path = changed(name, filename, body)
            spy = out / name / 'allocation.c'
            spy.write_text('#include <xrt.h>\n#include <stdio.h>\n#include <stdlib.h>\nextern _Thread_local int xlsNativeTestAllocation;\nstatic unsigned denied;\n' + prototype + '\n' + wrapper + '\nstatic void report(void){if(!denied){fputs("producer allocation not observed\\n",stderr);_Exit(2);}printf("observed native allocation refusals:%u\\n",denied);}\n__attribute__((constructor)) static void setup(void){atexit(report);}\n', encoding='utf8', newline='\n')
            protected[str(spy)] = sha(spy)
            probe = compile(spy, root / 'ide/tests' / (test + '.c'), name + '-allocation')
            exe = link(name, [build / (test + '.o'), obj, probe], ['-Wl,--wrap=' + function])
            run(name + '-run', [str(exe), '--expect-eager'], refusals=True)
            record['cases'].append(dict(name=name, status='fallback-passed', source=str(path), sha256=sha(path), allocation=str(spy), allocationSha256=sha(spy)))
        verify_build(normal, build)
        for path, digest in protected.items():
            assert sha(path) == digest, path
        record['status'] = 'passed'
    except BaseException as error:
        record.update(status='failed', error=repr(error))
        raise
    finally:
        save()
    print(json.dumps(dict(status=record['status'], cases=len(record['cases']), commands=len(record['commands']), report=str(out / 'result.json'))))


if __name__ == '__main__':
    main()
