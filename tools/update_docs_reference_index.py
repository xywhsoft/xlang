"""Maintain supplemental links without changing the hand-maintained design hierarchy."""
from pathlib import Path
import argparse
import subprocess

START='<!-- supplemental-reference-index:start -->'
END='<!-- supplemental-reference-index:end -->'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--write',action='store_true')
    args=parser.parse_args();root=args.root.resolve()
    listed=subprocess.check_output(['git','-C',str(root),'ls-files','-z','--','docs/*.md']).split(b'\0')
    paths={root/name.decode('utf-8') for name in listed if name}
    paths.update(root/'docs'/name for name in ('compiled-host-packages.md','embedded-package-delivery-2026-10-28.md'))
    paths=sorted(p for p in paths if p.is_file() and not p.name.startswith('Typed_HIR'))
    changed=[]
    for name in ('README.md','文档状态矩阵.md'):
        path=root/'docs'/name;data=path.read_bytes();bom=data.startswith(b'\xef\xbb\xbf')
        text=data.decode('utf-8-sig');newline='\r\n' if '\r\n' in text else '\n'
        if START in text:
            before,tail=text.split(START,1);_,after=tail.split(END,1)
            base=before.rstrip()+after.rstrip()
        else:base=text.rstrip()
        missing=[p for p in paths if p.name not in base]
        section=['## Supplemental Reference Index','',
                 'These references supplement the curated sections above. Inclusion is not a',
                 'completion claim; each note retains its own scope, evidence and limitations.',
                 'Maintain links when versioned documents are added or removed.','',START]
        for note in missing:
            title=next((line.lstrip('#').strip() for line in note.read_text(encoding='utf-8-sig').splitlines() if line.startswith('# ')),note.stem)
            section.append('- ['+title.replace('[','\\[').replace(']','\\]')+'](<'+note.name+'>)')
        section.append(END)
        # Preserve the curated bytes and replace only the generated tail.
        anchor=base.find(newline+'## Supplemental Reference Index')
        if anchor>=0:base=base[:anchor].rstrip()
        updated=(base+newline*2+newline.join(section)+newline).encode('utf-8')
        if bom:updated=b'\xef\xbb\xbf'+updated
        if updated!=data:
            changed.append(str(path))
            if args.write:path.write_bytes(updated)
    print('Supplemental documentation indexes:',len(changed),'changed')
    if changed and not args.write:raise SystemExit(1)


if __name__=='__main__':main()
