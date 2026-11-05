"""Run bounded real workbench startup; --help alone is not a startup test."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(runtime, output):
    runtime, output = runtime.resolve(), output.resolve()
    if output.is_relative_to(runtime):
        raise ValueError('Keep validation artifacts outside the runtime directory')
    output.mkdir(parents=True, exist_ok=False)
    executable = runtime/'xlang_ide.exe'
    protected = [executable, runtime/'xge.dll', runtime/'xlang-ls.exe']
    settings = runtime/'option/settings.json'
    if settings.exists():
        protected.append(settings)
    inputs = {str(path): sha(path) for path in protected}
    copied = output/'copied-settings.json'
    copied.write_bytes(settings.read_bytes() if settings.exists() else b'{"format":"xlang-ide-settings-1"}')
    malformed = output/'malformed-settings.json'
    malformed.write_bytes(b'{broken')
    report = dict(complete=False, passed=False, inputs=inputs, rows=[],
                  scope='Real hidden window/workbench creation, rendering and isolated session restore; no user setting/session writes')

    def save():
        (output/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    cases = [('default', ['--no-session']),
             ('fresh-session', ['--settings', str(copied)]),
             ('restored-session', ['--settings', str(copied)]),
             ('malformed-preferences', ['--settings', str(malformed)])]
    save()
    for name, extra in cases:
        image = output/(name+'.png')
        command = [str(executable), '--hidden', '--frames', '4', '--no-auto-diagnostics',
                   '--session-root', str(output/('malformed-session' if name=='malformed-preferences' else 'session')),
                   '--diagnostic-root', str(output/'diagnostics'), '--screenshot', str(image), *extra]
        started = time.monotonic()
        process = subprocess.run(command, cwd=runtime, capture_output=True, timeout=60)
        log = output/(name+'.log')
        log.write_bytes(process.stdout+process.stderr)
        rendered = b'rendered 4 frames' in process.stdout+process.stderr
        captured = image.is_file() and image.read_bytes()[:8]==b'\x89PNG\r\n\x1a\n'
        report['rows'].append(dict(name=name, command=command, exitCode=process.returncode,
                                   seconds=round(time.monotonic()-started, 3),
                                   rendered=rendered, screenshot=captured, logSha256=sha(log)))
        save()
        if process.returncode or not rendered or not captured:
            raise RuntimeError('Workbench startup failed; see '+str(log))
    report['inputsUnchanged'] = all(sha(Path(path))==digest for path, digest in inputs.items())
    report['malformedPreferencesPreserved'] = malformed.read_bytes()==b'{broken'
    report['complete'] = True
    report['passed'] = report['inputsUnchanged'] and report['malformedPreferencesPreserved']
    save()
    if not report['passed']:
        raise RuntimeError('A protected input changed during startup checks')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=Path(__file__).resolve().parents[2]/'release/win_x64')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    report = check(args.runtime, args.out)
    print(json.dumps(dict(passed=report['passed'], startupCases=len(report['rows']))))


if __name__=='__main__':
    main()
