"""Native protocol checks for explicit closed workspace file roots."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
from check_language_snapshots import Client
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server, out = args.server.resolve(strict=True), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = out/'workspace 文件😀'
    root.mkdir()
    bodies = dict(main='import shared\nfunc main(...) as int\n return shared.compute(1)\nend\n',
                  shared='export func compute(int value) as int\n return value\nend\n',
                  closed='import shared\nexport func other() as int\n return shared.compute(2)\nend\n')
    paths = {key: root/(key+'.xl') for key in bodies}
    for key, path in paths.items():
        path.write_bytes(bodies[key].encode())
    config = dict(schema=2, selectedProject='', packagePath=None, projects=[], complete=True,
                  standaloneRoots=[p.as_posix() for p in paths.values()])
    protected = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in [server, Path(__file__), *paths.values()]}
    clients, cases = [], []
    report = dict(status='failed', cases=cases, performanceQualified=False, fullGoalComplete=False)
    next_id = 100

    def done(name):
        cases.append(dict(name=name, passed=True))

    def call(c, method, params, error=None):
        nonlocal next_id
        next_id += 1
        c.send(method, params, next_id)
        reply, _ = c.reply(next_id)
        if error is not None:
            assert reply.get('error', {}).get('code') == error, reply
        else:
            assert 'result' in reply and 'error' not in reply, reply
        return reply.get('result')

    def start(catalog=config, opened=True, invalid=False):
        c = Client(server, root/'absent-xl.exe', root)
        clients.append(c)
        c.send('initialize', dict(capabilities=dict(textDocument=dict(rename=dict(prepareSupport=True)),
                            workspace=dict(workspaceEdit=dict(documentChanges=True))),
               initializationOptions=dict(checkOnSave=False, automaticDiagnostics=False,
                    backgroundWorkspaceSymbols=False, analysisConfiguration=dict(epoch=1, catalog=catalog))), 1)
        reply, _ = c.reply(1)
        if invalid:
            assert reply.get('error', {}).get('code') == -32602, reply
            return c
        assert 'result' in reply, reply
        assert reply['result']['capabilities']['experimental']['analysisCatalog'] == 2
        c.send('initialized', {})
        if opened:
            c.open(paths['main'], bodies['main'], 1)
        return c

    prefix = bodies['main'][:bodies['main'].index('compute')+1]
    intent = dict(textDocument=dict(uri=paths['main'].as_uri()),
                  position=dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1])))

    def rename(c):
        value = call(c, 'textDocument/rename', dict(**intent, newName='renamed'))
        rows = value['documentChanges']
        assert len(rows) == 3, rows
        assert {row['textDocument']['uri'] for row in rows} == {p.as_uri() for p in paths.values()}, rows
        for row in rows:
            assert row['textDocument']['version'] == (1 if row['textDocument']['uri'] == paths['main'].as_uri() else None), row
            assert all(edit['newText'] == 'renamed' for edit in row['edits']), row
        return value

    try:
        c = start()
        prepared = call(c, 'textDocument/prepareRename', intent)
        assert prepared['placeholder'] == 'compute', prepared
        done('prepare-binds-unopened-explicit-standalone-declaration')
        rename(c)
        done('rename-includes-unopened-standalone-caller-and-declaration')
        c.stop()
        c = start(opened=False)
        symbols = call(c, 'workspace/symbol', dict(query='compute'))
        assert any(s['name'] == 'compute' and s['location']['uri'] == paths['shared'].as_uri() for s in symbols), symbols
        done('workspace-symbol-discovers-roots-with-zero-open-documents')
        c.stop()
        duplicate = copy.deepcopy(config)
        duplicate['standaloneRoots'] += [str(paths['main']).upper(), paths['closed'].as_posix()]
        c = start(duplicate)
        rename(c)
        done('native-alias-and-open-root-union-does-not-duplicate-edits')
        c.stop()
        # Project ownership is authoritative; no artificial second standalone role.
        project = dict(schema=1, kind='project', project=(root/'test.xlp').as_posix(),
                       configuration='debug', entry=paths['main'].as_posix(), directory=root.as_posix(),
                       analysisProfile=PROFILE, targetKind='exe', packagePath=None,
                       lists={key: [] for key in LISTS})
        project['lists']['members'] = [p.as_posix() for p in paths.values()]
        owned = copy.deepcopy(config)
        owned.update(projects=[project], selectedProject=project['project'])
        c = start(owned)
        rename(c)
        done('project-and-loose-file-overlap-keeps-one-authoritative-owner')
        c.stop()
        incomplete = dict(config, complete=False)
        c = start(incomplete)
        for method, params in [('textDocument/prepareRename', intent),
                               ('textDocument/rename', dict(**intent, newName='renamed')),
                               ('workspace/symbol', dict(query='compute'))]:
            call(c, method, params, -32803)
        # A failed workspace does not prevent checking the active file itself.
        value = call(c, 'xlang/check', dict(textDocument=intent['textDocument'], version=1))
        assert value['count'] == 0, value
        done('incomplete-workspace-refuses-global-operations-but-keeps-file-analysis')
        call(c, 'xlang/configureAnalysis', dict(epoch=2, catalog=config))
        rename(c)
        done('configuration-refresh-restores-complete-domain')
        # Remove a closed caller from the declared domain: it must disappear
        # from symbols, never survive in a prior cached workspace result.
        reduced = dict(config, standaloneRoots=config['standaloneRoots'][:2])
        call(c, 'xlang/configureAnalysis', dict(epoch=3, catalog=reduced))
        symbols = call(c, 'workspace/symbol', dict(query='other'))
        assert not symbols, symbols
        done('removed-closed-root-does-not-survive-configuration-cache')
        c.stop()
        for name, catalog in [('relative-root', dict(config, standaloneRoots=['relative.xl'])),
                              ('c-root', dict(config, standaloneRoots=[str(root/'bad.c')])),
                              ('nonboolean-completeness', dict(config, complete=1)),
                              ('unknown-schema2-key', dict(config, unknown=1)),
                              ('65-unique-scopes', dict(config, standaloneRoots=[str(root/f'f{i}.xl') for i in range(65)]))]:
            c = start(catalog, invalid=True)
            c.stop()
            done('reject-'+name)
        many = dict(config, standaloneRoots=[paths['main'].as_posix()]*65 + config['standaloneRoots'][1:])
        c = start(many)
        rename(c)
        done('input-roster-quota-is-separate-from-deduplicated-scope-quota')
        c.stop()
        report['status'] = 'passed'
    finally:
        report['stderr'] = [c.stop() for c in clients]
        report['transcripts'] = [c.transcript for c in clients]
        report['protectedUnchanged'] = all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    assert report['protectedUnchanged'] and not any(report['stderr']), report['stderr']
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(out/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
