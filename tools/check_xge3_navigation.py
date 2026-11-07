"""Exercise new XGE/XUI navigation declarations through the shipped compiler.

CPU-only JIT, generated-C inspection and relocated static AOT execution. Native
resource policy, multi-output status and expired receiver checks stay enabled.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'production', 'packages', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--static-packages', type=Path)
    args = parser.parse_args()
    root, production, packages, out = [p.resolve() for p in
                                      (args.root, args.production, args.packages, args.out)]
    out.mkdir(parents=True, exist_ok=False)
    windows = os.name == 'nt'
    cli = production / ('xl.exe' if windows else 'xl')
    fixture = root / 'test/cases/foreign_xge3_navigation.xl'
    tracked = [Path(__file__).resolve(), fixture, cli]
    tracked += [production / name for name in
                (('xl.dll', 'xlc.dll') if windows else ('libxl.so', 'libxlc.so'))]
    for package_root in (packages, args.static_packages):
        if package_root:
            tracked += [p for p in package_root.rglob('*') if p.is_file()]
    inputs = {str(p): sha(p) for p in tracked}
    report = dict(complete=False, passed=False, inputs=inputs, rows=[],
                  scope=__doc__, shippingDelivery=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    environment = {k: v for k, v in os.environ.items()
                   if not k.startswith('XL6_') and k not in
                   ('XLANG_PACKAGE_PATH', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    environment['XLANG_PACKAGE_PATH'] = str(packages)

    def run(name, command, env=environment, cwd=root):
        start = time.monotonic()
        result = subprocess.run(list(map(str, command)), cwd=cwd, env=env,
                                capture_output=True, timeout=300)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        report['rows'].append(dict(name=name, command=list(map(str, command)),
                                   exitCode=result.returncode,
                                   seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError(str(log))
        return result.stdout.decode('utf-8', errors='replace')

    generated = out / 'navigation.c'
    run('cgen', [cli, 'cgen', fixture, '--emit', generated, '--native-linkage', 'dynamic'])
    text = generated.read_text(encoding='utf-8')
    symbols = ['xuiCodeEditSetBracketMatching', 'xuiCodeEditGetBracketMatching',
               'xuiTabsSetScrollButtonsVisible', 'xuiTabsGetScrollButtonsVisible',
               'xuiTabsSetTabListButtonVisible', 'xuiTabsGetTabListButtonVisible',
               'xuiTabsGetScrollBackwardRect', 'xuiTabsGetScrollForwardRect',
               'xuiDockPanelSetTabScrollButtonsVisible', 'xuiDockPanelGetTabScrollButtonsVisible',
               'xuiDockPanelGetPaneTabScrollRects']
    assert all(re.search(r'\b' + s + r'\s*\(', text) for s in symbols)
    report['directNativeCalls'] = symbols
    run('jit', [cli, 'run', fixture])
    for package, name in [('xge', 'headless_frame'), ('xge', 'headless_shape'),
                          ('xui', 'headless_cell'), ('xui', 'context_graph'),
                          ('xui', 'manager_lifetime')]:
        run('jit-' + name, [cli, 'run', packages / package / 'examples' / (name + '.xl')])
    for name in ('codeedit', 'tabs', 'dockpanel'):
        run('example-' + name, [cli, 'cgen', packages / 'xui/examples' / (name + '.xl'),
                               '--emit', out / (name + '.c'), '--native-linkage', 'dynamic'])
    if args.static_packages:
        static_env = dict(environment, XLANG_PACKAGE_PATH=str(args.static_packages.resolve()))
        binary = out / ('navigation.exe' if windows else 'navigation')
        run('aot-build', [cli, 'build', fixture, '-o', binary, '--native-linkage', 'static'], static_env)
        if windows:
            image = run('aot-imports', [shutil.which('objdump'), '-p', binary])
            imports = re.findall(r'DLL Name:\s*(\S+)', image)
            assert imports and not any(re.match(r'^(?:lib)?(?:xge.*|xlc?|tcc.*)\.dll$', n, re.I)
                                       for n in imports), imports
        else:
            image = run('aot-imports', ['readelf', '-d', binary])
            imports = re.findall(r'Shared library: \[([^\]]+)\]', image)
            assert not any(re.match(r'lib(?:xge|xl|tcc)', n) for n in imports), imports
        report['staticImports'] = imports
        relocated = out / 'standalone'
        relocated.mkdir()
        target = relocated / binary.name
        shutil.copy2(binary, target)
        standalone = {k: v for k, v in environment.items() if k != 'XLANG_PACKAGE_PATH'}
        if windows:
            standalone['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32') + ';' + os.environ['SystemRoot']
        run('aot-relocated', [target], standalone, relocated)
        report['standaloneBinary'] = dict(sha256=sha(target), bytes=target.stat().st_size)
    report.update(complete=True, passed=True,
                  inputsUnchanged=all(sha(p) == h for p, h in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
