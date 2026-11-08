"""Real standard declaration queries, source roles and point/full consistency."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_document_symbols import checked, flatten
from check_language_definitions import position


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--server', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args(); server = a.server.resolve(strict=True)
    root = a.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    fixture = Path(__file__).resolve().parents[1]/'tests/declaration_symbols_source.h'
    body = ''.join(json.loads(line.strip()) for line in fixture.read_text(encoding='utf-8').splitlines() if line.startswith('"'))
    source = '\ufeff'+body; path = root/'声明 中文😀.xl'
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in (server, Path(__file__), fixture)}
    clients, cases = [], []
    report = dict(status='failed', cases=cases, protected=protected,
        scope='Actual native stdio declarations, callable scopes, lexical hierarchy and declaration-name definition consistency; no overall performance acceptance')

    def client(hierarchy):
        c = AutomaticClient(server, root/'absent-compiler/xl.exe', root); clients.append(c)
        caps = dict(textDocument=dict(documentSymbol=dict(hierarchicalDocumentSymbolSupport=True))) if hierarchy else {}
        c.send('initialize', dict(capabilities=caps, initializationOptions=dict(automaticDiagnostics=False, checkOnSave=False)), 1)
        reply, _ = c.reply(1); assert 'error' not in reply, reply
        c.send('initialized', {}); c.open(path, source, 1)
        return c

    def symbols(c):
        return c.call('textDocument/documentSymbol', dict(textDocument=dict(uri=path.as_uri())))

    def child_list(tree, parent):
        return next(row['children'] for row in tree if row['name'] == parent)

    def pairs(rows):
        return [(v['name'], v['kind']) for v in rows]

    def definition(c, item, text):
        params = dict(textDocument=dict(uri=path.as_uri()), position=item['selectionRange']['start'])
        expected = dict(uri=path.as_uri(), range=item['selectionRange'])
        assert c.call('textDocument/definition', params) == expected, (item, text)
        return params

    try:
        rich = client(True)
        assert checked(rich, path, 1)['ok']
        tree = symbols(rich)
        assert [v['name'] for v in tree] == ['shared', 'LIMIT', 'linked', 'Reader', 'Choice', 'Payload', 'Box', 'NativeBox', 'callback', 'pair', 'left', 'right', 'main'], tree
        assert pairs(child_list(tree, 'Reader')) == [('T', 26), ('read', 6), ('current', 7), ('current', 7)]
        assert pairs(child_list(tree, 'Choice')) == [('T', 26), ('value', 22), ('empty', 22)]
        assert pairs(child_list(tree, 'Payload')) == [('number', 8), ('decimal', 8)]
        assert pairs(child_list(tree, 'Box')) == [('T', 26), ('item', 8), ('count', 8), ('create', 9), ('destroy', 6), ('current', 7), ('current', 7), ('read', 6)]
        assert pairs(child_list(tree, 'NativeBox')) == [('handle', 8), ('create', 9), ('destroy', 6), ('read', 6)]
        expected = flatten(tree, path.as_uri()); assert len(expected) == 34
        assert not any('hidden_' in v['name'] for v in expected)
        assert tree[0]['selectionRange']['start'] == position(source, source.index('shared'))
        left, right = [next(v for v in tree if v['name'] == name) for name in ('left', 'right')]
        assert left['range'] == right['range'] and left['selectionRange'] != right['selectionRange']
        cases.append(dict(name='complete-source-declaration-roles-and-lexical-hierarchy', passed=True, symbols=34))
        flat = client(False); assert symbols(flat) == expected
        cases.append(dict(name='standard-cold-flat-query-preserves-all-roles-and-containers', passed=True))
        # These declarations have real checked SymbolIds. Fields deliberately
        # have no invented executable IDs and are tested through outline jumps.
        callable_rows = [v for parent in ('Box', 'NativeBox') for v in child_list(tree, parent) if v['name'] in ('create', 'destroy', 'current')]
        for item in callable_rows: definition(rich, item, source)
        # A highlight demand constructs the ordinary full navigation table.
        # Completion of that demand must preserve the same declaration targets.
        at = next(v for v in callable_rows if v['name'] == 'current')
        rich.call('textDocument/documentHighlight', dict(textDocument=dict(uri=path.as_uri()), position=at['selectionRange']['start']))
        for item in callable_rows: definition(rich, item, source)
        assert symbols(rich) == tree
        cases.append(dict(name='property-and-special-member-targets-match-before-and-after-full-navigation', passed=True))
        changed = source.replace('int count=0', 'int total=1').replace('self.count', 'self.total')
        rich.change(path, changed, 2); assert checked(rich, path, 2)['ok']
        current = symbols(rich); assert ('total', 8) in pairs(child_list(current, 'Box')) and ('count', 8) not in pairs(child_list(current, 'Box'))
        flat.change(path, changed, 2); assert symbols(flat) == flatten(current, path.as_uri())
        cases.append(dict(name='edited-declaration-projection-replaces-source-names-and-ranges', passed=True))
        for c in clients: c.stop()
        for p, h in protected.items(): assert sha(p) == h, p
        report['status'] = 'passed'
    finally:
        for i, c in enumerate(clients):
            (root/f'client-{i}-stderr.log').write_text(c.stop(), encoding='utf-8')
            (root/f'client-{i}-transcript.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(status=report['status'], cases=len(cases))))


if __name__ == '__main__':
    main()
