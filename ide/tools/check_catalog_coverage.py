"""Reject stale compiler bindings, missing APIs, hidden fields and altered signatures."""
from pathlib import Path
import argparse,json,re,subprocess
from catalog_sources import verify,sha
from catalog_inventory import validate

IDE=Path(__file__).resolve().parents[1]

def check(root,catalog):
    root=Path(root).resolve();catalog=Path(catalog).resolve()
    data=json.loads(catalog.read_text(encoding='utf-8'))
    binding=data.get('binding',{});revision=binding.get('sourceRevision','')
    if not re.fullmatch('[0-9a-f]{40}',revision):raise ValueError('Catalog has no current compiler source binding; regenerate it')
    compiler_files=binding.get('compilerFiles',{})
    if set(compiler_files)!={'xl.exe','xl.dll','xlc.dll'}:raise ValueError('Incomplete compiler binding')
    for name,digest in compiler_files.items():
        if sha(root/'release'/name)!=digest:raise ValueError('Catalog compiler changed: '+name)
    if data['compilerSha256']!=compiler_files['xl.exe']:raise ValueError('Parser identity differs from its binding')
    subprocess.run(['git','diff','--quiet','--no-ext-diff',revision,'--','src','lib','include','release/include'],cwd=root,check=True)
    counts=validate(data,data['apiInventory'])
    material=verify(catalog.parent/'catalog-sources',catalog)
    if material['unavailable']:raise ValueError('Catalog has missing source snapshots')
    ids=[r['id'] for r in data['entries']]
    if len(set(ids))!=len(ids):raise ValueError('Duplicate command identity')
    if 'extensionCatalog' in data:
        from refresh_extensions import check as check_extensions
        check_extensions(root,catalog.parent)
    return dict(status='passed',entries=len(ids),documents=len(data['documents']),publicApi=counts,availableSources=material['available'],catalogSha256=sha(catalog),sourceRevision=revision)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,default=IDE.parent);ap.add_argument('--catalog',type=Path);ap.add_argument('--report',type=Path);a=ap.parse_args()
    try:report=check(a.root,a.catalog or a.root/'release/data/commands.json')
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:ap.exit(1,str(error)+'\n')
    if a.report:a.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))
if __name__=='__main__':main()
