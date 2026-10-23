"""Causal diagnostic overlay proofs over a real stdio connection."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
from check_lsp_diagnostic_dependencies import DependencyClient
from check_lsp_input_state import digest


class OverlayClient(DependencyClient):
    def __init__(self, *args):
        super().__init__(*args)
        self.serial = 0
        self.bindings = {}
        self.history = {0: {}}
        self.proofs = 0
        self.verify = False

    def send(self, method, params=None, id=None):
        if id is None and method in {
            'textDocument/didOpen', 'textDocument/didChange',
            'textDocument/didClose', 'textDocument/didSave',
            'workspace/didChangeWatchedFiles'
        }:
            item = (params or {}).get('textDocument', {})
            if method in {'textDocument/didOpen', 'textDocument/didChange'}:
                self.bindings[item['uri']] = item['version']
            elif method == 'textDocument/didClose':
                del self.bindings[item['uri']]
            self.serial += 1
            self.history[self.serial] = dict(self.bindings)
        return super().send(method, params, id)

    def rejected(self, method, params):
        # Deliberately invalid notifications must not advance the accepted watermark.
        return DependencyClient.send(self, method, params)

    def next(self):
        message = super().next()
        if self.verify and message.get('method') == 'textDocument/publishDiagnostics':
            p = message['params']
            proof = p['xlang']
            serial = proof['serial']
            assert isinstance(serial, str) and re.fullmatch(r'0|[1-9][0-9]*', serial), proof
            serial = int(serial)
            assert serial in self.history and serial <= self.serial, proof
            if proof['cleared']:
                assert 'overlays' not in proof, proof
            else:
                rows = proof['overlays']
                assert isinstance(rows, list) and len(rows) <= 64, proof
                actual = {}
                for row in rows:
                    assert set(row) == {'uri', 'version'} and row['uri'] not in actual, row
                    assert type(row['version']) is int and -(2**31) <= row['version'] < 2**31, row
                    actual[row['uri']] = row['version']
                assert actual == self.history[serial], (proof, self.history[serial])
                if proof['document']['origin'] == 'overlay':
                    assert actual[p['uri']] == p['version'], p
            self.proofs += 1
        return message


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in (
        server, Path(__file__),
        Path(__file__).with_name('check_lsp_diagnostic_dependencies.py'),
        Path(__file__).with_name('check_lsp_automatic_diagnostics.py'),
        Path(__file__).with_name('check_lsp_input_state.py'),
        Path(__file__).with_name('check_language_snapshots.py'))}
    clients, cases = [], []
    status = 'failed'
    good = 'func main(...) as int\n return 0\nend\n'
    bad = good.replace('return 0', 'return missingName')
    imported = 'import shared\nfunc main(...) as int\n return shared.value()\nend\n'
    shared = 'export func value() as int\n return "bad"\nend\n'
    options = dict(automaticDiagnostics=False, automaticDiagnosticsIfSupported=True,
                   checkOnSave=False, diagnosticDelay=0)
    caps = dict(textDocument=dict(publishDiagnostics=dict(versionSupport=True)),
                experimental=dict(diagnosticInputProof=True,
                                  diagnosticDependencyProof=True,
                                  diagnosticOverlayProof=True))

    def done(name, **data):
        cases.append(dict(name=name, passed=True, **data))

    def create(name, initialize=True, capabilities=None, init=None):
        folder = root / name
        folder.mkdir()
        c = OverlayClient(server, folder / 'absent/xl.exe', folder)
        clients.append(c)
        if initialize:
            c.send('initialize', dict(capabilities=capabilities or caps,
                                     initializationOptions=init or options), 1)
            reply, _ = c.reply(1)
            assert reply['result']['capabilities']['experimental']['diagnosticOverlayProof'] is True, reply
            assert reply['result']['capabilities']['experimental']['automaticDiagnostics'] is True, reply
            c.verify = True
            c.send('initialized', {})
        return c, folder

    def receipt(c):
        result = c.call('xlang/inputState')
        assert set(result) == {'schema', 'generation', 'documents', 'sha256', 'serial'}, result
        assert result['schema'] == 1 and result['serial'] == str(c.serial), result
        assert result['documents'] == len(c.bindings) and result['sha256'] == digest(c.bindings), result
        return result

    try:
        c, folder = create('independent-中文😀')
        a, b = folder / '入口😀.xl', folder / 'b.xl'
        c.open(a, bad, 1)
        c.quiet()
        first = copy.deepcopy(c.latest[a.as_uri()])
        assert first['diagnostics'] and first['xlang']['serial'] == '1', first
        done('conditional-option-enables-native-automatic-checks-without-executor')
        c.open(b, bad, 2)
        c.quiet()
        first = copy.deepcopy(c.latest[a.as_uri()])
        c.change(b, good, 3)
        c.quiet()
        assert c.latest[a.as_uri()] == first and not c.latest[b.as_uri()]['diagnostics']
        assert receipt(c)['serial'] == '3'
        done('independent-file-keeps-older-valid-proof-instead-of-global-generation-dropping')
        c.rejected('textDocument/didChange', dict(textDocument=dict(uri=b.as_uri(), version=2),
                                                contentChanges=[dict(text=bad)]))
        c.rejected('textDocument/didOpen', dict(textDocument=dict(uri=b.as_uri(), version=9,
                                                               text=bad, languageId='xlang')))
        c.rejected('textDocument/didSave', dict(textDocument=dict(uri=b.as_uri()), text=bad))
        c.rejected('textDocument/didClose', dict(textDocument=dict(uri=(folder/'not-open.xl').as_uri())))
        c.rejected('workspace/didChangeWatchedFiles', dict(changes=[]))
        assert receipt(c)['serial'] == '3'
        done('stale-change-duplicate-open-invalid-save-close-and-empty-watch-do-not-advance-watermark')
        c.send('textDocument/didSave', dict(textDocument=dict(uri=b.as_uri()), text=good))
        c.send('workspace/didChangeWatchedFiles', dict(changes=[dict(uri=b.as_uri(), type=2)]))
        c.quiet()
        assert receipt(c)['serial'] == '5'
        done('save-and-watched-file-advance-watermark-without-requiring-version-change')
        old = c.latest[a.as_uri()]['xlang']['serial']
        c.send('textDocument/didClose', dict(textDocument=dict(uri=a.as_uri())))
        c.open(a, bad, 4)
        c.change(a, bad+'\n', 5)
        c.send('textDocument/didClose', dict(textDocument=dict(uri=b.as_uri())))
        c.quiet()
        assert c.latest[a.as_uri()]['version'] == 5 and c.latest[a.as_uri()]['diagnostics']
        assert int(c.latest[a.as_uri()]['xlang']['serial']) > int(old)
        assert receipt(c)['serial'] == '9'
        done('rapid-open-change-close-and-reopen-proofs-match-exact-accepted-prefix')
        c.stop()

        c, folder = create('dependency-disk')
        entry, dep = folder/'entry.xl', folder/'shared.xl'
        dep.write_text(shared, encoding='utf-8')
        c.open(entry, imported, 10)
        c.quiet()
        assert c.latest[dep.as_uri()]['diagnostics']
        before = receipt(c)
        before_invalidations = c.stats()['lastAnalysis']['invalidations']
        old_document = copy.deepcopy(c.latest[dep.as_uri()]['xlang']['document'])
        replacement = folder/'replacement.tmp'
        replacement.write_bytes(dep.read_bytes())
        os.replace(replacement, dep)
        c.wait_for(lambda s: s.get('lastAnalysis', {}).get('invalidations', 0) > before_invalidations
                   and c.latest[dep.as_uri()]['xlang'].get('document') != old_document
                   and bool(c.latest[dep.as_uri()]['diagnostics'])
                   and not s['checking'] and not s['pendingReport'])
        after = receipt(c)
        assert after['serial'] == before['serial']
        done('external-same-byte-disk-replacement-renews-file-identity-without-client-watermark')
        c.open(dep, shared, 11)
        c.quiet()
        proof = c.latest[dep.as_uri()]['xlang']
        assert proof['document']['origin'] == 'overlay' and len(proof['overlays']) == 2, proof
        c.change(dep, shared.replace('"bad"', '7'), 12)
        c.quiet()
        assert not c.latest[dep.as_uri()]['diagnostics']
        c.send('textDocument/didClose', dict(textDocument=dict(uri=dep.as_uri())))
        c.quiet()
        assert c.latest[dep.as_uri()]['diagnostics'] and c.latest[dep.as_uri()]['xlang']['document']['origin'] == 'disk'
        assert c.latest[dep.as_uri()]['xlang']['overlays'] == [dict(uri=entry.as_uri(), version=10)]
        done('dependency-overlay-edit-and-close-to-disk-carry-current-complete-overlay-state')
        c.stop()

        c, folder = create('navigation-after-disk-withdrawal', init=dict(options, diagnosticDelay=250))
        entry, dep = folder/'entry.xl', folder/'shared.xl'
        dep.write_text('export func value() as int\n return 7\nend\n', encoding='utf-8')
        body = 'import shared\nfunc main(...) as int\n auto result = shared.value()\n return 0\nend\n'
        c.open(entry, body, 1)
        c.quiet()
        params = dict(textDocument=dict(uri=entry.as_uri()), version=1,
                      position=dict(line=2, character=body.splitlines()[2].index('value')))
        hover = c.call('textDocument/hover', params)
        assert 'value() as int' in hover['contents']['value'], hover
        at = len(c.events)
        replacement = folder/'replacement.tmp'
        replacement.write_text('export func value() as string\n return "new type"\nend\n', encoding='utf-8')
        os.replace(replacement, dep)
        c.wait_for(lambda s: any(e['uri'] == entry.as_uri() and e['xlang']['cleared']
                                for e in c.events[at:]))
        for attempt in range(4):
            c.request += 1
            c.send('textDocument/hover', params, c.request)
            reply, _ = c.reply(c.request)
            if 'error' not in reply:
                assert 'value() as string' in reply['result']['contents']['value'], reply
                break
            assert reply['error']['code'] == -32801, reply
            c.quiet()
        else:
            raise AssertionError('Current semantic navigation did not recover after disk withdrawal')
        done('diagnostic-observation-revokes-old-native-navigation-guard-before-reanalysis')
        c.stop()

        invalid = []
        for key, value in [('diagnosticOverlayProof', 'true'), ('diagnosticOverlayProof', 1),
                           ('diagnosticDependencyProof', False), ('diagnosticInputProof', False)]:
            cp = copy.deepcopy(caps)
            cp['experimental'][key] = value
            invalid.append((cp, options))
        cp = copy.deepcopy(caps)
        cp['textDocument']['publishDiagnostics']['versionSupport'] = False
        invalid.append((cp, options))
        invalid.append((caps, dict(options, automaticDiagnosticsIfSupported='yes')))
        invalid.append((caps, dict(options, diagnosticsBackend='cli')))
        cp = copy.deepcopy(caps)
        cp['experimental']['diagnosticOverlayProof'] = False
        invalid.append((cp, options))
        for i, (cp, op) in enumerate(invalid):
            c, folder = create('invalid-'+str(i), initialize=False)
            c.send('initialize', dict(capabilities=cp, initializationOptions=op), 1)
            reply, _ = c.reply(1)
            assert reply['error']['code'] == -32602, reply
            c.send('initialize', dict(capabilities=caps, initializationOptions=options), 2)
            reply, _ = c.reply(2)
            assert reply['result']['capabilities']['experimental']['automaticDiagnostics'] is True, reply
            c.verify = True
            c.send('initialized', {})
            assert receipt(c)['serial'] == '0'
            c.stop()
        done('strict-types-required-capability-chain-native-backend-and-retryable-initialization', variants=len(invalid))
        assert sum(c.proofs for c in clients) >= 12
        assert all(sha(p) == value for p, value in protected.items())
        done('tools-server-bytes-preserved-and-all-publications-validated', publications=sum(c.proofs for c in clients))
        status = 'passed'
    finally:
        for i, c in enumerate(clients):
            stderr = c.stop()
            (root/f'server-{i}.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2), encoding='utf-8')
            (root/f'server-{i}.stderr').write_text(stderr, encoding='utf-8')
        result = dict(status=status, scope='causal client-input watermark and complete overlay bindings; '
                      'not semantic incrementality or end-to-end latency', protected=protected, cases=cases)
        (root/'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(dict(status=status, cases=len(cases), proof=str(root/'result.json'))))


if __name__ == '__main__':
    main()
