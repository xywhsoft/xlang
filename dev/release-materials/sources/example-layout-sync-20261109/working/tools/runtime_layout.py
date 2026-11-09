"""Resolve the active platform SDK without creating legacy release aliases."""
from pathlib import Path
import os

def runtime_directory(root, target=None):
    if target not in (None,'win64','linux64'):
        raise ValueError('Unknown runtime target: '+str(target))
    root=Path(root).absolute()
    platform='win_x64' if (target=='win64' or (target is None and os.name=='nt')) else 'linux_x64'
    selected=root/'release'/platform
    if selected.is_dir():return selected
    # Historical isolated source cohorts may contain only a flat SDK.
    legacy=root/'release'
    if (legacy/'include').is_dir():return legacy
    raise FileNotFoundError('Missing platform runtime directory: '+str(selected))

def runtime_source(root, path, target=None):
    """Map only legacy SDK material paths; preserve every other source path."""
    root=Path(root).absolute();path=Path(path).absolute()
    try:relative=path.relative_to(root/'release')
    except ValueError:return path
    if relative.parts and relative.parts[0] in ('include','extlibs'):
        return runtime_directory(root,target)/relative
    return path


def runtime_example(runtime, relative):
    """Resolve an example in either the categorized or legacy flat layout.

    Only legacy leaf names (or category/leaf) may be relocated. Fully specified
    paths must exist as written. Never pick the first of ambiguous matches.
    """
    relative = Path(str(relative).replace('\\', '/'))
    if relative.anchor or '..' in relative.parts or not relative.parts:
        raise ValueError('Example path must stay under examples/: ' + str(relative))
    examples = Path(runtime) / 'examples'
    direct = examples / relative
    if direct.is_file():
        return direct
    if len(relative.parts) == 1:
        directory = examples
    elif len(relative.parts) == 2:
        directory = examples / relative.parts[0]
    else:
        raise FileNotFoundError(direct)
    matches = sorted(p for p in directory.rglob(relative.name) if p.is_file())
    if len(matches) == 1:
        return matches[0]
    if matches:
        raise ValueError('Ambiguous example ' + str(relative) + ': ' + ', '.join(map(str, matches)))
    raise FileNotFoundError(direct)

def source_directories(root):
    root=Path(root);runtime=runtime_directory(root)
    return [root/name for name in ('src','lib/tcc','lib/lzma','lib/xrt','lib/xruntime')]+[
        runtime/'include',runtime/'extlibs/xacme']
