"""Check current compiler foreign-output and native-arithmetic semantics through real LSP."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

CORPUS = (
    ('foreign_owned_signed_output', False), ('foreign_span_prefix', False),
    ('foreign_span_region_prefix_nested', True), ('foreign_cstring_result', False),
    ('foreign_cstring_output', False), ('foreign_cstring_fields', False),
    ('foreign_borrowed_output', False), ('foreign_product_nullable_lift', False),
    ('foreign_owned_admission_calls', False), ('foreign_owned_error_priority', False),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, default=Path(__file__).resolve().parents[2]/'test/cases')
    parser.add_argument('--baseline-legacy-projection', type=Path)
    args = parser.parse_args(); server = args.server.resolve(strict=True)
    root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    clients, cases = [], []; protected = {str(server): sha(server), str(Path(__file__)): sha(Path(__file__))}
    report = dict(status='failed', scope='Native full checks,current typed IR,guarded editing and navigation;no C execution,DAP,formal performance or release promotion.', cases=cases, protected=protected)
    def done(name, **extra): cases.append(dict(name=name, passed=True, **extra))
    def start(binary):
        client = AutomaticClient(binary, root/'missing-xl.exe', root); clients.append(client)
        client.send('initialize', dict(capabilities={}, initializationOptions=dict(automaticDiagnostics=False, checkOnSave=False)), 1)
        reply, _ = client.reply(1); caps = reply['result']['capabilities']['experimental']
        assert caps['nativeSnapshotDiagnostics'] and caps['diagnosticsBackend'] == 'native', reply
        client.send('initialized', {}); return client
    def check(client, path, version, accepted):
        value = client.call('xlang/check', dict(textDocument=dict(uri=path.as_uri()), version=version))
        assert value['version'] == version and value['ok'] == accepted and value['semanticDefinitions'] == accepted, value
        assert (value['count'] == 0) == accepted and value['guardScope'] == 'background-poll', value
        if accepted:
            assert value['phase'] == 'checked' and value['analysis']['lastBuildPersistentIr']['verified'] >= 1, value
        return value
    try:
        corpus = []
        for name, legacy in CORPUS:
            original = args.corpus/(name+'.xl'); path = root/(name+'.xl'); raw = original.read_bytes()
            protected[str(original)] = sha(original); path.write_bytes(raw); corpus.append((name, path, legacy))
            client = start(server); client.open(path, path.read_text(encoding='utf8'), 1)
            value = check(client, path, 1, True); done('current-' + name, sourceSha256=sha(path), phase=value['phase'])
            client.stop()
        math = 'func add(c_uint lhs,c_uint rhs) as c_uint\n return lhs+rhs\nend\nfunc main(...) as int\n return int(add(c_uint(2),c_uint(3)))\nend\n'
        path = root/'native-integer.xl'; path.write_text(math, encoding='utf8')
        client = start(server); client.open(path, math, 1); check(client, path, 1, True); done('current-native-integer-arithmetic-signature'); client.stop()
        declaration = 'extern "make_data" @header("fixture.h") @no_raise @c_output_bytes(0,1,"void*","size_t","xrt",0) func makeData() as bytes\n'
        body = declaration+'func main(...) as int\n auto value=makeData()\n return 0\nend\n'
        path = root/'editing.xl'; path.write_text(body, encoding='utf8'); client = start(server); client.open(path, body, 1)
        check(client, path, 1, True)
        query = dict(textDocument=dict(uri=path.as_uri()), position=position(body, body.rindex('makeData')))
        definitions = client.call('textDocument/definition', query)
        if isinstance(definitions, dict): definitions = [definitions]
        assert len(definitions) == 1 and definitions[0]['uri'] == path.as_uri() and definitions[0]['range']['start']['line'] == 0, definitions
        done('owned-output-declaration-native-definition')
        references = client.call('textDocument/references', dict(query, context=dict(includeDeclaration=True)))
        assert len(references) == 2 and {row['range']['start']['line'] for row in references} == {0, 2}, references
        done('owned-output-exact-native-reference-frame')
        for version, (label, invalid) in enumerate([
            ('allocator', body.replace('"xrt"', '"malloc"')),
            ('slot-order', body.replace('c_output_bytes(0,1,', 'c_output_bytes(1,0,')),
            ('size-ABI', body.replace('"size_t"', '"float"')),
            ('pointer-ABI', body.replace('"void*"', '"int*"')),
        ], 2):
            client.change(path, invalid, version); value = check(client, path, version, False)
            assert value['diagnostics'], value; done('invalid-owned-output-' + label + '-rejects-after-edit')
        client.change(path, body, 6); check(client, path, 6, True)
        assert len(client.call('textDocument/references', dict(query, context=dict(includeDeclaration=False)))) == 1
        done('failed-contract-edit-recovers-with-current-native-bindings')
        incomplete = declaration+'func main(...) as int\n auto value=makeD\n return 0\nend\n'
        client.change(path, incomplete, 7)
        completion = client.call('textDocument/completion', dict(textDocument=dict(uri=path.as_uri()), version=7, position=position(incomplete, incomplete.rindex('makeD')+5)))
        assert completion['available'] and any(row['label'] == 'makeData' for row in completion['items']), completion
        done('editing-completion-with-new-owned-output-declaration')
        client.stop()
        if args.baseline_legacy_projection:
            baseline = args.baseline_legacy_projection.resolve(strict=True); protected[str(baseline)] = sha(baseline)
            for name, path, legacy in corpus:
                client = start(baseline); client.open(path, path.read_text(encoding='utf8'), 1)
                check(client, path, 1, legacy); done('legacy-projection-control-' + name, accepted=legacy); client.stop()
        assert all(sha(Path(path)) == digest for path, digest in protected.items()); report['status'] = 'passed'
    finally:
        for index, client in enumerate(clients):
            stderr = client.stop(); (root/f'client-{index}.json').write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf8'); (root/f'client-{index}.stderr').write_text(stderr, encoding='utf8')
        (root/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(root/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
