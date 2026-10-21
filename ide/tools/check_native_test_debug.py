"""Exercise the diagnostic runner against seven distinct native outcomes."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import uuid

from native_test_debug import observe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output')
    parser.add_argument('--gcc', default=shutil.which('gcc'))
    args = parser.parse_args()
    ide = Path(__file__).resolve().parent.parent
    root = Path(args.output).absolute() if args.output else ide/'build'/('native-debug-'+uuid.uuid4().hex[:8])
    root.mkdir(parents=True, exist_ok=False)
    if not args.gcc:
        parser.error('gcc is required to compile the native outcome fixture')
    files = [ide/'tests/native_debug_program.c', Path(__file__).resolve(),
             ide/'tools/native_test_debug.py']
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    before = {str(p): sha(p) for p in files}
    for p in files:
        shutil.copy2(p, root/p.name)
    exe = root/'native_debug_program.exe'
    command = [args.gcc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
               str(files[0]), '-o', str(exe)]
    p = subprocess.run(command, capture_output=True, timeout=30)
    (root/'build.stdout').write_bytes(p.stdout)
    (root/'build.stderr').write_bytes(p.stderr)
    if p.returncode:
        raise RuntimeError('Native diagnostic fixture compile failed; see '+str(root/'build.stderr'))
    checks = []
    for mode in ('success', 'exit', 'handled', 'unhandled', 'breakpoint', 'thread', 'timeout'):
        result = observe([exe, mode], root/mode, timeout=1 if mode == 'timeout' else 15)
        events = [json.loads(line) for line in (root/mode/'events.jsonl').read_text(encoding='utf8').splitlines()]
        exceptions = [x for x in events if x['kind'] == 1]
        assert sum(x['loaderBreakpointConsumed'] for x in exceptions) == 1, mode
        expected = {'success': 0, 'exit': 17, 'handled': 0, 'unhandled': 0xc0000005,
                    'breakpoint': 0x80000003, 'thread': 0}.get(mode)
        if expected is not None:
            assert result['exitCode'] == expected, (mode, result['exitHex'])
        if mode in ('unhandled', 'breakpoint'):
            code = '0xc0000005' if mode == 'unhandled' else '0x80000003'
            records = [x for x in exceptions if x['exception'] == code and not x['loaderBreakpointConsumed']]
            assert [x['firstChance'] for x in records] == [True, False], mode
            assert len(result['unhandledExceptions']) == 1 and len(result['minidumps']) == 1
            dump = result['minidumps'][0]
            assert dump['status'] == 'saved' and dump['bytes'] > 1024
            data = Path(dump['path']).read_bytes()
            assert data[:4] == b'MDMP'
            count, directory = struct.unpack_from('<II', data, 8)
            streams = {t: (size, at) for t, size, at in
                       [struct.unpack_from('<III', data, directory+12*i) for i in range(count)]}
            size, at = streams[6]
            assert size == 168 and struct.unpack_from('<I', data, at+8)[0] == expected
            context_size, context_at = struct.unpack_from('<II', data, at+160)
            assert context_size >= 256
            assert struct.unpack_from('<Q', data, context_at+248)[0] == int(dump['rip'], 16)
            assert struct.unpack_from('<Q', data, context_at+152)[0] == int(dump['rsp'], 16)
            assert struct.unpack_from('<I', data, at)[0] == result['unhandledExceptions'][0]['tid']
            if mode == 'unhandled':
                assert struct.unpack_from('<Q', data, at+48)[0] == 0x1234
        else:
            assert not result['unhandledExceptions'] and not result['minidumps'], mode
        if mode == 'handled':
            records = [x for x in exceptions if x['exception'] == '0xe0421234']
            assert len(records) == 1 and records[0]['firstChance']
            assert records[0]['continueStatus'] == '0x80010001'
        if mode == 'thread':
            assert any(x['kind'] == 4 and x['exitCode'] == 23 for x in events)
        assert result['timedOut'] == (mode == 'timeout')
        checks.append({'mode': mode, 'path': str(root/mode/'result.json'),
                       'sha256': sha(root/mode/'result.json'), 'exitCode': result['exitCode']})
        print('Native observer contract '+mode+' passed: '+result['exitHex'], flush=True)
    assert {str(p): sha(p) for p in files} == before
    evidence = {'status': 'passed', 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'inputs': before, 'buildCommand': command, 'fixtureSha256': sha(exe),
                'checks': checks, 'formalRelease': False,
                'scope': 'Windows x64 diagnostic runner; seven outcomes and independent minidump exception/context parsing.'}
    (root/'evidence.json').write_text(json.dumps(evidence, indent=2), encoding='utf8')
    print('Native observer evidence: '+str(root/'evidence.json'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
