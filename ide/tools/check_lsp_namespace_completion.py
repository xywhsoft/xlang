"""Real native stdio namespace completion, exported facts and source guards.

The optional prior server demonstrates the original missing-candidate failure.
No compiler executable, project context routing or latency claim is involved.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from check_language_definitions import position
from check_language_snapshots import Client


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    baseline = args.baseline.resolve(strict=True) if args.baseline else None
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    root = output / '命名空间😀'
    package = root / 'query_package'
    package.mkdir(parents=True)
    entry, api = root / 'main.xl', package / 'api.xl'
    manifest = package / 'xlang.package.json'
    manifest.write_text(json.dumps(dict(format='xlang-package-1', name='query_package',
        version='1.0.0', abiVersion='1', kind='source',
        artifact=dict(kind='source', xlSources=['api.xl']), dependencies=[], links=[], loadlibs=[])), encoding='utf8')
    exported = ('export class Box\n int value\nend\n'
                'export func pick(int value) as int\n return value\nend\n'
                'export func pick(string value) as int\n return 2\nend\n'
                'export func make() as Box\n return Box()\nend\n'
                'func hidden() as int\n return 9\nend\n')
    api.write_bytes(exported.encode())
    entry.write_bytes(b'func main(...) as int\n return 0\nend\n')
    compiler = output / 'absent-toolchain/xl.exe'
    protected = {str(p): sha(p) for p in [server, Path(__file__), entry, manifest] + ([baseline] if baseline else [])}
    original_api = sha(api)
    clients, cases = [], []
    report = dict(status='failed', scope='Standard completion native namespace candidates; no project routing, scale or UI latency claim',
                  cases=cases, protected=protected, originalApiSha256=original_api)
    rid = 100

    def client(executable=server):
        c = Client(executable, compiler, root)
        clients.append(c)
        c.initialize()
        return c

    def query(c, text, marker, version=1):
        nonlocal rid
        rid += 1
        at = text.rindex(marker) + len(marker)
        c.send('textDocument/completion', dict(textDocument=dict(uri=entry.as_uri()),
               version=version, position=position(text, at)), rid)
        reply, _ = c.reply(rid)
        assert 'error' not in reply, reply
        value = reply['result']
        assert value['version'] == version and value['queryId'] == rid, value
        return value, at

    def candidates(name, text, marker, labels, prefix='', suffix='', check=None):
        c = client()
        c.open(entry, text)
        value, at = query(c, text, marker)
        assert value['available'] and value['guardScope'] == 'background-poll', value
        items = value['items']
        assert {item['label'] for item in items} == set(labels), value
        expected = dict(start=position(text, at-len(prefix)), end=position(text, at+len(suffix)))
        for item in items:
            assert item['textEdit'] == dict(newText=item['label'], range=expected), item
            decl = item['data']['declaration']
            assert decl['uri'] == api.as_uri() and item['detail'], item
            assert not item['label'].startswith('__xl6_') and item['label'] != 'hidden', item
        if check:
            check(value)
        cases.append(dict(name=name, passed=True, result=value))
        c.stop()
        return value

    def overloaded(value):
        items = [i for i in value['items'] if i['label'] == 'pick']
        assert len(items) == 2 and len({i['detail'] for i in items}) == 2, value
        assert len({json.dumps(i['data']['declaration'], sort_keys=True) for i in items}) == 2, value
        for needle in ['int value', 'string value']:
            at = exported.index('pick(' + needle)
            expected = dict(start=position(exported, at), end=position(exported, at+4))
            assert any(i['data']['declaration']['range'] == expected for i in items), value

    try:
        plain = 'import query_package\nfunc main(...) as int\n query_package.\n return 0\nend\n'
        if baseline:
            c = client(baseline)
            c.open(entry, plain)
            value, _ = query(c, plain, 'query_package.')
            assert not value['available'] and not value['items'], value
            cases.append(dict(name='prior-server-retains-original-unavailable-namespace', passed=True, result=value))
            c.stop()
        candidates('public-exports-and-distinct-semantic-overloads', plain, 'query_package.', ['Box', 'pick', 'make'], check=overloaded)
        alias = plain.replace('import query_package\n', 'import query_package as qp\n').replace(' query_package.', ' qp.')
        candidates('import-alias-public-exports', alias, 'qp.', ['Box', 'pick', 'make'], check=overloaded)
        partial = alias.replace(' qp.\n', ' qp.pi\n')
        candidates('partial-public-prefix', partial, 'qp.pi', ['pick'], prefix='pi', check=overloaded)
        middle = alias.replace(' qp.\n', ' qp.piSuffix\n')
        candidates('whole-token-replacement-from-middle', middle, 'qp.pi', ['pick'], prefix='pi', suffix='Suffix', check=overloaded)
        unicode = '\ufeff' + partial.replace(' qp.pi\n', ' string 文本 = "😀"; qp.pi\n').replace('\n', '\r\n')
        candidates('bom-crlf-non-bmp-utf16-replacement', unicode, 'qp.pi', ['pick'], prefix='pi', check=overloaded)
        shadow = ('import query_package\nclass Local\n int localField\nend\n'
                  'func main(...) as int\n auto query_package = Local()\n query_package.\n return 0\nend\n')
        c = client()
        c.open(entry, shadow)
        value, _ = query(c, shadow, 'query_package.')
        assert value['available'] and {i['label'] for i in value['items']} == {'localField'}, value
        cases.append(dict(name='local-object-shadow-keeps-original-member-rules', passed=True, result=value))
        c.stop()
        c = client()
        c.open(entry, partial)
        value, _ = query(c, partial, 'qp.pi')
        assert len(value['items']) == 2, value
        old_id = value['queryId']
        api.write_bytes(exported.replace('pick', 'pickChanged').encode())
        while True:
            message = c.next()
            if message.get('method') == 'xlang/completionInvalidated':
                assert message['params']['queryId'] == old_id, message
                break
        value, _ = query(c, partial, 'qp.pi')
        assert value['available'] and len(value['items']) == 2 and {i['label'] for i in value['items']} == {'pickChanged'}, value
        cases.append(dict(name='actual-package-source-edit-invalidates-and-rebuilds-public-candidates', passed=True, result=value))
        c.stop()
        api.write_bytes(exported.encode())
        assert all(sha(p) == h for p, h in protected.items()) and sha(api) == original_api
        assert not compiler.exists()
        cases.append(dict(name='sources-restored-executables-and-unsaved-entry-unchanged-no-compiler', passed=True))
        report['status'] = 'passed'
    finally:
        for c in clients:
            c.stop()
        report['exitCodes'] = [c.process.returncode for c in clients]
        report['transcripts'] = [c.transcript for c in clients]
        report['stderr'] = [c.stderr for c in clients]
        if any(report['exitCodes']):
            report['status'] = 'failed'
        (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    assert report['status'] == 'passed'
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(output/'result.json'))))


if __name__ == '__main__':
    main()
