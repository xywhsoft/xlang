"""Prove actual stdio idle indexing reuses the complete foreground project check."""
from pathlib import Path
import argparse
import hashlib
import json
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize, checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in (server, Path(__file__))}
    report = dict(status='failed', scope=__doc__, protected=protected, cases=[])
    try:
        for opened in (False, True):
            directory = output/('opened' if opened else 'disk'); directory.mkdir()
            entry, helper = directory/'main.xl', directory/'helper.xl'
            body = 'import helper\nfunc main(...) as int\n return helper.value()\nend\n'
            library = 'class Payload\n int number\nend\nexport func value() as int\n return 1\nend\n'
            dirty = library.replace('number', 'amount').replace('return 1', 'return 2')
            entry.write_text(body, encoding='utf-8'); helper.write_text(library, encoding='utf-8')
            client = AutomaticClient(server, directory/'absent/xl.exe', directory)
            # Hold the initialized notification until the first foreground check.
            # No timer/CPU-speed assumption determines which actor warms first.
            original_send = client.send
            def send(method, *args, **kwargs):
                if method != 'initialized': return original_send(method, *args, **kwargs)
            client.send = send
            try:
                initialize(client, entry, [entry, helper], directory)
                client.send = original_send
                if opened: client.open(entry, body, 1)
                client.open(helper, library, 1)
                cold = checked(client, helper, 1)
                assert cold['ok'] and cold['analysis']['builds'] == 1 and not cold['analysis']['cacheHits'], cold
                client.send('initialized', {})
                def warm(completed):
                    until = time.monotonic()+30
                    while True:
                        value = client.call('xlang/workspaceSymbolStats')['background']
                        assert not value['failed'], value
                        if value['completed'] >= completed and not value['running'] and not value['pending']: return value
                        assert time.monotonic() < until, value
                        time.sleep(.01)
                first = warm(1)
                assert first['roots'] == 2 and first['captured'] == 1 and first['covered'] == 1, first
                assert first['analysisBuilds'] == 1 and first['analysisHits'] == 0 and first['analysisSnapshots'] == 1 and first['primaryFallbacks'] == 0, first
                repeated = checked(client, helper, 1)
                assert repeated['ok'] and repeated['analysis']['builds'] == 1 and repeated['analysis']['cacheHits'] == 1, repeated
                client.change(helper, dirty, 2)
                edited = checked(client, helper, 2)
                assert edited['ok'] and edited['analysis']['builds'] == 2, edited
                second = warm(2)
                assert second['captured'] == 1 and second['analysisBuilds'] == 2 and second['analysisSnapshots'] == 1 and second['primaryFallbacks'] == 0 and second['analysisHits'] == 1, second
                assert len(client.call('workspace/symbol', dict(query='Payload amount'))) == 1
                assert client.call('workspace/symbol', dict(query='Payload number')) == []
                report['cases'].append(dict(entryOpen=opened, passed=True, cold=cold['analysis'], first=first,
                    repeated=repeated['analysis'], edited=edited['analysis'], second=second))
            finally:
                client.send = original_send
                stderr = client.stop()
                (directory/'transcript.json').write_text(json.dumps(client.transcript, indent=2)+'\n', encoding='utf-8')
                (directory/'stderr.log').write_text(stderr, encoding='utf-8')
                assert client.process.returncode == 0 and not stderr
        for p, h in protected.items(): assert sha(p) == h, p
        report['status'] = 'passed'
    finally:
        (output/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('Workspace analysis reuse passed: disk/open entries, complete contexts, actual foreground cache hits, current imported overlays.')


if __name__ == '__main__': main()
