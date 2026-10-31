"""Build current IDE sources in a fresh tree and retain older build outputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

IDE = Path(__file__).resolve().parents[1]


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def regular(path):
    for parent in (path, *path.parents):
        if parent.is_symlink() or parent.stat().st_file_attributes & 0x400:
            raise ValueError('Reparse input refused: ' + str(parent))
    if not path.is_file():
        raise ValueError('Missing regular source: ' + str(path))


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf8')


def receipt(report_path, report):
    write(report_path, report)
    write(IDE / 'build/current-build.json', dict(status=report['status'], root=report['root'],
          report=str(report_path), reportSha256=sha(report_path), exitCode=report.get('exitCode')))


def compile_snapshot(options, root, report):
    environment = dict(os.environ)
    if options.toolchain_directory:
        directory = options.toolchain_directory.resolve(strict=True)
        environment['PATH'] = str(directory) + os.pathsep + environment.get('PATH', '')
    compiler = shutil.which('gcc', path=environment.get('PATH'))
    if not compiler or not shutil.which('objdump', path=environment.get('PATH')):
        raise ValueError('gcc and objdump are required; set PATH or -ToolchainDirectory')
    if not shutil.which('windres', path=environment.get('PATH')):
        raise ValueError('windres is required to embed the IDE application icon')
    # build.ps1 invokes Python for pinned font verification; use this same runtime.
    environment['PATH'] = str(Path(sys.executable).parent) + os.pathsep + environment.get('PATH', '')
    frontend = options.frontend_root
    if frontend is None:
        portable = json.loads((IDE / 'data/portable-build-inputs.json').read_text(encoding='utf-8-sig'))
        pinned = (IDE / portable['compiler_root']).parent
        shipped = IDE.parent / 'dev/release-materials/sources/xlang'
        frontend = next((path for path in (pinned, shipped, IDE.parent)
                         if (path / 'src/parser/xl6_parser.c').is_file()), IDE.parent)
    frontend = frontend.resolve(strict=True)
    regular(frontend / 'lib/xrt/xrt.h')
    sdk = IDE / 'third_party/xge_sdk'
    manifest_path = sdk / 'sdk_manifest.json'
    regular(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding='utf-8-sig'))
    expected = {}
    for row in manifest['files']:
        relative = row['path']
        components = relative.split('/')
        if any(not p or p in ('.', '..') or ':' in p or '\\' in p for p in components):
            raise ValueError('Invalid SDK manifest path: ' + relative)
        path = sdk.joinpath(*components)
        regular(path)
        if relative.casefold() in expected or sha(path) != row['sha256']:
            raise ValueError('SDK matched-set validation failed: ' + relative)
        expected[relative.casefold()] = row['sha256']
    inputs = []
    for directory in ('src', 'tests', 'tools', 'data', 'third_party/xge_sdk',
                      'third_party/sdk_patches/xrt-process-tree-v1'):
        for path in sorted((IDE / directory).rglob('*')):
            if '__pycache__' in path.parts or path.suffix == '.pyc':
                continue
            if path.is_dir():
                if path.is_symlink() or path.stat().st_file_attributes & 0x400:
                    raise ValueError('Reparse source directory refused: ' + str(path))
                continue
            regular(path)
            inputs.append(dict(path=path.relative_to(IDE).as_posix(), sha256=sha(path)))
    for row in inputs:
        source = IDE / row['path']
        destination = root / row['path']
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        if sha(destination) != row['sha256'] or sha(source) != row['sha256']:
            raise ValueError('Source changed while copying: ' + row['path'])
    report.update(status='current-source-snapshot-prepared', sourceRoot=str(IDE), root=str(root),
                  configuration=options.configuration, testsBuilt=options.tests, compiler=compiler,
                  inputs=inputs, sdkManifestSha256=sha(root / 'third_party/xge_sdk/sdk_manifest.json'),
                  historicalReleaseOverwritten=False)
    report_path = root / 'current-build-report.json'
    receipt(report_path, report)
    arguments = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                 str(root / 'tools/build.ps1'), '-Configuration', options.configuration,
                 '-FrontendRoot', str(frontend), '-ReleaseDirectory', str(root / 'release')]
    if options.tests:
        arguments.append('-Tests')
    print('Building current sources: ' + str(root), flush=True)
    started = time.monotonic()
    log_path = root / 'current-build.log'
    with log_path.open('wb') as log:
        process = subprocess.Popen(arguments, cwd=root, env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            for line in iter(process.stdout.readline, b''):
                log.write(line)
                log.flush()
                print(line.decode('utf-8', errors='replace').rstrip(), flush=True)
            exit_code = process.wait()
        except BaseException:
            process.terminate()
            process.wait()
            raise
        finally:
            process.stdout.close()
    outputs = {p.relative_to(root).as_posix(): sha(p)
               for p in sorted((root / 'release').glob('*')) if p.is_file()}
    objects = {p.relative_to(root).as_posix(): sha(p)
               for p in sorted((root / 'build' / options.configuration).glob('*.c.o'))}
    source_changes = [r['path'] for r in inputs if not (IDE / r['path']).is_file()
                      or sha(IDE / r['path']) != r['sha256']]
    native_pointer = root / 'build' / options.configuration / 'language-server-build.json'
    native_build = json.loads(native_pointer.read_text(encoding='utf-8-sig')) if native_pointer.is_file() else None
    if exit_code == 0:
        if not native_build or sha(Path(native_build['report'])) != native_build['reportSha256']:
            raise ValueError('Standalone language-server build receipt is missing or changed')
        native_report = json.loads(Path(native_build['report']).read_text(encoding='utf-8'))
        if native_report['status'] != 'passed' or not native_report['serverBuilt'] or native_report['outputs']['xlang-ls.exe'] != outputs['release/xlang-ls.exe']:
            raise ValueError('Standalone language-server output does not match its build receipt')
    report.update(status='current-source-build-passed' if exit_code == 0 else 'current-source-build-failed',
                  exitCode=exit_code, elapsedSeconds=round(time.monotonic() - started, 3),
                  arguments=arguments, log=str(log_path), outputs=outputs, objects=objects,
                  currentSourceChangesDuringBuild=source_changes,
                  languageServerBuild=native_build,
                  guiExecutable=str(root / 'release/xlang_ide.exe'))
    receipt(report_path, report)
    print(json.dumps(dict(status=report['status'], exitCode=exit_code,
                          executable=report['guiExecutable'], report=str(report_path)), ensure_ascii=False))
    return exit_code


def build(options):
    root = IDE / 'build' / ('cb-' + uuid.uuid4().hex[:8])
    root.mkdir(parents=True)
    report_path = root / 'current-build-report.json'
    report = dict(status='current-source-build-preparing', sourceRoot=str(IDE), root=str(root),
                  configuration=options.configuration, testsBuilt=False,
                  testsRequested=options.tests, exitCode=None, inputs=[],
                  historicalReleaseOverwritten=False)
    receipt(report_path, report)
    try:
        return compile_snapshot(options, root, report)
    except (OSError, ValueError, KeyError, KeyboardInterrupt) as error:
        code = 130 if isinstance(error, KeyboardInterrupt) else 1
        report.update(status='current-source-build-cancelled' if code == 130 else 'current-source-build-failed',
                      exitCode=code, error=str(error) or 'Interrupted')
        receipt(report_path, report)
        print(report['error'], file=sys.stderr)
        return code


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--tests', action='store_true')
    parser.add_argument('--toolchain-directory', type=Path)
    parser.add_argument('--frontend-root', type=Path)
    try:
        raise SystemExit(build(parser.parse_args()))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, str(error) + '\n')
