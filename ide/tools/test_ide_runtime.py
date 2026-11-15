"""Exercise publication interruption and shared-engine preservation."""
import argparse
import json
import os
from pathlib import Path
from publish_ide_runtime import publish
from ide_runtime_library import archive_members, runtime_basename


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    root = args.work.absolute()
    root.mkdir(parents=True, exist_ok=False)
    name = runtime_basename(Path(__file__).resolve().parents[1])
    rows = []
    for failure in (-1, 0, 1, 2):
        for existing_dll in (False, True):
            case = root / (str(failure) + '-' + str(existing_dll))
            runtime, inputs = case / 'runtime', case / 'inputs'
            runtime.mkdir(parents=True)
            inputs.mkdir()
            names = (name, 'xlang-ls.exe', 'xlang_ide.exe')
            sources = {}
            for n in names:
                sources[n] = inputs / n
                sources[n].write_bytes(('NEW-' + n).encode())
                if n != name or existing_dll:
                    (runtime / n).write_bytes(('OLD-' + n).encode())
            (runtime / 'xge.dll').write_bytes(b'ENGINE ABI OWNED BY OTHER PACKAGE')
            (runtime / 'settings.json').write_bytes(b'USER SETTINGS')
            original = {p.name: p.read_bytes() for p in runtime.iterdir()}
            calls = []

            def replace(source, target):
                calls.append(target.name)
                if len(calls) - 1 == failure:
                    raise PermissionError('Injected sharing violation')
                os.replace(source, target)

            try:
                result = publish(runtime, sources, case / 'receipt', replace)
                assert failure == -1 and result['status'] == 'passed'
                for n in names:
                    assert (runtime / n).read_bytes() == sources[n].read_bytes()
                for n in ('xge.dll', 'settings.json'):
                    assert (runtime / n).read_bytes() == original[n]
            except PermissionError:
                assert failure != -1
                assert {p.name: p.read_bytes() for p in runtime.iterdir()} == original
                result = json.loads((case / 'receipt/publication.json').read_text())
                assert result['status'] == 'rolled-back'
            assert not list(runtime.glob('.xide-publish-*'))
            rows.append(dict(failureAfter=failure, existingPrivateDll=existing_dll, status=result['status']))
    for invalid in (b'', b'!<arch>\ncorrupt', b'!<arch>\n' + b'x' * 60):
        try:
            archive_members(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid archive accepted')
    (root / 'result.json').write_text(json.dumps(dict(status='passed', cases=rows), indent=2) + '\n')
    print('PASS runtime publication: 8 success/interruption cases, engine/settings preserved')


if __name__ == '__main__':
    main()
