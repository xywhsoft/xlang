"""Resolve a complete compiler cohort or an explicitly partial frontend overlay."""
from pathlib import Path
import hashlib,json,os
from full_compiler_qualification import verify_full_compiler


def load_compiler_cohort(root,compiler,build,partial=False):
    root,compiler,build=[Path(p).resolve()for p in(root,compiler,build)]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    if not partial:
        verify_full_compiler(build,compiler)
        return compiler.parent/'include',[]
    metadata=json.loads(build.read_text())
    assert metadata['passed']and metadata['inputsUnchanged']and not metadata['fullCompilerBuild']
    assert compiler.parent==build.parent/'runtime'
    assert all(sha(p)==h for p,h in metadata['inputs'].items())
    parent=Path(metadata['parent']);assert sha(parent)==metadata['parentSha256']
    snapshot=Path(json.loads(parent.read_text())['source'])
    sdk=snapshot/'release'/('win_x64'if os.name=='nt'else'linux_x64')/'include'
    extra=[parent,*map(Path,metadata['inputs'])]
    for name,digest in metadata['overrides'].items():
        assert sha(root/name)==digest,name
        extra.append(root/name)
    return sdk,extra
