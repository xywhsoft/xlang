"""Install IDE assets into the repository's shared release, preserving its engine.

The previous directory is retained until the caller verifies the new runtime.
The receipt records both the old package and the exact engine actually used.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import uuid

IDE = Path(__file__).resolve().parents[1]
REPO = IDE.parent
APPLICATIONS = ('xlang_ide.exe', 'xlang-ls.exe', 'xge.dll')
ENGINE = ('xl.exe', 'xl.dll', 'xlc.dll')


def sha(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def files(root):
    result = {}
    for path in sorted(root.rglob('*')):
        attrs = path.lstat()
        if stat.S_ISLNK(attrs.st_mode) or getattr(attrs, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Reparse input refused: ' + str(path))
        if path.is_file():
            result[path.relative_to(root).as_posix()] = sha(path)
    return result


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def preferences(old, destination, value):
    value = json.loads(json.dumps(value))
    tools = value.setdefault('toolchain', {})
    if not isinstance(tools, dict):
        raise ValueError('Invalid existing toolchain preferences')
    tools.pop('cServer', None)
    old_tools = {'compiler': ('toolchains/xlang/xl.exe', 'xl.exe')}
    for key, (previous, current) in old_tools.items():
        stored = tools.get(key)
        bundled = stored is None
        if isinstance(stored, str):
            absolute = (old / 'option' / stored).resolve()
            bundled = any(str(absolute).casefold() == str((old / path).resolve()).casefold()
                          for path in (previous, current))
            if not bundled and not Path(stored).is_absolute():
                tools[key] = str(absolute)
        if bundled:
            tools[key] = Path(os.path.relpath(destination / current, destination / 'option')).as_posix()
    for section in ('editor', 'ui'):
        font = value.get(section, {}).get('font', {})
        stored = font.get('path') if isinstance(font, dict) else None
        if isinstance(stored, str) and stored:
            absolute = (old / 'option' / stored).resolve()
            try:
                relative = absolute.relative_to((old / 'data/fonts').resolve())
            except ValueError:
                if not Path(stored).is_absolute():
                    font['path'] = str(absolute)
                continue
            font['path'] = '../data/fonts/' + relative.as_posix()
    value.setdefault('format', 'xlang-ide-settings-1')
    return value


def merge(source, application_release, output, language_server=None):
    source = source.resolve(strict=True)
    target = (REPO / 'release').resolve(strict=True)
    output = output.resolve()
    if source != (IDE / 'release').resolve() or not output.is_relative_to((IDE / 'build').resolve()):
        raise ValueError('Migration requires ide/release and a fresh receipt under ide/build')
    if output.exists():
        raise ValueError('Receipt directory already exists: ' + str(output))
    output.mkdir(parents=True)
    original, protected = files(source), files(target)
    prior_receipt = target / 'ide-install-receipt.json'
    if prior_receipt.is_file():
        shutil.copy2(prior_receipt, output / 'previous-install-receipt.json')
        protected.pop('ide-install-receipt.json', None)
    for name in ENGINE:
        if name not in protected:
            raise ValueError('Shared engine missing: ' + name)
    applications = application_release.resolve(strict=True) if application_release else source
    selected, development = {}, {}
    material_root = REPO / 'dev/release-materials'
    for name, digest in original.items():
        if name == 'xlang_ide_console.exe':
            continue
        if name.startswith(('toolchains/xlang/', 'toolchains/clangd/', 'sources/clangd-adaptation/', 'notices/clangd/', 'provenance/clangd-adaptation/')) or name == 'provenance/clangd-inputs.json':
            continue
        if name == 'package-manifest.json':
            name = 'provenance/ide-original-package-manifest.json'
            development[name] = (source / 'package-manifest.json', digest)
        elif name.startswith(('sources/', 'provenance/')):
            development[name] = (source / name, digest)
        else:
            selected[name] = (source / name, digest)
    for name in APPLICATIONS:
        path = language_server.resolve(strict=True) if name == 'xlang-ls.exe' and language_server else applications / name
        selected[name] = (path, sha(path))
    # Commands/fonts copied by the real build belong to the same source build.
    if application_release:
        for name, digest in files(applications / 'data').items():
            selected['data/' + name] = (applications / 'data' / name, digest)
    allowed = {'QUICKSTART.md', *APPLICATIONS}
    namespaces = ('data/', 'option/', 'notices/')
    for name, (path, digest) in selected.items():
        if name not in allowed and not name.startswith(namespaces):
            raise ValueError('Unexpected IDE runtime asset: ' + name)
        if sha(path) != digest:
            raise ValueError('Input changed: ' + str(path))
        if name in protected and protected[name] != digest and name not in APPLICATIONS and not name.startswith('option/'):
            raise ValueError('Shared release conflict: ' + name)
    for name, (path, digest) in development.items():
        destination = material_root / name
        if sha(path) != digest or (destination.exists() and sha(destination) != digest):
            raise ValueError('Development materials conflict: ' + name)
        for parent in (destination, *destination.parents):
            if parent.exists() and (parent.is_symlink() or getattr(parent.stat(), 'st_file_attributes', 0) & 0x400):
                raise ValueError('Reparse development materials destination: ' + str(parent))
    settings = 'option/settings.json'
    prior_settings = target / settings if (target / settings).is_file() else source / settings
    value = json.loads(prior_settings.read_text(encoding='utf-8-sig'))
    origin = target if prior_settings == target / settings else source
    value = preferences(origin, target, value)
    report = dict(format='xlang-ide-release-merge-1', status='prepared', source=str(source), target=str(target),
                  createdUtc=datetime.now(timezone.utc).isoformat(), applicationRelease=str(applications),
                  languageServerSource=str(selected['xlang-ls.exe'][0]),
                  previousFiles=original, protectedFiles=protected, installed={},
                  developmentMaterials=dict(directory=str(material_root), installed={}),
                  engine={name: protected[name] for name in ENGINE},
                  oldCompilerRuntimeCopied=False, archive=str(output / 'previous-ide-release'),
                  note='Historical IDE compiler materials do not describe the existing shared engine; its exact binding is recorded separately.')
    write(output / 'merge-receipt.json', report)
    for name, (path, digest) in development.items():
        destination = material_root / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            with path.open('rb') as incoming, destination.open('xb') as outgoing:
                shutil.copyfileobj(incoming, outgoing)
        if sha(destination) != digest:
            raise ValueError('Development material copy differs: ' + name)
        report['developmentMaterials']['installed'][name] = digest
    for name, (path, digest) in selected.items():
        destination = target / name
        # Existing user state wins; only known bundled paths are rebased below.
        if name.startswith('option/') and destination.exists():
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        if sha(destination) != digest:
            raise ValueError('Copy verification failed: ' + name)
        report['installed'][name] = digest
    write(target / settings, value)
    report['installed'][settings] = sha(target / settings)
    for name, digest in protected.items():
        if name in APPLICATIONS or name.startswith('option/'):
            continue
        if sha(target / name) != digest:
            raise ValueError('Existing shared release asset changed: ' + name)
    if files(source) != original:
        raise ValueError('Original IDE release changed during migration')
    if (target / 'toolchains/xlang').exists():
        raise ValueError('A duplicate xlang runtime remains in shared release')
    report['status'] = 'installed-awaiting-runtime-verification'
    write(output / 'merge-receipt.json', report)
    write(target / 'ide-install-receipt.json', dict(format=report['format'], createdUtc=report['createdUtc'],
          engine=report['engine'], installed=report['installed'], developmentMaterials=report['developmentMaterials'], sourceReceipt=str(output / 'merge-receipt.json')))
    print(json.dumps(dict(status=report['status'], target=str(target), installed=len(report['installed']),
                          protected=len(protected), receipt=str(output / 'merge-receipt.json')), ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=IDE / 'release')
    parser.add_argument('--application-release', type=Path)
    parser.add_argument('--language-server', type=Path, help='Explicit released service to retain independently of the GUI build')
    parser.add_argument('--output', type=Path, default=IDE / 'build' / ('release-unify-' + uuid.uuid4().hex[:8]))
    args = parser.parse_args()
    try:
        merge(args.source, args.application_release, args.output, args.language_server)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, str(error) + '\n')
