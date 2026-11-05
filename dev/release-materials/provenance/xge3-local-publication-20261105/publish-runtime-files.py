from pathlib import Path, PurePosixPath
import argparse, hashlib, json, os, shutil, stat, uuid

ROOT=Path('D:/GIT/x-lang').resolve()
B=Path('F:/xlang-validation/foreign-completion-20261103')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def local(name):return Path('//wsl.localhost/Ubuntu'+name) if name.startswith('/home/') else Path(name)
def digest(path):return sha(path) if path.is_file() else None
def save(path,value):
    temp=path.with_suffix(path.suffix+'.tmp');temp.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8');os.replace(temp,path)
def target(name):
    parts=PurePosixPath(name).parts
    assert name==PurePosixPath(name).as_posix() and '..' not in parts and not PurePosixPath(name).is_absolute()
    assert (parts[:3]==('ide','third_party','xge_sdk') or
        (len(parts)>=3 and parts[:2] in (('release','win_x64'),('release','linux_x64')) and
         (parts[2]=='include' or parts[2] in ('xl.exe','xl.dll','xlc.dll','xl.def','xlc.def','xl','libxl.so','libxlc.so','xlang_ide.exe','xge.dll','xlang-ls.exe')))),name
    path=ROOT/name
    assert path.resolve().is_relative_to(ROOT)
    for item in (path,*path.parents):
        if item.exists():
            info=item.lstat();assert not stat.S_ISLNK(info.st_mode) and not(getattr(info,'st_file_attributes',0)&0x400),item
        if item==ROOT:break
    return path
def verified_copy(source,destination,expected):
    destination.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,destination);assert sha(destination)==expected,destination
def build_plan():
    rows={};inputs={}
    def add(source,name,expected):
        source=local(str(source));assert source.is_file() and sha(source)==expected,source
        destination=target(name);assert name not in rows
        rows[name]=dict(source=str(source),target=name,after=expected,before=digest(destination),bytes=source.stat().st_size)
    receipt_path=B/'final-production-delivery-receipt-r1/receipt.json'
    assert sha(receipt_path)=='1be446145329f2d31b73246a8d80bb3fea6ff0ce005698073cbac2edfdf22483'
    inputs[str(receipt_path)]=sha(receipt_path)
    win_manifest=B/'production-final-win-r1/production-build.json'
    assert sha(win_manifest)=='c550e48a35cf57e8006c46c3513e319b977d0864d69d5af84b5752a8388e0c62'
    inputs[str(win_manifest)]=sha(win_manifest);win=read(win_manifest)
    for name,row in win['images'].items():add(row['path'],'release/win_x64/'+name,row['sha256'])
    linux_receipt=B/'production-cli-examples-linux-receipt-r1/receipt.json'
    assert sha(linux_receipt)=='1e5e3103f463df9d299416894988ba34b4d11dd3408fa2fd091c7d73dacea30d'
    inputs[str(linux_receipt)]=sha(linux_receipt);lin=read(linux_receipt)
    for name,row in lin['production'].items():add(row['path'],'release/linux_x64/'+name,row['sha256'])
    for platform,folder in [('win_x64',B/'sdk-final-core-win-r3'),('linux_x64',Path('//wsl.localhost/Ubuntu/home/ubuntu/xlang-validation/sdk-final-core-linux-r3'))]:
        manifest=folder/'manifest.json';m=read(manifest);assert m['complete'] and m['passed']
        assert sha(manifest)==dict(win_x64='5516ba90f26d8131181ef177b57c2a7796a3360e831dbcd418d3b2a63ae9906a',linux_x64='30d3194c7c278b56e5f96537ae808f18fc5b52b51e6b24f5199f4b1902d7348b')[platform]
        inputs[str(manifest)]=sha(manifest)
        for row in m['files']:add(folder/row['path'],'release/'+platform+'/'+row['path'],row['sha256'])
    gui=read(B/'ide-final-cpu-r1.json')
    assert gui['guiSha256']=='5fdc6080d90c60aea26cd3416c3217599a1fdbd1fdf2d70a2e1c55a1648bac8c'
    assert gui['engineSha256']=='edafbc3d3520f47cbb851365c98375b27f9a0f09404535dc7e03237e2ddc02fc'
    add(gui['guiCandidate'],'release/win_x64/xlang_ide.exe',gui['guiSha256'])
    add(B/'ide-final-runtime-r1/xge.dll','release/win_x64/xge.dll',gui['engineSha256'])
    add(B/'lsp-final-build-probe-r8/xlang-ls.exe','release/win_x64/xlang-ls.exe','2ed3bd2eb158bab6d807d10299c7c3aa256b012c050ba2397741da057246f232')
    sdk=B/'ide-sdk-final-r3';m=read(sdk/'sdk_manifest.json');inputs[str(sdk/'sdk_manifest.json')]=sha(sdk/'sdk_manifest.json')
    for row in m['files']:add(sdk/row['path'],'ide/third_party/xge_sdk/'+row['path'],row['sha256'])
    add(sdk/'sdk_manifest.json','ide/third_party/xge_sdk/sdk_manifest.json',sha(sdk/'sdk_manifest.json'))
    return dict(root=str(ROOT),inputs=inputs,rows=[rows[k] for k in sorted(rows)],scope='Exact matched production images, public SDK headers and IDE SDK only; no user resources/settings or package cleanup.')
def apply(plan,work):
    assert not work.exists() and not work.resolve().is_relative_to(ROOT);work.mkdir(parents=True)
    save(work/'plan.json',plan);journal=dict(complete=False,applied=[],rollbackFailures=[]);save(work/'journal.json',journal)
    changed=[row for row in plan['rows'] if row['before']!=row['after']]
    for row in plan['rows']:
        assert sha(row['source'])==row['after'] and digest(target(row['target']))==row['before']
    for row in changed:
        if row['before'] is not None:verified_copy(target(row['target']),work/'backups'/row['target'],row['before'])
        verified_copy(row['source'],work/'prepared'/row['target'],row['after'])
    def replace(source,path,expected):
        path.parent.mkdir(parents=True,exist_ok=True);temp=path.with_name('.__matched_'+uuid.uuid4().hex+'.tmp')
        try:verified_copy(source,temp,expected);os.replace(temp,path);assert sha(path)==expected
        finally:
            if temp.exists():temp.unlink()
    try:
        for row in changed:
            path=target(row['target']);assert digest(path)==row['before']
            # Register intent before a possibly successful replace/journal error.
            journal['applied'].append(row);save(work/'journal.json',journal)
            replace(work/'prepared'/row['target'],path,row['after']);save(work/'journal.json',journal)
        assert all(sha(name)==expected for name,expected in plan['inputs'].items())
        assert all(digest(target(row['target']))==row['after'] for row in plan['rows'])
        journal.update(complete=True,passed=True,filesWritten=len(changed),targetsVerified=len(plan['rows']));save(work/'journal.json',journal)
    except Exception as error:
        for row in reversed(journal['applied']):
            try:
                path=target(row['target']);assert digest(path) in (row['before'],row['after'])
                if row['before'] is None:
                    if path.exists():path.unlink()
                elif digest(path)!=row['before']:replace(work/'backups'/row['target'],path,row['before'])
            except Exception as rollback_error:journal['rollbackFailures'].append(str(rollback_error))
        journal.update(error=str(error),passed=False);save(work/'journal.json',journal);raise
    return journal
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--apply',action='store_true');args=parser.parse_args();plan=build_plan()
    if args.apply:print(json.dumps({k:v for k,v in apply(plan,B/'runtime-files-publish-r1').items() if k!='applied'}))
    else:save(B/'runtime-files-plan-r1.json',plan);print('Prepared',len(plan['rows']),'explicit files; no target writes')
