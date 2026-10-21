"""Prepare SDK67: preserve UTF-8 through zero-width replacement and escapes."""
from datetime import datetime,timezone
from pathlib import Path
import difflib,hashlib,json,os,shutil,subprocess,time,uuid
IDE=Path(__file__).resolve().parents[1]
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def write(p,v):
    t=p.with_suffix('.tmp');t.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');os.replace(t,p)
def main():
    parent_path=IDE/'build/editor-sdk-find-boundaries-cfc541fca83a/sdk-build.json';parent=read(parent_path)
    assert parent['exitCode']==0 and parent['wholeSdkWerror'] and parent['sourceUnchanged']
    descriptor=Path(parent['sourceManifest']);before=read(descriptor);assert sha(descriptor)==parent['sourceManifestSha256']
    root=IDE/'build'/('editor-sdk-find-boundaries-'+uuid.uuid4().hex[:12]);source=root/'source';source.mkdir(parents=True)
    protected={parent_path:sha(parent_path),descriptor:sha(descriptor),Path(__file__):sha(__file__)}
    for row in before['files']:
        p=Path(parent['source'])/row['path'];assert sha(p)==row['sha256'];protected[p]=row['sha256']
        out=source/row['path'];out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,out)
    for row in parent['outputs']:assert sha(row['path'])==row['sha256'];protected[Path(row['path'])]=row['sha256']
    p=source/'src/xui_find.c';old=p.read_text(encoding='utf-8');content=old
    marker='\t\tiPos = ((int)tSpan.End > iPos) ? (int)tSpan.End : (int)tSpan.Begin + 1;'
    replacement='\t\tif ( (int)tSpan.Begin >= iEnd ) break;\n\t\tiPos = ((int)tSpan.End > (int)tSpan.Begin) ? (int)tSpan.End : __xuiFindNextUtf8Offset(sText, (int)tSpan.Begin, iEnd);'
    assert content.count(marker)==1;content=content.replace(marker,replacement)
    marker='\t\tiPos = (tCurrent.iEnd > tCurrent.iStart) ? tCurrent.iEnd : tCurrent.iStart + 1;'
    replacement='\t\tif ( tCurrent.iEnd == tCurrent.iStart && tCurrent.iStart >= iEnd ) break;\n\t\tiPos = (tCurrent.iEnd > tCurrent.iStart) ? tCurrent.iEnd : __xuiFindNextUtf8Offset(sText, tCurrent.iStart, iEnd);'
    assert content.count(marker)==1;content=content.replace(marker,replacement)
    marker='\t\tiOffset = (tResult.iEnd > tResult.iStart) ? tResult.iEnd : tResult.iStart + 1;'
    replacement='\t\tif ( tResult.iEnd == tResult.iStart && tResult.iStart >= iRangeEnd ) break;\n\t\tiOffset = (tResult.iEnd > tResult.iStart) ? tResult.iEnd : __xuiFindNextUtf8Offset(sText, tResult.iStart, iRangeEnd);'
    assert content.count(marker)==1;content=content.replace(marker,replacement)
    p.write_text(content,encoding='utf-8');patch=IDE/'third_party/sdk_patches/find-boundaries-v2';patch.mkdir(parents=True,exist_ok=True)
    payload=patch/'xui_find.c'
    if payload.exists():assert payload.read_text(encoding='utf-8')==content
    else:payload.write_text(content,encoding='utf-8')
    protected[payload]=sha(payload);(root/'find-boundaries.diff').write_text(''.join(difflib.unified_diff(old.splitlines(True),content.splitlines(True),fromfile='SDK67-r1/src/xui_find.c',tofile='SDK67-r2/src/xui_find.c')),encoding='utf-8')
    strict=Path(parent['command'][3]);assert sha(strict)==parent['strictBuildScriptSha256'];protected[strict]=sha(strict);shutil.copy2(strict,root/'build_strict_sdk.bat');shutil.copy2(__file__,root/Path(__file__).name)
    files=[dict(path=p.relative_to(source).as_posix(),sha256=sha(p),size=p.stat().st_size) for p in sorted(source.rglob('*')) if p.is_file()]
    manifest=root/'source-manifest.json';write(manifest,dict(root=str(root),source=str(source),files=files,parentManifest=str(descriptor),parentManifestSha256=sha(descriptor),sdkVersion=67,privateRevision=2,changedSources=['src/xui_find.c'],publicHeadersChanged=False,findBoundaryPatchSha256=sha(payload),productionChanged=False,sdkInstalled=False))
    log=root/'actual-sdk-build.log';receipt=root/'sdk-build.json';cmd=[os.environ['COMSPEC'],'/d','/c',str(root/'build_strict_sdk.bat'),'ide']
    record=dict(status='building',source=str(source),sourceManifest=str(manifest),sourceManifestSha256=sha(manifest),utc=datetime.now(timezone.utc).isoformat(),command=cmd,log=str(log),wholeSdkWerror=True,strictBuildScriptSha256=sha(strict),productionChanged=False,sdkInstalled=False,compiler=parent['compiler'],compilerPath=parent['compilerPath'],compilerSha256=parent['compilerSha256'])
    assert sha(record['compilerPath'])==record['compilerSha256'];write(receipt,record);print(root,flush=True)
    env=dict(os.environ);env['PATH']=r'E:\software\w64devkit\bin;'+env.get('PATH','');started=time.monotonic()
    with log.open('xb') as f:
        child=subprocess.Popen(cmd,cwd=source,env=env,stdout=f,stderr=f,creationflags=0x08000000);record['pid']=child.pid
        while child.poll() is None:
            record.update(elapsedSeconds=round(time.monotonic()-started,3),lastConfirmedLiveUtc=datetime.now(timezone.utc).isoformat());write(receipt,record);time.sleep(.5)
    for path,digest in protected.items():assert sha(path)==digest,str(path)
    record['sourceUnchanged']=all(sha(source/r['path'])==r['sha256'] for r in files);assert record['sourceUnchanged']
    record.update(status='sdk-build-finished',exitCode=child.returncode,elapsedSeconds=round(time.monotonic()-started,3),logSha256=sha(log),parentUnchanged=True,finishedUtc=datetime.now(timezone.utc).isoformat(),outputs=[dict(path=str(source/'build/ide'/name),sha256=sha(source/'build/ide'/name),size=(source/'build/ide'/name).stat().st_size) for name in ('xge.dll','xge.lib','xge_res.o') if (source/'build/ide'/name).exists()])
    write(receipt,record);print(json.dumps(dict(exitCode=child.returncode,report=str(receipt),elapsedSeconds=record['elapsedSeconds'])),flush=True)
    if child.returncode:print(log.read_text(encoding='utf-8',errors='replace')[-3000:])
    return child.returncode
if __name__=='__main__':raise SystemExit(main())
