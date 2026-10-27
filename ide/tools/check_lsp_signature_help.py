"""Exercise standard signatureHelp through the real stdio host and input guards."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position
from check_lsp_module_semantics import initialize as configured_initialize


def wait_message(client, predicate):
    until = time.monotonic()+12
    while True:
        value = client.next()
        if predicate(value):
            return value
        assert time.monotonic() < until, value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(server): sha(server), str(Path(__file__)): sha(__file__)}
    cases = []
    report = dict(status='failed', cases=cases, protected=protected, scope=__doc__)
    def done(name, **details):
        cases.append(dict(name=name, passed=True, **details))
    try:
        for offsets in [False, True]:
            directory = output/('offsets' if offsets else 'strings')
            directory.mkdir()
            client = AutomaticClient(server, directory/'absent/xl.exe', directory)
            path = directory/'main.xl'
            version = 0
            try:
                caps = dict(textDocument=dict(signatureHelp=dict(contextSupport=True,
                    signatureInformation=dict(activeParameterSupport=offsets,
                        parameterInformation=dict(labelOffsetSupport=offsets)))))
                client.send('initialize', dict(capabilities=caps, initializationOptions=dict(
                    automaticDiagnostics=False, checkOnSave=False)), 1)
                reply, _ = client.reply(1)
                init = reply['result']
                assert init['serverInfo']['version'] == '0.88', init
                assert init['capabilities']['signatureHelpProvider'] == dict(
                    triggerCharacters=['(', ','], retriggerCharacters=[')']), init
                client.send('initialized', {})
                done(f'{offsets}-negotiated-provider', capabilities=init['capabilities']['signatureHelpProvider'])
                def request(body, context=None, *, change=True, at=None, extra=None):
                    nonlocal version
                    if '@' in body:
                        at = body.index('@')
                        body = body.replace('@', '', 1)
                    assert at is not None
                    if change:
                        version += 1
                        if version == 1:
                            client.open(path, body, version)
                        else:
                            client.change(path, body, version)
                    client.request += 1
                    params = dict(textDocument=dict(uri=path.as_uri()), position=position(body, at))
                    if context is not None:
                        params['context'] = context
                    params.update(extra or {})
                    client.send('textDocument/signatureHelp', params, client.request)
                    reply, _ = client.reply(client.request)
                    assert 'error' not in reply, reply
                    result = reply['result']
                    if result is not None:
                        assert result['version'] == version and result['queryId'] == client.request, result
                    return result, body, at
                definitions = 'func plus(int left,string right="") as int\n return left\nend\n'
                samples = [
                    (definitions, ' return plus(@)\nend\n', 'plus(int left, string right = ...) as int', 0),
                    (definitions, ' return plus(7,@\nend\n', 'plus(int left, string right = ...) as int', 1),
                    (definitions, ' return plus(7,"unfinished@\nend\n', 'plus(int left, string right = ...) as int', 1),
                    (definitions, ' return plus(right="x",left=@7)\nend\n', 'plus(int left, string right = ...) as int', 0),
                    ('class Box<T>\n func apply(T value) as T\n return value\n end\nend\n',
                        ' auto box=Box::<int>()\n return box.apply(@)\nend\n', 'box.apply(int value) as int', 0),
                    ('class Box\n create(int value)\n end\nend\n', ' auto box=Box(@)\n return 0\nend\n', 'Box(int value)', 0),
                    ('func unary(int value) as int\n return value\nend\n', ' auto callback=unary\n return callback(@)\nend\n', 'callback(int) as int', 0),
                    ('func make(string value) as func(int) as int\n return func(int number) as int\n return number\n end\nend\n',
                        ' return make("😀")(@)\nend\n', 'make("😀")(int) as int', 0),
                    ('\ufefffunc 中文(int 数字,string 文本="😀") as int\r\n return 数字\r\nend\r\n',
                        ' return 中文(7,文本=@"x")\r\nend\r\n', '中文(int 数字, string 文本 = ...) as int', 1),
                ]
                for i, (defs, tail, expected, active) in enumerate(samples):
                    marked = defs+'func main(...) as int\n'+tail
                    result, body, at = request(marked, dict(triggerKind=1, isRetrigger=False))
                    selected = result['signatures'][result['activeSignature']]
                    assert selected['label'] == expected and result['activeParameter'] == active, result
                    assert ('activeParameter' in selected) == offsets, selected
                    for p in selected['parameters']:
                        label = p['label']
                        if offsets:
                            assert isinstance(label, list) and len(label) == 2
                            piece = expected.encode('utf-16-le')[2*label[0]:2*label[1]].decode('utf-16-le')
                            assert piece and piece in expected, (selected, p)
                        else:
                            assert isinstance(label, str) and label and label in expected, (selected, p)
                    done(f'{offsets}-native-shape-{i}', label=expected, active=active)
                result, body, at = request(definitions+'func main(...) as int\n return plus(7,@"x")\nend\n',
                    dict(triggerKind=2, triggerCharacter=',', isRetrigger=False))
                previous = result
                result, _, _ = request(body, dict(triggerKind=3, isRetrigger=True, activeSignatureHelp=previous), change=False, at=at)
                assert result['analysis']['cacheHits'] >= 1 and result['signatures'] == previous['signatures'], result
                done(f'{offsets}-retrigger-real-cache', analysis=result['analysis'])
                # Same-call/current-argument views keep exact fresh metadata and
                # reuse the semantic model across whitespace and non-BMP text.
                result, body, at = request(definitions+'func main(...) as int\n return plus(7,@ "😀 文本")\nend\n')
                previous = result
                for target in range(at+1, at+6):
                    result, _, _ = request(body, dict(triggerKind=3, isRetrigger=True), change=False, at=target)
                    assert result['signatures'] == previous['signatures'] and result['activeParameter'] == 1, result
                    assert result['analysis']['caretHits'] == previous['analysis']['caretHits']+1, result
                    assert result['analysis']['indexBuilds'] == previous['analysis']['indexBuilds'] and result['analysis']['builds'] == previous['analysis']['builds'], result
                    previous = result
                done(f'{offsets}-immutable-caret-views', analysis=result['analysis'])
                result, _, _ = request(body, change=False, at=body.index('plus(7,')+5)
                assert result['activeParameter'] == 0 and result['analysis']['caretHits'] == previous['analysis']['caretHits'] and result['analysis']['builds'] > previous['analysis']['builds'], result
                done(f'{offsets}-argument-change-rechecks', analysis=result['analysis'])
                nested = definitions+'func main(...) as int\n return plus(plus(1,"inner"), "outer")\nend\n'
                result, body, _ = request(nested, at=nested.index('outer'))
                previous = result
                result, _, _ = request(body, change=False, at=body.index('inner'))
                assert result['analysis']['caretHits'] == previous['analysis']['caretHits'] and result['analysis']['builds'] > previous['analysis']['builds'], result
                assert result['activeParameter'] == 1, result
                done(f'{offsets}-nested-call-change-rechecks', analysis=result['analysis'])
                # These are real source changes, not caret views. Require the
                # native query-file parser to reuse untouched declarations.
                stable = ''.join(f'func stable_{i}(int value) as int\n if value>0 then\n return value+{i}\n end\n return 0\nend\n' for i in range(24))
                for edit in range(5):
                    marked = '\ufeff'+stable+definitions+f'func main(...) as int\r\n int 中文={edit}\r\n return plus(中文,@"😀 {edit}")\r\nend\r\n'
                    result, body, at = request(marked)
                    analysis = result['analysis']
                    assert result['signatures'][0]['label'] == 'plus(int left, string right = ...) as int' and result['activeParameter'] == 1, result
                    assert 0 < analysis['signatureSyntaxCacheBytes'] <= 32*1024*1024, analysis
                    if edit:
                        assert analysis['signatureFullParses'] == 0 and analysis['signatureFragmentParses'] == 1, analysis
                        assert analysis['signatureReusedDeclarations'] == 25 and analysis['signatureParsedBytes'] < len(body.encode('utf8'))/4, analysis
                        assert analysis['signatureSyntaxFallbacks'] == 0, analysis
                        assert analysis['reused'] >= 24, analysis
                        assert analysis['signatureProofHits'] >= 24 and analysis['signatureProofNodes'] > analysis['signatureProofHits'], analysis
                        # Query-neutral bodies retain independent immutable
                        # facts; this response must not construct their types.
                        # Full AST and eager pool paths have native oracles.
                        assert analysis['signatureTypeNodes'] == 0, analysis
                        assert analysis['signatureTypePages'] == analysis['signatureTypeBytes'] == 0, analysis
                        assert analysis['signatureTypeHeapFallbacks'] == 0, analysis
                    else:
                        assert analysis['signatureFullParses'] == 1, analysis
                    done(f'{offsets}-native-incremental-signature-{edit}', analysis=analysis)
                marked = body.replace('string right=""', 'string right="changed"')
                result, body, at = request(marked, at=marked.index('😀'))
                assert result['analysis']['signatureFullParses'] == 1 and result['analysis']['signatureSyntaxFallbacks'] == 1, result
                done(f'{offsets}-signature-header-fallback', analysis=result['analysis'])
                # Both foreground modes accept the same current input concurrently.
                mixed = definitions+'func main(...) as int\n int completionLocal=7\n return plus(7, completionL)\nend\n'
                version += 1
                client.change(path, mixed, version)
                cursor = position(mixed, mixed.index('completionL)')+len('completionL'))
                client.batch([dict(jsonrpc='2.0', method='textDocument/signatureHelp', id=600,
                    params=dict(textDocument=dict(uri=path.as_uri()), position=cursor)),
                    dict(jsonrpc='2.0', method='textDocument/completion', id=601,
                    params=dict(textDocument=dict(uri=path.as_uri()), position=cursor))])
                replies = {}
                while len(replies) < 2:
                    value = client.next()
                    if value.get('id') in [600, 601]:
                        assert 'error' not in value, value
                        replies[value['id']] = value['result']
                assert replies[600]['signatures'][0]['label'].startswith('plus('), replies
                assert [r['label'] for r in replies[601]['items']] == ['completionLocal'], replies
                result, _, _ = request(mixed, change=False, at=mixed.index('completionL)')+len('completionL'))
                assert result['analysis']['cacheHits'] >= 1, result
                assert result['analysis']['workerThreadId'] != replies[601]['analysis']['workerThreadId'], replies
                done(f'{offsets}-independent-concurrent-completion-and-signature', signature=result['analysis'], completion=replies[601]['analysis'])
                for marked in [definitions+'func main(...) as int\n return @7\nend\n',
                               definitions+'func main(...) as int\n return plus( // @comment\n )\nend\n']:
                    result, body, at = request(marked)
                    assert result is None, result
                done(f'{offsets}-empty-and-comment-null')
                for bad in [{}, dict(triggerKind=2, isRetrigger=False),
                            dict(triggerKind=1, isRetrigger='no'),
                            dict(triggerKind=9, isRetrigger=False),
                            dict(triggerKind=1, isRetrigger=False, activeSignatureHelp=dict(signatures=[dict(label='f', parameters=[dict(label=[0,99])])]))]:
                    client.request += 1
                    client.send('textDocument/signatureHelp', dict(textDocument=dict(uri=path.as_uri()),
                        position=position(body, at), context=bad), client.request)
                    reply, _ = client.reply(client.request)
                    assert reply['error']['code'] == -32602, reply
                done(f'{offsets}-malformed-context')
                result, body, at = request(definitions+'func main(...) as int\n return plus(7,@"x")\nend\n')
                published = result['queryId']
                client.change(path, body.replace('plus(7,', 'plus(8,'), version+1)
                version += 1
                invalid = wait_message(client, lambda v: v.get('method') == 'xlang/signatureInvalidated' and v['params']['queryId'] == published)
                assert invalid['params']['version'] == version-1, invalid
                done(f'{offsets}-published-edit-invalidated')
                client.send('textDocument/signatureHelp', dict(textDocument=dict(uri=path.as_uri()),
                    version=version-1, position=position(body, at)), 800)
                reply, _ = client.reply(800)
                assert reply['error']['code'] == -32602, reply
                done(f'{offsets}-stale-explicit-version')
                # Queue a cold large query, supersede its waiting request, then
                # cancel the latest ID in the same framed input batch.
                large = ''.join(f'func helper_{i}(int value) as int\n return value+{i}\nend\n' for i in range(12000))
                large += definitions+'func main(...) as int\n return plus(7,"x")\nend\n'
                version += 1
                params = dict(textDocument=dict(uri=path.as_uri()), position=position(large, large.rindex('plus(7,')+7))
                client.batch([dict(jsonrpc='2.0', method='textDocument/didChange', params=dict(
                    textDocument=dict(uri=path.as_uri(),version=version),contentChanges=[dict(text=large)])),
                    *[dict(jsonrpc='2.0',method='textDocument/signatureHelp',id=i,params=params) for i in [810,811,812]],
                    dict(jsonrpc='2.0',method='$/cancelRequest',params=dict(id=812))])
                replies = {}
                while len(replies) < 3:
                    value = client.next()
                    if value.get('id') in [810,811,812]:
                        assert value.get('error',{}).get('code') == -32800, value
                        assert value['id'] not in replies, value
                        replies[value['id']] = value
                done(f'{offsets}-bounded-latest-pending-and-cancel')
                result, body, at = request(definitions+'func main(...) as int\n return plus(7,@"x")\nend\n')
                assert result['activeParameter'] == 1, result
                done(f'{offsets}-cancelled-worker-recovery')
                client.send('textDocument/didClose', dict(textDocument=dict(uri=path.as_uri())))
                client.send('textDocument/signatureHelp', dict(textDocument=dict(uri=path.as_uri()), position=dict(line=0, character=0)), 801)
                reply, _ = client.reply(801)
                assert reply['error']['code'] == -32602, reply
                done(f'{offsets}-closed-document-rejected')
            finally:
                client.stop()
                (directory/'transcript.json').write_text(json.dumps(client.transcript,ensure_ascii=False,indent=2),encoding='utf8')
                assert client.process.returncode == 0
        # A real configured closed entry resolves an unsaved imported query.
        directory = output/'configured'
        directory.mkdir()
        entry, helper = directory/'main.xl', directory/'helper.xl'
        entry.write_text('import helper\nfunc main(...) as int\n return helper.invoke()\nend\n', encoding='utf8')
        protected[str(entry)] = sha(entry)
        client = AutomaticClient(server, directory/'absent/xl.exe', directory)
        try:
            configured_initialize(client, entry, [entry, helper], directory)
            body = 'func target(int value) as int\n return value\nend\nexport func invoke() as int\n return target(7)\nend\n'
            client.open(helper, body, 7)
            at = body.index('target(7)')+7
            client.send('textDocument/signatureHelp', dict(textDocument=dict(uri=helper.as_uri()), position=position(body, at)), 300)
            reply, _ = client.reply(300)
            result = reply['result']
            assert result['signatures'][0]['label'] == 'target(int value) as int' and result['analysisContext']['epoch'] == 1, result
            done('configured-closed-entry-unsaved-query', context=result['analysisContext'])
            client.send('textDocument/signatureHelp', dict(textDocument=dict(uri=helper.as_uri()), position=position(body, at+1)), 302)
            reply, _ = client.reply(302)
            view = reply['result']
            assert view['signatures'] == result['signatures'] and view['analysis']['caretHits'] == result['analysis']['caretHits']+1 and view['analysis']['builds'] == result['analysis']['builds'], view
            done('configured-caret-view-guard', analysis=view['analysis'])
            entry.write_text(entry.read_text()+'// changed disk proof\n', encoding='utf8')
            invalid = wait_message(client, lambda v: v.get('method') == 'xlang/signatureInvalidated' and v['params']['queryId'] == 302)
            done('configured-disk-proof-invalidated', reason=invalid['params']['reason'])
            protected[str(entry)] = sha(entry)
            client.send('textDocument/signatureHelp', dict(textDocument=dict(uri=helper.as_uri()), position=position(body, at)), 301)
            reply, _ = client.reply(301)
            assert reply['result']['signatures'][0]['label'] == 'target(int value) as int', reply
            assert reply['result']['analysis']['builds'] > view['analysis']['builds'], reply
            done('configured-disk-proof-rebuild')
            prefix = body[:body.index(' return target(')]
            for edit in range(3):
                changed = prefix+f' int 中文={edit}\n return target(中文)\nend\n'
                client.change(helper, changed, 8+edit)
                at = changed.index('target(中文)')+7
                query = 310+edit
                client.send('textDocument/signatureHelp', dict(textDocument=dict(uri=helper.as_uri()), position=position(changed, at)), query)
                reply, _ = client.reply(query)
                result = reply['result']
                analysis = result['analysis']
                assert result['version'] == 8+edit and result['analysisContext']['epoch'] == 1, result
                assert result['signatures'][0]['label'] == 'target(int value) as int' and not helper.exists(), result
                assert analysis['signatureFullParses'] == 0 and analysis['signatureFragmentParses'] == 1 and analysis['signatureReusedDeclarations'] == 1, analysis
                done(f'configured-unsaved-query-incremental-{edit}', analysis=analysis)
                body = changed
            entry.write_text(entry.read_text()+'// changed after incremental snapshot\n', encoding='utf8')
            invalid = wait_message(client, lambda v: v.get('method') == 'xlang/signatureInvalidated' and v['params']['queryId'] == query)
            protected[str(entry)] = sha(entry)
            done('configured-incremental-snapshot-disk-invalidated', reason=invalid['params']['reason'])
        finally:
            client.stop()
            (directory/'transcript.json').write_text(json.dumps(client.transcript,ensure_ascii=False,indent=2),encoding='utf8')
            assert client.process.returncode == 0
        assert all(sha(p) == h for p, h in protected.items())
        report['status'] = 'passed'
    finally:
        (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(status=report['status'], cases=len(cases), proof=str(output/'result.json'))))


if __name__ == '__main__':
    main()
