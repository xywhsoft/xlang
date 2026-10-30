"""Exercise ordinary multi-function edits, rejection and recovery over stdio.

The reference service uses the same compiler cohort and the original whole
parse fallback. Native fixtures separately compare complete fresh AST and IR.
This suite verifies actual unsaved entry/imported edits and reported work.
"""
from pathlib import Path
import argparse
import hashlib
import json
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize, checked


def source(count, generation, imported):
    body = '\ufeff// 中文😀\r\n'
    for index in range(count):
        if index == count//2 and generation != 8:
            body += f'#doc\r\ndesc:\r\n documented_{generation if generation >= 7 else 0}\r\n#end\r\n'
        changed = generation and index in (0, count-1)
        extra = f' int extra = {generation}\r\n' if generation and index == 0 else ''
        local = ' int local = "not closed\r\n' if generation == 4 and index == 0 else f' int local = value+{index}\r\n'
        result = 'unknown_value' if generation == 3 and index == 0 else 'local+extra' if extra else f'local+{generation}' if changed else 'local'
        declaration = 'export func' if imported else 'func'
        body += f'{declaration} work_{index}(int value) as int\r\n'+local+extra+f' return {result}\r\nend\r\n\r\n'
        if index == 0 and generation >= 6:
            body += '/// changed documentation between declarations\r\n'
    if not imported:
        body += f'func main(...) as int\r\n return work_0(7)+work_{count-1}(3)+work_{count//2}(1)\r\nend\r\n'
    return body


def facts(result):
    # Each request's checkId is fenced separately, including cache repeats.
    return {key: value for key, value in result.items() if key not in ('analysis', 'durationMs', 'checkId', 'queryId')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    programs = dict(reference=args.baseline.resolve(strict=True), candidate=args.server.resolve(strict=True))
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    protected = {str(path): sha(path) for path in programs.values()}
    report = dict(status='failed', scope=__doc__, cases=[], processes=[], protected=protected)
    try:
        for count in (8, 96):
            for imported in (False, True):
                directory = output/(str(count)+('-imported' if imported else '-single')); directory.mkdir()
                entry = directory/'入口😀.xl'; changed = directory/'dep.xl' if imported else entry
                for path in dict.fromkeys((entry, changed)):
                    path.write_bytes(b'// immutable disk; content comes from unsaved editor overlays\n')
                    protected[str(path)] = sha(path)
                clients = {}; previous = None
                try:
                    for variant, program in programs.items():
                        client = AutomaticClient(program, directory/'absent/xl.exe', directory); clients[variant] = client
                        initialize(client, entry, [entry, changed] if imported else [entry], directory)
                        if imported:
                            client.open(entry, f'import dep\nfunc main(...) as int\n return dep.work_0(7)+dep.work_{count-1}(3)+dep.work_{count//2}(1)\nend\n', 1)
                    for generation in range(10):
                        responses = {}
                        for variant in tuple(clients)[::1 if generation % 2 == 0 else -1]:
                            client = clients[variant]; body = source(count, generation, imported)
                            if generation: client.change(changed, body, generation+1)
                            else: client.open(changed, body, 1)
                            result = checked(client, entry, 1 if imported else generation+1)
                            assert result['checkId'] == client.request, result
                            assert result['ok'] == (generation not in (3, 4)), (generation, variant, result)
                            assert (result['count'] == 0) == (generation not in (3, 4)), (generation, variant, result)
                            responses[variant] = result
                            repeated = checked(client, entry, 1 if imported else generation+1)
                            assert repeated['checkId'] == client.request, repeated
                            assert facts(repeated) == facts(result)
                            assert repeated['analysis']['builds'] == result['analysis']['builds']
                            assert repeated['analysis']['entryFragmentParses'] == result['analysis']['entryFragmentParses']
                            signature = None
                            if result['ok']:
                                entry_body = f'import dep\nfunc main(...) as int\n return dep.work_0(7)+dep.work_{count-1}(3)+dep.work_{count//2}(1)\nend\n' if imported else body
                                marker = ('dep.' if imported else '')+f'work_{count//2}('
                                prefix = entry_body[:entry_body.rindex(marker)+len(marker)]
                                position = dict(line=prefix.count('\n'),character=len(prefix.rsplit('\n',1)[-1].encode('utf-16-le'))//2)
                                client.request += 1
                                client.send('textDocument/signatureHelp',dict(textDocument=dict(uri=entry.as_uri()),position=position),client.request)
                                reply, _ = client.reply(client.request)
                                assert 'error' not in reply, reply
                                signature = reply['result']
                                assert signature['queryId'] == client.request and signature['version'] == (1 if imported else generation+1), signature
                                assert signature['analysisContext']['epoch'] == 1, signature
                                if generation == 8:
                                    assert 'documented_' not in json.dumps(signature), signature
                                else:
                                    assert f'documented_{generation if generation >= 7 else 0}' in json.dumps(signature), signature
                            report['cases'].append(dict(functions=count, imported=imported, generation=generation,
                                variant=variant, result=result, repeatedAnalysis=repeated['analysis'],signature=signature))
                            if signature is not None:
                                result_signature = signature
                                responses[variant+'_signature'] = facts(result_signature)
                        assert facts(responses['candidate']) == facts(responses['reference']), (count, imported, generation, responses)
                        assert responses.get('candidate_signature') == responses.get('reference_signature'), responses
                        work = responses['candidate']['analysis']
                        if generation in (1, 2):
                            if imported:
                                module = work['lastBuildModuleSyntax']
                                assert not module['full'] and module['fragments'] == 2 and module['reused'] >= count-2, module
                                assert module['bytes'] < len(body.encode('utf8'))//2, module
                            else:
                                assert work['entryFragmentParses'] == previous['entryFragmentParses']+2, work
                                assert work['entryFullParses'] == previous['entryFullParses'], work
                        if generation == 6:
                            if imported: assert work['lastBuildModuleSyntax']['full'] == 1, work
                            else: assert work['entryFullParses'] > previous['entryFullParses'], work
                        previous = work
                finally:
                    for variant, client in clients.items():
                        stderr = client.stop()
                        (directory/(variant+'-stderr.log')).write_text(stderr, encoding='utf8')
                        (directory/(variant+'-transcript.json')).write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2), encoding='utf8')
                        report['processes'].append(dict(variant=variant, pid=client.process.pid, exit=client.process.returncode))
                        assert client.process.returncode == 0
        assert len(report['cases']) == 80 and len(report['processes']) == 8
        assert all(sha(path) == value for path, value in protected.items())
        report['status'] = 'passed'
    finally:
        (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),processes=len(report['processes']),proof=str(output/'result.json'))))


if __name__ == '__main__':
    main()
