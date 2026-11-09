"""Install the repository budget guard without replacing existing Git/LFS behavior."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    raw = subprocess.check_output(['git', 'rev-parse', '--path-format=absolute', '--git-path', 'hooks'], cwd=ROOT, text=True).strip()
    directory = Path(raw)
    common = Path(subprocess.check_output(['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'], cwd=ROOT, text=True).strip())
    if not directory.resolve().is_relative_to(common.resolve()):
        raise RuntimeError('A shared/custom hooksPath requires manual integration; it was not modified')
    directory.mkdir(parents=True, exist_ok=True)
    hook = directory / 'pre-push'
    backup = directory / 'pre-push.xlang-original'
    current = hook.read_bytes() if hook.exists() else b''
    if b'xlang-repository-budget-hook-v1' in current:
        print('Repository budget hook already installed')
        return
    if current:
        if backup.exists() and backup.read_bytes() != current:
            raise RuntimeError('An older hook backup already exists; preserve it before installing')
        backup.write_bytes(current)
    temporary = hook.with_name('pre-push.xlang-install.tmp')
    temporary.write_bytes((ROOT / 'tools/hooks/pre-push').read_bytes().replace(b'\r\n', b'\n'))
    temporary.chmod(0o755)
    os.replace(temporary, hook)
    print('Repository budget hook installed; previous hook preserved and chained')


if __name__ == '__main__':
    main()
