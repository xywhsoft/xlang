"""Check direct-unit capture guards with real compiled omissions."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    args = parser.parse_args()
    build = json.loads(args.build.read_text(encoding='utf8'))
    if build['status'] != 'passed':
        raise ValueError('Need a successful matching native build')
    for path, digest in build['inputs'].items():
        if sha(path) != digest:
            raise ValueError('Changed build input: ' + path)
    runtime = args.runtime.resolve()
    if runtime != (Path(__file__).resolve().parents[2] / 'release/win_x64').resolve():
        raise ValueError('Use repository release/win_x64')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    link = next(c for c in build['commands'] if '-c' not in c and '-o' in c
                and Path(c[c.index('-o') + 1]).name == 'analysis_ir_unit_capture_test.exe')
    core = Path(next(p for p in link if p.endswith('xlang-analysis-core.a')))
    if sha(core) != build['outputs'][core.name]:
        raise ValueError('Changed matching core')
    record = dict(status='failed', buildSha256=sha(args.build),
                  toolSha256=sha(__file__), commands=[], runs=[], outputs={})
    cases = []

    def mutation(label, name, old, new):
        command = next(c for c in build['commands'] if '-c' in c
                       and Path(c[c.index('-c') + 1]).stem == name)
        text = Path(command[command.index('-c') + 1]).read_text(encoding='utf8')
        if text.count(old) != 1:
            raise ValueError('Mutation seam changed: ' + label)
        cases.append((label, name, command, text.replace(old, new, 1)))

    mutation('positive', 'ir_node_graph', 'if(seen!=input->count)goto failed;',
             'if(seen!=input->count)goto failed; /* positive */')
    mutation('node-disconnected-cycle', 'ir_node_graph',
             'if(seen!=input->count)goto failed;', 'if(seen>input->count)goto failed;')
    mutation('node-parent', 'ir_node_graph',
             'input->entries[child].node->parent!=e->id', '0')
    mutation('node-root-duplicate', 'ir_node_graph',
             'if(stack[i]==stack[i-1])goto failed;', 'if(stack[i]==stack[i-1] && 0)goto failed;')
    mutation('node-borrow-text', 'ir_node_graph',
             'memcpy(*target,source,bytes);*used+=bytes;',
             'memcpy(*target,source,bytes);*target=(char*)source;*used+=bytes;')
    mutation('node-id', 'ir_node_graph', 'unit->ids[local]=id;', 'unit->ids[local]=id+1;')
    mutation('node-undercharge', 'ir_node_graph',
             'peak=total;if(!add(&peak,unit->bytes)', 'peak=0;if(!add(&peak,unit->bytes)')
    mutation('node-no-cancel-publish', 'ir_node_graph',
             'if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}\n    xrtFree(scratch);',
             'if(cancelled && cancelled(user) && 0){error=XLS_IR_GRAPH_CANCELLED;goto failed;}\n    xrtFree(scratch);')
    mutation('table-alias-selector', 'ir_table_graph',
             'unsigned xlsIrTableUnitKeyAlias(const xls_ir_table_unit* u){return u ? u->keyAlias : 0;}',
             'unsigned xlsIrTableUnitKeyAlias(const xls_ir_table_unit* u){(void)u;return 0;}')
    mutation('table-no-cancel-publish', 'ir_table_graph',
             'if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}\n    if(peakBytes)',
             'if(cancelled && cancelled(user) && 0){error=XLS_IR_GRAPH_CANCELLED;goto failed;}\n    if(peakBytes)')
    mutation('table-undercharge', 'ir_table_graph',
             '*peakBytes=unit->bytes+sizeof(table_namespace);', '*peakBytes=unit->bytes;')
    alias = runtime / '.ir-unit-capture-mutant-test.exe'
    owned = None
    env = dict(os.environ, TEMP=str(output), TMP=str(output))

    def run(label, command):
        record['commands'].append(command)
        result = subprocess.run(command, cwd=output, env=env, capture_output=True, timeout=240)
        (output / (label + '.log')).write_bytes(result.stdout + result.stderr)
        return result

    try:
        for label, name, command, text in cases:
            directory = output / label
            directory.mkdir()
            source = directory / (name + '.c')
            source.write_text(text, encoding='utf8')
            obj = directory / (name + '.o')
            compile_command = [*command[:command.index('-c')], '-c', str(source), '-o', str(obj)]
            if run(label + '-compile', compile_command).returncode:
                raise RuntimeError('Compile failed: ' + label)
            program = directory / 'test.exe'
            link_command = [link[0], str(obj), *link[1:]]
            link_command[link_command.index('-o') + 1] = str(program)
            if run(label + '-link', link_command).returncode:
                raise RuntimeError('Link failed: ' + label)
            digest = sha(program)
            record['outputs'][str(program)] = digest
            if alias.exists():
                raise ValueError('Runtime test alias exists')
            shutil.copy2(program, alias)
            owned = digest
            result = run(label + '-run', [str(alias)])
            record['runs'].append(dict(label=label, exit=result.returncode,
                stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace')))
            if sha(alias) != digest:
                raise ValueError('Owned alias changed')
            alias.unlink()
            owned = None
            if label == 'positive':
                if result.returncode or b'analysis_ir_unit_capture_test passed:' not in result.stdout:
                    raise RuntimeError('Positive control failed')
            elif not result.returncode or b'analysis_ir_memo_test:' not in result.stderr:
                raise RuntimeError('Omission not rejected by fixture: ' + label)
        for path, digest in build['inputs'].items():
            if sha(path) != digest:
                raise ValueError('Changed final input: ' + path)
        if sha(core) != build['outputs'][core.name]:
            raise ValueError('Changed matching core')
        record['status'] = 'passed'
        print('PASS direct-unit capture: 1 positive control, 10 compiled omissions')
    finally:
        if owned:
            if sha(alias) != owned:
                raise ValueError('Owned alias changed')
            alias.unlink()
        (output / 'result.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf8')


if __name__ == '__main__':
    main()
