"""Configured compilation owners: real LSP, independent complete source ranges.

Every configured source remains covered by full semantic checks. Imported
libraries keep module roles, including export-main libraries, while the explicit
project entry keeps its program role. This is current-project reference coverage;
catalog discovery for unlisted imports and multi-project ambiguity remain pending.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
from urllib.parse import unquote
from check_language_snapshots import Client
from check_language_definitions import position
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-version', default='0.95')
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = out / '工程归属😀'
    root.mkdir()
    entry, library, first, second, unused = [root / n for n in
        ['middle.xl', 'a_library.xl', 'z1.xl', 'z2.xl', 'unused.xl']]
    imported_main = 'import a_library as api\nfunc main(...) as int\n return api.main(7)\nend\n'
    plain_main = 'func main(...) as int\n return 0\nend\n'
    body = 'export func main(int value) as int\n return value+1\nend\n'
    consumers = {first: 'import a_library as api\nfunc main(...) as int\n return api.main(8)\nend\n',
                 second: 'import a_library as api\nfunc main(...) as int\n return api.main(9)\nend\n'}
    for path, text in {entry: imported_main, library: body, **consumers}.items():
        path.write_bytes(text.encode())
    lists = {k: [] for k in LISTS}
    lists['members'] = [p.as_posix() for p in [entry, library, first, second]]
    scope = dict(schema=1, kind='project', project=(root/'project.xlp').as_posix(),
        configuration='debug', entry=entry.as_posix(), directory=root.as_posix(),
        analysisProfile=PROFILE, targetKind='exe', packagePath=None, lists=lists)
    packet = lambda epoch: dict(epoch=epoch, catalog=dict(schema=1,
        projects=[copy.deepcopy(scope)], selectedProject=scope['project'], packagePath=None))
    cases, clients = [], []
    report = dict(status='failed', scope=__doc__, cases=cases,
        protected={str(server): hashlib.file_digest(server.open('rb'), 'sha256').hexdigest()})
    rid = 10
    current_body = body
    primary_uses_library = True

    def done(name, **data):
        cases.append(dict(name=name, **data))

    def call(client, method, params, code=None):
        nonlocal rid
        rid += 1
        client.send(method, params, rid)
        result, _ = client.reply(rid)
        if code is not None:
            assert result.get('error', {}).get('code') == code, result
            return result
        assert 'error' not in result, result
        return result['result']

    def location(path, text, at):
        return dict(uri=path.as_uri(), range=dict(start=position(text, at), end=position(text, at+4)))

    def wanted(include):
        rows = [location(p, text, text.index('api.main(')+4) for p, text in consumers.items()]
        if primary_uses_library:
            rows.append(location(entry, imported_main, imported_main.index('api.main(')+4))
        if include:
            rows.append(location(library, current_body, current_body.index('main(')))
        return sorted(rows, key=lambda row: (unquote(row['uri']).casefold(),
            row['range']['start']['line'], row['range']['start']['character']))

    def refs(client, include=True):
        result = call(client, 'textDocument/references', dict(
            textDocument=dict(uri=library.as_uri()),
            position=position(current_body, current_body.index('main(')),
            context=dict(includeDeclaration=include)))
        assert result == wanted(include), (result, wanted(include))
        return call(client, 'xlang/referenceStats', {})

    def watched(client, path):
        client.send('workspace/didChangeWatchedFiles', dict(changes=[dict(uri=path.as_uri(), type=2)]))

    try:
        c = Client(server, root/'absent-xl.exe', root)
        clients.append(c)
        c.send('initialize', dict(capabilities={}, initializationOptions=dict(
            automaticDiagnostics=False, checkOnSave=False, analysisConfiguration=packet(1))), 1)
        init, _ = c.reply(1)
        assert init['result']['serverInfo']['version'] == args.expected_version, init
        assert init['result']['capabilities']['experimental']['referenceCompilationOwners'], init
        c.send('initialized', {})
        c.open(library, current_body, 1)
        cold = refs(c)
        assert cold['roots'] == cold['captured'] == 3 and cold['ownerProbes'] == 1 and cold['coveredRoots'] >= 1, cold
        done('listed-export-main-library-uses-real-closed-primary', work=cold)
        warm = refs(c)
        assert warm['captured'] == 1 and warm['reused'] == 2 and not warm['ownerProbes'] and warm['ownerRouteHits'], warm
        assert warm['keyActorBuilds'] == cold['keyActorBuilds'], (cold, warm)
        done('guarded-warm-owner-route-preserves-key-session', work=warm)
        refs(c, False)
        done('declaration-exclusion-preserves-all-real-importers')
        primary_uses_library = False
        entry.write_bytes(plain_main.encode())
        watched(c, entry)
        work = refs(c)
        assert work['roots'] == 3 and work['ownerRouteHits'], work
        done('alternative-importer-owns-library-when-primary-does-not', work=work)
        c.open(entry, plain_main, 1)
        own = call(c, 'textDocument/references', dict(textDocument=dict(uri=entry.as_uri()),
            position=position(plain_main, plain_main.index('main(')), context=dict(includeDeclaration=True)))
        assert own == [location(entry, plain_main, plain_main.index('main('))]
        work = call(c, 'xlang/referenceStats', {})
        assert not work['ownerProbes'] and not work['sourceRejections'] and work['coverageCacheRoots'], work
        done('warm-primary-query-skips-known-false-library-program-role', work=work)
        second.write_bytes(b'func main(...) as int\n return "bad"\nend\n')
        watched(c, second)
        call(c, 'textDocument/references', dict(textDocument=dict(uri=library.as_uri()),
            position=position(current_body, current_body.index('main(')), context=dict(includeDeclaration=True)), -32803)
        done('uncovered-real-type-error-never-publishes-partial-references')
        second.write_bytes(consumers[second].encode())
        watched(c, second)
        refs(c)
        done('real-source-error-recovery')
        current_body = '\n' + body.replace('value+1', 'value+2')
        c.change(library, current_body, 2)
        refs(c)
        done('current-unsaved-library-ranges-and-binding-source')
        before = refs(c)
        c.open(unused, 'broken unused', 1)
        after = refs(c)
        assert after['keyActorBuilds'] == before['keyActorBuilds'] and not after['ownerProbes'], (before, after)
        done('unused-broken-overlay-does-not-rebuild-owner', work=after)
        c.send('textDocument/didClose', dict(textDocument=dict(uri=unused.as_uri())))
        refs(c)
        done('unused-overlay-close')
        c.send('textDocument/didClose', dict(textDocument=dict(uri=library.as_uri())))
        c.open(library, current_body, 1)
        refs(c)
        done('reopened-library-identity-with-restarted-version')
        scope['configuration'] = 'release'
        call(c, 'xlang/configureAnalysis', packet(2))
        refs(c)
        done('configuration-epoch-keeps-complete-source-owned-results')
        assert all(hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest() == h
            for p, h in report['protected'].items())
        report['status'] = 'passed'
    finally:
        for index, client in enumerate(clients):
            if client.process.poll() is None:
                client.stop()
            (out/f'transcript-{index}.json').write_text(
                json.dumps(client.transcript, ensure_ascii=False, indent=2), encoding='utf8')
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print('PASS configured compilation owners', len(cases), 'cases;', out/'result.json')


if __name__ == '__main__':
    main()
