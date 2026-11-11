"""Compile exact actor units plus private pool/finalizer accessors and test leases.

The accepted native build receipt supplies the matched frontend/XRT cohort.
Executables run beside the real runtime; no runtime library is overwritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-result', type=Path, required=True)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    build = json.loads(args.build_result.read_text(encoding='utf-8-sig'))
    assert build['status'] == 'passed'
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    record = dict(status='failed', build=str(args.build_result.resolve()), buildSha256=sha(args.build_result),
                  commands=[], adapters={}, objects=build['objectOutputs'])
    target = args.runtime.resolve() / '.lsp-analysis-lease-native-test.exe'
    assert not target.exists()
    commands = build['commands']
    link = next(c.copy() for c in commands if '-o' in c and Path(c[c.index('-o')+1]).name == 'xlang-ls.exe')
    accessors = {
        'worker': '\nxtaskpool* xideTestWorkerPool(xls_worker* s){return s->pool;}\n',
        'diagnostic_store': '\nxtaskpool* xideTestDiagnosticPool(xide_diagnostic_store* s){return s->pool;}\n',
        'project_references': '\nxtaskpool* xideTestWorkerPool(xls_worker*);\nxtaskpool* xideTestReferencePool(xide_project_references* s,size_t i){return !i?s->pool:i==1?xideTestWorkerPool(s->keys):i-2<s->memberCount?xideTestWorkerPool(s->members[i-2]):NULL;}\n',
        'workspace_symbols': '\nxtaskpool* xideTestWorkerPool(xls_worker*);\nxtaskpool* xideTestWorkspacePool(xide_workspace_symbols* s,size_t i){return !i?s->pool:xideTestWorkerPool(s->actor);}\n',
        'runtime': '\nvoid xideTestPostFinalizer(xtaskpool* p,void (*proc)(void*),void* data){static xrt_task_finalizer node;memset(&node,0,sizeof(node));__xrtTaskPoolFinalizeBody(p,&node,proc,data);}\n',
    }

    def run(c, name):
        record['commands'].append(c)
        result = subprocess.run(c, capture_output=True, timeout=240)
        (out / (name + '.log')).write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, (name, result.returncode, result.stderr.decode('utf-8', 'replace')[-2500:])
        return result

    try:
        for f, h in build['objectOutputs'].items(): assert sha(f) == h, f
        replaced = {}
        for name, suffix in accessors.items():
            c = next(c.copy() for c in commands if '-c' in c and Path(c[c.index('-c')+1]).name == name + '.c')
            original = Path(c[c.index('-c')+1]); raw = original.read_bytes()
            adapter = out / (name + '.c'); adapter.write_bytes(raw + suffix.encode())
            record['adapters'][str(adapter)] = dict(product=str(original), productSha256=sha(original), prefixBytes=len(raw), sha256=sha(adapter))
            obj = out / (name + '.o'); replaced[c[c.index('-o')+1]] = str(obj)
            c[c.index('-c')+1] = str(adapter); c[c.index('-o')+1] = str(obj)
            c.insert(1, '-I' + str(original.parent))
            run(c, name)
            record['adapters'][str(adapter)].update(object=str(obj), objectSha256=sha(obj))
        c = next(c.copy() for c in commands if '-c' in c and Path(c[c.index('-c')+1]).name == 'xlang_ls.c')
        fixture = Path(__file__).resolve().parents[1] / 'tests/analysis_lease_test.c'
        frozen = out / fixture.name; shutil.copy2(fixture, frozen)
        record['fixture'] = dict(source=str(fixture), sha256=sha(fixture))
        old_server = c[c.index('-o')+1]; obj = out / 'test.o'
        c[c.index('-c')+1] = str(frozen); c[c.index('-o')+1] = str(obj); run(c, 'fixture')
        # worker/runtime live in the core archive. Explicit exact adapters
        # satisfy their symbols before archive extraction.
        command = [replaced.get(x, str(obj) if x == old_server else x) for x in link]
        for name in ('worker', 'runtime'): command.insert(1, str(out / (name + '.o')))
        program = out / 'test.exe'; command[command.index('-o')+1] = str(program); run(command, 'link')
        record['linkedInputs'] = {x: sha(x) for x in command if Path(x).suffix.lower() in ('.o', '.a') and Path(x).is_file()}
        shutil.copy2(program, target); data = out / 'data'; data.mkdir()
        result = run([str(target), str(data)], 'test')
        assert sha(target) == sha(program) and sha(fixture) == record['fixture']['sha256']
        for f, item in record['adapters'].items():
            assert sha(f) == item['sha256'] and sha(item['product']) == item['productSha256']
            assert sha(item['object']) == item['objectSha256']
        for f, h in record['linkedInputs'].items(): assert sha(f) == h, f
        assert sha(args.build_result) == record['buildSha256']
        record.update(status='passed', programSha256=sha(program), stdout=result.stdout.decode('utf-8', 'replace'), stderr=result.stderr.decode('utf-8', 'replace'))
        print(record['stderr'])
    finally:
        if target.exists():
            assert 'program' in locals() and sha(target) == sha(program)
            target.unlink()
        (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__': main()
