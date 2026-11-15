"""Real native prepareRename: precise binding/ranges, scope and invalidation."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import time

from check_language_snapshots import Client
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    server, out = args.server.resolve(strict=True), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = out/'prepare 空间😀'
    root.mkdir()
    bodies = dict(main='\ufeffimport shared\r\nfunc main(...) as int\r\n // compute comment\r\n string text = "compute"\r\n return shared.compute(1)\r\nend\r\n',
                  shared='\ufeffexport func compute(int value) as int\r\n return value\r\nend\r\n',
                  broken='func broken(\n',
                  unicode='\ufefffunc main(...) as int\r\n int 数量😀 = 1\r\n return 数量😀\r\nend\r\n',
                  foreign='extern "native_call" func nativeCall(c_int number) as c_int\nfunc main(...) as int\n return int(nativeCall(c_int(1)))\nend\n')
    paths = {key: root/(key+'.xl') for key in bodies}
    for key, path in paths.items():
        path.write_bytes(bodies[key].encode())

    def context(name, entry, members):
        lists = {key: [] for key in LISTS}
        lists['members'] = [paths[key].as_posix() for key in members]
        return dict(schema=1, kind='project', project=(root/(name+'.xlp')).as_posix(),
                    configuration='debug', entry=paths[entry].as_posix(), directory=root.as_posix(),
                    analysisProfile=PROFILE, targetKind='exe', packagePath=None, lists=lists)

    a = context('A', 'main', ['main', 'shared'])
    catalog = dict(schema=1, selectedProject=a['project'], packagePath=None, projects=[a])
    protected = {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in [server, Path(__file__), *paths.values()]}
    clients, cases, timings = [], [], []
    report = dict(status='failed', cases=cases, timings=timings, protected=protected,
                  performanceQualified=False, fullGoalComplete=False)
    request = 100

    def done(name):
        cases.append(dict(name=name, passed=True))

    def start(config=catalog, opened='main', support=True, backend='native', **roots):
        c = Client(server, root/'absent-xl.exe', root)
        clients.append(c)
        rename = {} if support is None else dict(prepareSupport=support)
        options = dict(automaticDiagnostics=False, checkOnSave=False,
                       backgroundWorkspaceSymbols=False, diagnosticsBackend=backend)
        if config is not None:
            options['analysisConfiguration'] = dict(epoch=1, catalog=config)
        c.send('initialize', dict(capabilities=dict(textDocument=dict(rename=rename)),
                                 initializationOptions=options, **roots), 1)
        reply, _ = c.reply(1)
        assert 'result' in reply, reply
        cap = reply['result']['capabilities'].get('renameProvider')
        assert cap == (dict(prepareProvider=True) if support else True) if backend == 'native' else cap is None, cap
        c.send('initialized', {})
        c.open(paths[opened], bodies[opened], 1)
        return c

    def cursor(key='main', needle='compute(1)', inside=1):
        prefix = bodies[key][:bodies[key].index(needle)+inside]
        return dict(textDocument=dict(uri=paths[key].as_uri()),
                    position=dict(line=prefix.count('\n'),
                                  character=len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le'))//2))

    def call(c, params, code=None, rid=None, method='textDocument/prepareRename'):
        nonlocal request
        request += 1
        rid = request if rid is None else rid
        before = time.perf_counter()
        c.send(method, params, rid)
        reply, _ = c.reply(rid)
        assert type(reply['id']) is type(rid), reply
        timings.append(dict(method=method, milliseconds=(time.perf_counter()-before)*1000,
                            error=reply.get('error', {}).get('code')))
        if code is not None:
            assert reply.get('error', {}).get('code') == code, reply
        else:
            assert 'result' in reply and 'error' not in reply, reply
        return reply.get('result')

    expected = dict(range=dict(start=dict(line=4, character=15), end=dict(line=4, character=22)), placeholder='compute')

    def batch(c, messages):
        raw = b''
        for method, params, rid in messages:
            message = dict(jsonrpc='2.0', method=method, params=params)
            if rid is not None:
                message['id'] = rid
            c.transcript.append(dict(send=message))
            data = json.dumps(message, ensure_ascii=False, separators=(',', ':')).encode()
            raw += f'Content-Length: {len(data)}\r\n\r\n'.encode()+data
        c.process.stdin.write(raw)
        c.process.stdin.flush()

    try:
        c = start(rootUri=root.as_uri())
        intent = cursor()
        assert call(c, intent, rid='prepare-😀') == expected
        assert call(c, intent, rid=0) == expected
        assert call(c, intent, rid=-1) == expected
        done('negotiated-native-qualified-identifier-range-bom-crlf-and-typed-ids')
        for _ in range(5):
            assert call(c, intent) == expected
        done('repeated-stable-prepare-proposals-remain-read-only')
        extended = copy.deepcopy(intent)
        extended.update(workDoneToken='prepare-progress', newName='ignored invalid name', futureExtension={})
        assert call(c, extended) == expected
        done('prepare-needs-no-new-name-and-does-not-trust-lexical-claims')
        for needle, inside in [('compute comment', 1), ('"compute"', 2), ('func main', 1)]:
            call(c, cursor(needle=needle, inside=inside), -32803)
        done('comments-strings-and-reserved-word-positions-do-not-guess-a-word')
        c.stop()
        c = start(opened='shared')
        declaration = dict(range=dict(start=dict(line=0, character=13), end=dict(line=0, character=20)), placeholder='compute')
        assert call(c, cursor('shared', 'compute(int')) == declaration
        done('first-line-bom-declaration-keeps-editor-utf16-prefix')
        c.stop()
        for support in (False, None):
            c = start(support=support)
            assert call(c, intent) == expected
            done('boolean-provider-remains-compatible-without-prepare-support')
            c.stop()
        broken = copy.deepcopy(catalog)
        broken['projects'].append(context('B', 'broken', ['broken']))
        c = start(config=broken)
        assert call(c, intent) == expected
        rename = copy.deepcopy(intent)
        rename['newName'] = 'changed'
        call(c, rename, -32803, method='textDocument/rename')
        done('local-prepare-does-not-analyze-unrelated-broken-project-final-rename-does')
        c.stop()
        c = start(config=None, opened='unicode')
        unicode_expected = dict(range=dict(start=dict(line=1, character=5), end=dict(line=1, character=9)), placeholder='数量😀')
        assert call(c, cursor('unicode', '数量😀')) == unicode_expected
        assert call(c, cursor('unicode', '数量😀', inside=2)) == unicode_expected
        bad = cursor('unicode', '数量😀')
        bad['position']['character'] = 8
        call(c, bad, -32602)
        done('standalone-native-unicode-placeholder-and-strict-utf16-scalar-boundary')
        c.stop()
        restricted = copy.deepcopy(catalog)
        restricted['projects'][0]['lists']['members'] = [paths['main'].as_posix()]
        c = start(config=restricted)
        call(c, intent, -32803)
        done('imported-library-declaration-is-not-editable-membership')
        c.stop()
        c = start(config=None, opened='foreign')
        call(c, cursor('foreign', 'nativeCall(c_int'), -32803)
        done('actual-foreign-declaration-policy-refuses-preparation')
        c.stop()
        c = start(config=None, rootUri=root.as_uri())
        call(c, intent, -32803)
        done('unknown-workspace-roster-is-not-an-open-document-scope')
        c.stop()
        for name, mutation in [
            ('invalid-line', lambda v: v['position'].update(line=-1)),
            ('invalid-character', lambda v: v['position'].update(character=1.5)),
            ('missing-uri', lambda v: v['textDocument'].clear()),
            ('bad-progress-token', lambda v: v.update(workDoneToken=[])),
        ]:
            c = start()
            bad = copy.deepcopy(intent)
            mutation(bad)
            call(c, bad, -32602)
            done(name)
            c.stop()
        for name, method, payload, code in [
            ('cancel', '$/cancelRequest', dict(id='active'), -32800),
            ('document-change', 'textDocument/didChange', dict(textDocument=dict(uri=paths['main'].as_uri(), version=2), contentChanges=[dict(text=bodies['main']+'\n')]), -32801),
            ('close-reopen', 'textDocument/didClose', dict(textDocument=dict(uri=paths['main'].as_uri())), -32801),
            ('configuration', 'workspace/didChangeConfiguration', dict(settings=dict(xlang=dict(analysisConfiguration=dict(epoch=2, catalog=catalog)))), -32801),
            ('folder-roster', 'workspace/didChangeWorkspaceFolders', dict(event=dict(added=[dict(uri=root.as_uri(), name=root.name)], removed=[])), -32801),
        ]:
            c = start()
            batch(c, [('textDocument/prepareRename', intent, 'active'), (method, payload, None)])
            reply, _ = c.reply('active')
            assert reply.get('error', {}).get('code') == code, reply
            done(name+'-discards-pending-preparation')
            c.stop()
        c = start()
        batch(c, [('textDocument/prepareRename', intent, 0), ('textDocument/prepareRename', intent, '0'),
                  ('$/cancelRequest', dict(id='0'), None)])
        replies = {}
        while len(replies) < 2:
            reply = c.next()
            if reply.get('id') in (0, '0'):
                key = (type(reply['id']).__name__, reply['id'])
                assert key not in replies, reply
                replies[key] = reply
        assert replies[('int', 0)]['result'] == expected
        assert replies[('str', '0')].get('error', {}).get('code') == -32800, replies
        done('typed-same-spelling-prepare-ids-cancel-independently')
        c.stop()
        c = start()
        call(c, dict(token=1), method='xlang/suspendAnalysis')
        call(c, intent, -32001)
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(added=[dict(uri=root.as_uri(), name=root.name)], removed=[])))
        lease = call(c, {}, method='xlang/analysisLeaseState')
        assert lease['held'] and lease['invalidated'] and lease['deferred'] == 1, lease
        call(c, dict(token=1), method='xlang/resumeAnalysis')
        call(c, intent, -32803)
        done('lease-blocks-prepare-and-replays-folder-invalidation-before-new-work')
        c.stop()
        c = start(config=None, backend='cli')
        call(c, intent, -32601)
        done('cli-backend-does-not-offer-prepare')
        c.stop()
        report['status'] = 'passed'
    finally:
        report['stderr'] = [c.stop() for c in clients]
        report['transcripts'] = [c.transcript for c in clients]
        report['protectedUnchanged'] = all(hashlib.sha256(Path(f).read_bytes()).hexdigest() == h for f, h in protected.items())
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    assert report['protectedUnchanged'] and not any(report['stderr']), report['stderr']
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(out/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
