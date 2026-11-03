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

def source_directories(root):
    root=Path(root);runtime=runtime_directory(root)
    return [root/name for name in ('src','lib/tcc','lib/lzma','lib/xrt','lib/xruntime')]+[
        runtime/'include',runtime/'extlibs/xacme']
