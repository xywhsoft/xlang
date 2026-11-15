"""Publish the matched IDE/LS/runtime set, retaining backups and rolling back errors."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import uuid
from ide_runtime_library import runtime_basename


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def ordinary(path):
    attrs = path.lstat()
    if not stat.S_ISREG(attrs.st_mode) or getattr(attrs, 'st_file_attributes', 0) & 0x400:
        raise ValueError('Nonordinary runtime file: ' + str(path))


def writable(path):
    if os.name != 'nt' or not path.exists():
        return
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                  ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.CreateFileW(str(path), 0xC0000000, 1, None, 3, 0, None)
    if handle == ctypes.c_void_p(-1).value:
        raise OSError(ctypes.get_last_error(), 'Close the running IDE/language service before publication: ' + str(path))
    kernel.CloseHandle(handle)


def publish(runtime, sources, output, replace=os.replace):
    runtime, output = Path(runtime).absolute(), Path(output).absolute()
    if output.is_relative_to(runtime) or output.exists():
        raise ValueError('Use a fresh backup/receipt directory outside the runtime')
    if not runtime.is_dir() or getattr(runtime.lstat(), 'st_file_attributes', 0) & 0x400:
        raise ValueError('Runtime must be an ordinary directory')
    sources = {name: Path(path).absolute() for name, path in sources.items()}
    hashes, previous, staged, installed = {}, {}, {}, []
    for name, source in sources.items():
        if Path(name).name != name or not name.endswith(('.exe', '.dll')):
            raise ValueError('Invalid runtime member')
        ordinary(source)
        hashes[name] = sha(source)
        target = runtime / name
        if target.exists():
            ordinary(target)
            writable(target)
            previous[name] = sha(target)
        else:
            previous[name] = None
    output.mkdir(parents=True)
    report = dict(status='prepared', runtime=str(runtime), sources={n: str(p) for n, p in sources.items()},
                  expected=hashes, previous=previous, installed=[], rolledBack=[])

    def save():
        (output / 'publication.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def stage(name, source):
        path = runtime / ('.xide-publish-' + uuid.uuid4().hex + '-' + name)
        with source.open('rb') as incoming, path.open('xb') as outgoing:
            shutil.copyfileobj(incoming, outgoing)
            outgoing.flush()
            os.fsync(outgoing.fileno())
        return path

    save()
    try:
        for name, source in sources.items():
            if previous[name] is not None:
                shutil.copyfile(runtime / name, output / name)
                if sha(output / name) != previous[name]:
                    raise ValueError('Backup changed: ' + name)
            staged[name] = stage(name, source)
            if sha(staged[name]) != hashes[name] or sha(source) != hashes[name]:
                raise ValueError('Source changed before publication: ' + name)
        for name in sources:
            target = runtime / name
            if (sha(target) if target.exists() else None) != previous[name]:
                raise ValueError('Target changed before publication: ' + name)
            replace(staged[name], target)
            installed.append(name)
            report['installed'].append(name)
            save()
        if any(sha(runtime / name) != digest for name, digest in hashes.items()):
            raise ValueError('Installed runtime changed')
        report['status'] = 'passed'
    except BaseException as error:
        report['error'] = str(error)
        for name in reversed(installed):
            target = runtime / name
            # Never overwrite a concurrent or externally replaced file.
            if sha(target) != hashes[name]:
                report['status'] = 'rollback-conflict'
                save()
                raise RuntimeError('Publication rollback found a concurrent change: ' + name) from error
            if previous[name] is None:
                target.unlink()
            else:
                restored = stage(name, output / name)
                os.replace(restored, target)
            report['rolledBack'].append(name)
        report['status'] = 'rolled-back'
        raise
    finally:
        for name, path in staged.items():
            if path.exists() and sha(path) == hashes[name]:
                path.unlink()
        save()
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('ide', 'runtime', 'gui', 'server', 'dll', 'output', 'objdump'):
        p.add_argument('--' + key, required=True, type=Path)
    p.add_argument('--updater', type=Path, help='Optional already-qualified standalone updater')
    a = p.parse_args()
    name = runtime_basename(a.ide)
    sdk = a.ide / 'third_party/xge_sdk'
    manifest = json.loads((sdk / 'sdk_manifest.json').read_text(encoding='utf-8-sig'))
    expected = next(row['sha256'] for row in manifest['files'] if row['path'] == 'xge.dll')
    if sha(a.dll) != expected:
        raise ValueError('IDE DLL differs from selected SDK')
    imports = re.findall(r'DLL Name:\s*(\S+)', subprocess.check_output([str(a.objdump), '-p', str(a.gui)], text=True))
    if name not in imports or 'xge.dll' in imports:
        raise ValueError('IDE does not import its private matched runtime')
    # Dependency first, GUI last: the old GUI never imports the new DLL name.
    sources = {name: a.dll}
    if a.updater:
        updater_imports = re.findall(r'DLL Name:\s*(\S+)', subprocess.check_output([str(a.objdump), '-p', str(a.updater)], text=True))
        if any('xge' in dependency.lower() for dependency in updater_imports):
            raise ValueError('The standalone updater must not depend on XGE')
        sources['update.exe'] = a.updater
    sources.update({'xlang-ls.exe': a.server, 'xlang_ide.exe': a.gui})
    report = publish(a.runtime, sources, a.output)
    print('IDE/LS matched runtime publication:', report['status'])


if __name__ == '__main__':
    main()
