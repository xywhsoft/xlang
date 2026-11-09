"""Real stdio workspace symbols,closed projects,overlays and publication fences."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS, PROFILE
from check_language_definitions import position


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    entry, helper, orphan = (root/name for name in ('main.xl', 'helper.xl', '未打开😀.xl'))
    body = '\ufeffimport helper\r\nfunc main(...) as int\r\n return helper.value()\r\nend\r\n'
    library = '/* 中文😀 */ class Payload\r\n int number\r\nend\r\nexport func value() as int\r\n return 1\r\nend\r\n'
    unopened = ('class Box<T>\n T item\nend\nfunc 计算值() as int\n return 7\nend\n'
                'func invisible() as int\n return 1\nend\nfunc main(...) as int\n return invisible()+计算值()\nend\n')
    for path, text in ((entry, body), (helper, library), (orphan, unopened)): path.write_bytes(text.encode())
    lists = {name: [] for name in LISTS}; lists['members'] = [entry.as_posix(), helper.as_posix(), orphan.as_posix(), helper.as_posix()]
    project = dict(schema=1, kind='project', project=(root/'A.xlp').as_posix(), configuration='Debug',
                   entry=entry.as_posix(), directory=root.as_posix(), analysisProfile=PROFILE, targetKind='exe', packagePath=None, lists=lists)
    packet = lambda epoch=1, projects=None: dict(epoch=epoch, catalog=dict(schema=1, projects=copy.deepcopy([project] if projects is None else projects), selectedProject='', packagePath=None))
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in (server, Path(__file__))}
    clients, cases = [], []
    report = dict(status='failed', scope='Actual standard SymbolInformation stdio through production background service. No IDE UI,folder discovery,error recovery or overall latency claim.', protected=protected, cases=cases)

    def done(name, **extra): cases.append(dict(name=name, passed=True, **extra))

    def client(configuration=None, rich=False, cli=False, automatic=False, kinds=None):
        c = AutomaticClient(server, root/'absent/xl.exe', root); clients.append(c)
        options = dict(automaticDiagnostics=automatic, checkOnSave=False)
        if configuration is not None: options['analysisConfiguration'] = configuration
        if cli: options['diagnosticsBackend'] = 'cli'
        caps = dict(textDocument=dict(documentSymbol=dict(hierarchicalDocumentSymbolSupport=True)))
        if rich or kinds is not None: caps['workspace'] = dict(symbol=dict(symbolKind=dict(valueSet=list(range(1, 27)) if kinds is None else kinds), resolveSupport=dict(properties=['location.range'])))
        c.send('initialize', dict(capabilities=caps, initializationOptions=options), 1)
        reply, _ = c.reply(1); assert 'error' not in reply, reply
        capability = reply['result']['capabilities'].get('workspaceSymbolProvider')
        assert capability is (None if cli else True), reply
        c.send('initialized', {}); return c

    def symbols(c, query='', limit=None):
        params = dict(query=query)
        if limit is not None: params['xlangLimit'] = limit
        return c.call('workspace/symbol', params)

    def error(c, params, expected=-32602):
        c.request += 1; c.send('workspace/symbol', params, c.request); reply, _ = c.reply(c.request)
        assert reply.get('error', {}).get('code') == expected, reply

    def stats(c): return c.call('xlang/workspaceSymbolStats')

    def fence(c, trailing, expected, version):
        c.request += 1; rid = c.request
        c.batch([dict(jsonrpc='2.0', method='workspace/symbol', params=dict(query=''), id=rid), *trailing])
        reply, _ = c.reply(rid); assert reply.get('error', {}).get('code') == expected, reply
        rows = symbols(c, 'number Payload'); assert len(rows) == 1, rows
        return rid

    try:
        c = client(); assert symbols(c) == []; initial = stats(c)
        for params in ({}, dict(query=7), dict(query='\0'), dict(query='a'*513), dict(query='a'*129), dict(query=' '.join('x'+str(i) for i in range(17))), dict(query='', xlangLimit=0), dict(query='', xlangLimit=4097), dict(query='', xlangLimit=True)):
            error(c, params)
        assert stats(c)['index.generation'] == initial['index.generation']
        done('empty-workspace-and-invalid-query-admission-before-analysis'); c.stop()

        c = client(packet())
        rows = symbols(c, limit=4096); assert len(rows) == 10, rows
        assert [r['name'] for r in rows].count('main') == 2 and not any(r['kind'] > 18 for r in rows)
        cold = stats(c); assert cold['roots'] == 3 and cold['captured'] == 2 and cold['covered'] == 1, cold
        for row in rows: assert set(row) <= {'name', 'kind', 'location', 'containerName'} and set(row['location']) == {'uri', 'range'}
        field = next(r for r in rows if r['name'] == 'number')
        assert field['containerName'] == 'Payload' and field['location'] == dict(uri=helper.as_uri(), range=dict(start=position(library, library.index('number')), end=position(library, library.index('number')+6))), field
        assert symbols(c, 'Payload number') == symbols(c, 'number Payload') == [field]
        assert [r['name'] for r in symbols(c, '计值')] == ['计算值']
        assert symbols(c, limit=1) == rows[:1] and symbols(c, limit=7) == rows[:7]
        warm = stats(c); assert not warm['captured'] and warm['reused'] == 2, warm
        done('zero-open-complete-project-import-unimported-symbols-utf16-ranking-and-reuse', cold=cold, warm=warm)

        dirty = library.replace('number', 'amount')
        c.open(helper, dirty, 1); edited = symbols(c)
        assert any(r['name'] == 'amount' for r in edited) and not any(r['name'] == 'number' for r in edited)
        c.send('textDocument/didClose', dict(textDocument=dict(uri=helper.as_uri())))
        assert symbols(c, 'Payload number') == [field]
        helper.write_bytes(dirty.encode()); assert [r['name'] for r in symbols(c, 'Payload amount')] == ['amount']; helper.write_bytes(library.encode())
        assert symbols(c, 'Payload number') == [field]
        done('dependency-open-close-equal-length-overlay-and-disk-changes-without-main-open')
        orphan.write_text('func main(...) as int\n return "invalid"\nend\n', encoding='utf-8')
        error(c, dict(query=''), -32803)
        orphan.write_bytes(unopened.encode()); assert len(symbols(c, limit=4096)) == 10
        done('uncovered-source-error-is-not-stale-success-and-repair-recovers')

        second = copy.deepcopy(project); second['project'] = (root/'B.xlp').as_posix(); second['configuration'] = 'Release'
        c.call('xlang/configureAnalysis', packet(2, [project, second])); assert len(symbols(c, 'Payload number')) == 2
        c.call('xlang/configureAnalysis', packet(3)); assert symbols(c, 'Payload number') == [field]
        done('all-configured-projects-independent-scopes-and-removal')

        c.open(entry, body, 1); symbols(c)
        change = lambda version: dict(jsonrpc='2.0', method='textDocument/didChange', params=dict(textDocument=dict(uri=entry.as_uri(), version=version), contentChanges=[dict(text=body+'\n')]))
        fence(c, [change(2)], -32801, 2)
        watched = dict(jsonrpc='2.0', method='workspace/didChangeWatchedFiles', params=dict(changes=[dict(uri=helper.as_uri(), type=2)]))
        fence(c, [watched], -32801, 2)
        configured = dict(jsonrpc='2.0', method='workspace/didChangeConfiguration', params=dict(settings=dict(xlang=dict(analysisConfiguration=packet(4)))))
        fence(c, [configured], -32801, 2)
        c.request += 1; rid = c.request
        c.batch([dict(jsonrpc='2.0', method='workspace/symbol', params=dict(query=''), id=rid), dict(jsonrpc='2.0', method='$/cancelRequest', params=dict(id=rid)), change(3)])
        reply, _ = c.reply(rid); assert reply.get('error', {}).get('code') == -32800, reply
        assert symbols(c, 'Payload number') == [field]
        done('queued-edit-watcher-and-explicit-cancellation-fences')
        c.request += 50; ids = list(range(c.request-39, c.request+1))
        c.batch([*[dict(jsonrpc='2.0', method='workspace/symbol', params=dict(query=''), id=i) for i in ids], change(4)])
        replies = {}
        while len(replies) < len(ids):
            value = c.next()
            if value.get('id') in ids: assert value['id'] not in replies; replies[value['id']] = value
        codes = [v.get('error', {}).get('code') for v in replies.values()]
        assert -32603 in codes and -32801 in codes and all(code in (-32603, -32801) for code in codes), replies
        assert symbols(c, 'Payload number') == [field]
        done('bounded-request-queue-no-late-success-and-recovery'); c.stop()

        c = client(packet(), rich=True); assert next(r for r in symbols(c) if r['name'] == 'T')['kind'] == 26
        assert all('range' in r['location'] for r in symbols(c))
        done('negotiated-symbol-kinds-and-full-locations-with-resolve-capable-client'); c.stop()
        c = client(packet(), kinds=[26]*27); assert all(r['kind'] == 26 for r in symbols(c))
        done('repeated-client-kind-values-do-not-silently-fall-back-to-legacy-set'); c.stop()

        c = client(); c.open(entry, body, 1)
        names = {r['name'] for r in symbols(c)}; assert {'main', 'Payload', 'number', 'value'} <= names and 'invisible' not in names, names
        c.send('textDocument/didClose', dict(textDocument=dict(uri=entry.as_uri()))); assert symbols(c) == []
        done('unconfigured-standalone-document-imports-and-close'); c.stop()

        c = client(); c.open(entry, body, -2147483648)
        assert len(symbols(c, 'main')) == 1
        for version in (-2147483647, 2147483647):
            c.send('textDocument/didChange', dict(textDocument=dict(uri=entry.as_uri(), version=version), contentChanges=[dict(text=body+'\n')]))
            assert len(symbols(c, 'main')) == 1
        done('full-int32-document-version-range-including-native-revision-zero'); c.stop()

        c = client(cli=True); error(c, dict(query=''), -32601); done('compatibility-backend-does-not-advertise-unsupported-capability'); c.stop()
        for path, digest in protected.items(): assert sha(path) == digest, path
        report['status'] = 'passed'
    finally:
        for i, c in enumerate(clients):
            stderr = c.stop(); (root/f'client-{i}.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf-8'); (root/f'client-{i}.stderr.txt').write_text(stderr, encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(status=report['status'], cases=len(cases))), flush=True)


if __name__ == '__main__': main()
