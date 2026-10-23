"""Actual configured stdio checks/cursors/navigation, disk roots and fencing.

Uses explicit immutable context wire values, not a second project parser.
The IDE model-to-wire path is verified by the GUI fixture separately.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import time
from check_language_snapshots import Client
from check_language_definitions import position

LISTS = ['members', 'cSources', 'resources', 'includes', 'libraryPaths', 'linkLibraries',
         'cSystemIncludes', 'cForcedIncludes', 'cDefines', 'cUndefines']
PROFILE = 'xlang-ls/compiler-xrt-v1'


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    root = output / '工程配置😀'
    root.mkdir()
    entry, helper = root/'main.xl', root/'helper.xl'
    entry_text = '\ufeffimport helper\r\nfunc main(...) as int\r\n return helper.value()\r\nend\r\n'
    entry.write_bytes(entry_text.encode())
    helper_text = 'import context_package as cp\nexport func value() as int\n return cp.value()\nend\n'
    compiler = output/'absent-executor/xl.exe'
    projects = []
    api_paths = []
    for index, name in enumerate(['A', 'B']):
        folder = root/name
        package = folder/'packages/context_package'
        package.mkdir(parents=True)
        manifest = package/'xlang.package.json'
        manifest.write_text(json.dumps(dict(format='xlang-package-1', name='context_package',
            version='1.0.0', abiVersion='1', kind='source', artifact=dict(kind='source', xlSources=['api.xl']),
            dependencies=[], links=[], loadlibs=[])), encoding='utf8')
        api = package/'api.xl'
        api.write_text('export func value() as '+('string\n return "B"' if index else 'int\n return 7')+
                       '\nend\nexport func '+('beta' if index else 'alpha')+'() as int\n return 1\nend\n', encoding='utf8')
        api_paths.append(api)
        lists = {key: [] for key in LISTS}
        lists['members'] = [entry.as_posix(), helper.as_posix()]
        projects.append(dict(schema=1, kind='project', project=(folder/'project.xlp').as_posix(),
            configuration='release' if index else 'debug', entry=entry.as_posix(), directory=folder.as_posix(),
            analysisProfile=PROFILE, targetKind='exe', packagePath='packages', lists=lists))
    protected = {str(p): sha(p) for p in [server, Path(__file__), entry, *api_paths, *root.rglob('xlang.package.json')]}
    previous = os.environ.get('XLANG_PACKAGE_PATH')
    os.environ['XLANG_PACKAGE_PATH'] = str(root/'wrong-process-environment')
    clients, cases = [], []
    report = dict(status='failed', cases=cases, protected=protected,
                  scope='Real native configured protocol; unopened BOM/CRLF project entry, overlays, unchanged versions, context selection and source guards. No scale/UI/semantic-incrementality acceptance.')
    rid = 100

    def packet(epoch, selected=0, contexts=None):
        return dict(epoch=epoch, catalog=dict(schema=1, projects=copy.deepcopy(projects if contexts is None else contexts),
            selectedProject='' if selected is None else projects[selected]['project'], packagePath='packages'))

    def call(c, method, params, code=None):
        nonlocal rid
        until = time.monotonic()+12
        while True:
            rid += 1
            c.send(method, params, rid)
            reply, _ = c.reply(rid)
            if code is None and method == 'xlang/check' and reply.get('error', {}).get('code') == -32001:
                assert time.monotonic() < until, reply
                time.sleep(.01)
                continue
            break
        if code is not None:
            assert reply['error']['code'] == code, reply
            return reply['error']
        assert 'error' not in reply, reply
        return reply['result']

    def check(c, epoch, selected, accepted, version=7, imports=True):
        value = call(c, 'xlang/check', dict(textDocument=dict(uri=helper.as_uri()), version=version))
        assert value['ok'] == accepted and value['version'] == version, value
        stamp = value['analysisContext']
        assert stamp['epoch'] == epoch and stamp['project'] == projects[selected]['project'] and stamp['configuration'] == projects[selected]['configuration'], value
        paths = {Path(row['path']) for row in value['documents']}
        assert entry in paths and helper in paths and api_paths[1-selected] not in paths, value
        if imports:
            assert api_paths[selected] in paths, value
        else:
            assert value['phase'] == 'parse' and api_paths[selected] not in paths, value
        return value

    def query(c, text, version, epoch, selected, expected):
        value = call(c, 'textDocument/completion', dict(textDocument=dict(uri=helper.as_uri()), version=version,
                     position=position(text, text.index(' cp.')+4)))
        assert value['available'] and value['version'] == version and {i['label'] for i in value['items']} == expected, value
        assert value['analysisContext']['epoch'] == epoch and value['analysisContext']['configuration'] == projects[selected]['configuration'], value
        assert all(i['data']['declaration']['uri'] == api_paths[selected].as_uri() for i in value['items']), value
        return value

    def done(name, **data):
        cases.append(dict(name=name, passed=True, **data))

    try:
        c = Client(server, compiler, root)
        clients.append(c)
        c.send('initialize', dict(capabilities={}, initializationOptions=dict(automaticDiagnostics=False,
               checkOnSave=False, analysisConfiguration=packet(1))), 1)
        reply, _ = c.reply(1)
        caps = reply['result']['capabilities']['experimental']
        assert caps['analysisConfiguration'] == 1 and caps['analysisProfile'] == PROFILE and caps['retainedDiskEntry'] == 1 and caps['retainedEntryParse'] == 1, reply
        c.send('initialized', {})
        process = c.process.pid
        c.open(helper, helper_text, 7)
        value = check(c, 1, 0, True)
        root_report = next(row for row in value['documents'] if Path(row['path']) == entry)
        assert root_report['origin'] == 'disk' and 'version' not in root_report and value['retainedEntry'] and not value['entryOpen'] and value['analysis']['builds'] == 1, value
        done('unopened-bom-crlf-root-uses-unsaved-member-and-actual-project-package', result=value)
        again = check(c, 1, 0, True)
        assert again['analysis']['builds'] == value['analysis']['builds'] and again['analysis']['cacheHits'] == value['analysis']['cacheHits']+1, again
        done('unopened-project-check-reuses-real-validated-analysis')
        at = helper_text.index('value()', helper_text.index('return'))
        definition = call(c, 'textDocument/definition', dict(textDocument=dict(uri=helper.as_uri()), position=position(helper_text, at)))
        assert definition['uri'] == api_paths[0].as_uri(), definition
        done('standard-definition-from-member-uses-project-root')
        configured = call(c, 'xlang/configureAnalysis', packet(2, 1))
        assert configured['epoch'] == 2 and c.process.pid == process, configured
        value = check(c, 2, 1, False)
        assert value['count'] > 0, value
        done('same-physical-entry-body-version-switches-project-semantics-without-restart', result=value)
        for invalid in [packet(2), dict(epoch=3, catalog=dict(schema=1, projects=[], selectedProject='', packagePath=None, unknown=True))]:
            call(c, 'xlang/configureAnalysis', invalid, -32602)
        check(c, 2, 1, False)
        done('stale-and-invalid-configurations-reject-atomically')
        call(c, 'xlang/configureAnalysis', packet(3, None))
        call(c, 'xlang/check', dict(textDocument=dict(uri=helper.as_uri()), version=7), -32602)
        call(c, 'textDocument/documentSymbol', dict(textDocument=dict(uri=helper.as_uri())), -32602)
        done('shared-member-needs-selected-project')
        unsupported = copy.deepcopy(projects)
        unsupported[0]['lists']['includes'] = ['C:/explicit-unsupported-option']
        call(c, 'xlang/configureAnalysis', packet(4, 0, unsupported))
        error = call(c, 'xlang/check', dict(textDocument=dict(uri=helper.as_uri()), version=7), -32602)
        assert 'build options' in error['message'], error
        done('native-build-options-reject-without-dropping-or-cli-fallback')
        c.send('workspace/didChangeConfiguration', dict(settings=dict(xlang=dict(analysisConfiguration=packet(5, 0)))))
        check(c, 5, 0, True)
        done('standard-did-change-configuration-routes-same-catalog')
        cursor = helper_text.replace(' return cp.value()', ' cp.\n return 0')
        c.change(helper, cursor, 8)
        value = query(c, cursor, 8, 5, 0, {'value', 'alpha'})
        assert value['retainedEntry'] and not value['entryOpen'] and value['analysis']['builds'] == 1, value
        done('disk-root-namespace-completion-has-real-signatures-and-retained-actor-counters')
        again = query(c, cursor, 8, 5, 0, {'value', 'alpha'})
        assert not again['entryOpen'] and again['analysis']['builds'] == value['analysis']['builds'] and again['analysis']['cacheHits'] == value['analysis']['cacheHits']+1, again
        done('unopened-project-completion-reuses-real-validated-analysis')
        call(c, 'xlang/configureAnalysis', packet(6, 1))
        value = query(c, cursor, 8, 6, 1, {'value', 'beta'})
        disk_builds = value['analysis']['builds']
        done('unchanged-unsaved-cursor-switches-public-export-and-declaration')
        c.open(entry, entry_text, 11)
        value = query(c, cursor, 8, 6, 1, {'value', 'beta'})
        assert value['retainedEntry'] and value['entryOpen'] and value['analysis']['builds'] == disk_builds+1, value
        again = query(c, cursor, 8, 6, 1, {'value', 'beta'})
        assert again['analysis']['cacheHits'] == value['analysis']['cacheHits']+1, again
        done('open-project-root-uses-retained-cursor-actor-and-real-hit-counters')
        c.send('textDocument/didClose', dict(textDocument=dict(uri=entry.as_uri())))
        value = query(c, cursor, 8, 6, 1, {'value', 'beta'})
        assert value['retainedEntry'] and not value['entryOpen'] and value['analysis']['builds'] == disk_builds+2, value
        done('closing-root-restores-verified-retained-disk-route')
        old_query = value['queryId']
        replacement = entry.with_suffix('.replacement')
        replacement.write_bytes(entry.read_bytes())
        os.replace(replacement, entry)
        while True:
            message = c.next()
            if message.get('method') == 'xlang/completionInvalidated' and message['params']['queryId'] == old_query:
                break
        renewed = query(c, cursor, 8, 6, 1, {'value', 'beta'})
        assert renewed['analysis']['builds'] == value['analysis']['builds']+1 and renewed['analysis']['invalidations'] == value['analysis']['invalidations']+1, renewed
        done('same-byte-disk-root-identity-change-invalidates-completion')
        original_api = api_paths[1].read_bytes()
        try:
            api_paths[1].write_bytes(original_api.replace(b'beta', b'delta'))
            changed = query(c, cursor, 8, 6, 1, {'value', 'delta'})
            assert changed['analysis']['builds'] == renewed['analysis']['builds']+1 and changed['analysis']['invalidations'] == renewed['analysis']['invalidations']+1, changed
        finally:
            api_paths[1].write_bytes(original_api)
        restored = query(c, cursor, 8, 6, 1, {'value', 'beta'})
        assert restored['analysis']['builds'] == changed['analysis']['builds']+1 and restored['analysis']['invalidations'] == changed['analysis']['invalidations']+1, restored
        done('retained-disk-completion-revalidates-real-package-change-and-restoration')
        standalone = root/'unowned/main.xl'
        standalone.parent.mkdir()
        standalone_text = 'func main(...) as int\n int answer = 1\n return ans\nend\n'
        c.open(standalone, standalone_text, 1)
        value = call(c, 'textDocument/completion', dict(textDocument=dict(uri=standalone.as_uri()), version=1, position=position(standalone_text, standalone_text.index('ans\n')+3)))
        assert value['available'] and {i['label'] for i in value['items']} == {'answer'} and not value['analysisContext']['project'], value
        assert Path(value['analysisContext']['directory']) == standalone.parent, value
        done('unowned-file-is-independent-of-workspace-and-uses-own-directory')
        c.change(helper, helper_text, 9)
        bulk = 'import helper\n' + ''.join(f'func pending{i}() as int\n return {i}\nend\n' for i in range(6000)) + 'func main(...) as int\n return helper.value()\nend\n'
        c.open(entry, bulk, 12)
        rid += 1
        check_id = rid
        rid += 1
        config_id = rid
        batch = [dict(jsonrpc='2.0', method='xlang/check', id=check_id, params=dict(textDocument=dict(uri=helper.as_uri()), version=9)),
                 dict(jsonrpc='2.0', method='xlang/configureAnalysis', id=config_id, params=packet(7, 0))]
        frames = []
        for message in batch:
            c.transcript.append(dict(send=message))
            encoded = json.dumps(message, ensure_ascii=False, separators=(',', ':')).encode()
            frames.append(f'Content-Length: {len(encoded)}\r\n\r\n'.encode()+encoded)
        began = time.perf_counter()
        c.process.stdin.write(b''.join(frames))
        c.process.stdin.flush()
        responses = {}
        while len(responses) < 2:
            message = c.next()
            if message.get('id') in [check_id, config_id]:
                responses[message['id']] = message
        assert responses[check_id]['error']['code'] == -32801 and responses[config_id]['result']['epoch'] == 7, responses
        assert c.process.pid == process
        done('in-flight-native-work-cancels-on-configuration-without-owner-join', observedMs=(time.perf_counter()-began)*1000)
        value = check(c, 7, 0, True, 9)
        assert value['retainedEntry']
        alternate = copy.deepcopy(projects)
        alternate[0]['configuration'] = 'debug-2'
        call(c, 'xlang/configureAnalysis', packet(8, 0, alternate))
        value = call(c, 'xlang/check', dict(textDocument=dict(uri=helper.as_uri()), version=9))
        assert value['ok'] and value['analysisContext']['epoch'] == 8 and value['analysisContext']['configuration'] == 'debug-2', value
        done('same-project-configuration-identity-is-fenced-at-unchanged-document-versions')
        before = value['analysis']
        call(c, 'xlang/configureAnalysis', packet(9, 0, alternate))
        value = call(c, 'xlang/check', dict(textDocument=dict(uri=helper.as_uri()), version=9))
        assert value['analysisContext']['epoch'] == 9 and value['analysis']['builds'] == before['builds'] and value['analysis']['cacheHits'] == before['cacheHits']+1, value
        done('equal-context-replacement-fences-publication-without-rechecking-semantics')
        # Independent real process: the entry stays closed, member bytes change
        # on every request. These are semantic rebuilds, not exact-result hits.
        second = Client(server, compiler, root)
        clients.append(second)
        second.send('initialize', dict(capabilities={}, initializationOptions=dict(automaticDiagnostics=False,
                    checkOnSave=False, analysisConfiguration=packet(1))), 1)
        reply, _ = second.reply(1)
        assert reply['result']['capabilities']['experimental']['retainedEntryParse'] == 1, reply
        second.send('initialized', {})
        second.open(helper, cursor, 1)
        value = query(second, cursor, 1, 1, 0, {'value', 'alpha'})
        work = value['analysis']['lastBuildEntryParse']
        assert not value['entryOpen'] and work['parsed'] == 1 and work['reused'] == 0 and work['bytes'] > 0, value
        for version in range(2, 18):
            edited = cursor+f'// actual member edit {version}\n'
            second.change(helper, edited, version)
            value = query(second, edited, version, 1, 0, {'value', 'alpha'})
            work = value['analysis']['lastBuildEntryParse']
            assert value['analysis']['builds'] == version and value['analysis']['cacheHits'] == 0 and work['parsed'] == 0 and work['reused'] == 1, value
            assert all(i['detail'] == 'func() as int' for i in value['items']), value
        done('member-edits-rebuild-semantics-and-reuse-only-unchanged-disk-entry-parse')
        original_entry = entry.read_bytes()
        try:
            replacement = entry.with_suffix('.entry-cache-replacement')
            replacement.write_bytes(original_entry)
            os.replace(replacement, entry)
            value = query(second, edited, version, 1, 0, {'value', 'alpha'})
            assert value['analysis']['builds'] == 18 and value['analysis']['lastBuildEntryParse']['reused'] == 1, value
            entry.write_bytes(original_entry+b'\r\n// changed entry source\r\n')
            value = query(second, edited, version, 1, 0, {'value', 'alpha'})
            assert value['analysis']['builds'] == 19 and value['analysis']['lastBuildEntryParse']['parsed'] == 1 and value['analysis']['lastBuildEntryParse']['reused'] == 0, value
        finally:
            entry.write_bytes(original_entry)
        done('entry-identity-recaptures-proof-and-entry-body-change-reparses')
        call(second, 'xlang/configureAnalysis', packet(2, 1))
        value = query(second, edited, version, 2, 1, {'value', 'beta'})
        assert value['analysis']['lastBuildEntryParse']['parsed'] == 1 and any(i['label'] == 'value' and i['detail'] == 'func() as string' for i in value['items']), value
        done('entry-parse-context-change-keeps-real-package-types-and-declarations')
        again = query(second, edited, version, 2, 1, {'value', 'beta'})
        assert again['analysis']['builds'] == value['analysis']['builds'] and again['analysis']['cacheHits'] == value['analysis']['cacheHits']+1 and again['analysis']['lastBuildEntryParse'] == value['analysis']['lastBuildEntryParse'], again
        done('exact-result-hit-preserves-last-build-work-without-claiming-new-parse')
        second.open(entry, entry_text, 1)
        value = query(second, edited, version, 2, 1, {'value', 'beta'})
        assert value['entryOpen'] and value['analysis']['lastBuildEntryParse']['parsed'] == 1 and value['analysis']['lastBuildEntryParse']['bytes'] > 0, value
        builds = value['analysis']['builds']
        edited += '// changed member with opened entry\n'
        version += 1
        second.change(helper, edited, version)
        value = query(second, edited, version, 2, 1, {'value', 'beta'})
        assert value['analysis']['builds'] == builds+1 and value['analysis']['lastBuildEntryParse']['reused'] == 1 and value['analysis']['lastBuildEntryParse']['parsed'] == 0, value
        done('canonical-open-entry-also-reuses-ordinary-parse-after-member-edit')
        second.send('textDocument/didClose', dict(textDocument=dict(uri=entry.as_uri())))
        value = query(second, edited, version, 2, 1, {'value', 'beta'})
        assert not value['entryOpen'] and value['analysis']['lastBuildEntryParse']['bytes'] > 0 and value['analysis']['builds'] == builds+2, value
        done('closing-entry-restores-real-disk-proof-and-eligible-entry-cache')
        # Separate check actor: changed members must run semantics and IR anew,
        # while the unchanged closed entry may copy only its ordinary AST.
        third = Client(server, compiler, root)
        clients.append(third)
        third.send('initialize', dict(capabilities={}, initializationOptions=dict(automaticDiagnostics=False,
                   checkOnSave=False, analysisConfiguration=packet(1))), 1)
        reply, _ = third.reply(1)
        third.send('initialized', {})
        third.open(helper, helper_text, 1)
        value = check(third, 1, 0, True, 1)
        work = value['analysis']['lastBuildEntryParse']
        assert work['parsed'] == 1 and not work['reused'] and work['bytes'] > 0, value
        for version in range(2, 18):
            edited = helper_text+f'// real checked member edit {version} 中文😀\n'
            third.change(helper, edited, version)
            value = check(third, 1, 0, True, version)
            work = value['analysis']['lastBuildEntryParse']
            assert value['analysis']['builds'] == version and not value['analysis']['cacheHits'] and not work['parsed'] and work['reused'] == 1, value
        done('static-check-member-edits-rebuild-facts-and-reuse-ordinary-disk-entry')
        again = check(third, 1, 0, True, version)
        assert again['analysis']['builds'] == value['analysis']['builds'] and again['analysis']['cacheHits'] == value['analysis']['cacheHits']+1 and again['analysis']['lastBuildEntryParse'] == value['analysis']['lastBuildEntryParse'], again
        done('static-check-exact-hit-retains-last-build-parse-work')
        original_entry = entry.read_bytes()
        try:
            replacement = entry.with_suffix('.check-cache-replacement')
            replacement.write_bytes(original_entry)
            os.replace(replacement, entry)
            value = check(third, 1, 0, True, version)
            assert value['analysis']['builds'] == 18 and value['analysis']['lastBuildEntryParse']['reused'] == 1, value
            modified = original_entry+b'\r\n// checked entry source change\r\n'
            entry.write_bytes(modified)
            value = check(third, 1, 0, True, version)
            assert value['analysis']['builds'] == 19 and value['analysis']['lastBuildEntryParse']['parsed'] == 1 and not value['analysis']['lastBuildEntryParse']['reused'], value
            entry.write_text('import helper\nfunc main(\n', encoding='utf8')
            value = check(third, 1, 0, False, version, imports=False)
            assert value['phase'] == 'parse' and value['count'] and value['analysis']['lastBuildEntryParse']['parsed'] == 1, value
            entry.write_bytes(modified)
            value = check(third, 1, 0, True, version)
            assert value['analysis']['lastBuildEntryParse']['reused'] == 1, value
        finally:
            entry.write_bytes(original_entry)
        value = check(third, 1, 0, True, version)
        assert value['analysis']['lastBuildEntryParse']['parsed'] == 1, value
        done('static-check-entry-identity-body-and-syntax-errors-keep-real-proofs')
        original_api = api_paths[0].read_bytes()
        try:
            api_paths[0].write_text('export func value() as string\n return "new"\nend\n', encoding='utf8')
            value = check(third, 1, 0, False, version)
            assert value['count'] and value['phase'] == 'sem' and value['analysis']['lastBuildEntryParse']['reused'] == 1 and not value['analysis']['lastBuildEntryParse']['parsed'], value
        finally:
            api_paths[0].write_bytes(original_api)
        value = check(third, 1, 0, True, version)
        assert value['analysis']['lastBuildEntryParse']['reused'] == 1, value
        done('static-check-package-type-change-reruns-semantic-and-ir-validation')
        call(third, 'xlang/configureAnalysis', packet(2, 1))
        value = check(third, 2, 1, False, version)
        assert value['analysis']['lastBuildEntryParse']['parsed'] == 1 and not value['analysis']['lastBuildEntryParse']['reused'], value
        call(third, 'xlang/configureAnalysis', packet(3, 0))
        value = check(third, 3, 0, True, version)
        assert value['analysis']['lastBuildEntryParse']['parsed'] == 1, value
        done('static-check-full-context-change-reparses-before-current-package-checks')
        third.open(entry, entry_text, 1)
        value = check(third, 3, 0, True, version)
        work = value['analysis']['lastBuildEntryParse']
        assert value['entryOpen'] and not work['parsed'] and not work['reused'] and value['analysis']['entryFullParses'] > 0, value
        previous_builds = value['analysis']['builds']
        version += 1
        edited += '// opened-root member change\n'
        third.change(helper, edited, version)
        value = check(third, 3, 0, True, version)
        assert value['analysis']['builds'] == previous_builds+1 and value['analysis']['entrySyntaxHits'] > 0 and not value['analysis']['lastBuildEntryParse']['reused'], value
        third.send('textDocument/didClose', dict(textDocument=dict(uri=entry.as_uri())))
        value = check(third, 3, 0, True, version)
        assert not value['entryOpen'] and value['analysis']['lastBuildEntryParse']['reused'] == 1, value
        done('static-check-open-entry-keeps-chunk-syntax-and-close-restores-disk-cache')
        assert all(sha(p) == digest for p, digest in protected.items()) and not helper.exists() and not compiler.exists()
        done('unsaved-member-not-written-and-source-tools-unchanged')
        report['status'] = 'passed'
    finally:
        for c in clients:
            c.stop()
        if previous is None:
            os.environ.pop('XLANG_PACKAGE_PATH', None)
        else:
            os.environ['XLANG_PACKAGE_PATH'] = previous
        report['exitCodes'] = [c.process.returncode for c in clients]
        report['transcripts'] = [c.transcript for c in clients]
        report['stderr'] = [c.stderr for c in clients]
        if any(report['exitCodes']):
            report['status'] = 'failed'
        (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    assert report['status'] == 'passed'
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(output/'result.json'))))


if __name__ == '__main__':
    main()
