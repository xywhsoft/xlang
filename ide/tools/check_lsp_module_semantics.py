"""Verify real multi-file semantic reuse through the configured stdio actor.

Native fixtures compare the complete AST/index/IR against fresh checks. This
suite additionally proves actual server work, version/source fencing and
current navigation with both an opened and a closed project entry.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS, PROFILE
from check_language_definitions import position


def source(count, value, shifted):
    header = '\ufeff' + ('// 中文😀\r\n\r\n' if shifted else '')
    identity = 'func identity(int value) as int\r\n return value\r\nend\r\n'
    functions = ''
    for i in range(count):
        expression = 'identity(local)' if i % 2 else 'local'
        functions += (f'func work_{i}(int value) as int\r\n int local = value+{i}\r\n'
            ' if local > 0 then\r\n' + f' local = {expression}+1\r\n' * 12 +
            ' else\r\n' + f' local = {expression}-1\r\n' * 12 +
            ' end\r\n return local\r\nend\r\n')
    return header + identity + functions + f'export func compute(int value) as int\r\n return identity(value)+{value}\r\nend\r\n'


def initialize(client, entry, members, directory):
    lists = {key: [] for key in LISTS}
    lists['members'] = [p.as_posix() for p in members]
    context = dict(schema=1, kind='project', project=(directory/'project.xlp').as_posix(),
        configuration='debug', entry=entry.as_posix(), directory=directory.as_posix(),
        analysisProfile=PROFILE, targetKind='exe', packagePath='', lists=lists)
    client.send('initialize', dict(capabilities={}, initializationOptions=dict(automaticDiagnostics=False,
        checkOnSave=False, analysisConfiguration=dict(epoch=1, catalog=dict(schema=1,
            projects=[context], selectedProject=context['project'], packagePath='')))), 1)
    reply, _ = client.reply(1)
    assert 'error' not in reply, reply
    client.send('initialized', {})


def checked(client, path, version):
    until = time.monotonic() + 30
    while True:
        client.request += 1
        client.send('xlang/check', dict(textDocument=dict(uri=path.as_uri()), version=version), client.request)
        reply, _ = client.reply(client.request)
        if reply.get('error', {}).get('code') != -32001:
            break
        assert time.monotonic() < until, reply
        time.sleep(.001)
    assert 'error' not in reply, reply
    result = reply['result']
    assert result['version'] == version and result['analysisContext']['epoch'] == 1, result
    return result


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
    try:
        for opened in [False, True]:
            directory = output/('opened' if opened else 'disk')
            directory.mkdir()
            entry, left, right = [directory/p for p in ['main.xl', 'member_left.xl', 'member_right.xl']]
            entry_body = 'import member_left\nimport member_right\nfunc main(...) as int\n return member_left.compute(3)+member_right.compute(4)\nend\n'
            entry.write_bytes(entry_body.encode())
            protected[str(entry)] = sha(entry)
            client = AutomaticClient(server, directory/'absent/xl.exe', directory)
            def done(name, **details):
                cases.append(dict(name=('opened-' if opened else 'disk-')+name, passed=True, **details))
            try:
                initialize(client, entry, [entry, left, right], directory)
                if opened:
                    client.open(entry, entry_body, 1)
                bodies = [source(4, 0, False), source(4, 1, True)]
                client.open(left, bodies[0], 7)
                client.open(right, bodies[1], 7)
                result = checked(client, left, 7)
                assert result['ok'] and not result['count'] and result['entryOpen'] == opened and result['retainedEntry'], result
                previous = result['analysis']
                assert previous['semanticFunctionsChecked'] >= 13 and not previous['semanticFunctionsReused'], previous
                assert previous['workerThreadId'] != previous['captureThreadId'], previous
                done('initial-real-worker-and-semantic-capture', analysis=previous)
                for i in range(1, 5):
                    bodies = [source(4, i+d, bool((i+d) % 2)) for d in range(2)]
                    client.change(left, bodies[0], 7+i)
                    client.change(right, bodies[1], 7+i)
                    result = checked(client, left, 7+i)
                    assert result['ok'] and not result['count'] and result['semanticDefinitions'], result
                    current = result['analysis']
                    assert current['builds'] == i+1 and not current['cacheHits'], current
                    assert current['semanticContextHits'] == i and current['semanticFunctionsReused'] > previous['semanticFunctionsReused'], current
                    assert current['semanticFunctionsChecked'] > previous['semanticFunctionsChecked'], current
                    assert 0 < current['semanticMemoBytes'] <= 64*1024*1024, current
                    documents = {Path(d['path']): d for d in result['documents']}
                    assert documents[left]['version'] == documents[right]['version'] == 7+i, documents
                    assert documents[entry]['origin'] == ('overlay' if opened else 'disk'), documents
                    if not opened:
                        assert 'version' not in documents[entry], documents
                    start = bodies[1].index('func work_3(')
                    declaration = bodies[1].index('local =', start)
                    reference = bodies[1].index('return local', start) + len('return ')
                    definition = client.call('textDocument/definition', dict(textDocument=dict(uri=right.as_uri()),
                        version=7+i, position=position(bodies[1], reference)))
                    assert definition['uri'] == right.as_uri() and definition['range']['start'] == position(bodies[1], declaration), definition
                    previous = current
                done('four-two-member-body-edits-reuse-real-functions', analysis=current)
                done('current-versions-and-cross-file-relocated-navigation')
                repeated = checked(client, left, 11)
                assert repeated['analysis']['builds'] == 5 and repeated['analysis']['cacheHits'] == 1, repeated
                done('exact-hit-keeps-built-work-and-guards')
                client.change(left, 'export func compute(string value) as int\n return 0\nend\n', 12)
                failed = checked(client, left, 12)
                assert not failed['ok'] and failed['count'] and not failed['semanticDefinitions'], failed
                client.change(left, bodies[0], 13)
                repaired = checked(client, left, 13)
                assert repaired['ok'] and not repaired['count'], repaired
                done('signature-change-error-and-repair-use-original-checker')
                at=bodies[0].index(' if local > 0 then\r\n')
                local=bodies[0][:at]+' int inserted = local+1\r\n'+bodies[0][at:]
                client.change(left,local,14)
                inserted=checked(client,left,14)
                assert inserted['ok'] and inserted['analysis']['semanticFunctionsReused']-repaired['analysis']['semanticFunctionsReused']>=6,inserted
                done('member-local-insertion-rebinds-unchanged-function-facts',analysis=inserted['analysis'])
                client.change(left,bodies[0],15)
                repaired=checked(client,left,15)
                assert repaired['ok'] and repaired['analysis']['semanticFunctionsReused']-inserted['analysis']['semanticFunctionsReused']>=6,repaired
                done('member-local-removal-preserves-current-cross-member-facts',analysis=repaired['analysis'])
                before = repaired['analysis']['semanticContextHits']
                client.send('textDocument/didClose', dict(textDocument=dict(uri=left.as_uri())))
                client.open(left, bodies[0], 1)
                reopened = checked(client, left, 1)
                assert reopened['ok'] and reopened['analysis']['semanticContextHits'] == before, reopened
                done('reopened-member-identity-fences-memo-baseline')
                assert not left.exists() and not right.exists()
            finally:
                client.stop()
                assert client.process.returncode == 0
        assert len(cases) == 16 and all(sha(p) == h for p, h in protected.items())
        report['status'] = 'passed'
    finally:
        (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(status='passed', cases=len(cases), output=str(output)), ensure_ascii=False))


if __name__ == '__main__':
    main()
