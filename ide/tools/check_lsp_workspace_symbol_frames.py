"""Real stdio workspace symbol frames, exact request IDs and complete scopes."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    entry, closed, other, loose = [root/name for name in ('main.xl', 'closed.xl', 'other.xl', 'loose.xl')]
    entry.write_text('func main(...) as int\n return 0\nend\n', encoding='utf8')
    closed.write_text('export func closedOnly() as int\n return 1\nend\n', encoding='utf8')
    for path, name in [(other, 'otherOnly'), (loose, 'looseOnly')]:
        path.write_text(f'export func {name}() as int\n return 1\nend\nfunc main(...) as int\n return 0\nend\n', encoding='utf8')
    def project(name, main, members):
        lists = {key: [] for key in LISTS}; lists['members'] = [p.as_posix() for p in members]
        return dict(schema=1, kind='project', project=(root/name).as_posix(), configuration='Debug',
                    entry=main.as_posix(), directory=root.as_posix(), analysisProfile=PROFILE,
                    targetKind='exe', packagePath=None, lists=lists)
    a, b = project('A.xlp', entry, [entry, closed]), project('B.xlp', other, [other])
    configuration = dict(epoch=1, catalog=dict(schema=2, projects=[a, b], selectedProject=a['project'],
                        packagePath=None, standaloneRoots=[loose.as_posix()], complete=True))
    clients, cases = [], []
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in [server, Path(__file__), entry, closed, other, loose]}
    report = dict(status='failed', scope='Actual production stdio framed workspace symbols;no automatic folder bootstrap or formal latency/RSS claim.', cases=cases, protected=protected)
    def done(name, **extra): cases.append(dict(name=name, passed=True, **extra))
    def client(config=None):
        c = AutomaticClient(server, root/'absent-xl.exe', root); clients.append(c)
        options = dict(automaticDiagnostics=False, checkOnSave=False, workspaceSymbolBackgroundIndex=False)
        if config is not None: options['analysisConfiguration'] = config
        c.send('initialize', dict(capabilities={}, initializationOptions=options), 1)
        reply, _ = c.reply(1); assert 'error' not in reply and isinstance(reply['result']['serverInfo']['version'], str) and reply['result']['serverInfo']['version'], reply
        c.send('initialized', {}); return c
    def request(c, request_id, params, method='workspace/symbol'):
        c.send(method, params, request_id); reply, _ = c.reply(request_id)
        assert type(reply['id']) is type(request_id) and reply['id'] == request_id and 'error' not in reply, reply
        return reply['result']
    try:
        c = client(configuration)
        for label, request_id in [('empty-string', ''), ('zero', 0), ('int64-min', -(2**63)),
                                  ('int64-max', 2**63-1), ('unicode-escaped', '中文😀-"\\id'), ('max-string', 'x'*65535)]:
            rows = request(c, request_id, dict(query='Only'))
            assert {r['name'] for r in rows} == {'closedOnly', 'otherOnly', 'looseOnly'}, rows
            work = c.call('xlang/workspaceSymbolStats')
            assert work['encodedFrameBytes'] > 0 and work['scopeValidations'] == 2 and work['coordinatorThread'] != work['analysisThread'], work
            done('exact-id-' + label + '-full-selected-unselected-and-closed-file-scopes', frameBytes=work['encodedFrameBytes'])
        row = next(r for r in rows if r['name'] == 'closedOnly')
        navigation = request(c, 'navigate😀', dict(query='Only', symbol=row), 'xlang/workspaceSymbolNavigation')
        document = navigation['document']
        assert navigation['symbol'] == row and document['origin'] == 'disk' and Path(document['path']).resolve() == closed, navigation
        assert document['normalizedSha256'] == hashlib.sha256(closed.read_text(encoding='utf8').encode()).hexdigest(), document
        assert document['input']['rawSha256'] == sha(closed), document
        done('navigation-frame-keeps-original-row-and-current-source-proof')
        rows = request(c, 'no-matches', dict(query='definitelyMissingSymbol'))
        assert rows == [], rows
        done('empty-result-is-a-successful-framed-response')
        separate = client()
        separate.open(loose, loose.read_text(encoding='utf8'), 1)
        rows = request(separate, 'standalone', dict(query='Only'))
        assert [r['name'] for r in rows] == ['looseOnly'], rows
        work = separate.call('xlang/workspaceSymbolStats')
        assert work['encodedFrameBytes'] > 0 and work['scopeValidations'] == 0, work
        done('standalone-open-file-frame-without-a-workspace')
        assert all(sha(Path(path)) == digest for path, digest in protected.items())
        report['status'] = 'passed'
    finally:
        for index, c in enumerate(clients):
            stderr = c.stop()
            (root/f'client-{index}.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
            (root/f'client-{index}.stderr').write_text(stderr, encoding='utf8')
        (root/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(root/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
