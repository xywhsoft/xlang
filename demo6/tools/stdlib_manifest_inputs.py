"""Source-only project inputs explicitly selected by the shipping VFS manifests.

Index export resolves against INDEX names/text, never unrelated working edits.
Ordinary builders resolve the same rows against their isolated source tree.
Generated TCC/VFS inputs are deliberately not substituted by old build products.
"""
from pathlib import Path,PurePosixPath
import fnmatch

ROOT_TOKEN='%XL6_ROOT%'
GENERATED_TOKENS=('%XL6_EMBED_TCC_DIST%','%XL6_EMBED_TCC_DIST_X64%',
    '%XL6_XRT_OBJ%','%XL6_VFS_OUTPUT_DIR%')

def root_rows(text):
    rows=[]
    for line,raw in enumerate(text.splitlines(),1):
        raw=raw.strip()
        if not raw or raw.startswith('#'):continue
        parts=[part.strip() for part in raw.split('|')]
        if len(parts) not in (3,4) or parts[0].lower() not in ('file','dir'):
            raise ValueError('Invalid VFS manifest row at line '+str(line))
        kind,virtual,source=parts[0].lower(),parts[1],parts[2]
        if not virtual or not source or kind=='file' and len(parts)!=3:
            raise ValueError('Invalid VFS source row at line '+str(line))
        source=source.replace('\\','/')
        if source.startswith(ROOT_TOKEN):
            source=source[len(ROOT_TOKEN):]
            if not source.startswith('/'):
                raise ValueError('Invalid project-root token boundary')
            source=source[1:]
        elif any(source==token or source.startswith(token+'/') for token in GENERATED_TOKENS):
            continue
        relative=PurePosixPath(source)
        if not relative.parts or '%' in source or relative.is_absolute() or '..' in relative.parts or ':' in source:
            raise ValueError('VFS source escapes or does not identify project inputs: '+source)
        patterns=tuple(p.strip() for p in parts[3].split(';') if p.strip()) if len(parts)==4 else ()
        rows.append(dict(Line=line,Kind=kind,Source=relative.as_posix(),Patterns=patterns))
    return rows

def index_manifest_inputs(names,read_text):
    """Return exact relative index paths and an auditable per-manifest closure."""
    names=set(names);selected=set();closure=[]
    manifests=sorted(p for p in names if PurePosixPath(p).parent==PurePosixPath('tools') and
        PurePosixPath(p).match('tcc_vfs_manifest_*.txt'))
    if not manifests:raise ValueError('No indexed shipping VFS manifests')
    for manifest in manifests:
        selected.add(manifest)
        for row in root_rows(read_text(manifest)):
            source=row['Source']
            if row['Kind']=='file':
                if source not in names:raise ValueError('Manifest source is not indexed: '+source)
                matches=[source]
            else:
                descendants=[p for p in names if p.startswith(source+'/')]
                if not descendants:raise ValueError('Manifest directory has no indexed sources: '+source)
                matches=sorted(p for p in descendants if not row['Patterns'] or
                    any(fnmatch.fnmatch(PurePosixPath(p).name,pattern) for pattern in row['Patterns']))
            selected.update(matches)
            closure.append(dict(Manifest=manifest,**row,Files=matches))
    return selected,closure

def local_manifest_inputs(root):
    """Pin actual project sources; generated dependencies must be built fresh."""
    root=Path(root).resolve();selected=set()
    manifests=sorted((root/'tools').glob('tcc_vfs_manifest_*.txt'))
    if not manifests:raise ValueError('No shipping VFS manifests')
    for manifest in manifests:
        selected.add(manifest)
        for row in root_rows(manifest.read_text(encoding='utf-8-sig')):
            source=(root/row['Source']).resolve()
            if not source.is_relative_to(root):raise ValueError('Manifest input resolves outside isolated root')
            if row['Kind']=='file':
                if not source.is_file():raise ValueError('Missing manifest source: '+str(source))
                files=[source]
            else:
                if not source.is_dir():raise ValueError('Missing manifest directory: '+str(source))
                files=[p for p in source.rglob('*') if p.is_file() and (not row['Patterns'] or
                    any(fnmatch.fnmatch(p.name,pattern) for pattern in row['Patterns']))]
            for path in files:
                path=path.resolve()
                if not path.is_relative_to(root):raise ValueError('Manifest child resolves outside isolated root')
                selected.add(path)
    return selected
