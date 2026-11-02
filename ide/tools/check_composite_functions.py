"""Reject actual composite native ownership and metadata-accounting omissions.

Use the matching complete build, native AST and original test oracle. Private
variants change one ownership/ledger step. No semantic rules or fault fixtures
enter the server. This is correctness evidence, not a performance/release gate.
"""
from pathlib import Path
import argparse, json, subprocess
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
    root = Path(__file__).resolve().parents[2]
    test = root / 'ide/tests/analysis_composite_function_test.c'
    protected = {**normal['inputs'], str(build / 'build-result.json'): sha(build / 'build-result.json'), str(Path(__file__).resolve()): sha(__file__)}
    for name, digest in normal['outputs'].items():
        protected[str(Path(name) if Path(name).is_absolute() else build / name)] = digest
    report = dict(status='running', accepted=False, scope=__doc__, protected=protected, commands=[], cases=[])

    def save():
        (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8', newline='\n')

    def run(name, command, assertion=None):
        result = subprocess.run(command, cwd=out, capture_output=True, timeout=180)
        stdout, stderr = out / (name + '.stdout.log'), out / (name + '.stderr.log')
        stdout.write_bytes(result.stdout)
        stderr.write_bytes(result.stderr)
        report['commands'].append(dict(name=name, command=command, exit=result.returncode,
                                      stdout=str(stdout), stdoutSha256=sha(stdout), stderr=str(stderr),
                                      stderrSha256=sha(stderr), expected='assertion' if assertion else 'zero'))
        save()
        if assertion:
            assert result.returncode == 1 and assertion.encode() in result.stderr, (name, result.returncode, result.stderr[-2000:])
        else:
            assert not result.returncode, (name, result.returncode, result.stderr[-2000:])

    def compile(source, reference, name):
        command = list(next(c for c in normal['commands'] if '-c' in c and str(reference) in c))
        command[command.index(str(reference))] = str(source)
        command.insert(1, '-I' + str(test.parent))
        for flag in ['-MF', '-MT', '-o']:
            if flag in command:
                command[command.index(flag) + 1] = str(out / (name + ('.d' if flag == '-MF' else '.o')))
        run(name + '-compile', command)
        return out / (name + '.o')

    def link(name, objects):
        command = list(next(c for c in normal['commands'] if '-o' in c and Path(c[c.index('-o') + 1]).name == test.stem + '.exe'))
        command[1:2] = [str(p) for p in objects]
        target = out / (name + '.exe')
        command[command.index('-o') + 1] = str(target)
        run(name + '-link', command)
        return target

    try:
        run('normal-control', [str(build / (test.stem + '.exe'))])
        report['cases'].append(dict(name='normal-control', status='passed'))
        # Run the unchanged mixed-forest oracle directly for retain omissions:
        # it observes destruction BEFORE dereferencing any borrowed function.
        body = test.read_text(encoding='utf8')
        seam = 'int main(void)'
        assert body.count(seam) == 1
        # Observe the real retirement boundary before native recursive freeing.
        # This turns a missing detach into a deterministic assertion instead of
        # dereferencing an already freed shared function in a crash-only test.
        base = test.parent / 'analysis_typed_function_test.c'
        base_body = base.read_text(encoding='utf8')
        assert base_body.count('static unsigned checks;') == 1
        base_body = base_body.replace('static unsigned checks;', 'static unsigned checks;\nstatic const XL6_AstNode* guardRoot;')
        seam = '    int observed=0;'
        assert base_body.count(seam) == 1
        base_body = base_body.replace(seam, '''    if(node && node==guardRoot){
        for(size_t i=0;i<node->childCount;++i)if(node->children[i]){
            fprintf(stderr,"borrowed-detach-guard\\n");exit(1);
        }
        guardRoot=NULL;
    }
''' + seam)
        private_base = out / base.name
        private_base.write_text(base_body, encoding='utf8', newline='\n')
        protected[str(private_base)] = sha(private_base)
        seam = '    xlsTypedModuleRelease(module);xlsFunctionOriginRelease(origin);'
        assert body.count(seam) == 1
        body = body.replace(seam, '    guardRoot=root;\n' + seam)
        probe = out / 'sets_probe.c'
        probe.write_text(body.replace('int main(void)', 'int originalCompositeMain(void)') + '''
int main(void)
{
    for(unsigned i=0;i<3;++i){xrtAtomicPtrInit(&watched[i],NULL);xrtAtomic32Init(&drops[i],0);}
    xrtAtomic32Init(&cloneCalls,0);
    return compositeSets() ? 0 : 1;
}
''', encoding='utf8', newline='\n')
        protected[str(probe)] = sha(probe)
        probe_obj = compile(probe, test, 'sets-probe')
        probe_exe = link('sets-control', [probe_obj])
        run('sets-control-run', [str(probe_exe)])
        report['cases'].append(dict(name='sets-control', status='passed'))
        variants = [
            ('omit-borrowed-retain', 'typed_module.c',
             'module->borrowed[i]=xlsTypedFunctionRetain(owners[i]);root->children[i]=NULL;',
             'module->borrowed[i]=owners[i];root->children[i]=NULL;', probe_obj,
             '!xrtAtomic32Load(&drops[0],XMEMORY_RELAXED)'),
            ('omit-borrowed-detach', 'typed_module.c',
             'for(size_t i=0;i<module->borrowedSlots;++i)if(module->borrowed[i])module->root->children[i]=NULL;',
             '/* Fault: borrowed children are still presented to native destruction. */',
             probe_obj, 'borrowed-detach-guard'),
            ('omit-borrowed-lookup', 'typed_module.c',
             'if(module && slot<module->borrowedSlots && module->borrowed[slot])',
             'if(0 && module && slot<module->borrowedSlots && module->borrowed[slot])',
             build / (test.stem + '.o'), 'borrowed==f'),
            ('merge-distinct-native-owners', 'function_unit.c',
             'memcpy(key,&group->identity,sizeof(group->identity));',
             'memset(key,0,sizeof(group->identity));', probe_obj,
             'xlsFunctionSetInfo(set).ownedBytes==expected'),
            ('omit-index-catalog-credit', 'function_unit.c',
             'if(xrtMapHas(charged,groupKey(&groups[i],key)))',
             'if(groups[i].kind==0 && xrtMapHas(charged,groupKey(&groups[i],key)))',
             build / (test.stem + '.o'), 'set && xlsFunctionSetInfo(set).ownedBytes==expected'),
        ]
        for name, filename, seam, replacement, oracle, assertion in variants:
            source = root / 'ide/src/analysis' / filename
            body = source.read_text(encoding='utf8')
            assert body.count(seam) == 1, name
            changed = out / (name + '.c')
            changed.write_text(body.replace(seam, replacement), encoding='utf8', newline='\n')
            protected[str(changed)] = sha(changed)
            obj = compile(changed, source, name)
            exe = link(name, [oracle, obj])
            run(name + '-run', [str(exe)], assertion)
            report['cases'].append(dict(name=name, status='rejected', source=str(changed), sha256=sha(changed), assertion=assertion))
        verify_build(normal, build)
        for path, digest in protected.items():
            assert sha(path) == digest, path
        report['status'] = 'passed'
    except BaseException as error:
        report.update(status='failed', error=repr(error))
        raise
    finally:
        save()
    print(json.dumps(dict(status=report['status'], cases=len(report['cases']), commands=len(report['commands']), report=str(out / 'result.json'))))


if __name__ == '__main__':
    main()
