"""Prepare SDK64 privately: reuse geometry ink envelopes for the first paint."""
from datetime import datetime,timezone
import hashlib,json,os,shutil,subprocess,time,uuid
from pathlib import Path

IDE=Path(__file__).resolve().parents[1]
def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'))
def write(path,value):
    temp=path.with_suffix('.tmp');temp.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');os.replace(temp,path)

def main():
    parent_build=IDE/'build/editor-sdk-viewport-paint-ef5a58eb8a94/sdk-build.json'
    parent=read(parent_build);descriptor=Path(parent['sourceManifest']);before=read(descriptor)
    assert parent['exitCode']==0 and parent['sourceUnchanged'] and sha(descriptor)==parent['sourceManifestSha256']
    root=IDE/'build'/('editor-sdk-geometry-paint-'+uuid.uuid4().hex[:12]);source=root/'source';source.mkdir(parents=True)
    protected={parent_build:sha(parent_build),descriptor:sha(descriptor),Path(__file__):sha(__file__)}
    for row in before['files']:
        path=Path(parent['source'])/row['path'];assert sha(path)==row['sha256'];protected[path]=row['sha256']
        target=source/row['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    for row in parent['outputs']:protected[Path(row['path'])]=row['sha256']
    shutil.copy2(__file__,root/Path(__file__).name)
    edits=[]
    def replace(relative,old,new):
        path=source/relative;text=path.read_text(encoding='utf-8');assert text.count(old)==1,(relative,old,text.count(old))
        path.write_text(text.replace(old,new),encoding='utf-8');edits.append({'path':relative,'before':old,'after':new})
    cursor='src/xge_text_cursor_internal.inc'
    replace(cursor,'typedef struct xge_cursor_font_key_t {','static void* __xgeCursorInkCreate(int);\nstatic void __xgeCursorInkFree(void*);\nstatic void __xgeCursorInkCopyPrefix(void*,const void*,int);\nstatic void __xgeCursorInkRecord(void*,const xge_glyph_position_t*,double,const xge_glyph_run_t*);\ntypedef struct xge_cursor_font_key_t {')
    replace(cursor,'int64_t iNextOffset;','int64_t iNextOffset;\n    void* pPaintInk;\n    xge_glyph_run_t tFinal;')
    replace(cursor,'double fVisitedAdvance;','double fVisitedAdvance;\n    void* pPaintInk;\n    xge_glyph_run_t* pPaintMetrics;')
    replace(cursor,'xrtFree(cursor->sText);','__xgeCursorInkFree(cursor->pPaintInk);\n    xrtFree(cursor->sText);')
    replace(cursor,'r->tLastVisited=saved;++r->iVisited;','if(r->pPaintInk)__xgeCursorInkRecord(r->pPaintInk,&saved,r->fVisitedAdvance,r->pPaintMetrics);\n        r->tLastVisited=saved;++r->iVisited;')
    run='src/xge_text_run.c'
    replace(run,'int xgeInternalTextShapeVisitCached(const xge_text_shape_desc_t* desc,','static int __xgeTextShapeVisitCachedWithInk(const xge_text_shape_desc_t* desc,')
    replace(run,'xge_internal_text_cursor** output,xge_internal_text_resume_t* info) {','xge_internal_text_cursor** output,xge_internal_text_resume_t* info,int bPaintInk) {')
    replace(run,'cursor=__xgeCursorCreate(desc,size);','cursor=__xgeCursorCreate(desc,size);\n    if(cursor && bPaintInk)cursor->pPaintInk=__xgeCursorInkCreate(size);')
    replace(run,'record.onVisit=visit;record.pUser=user;record.pPrefixFonts=old?old->pRunFonts:NULL;','record.onVisit=visit;record.pUser=user;record.pPrefixFonts=old?old->pRunFonts:NULL;\n    record.pPaintInk=cursor?cursor->pPaintInk:NULL;record.pPaintMetrics=run;')
    replace(run,'info->iTextOffset=restore->iOffset;info->iReplayGlyphIndex=record.iVisited;info->iReplayTextOffset=(int)restore->tLastVisited.iCluster;','info->iTextOffset=restore->iOffset;info->iReplayGlyphIndex=record.iVisited;info->iReplayTextOffset=(int)restore->tLastVisited.iCluster;\n        __xgeCursorInkCopyPrefix(record.pPaintInk,old->pPaintInk,info->iReplayTextOffset);')
    replace(run,'if(cursor && !cursor->bFailed && __xgeCursorKeyEqual(cursor,desc)) {','if(cursor && !cursor->bFailed && __xgeCursorKeyEqual(cursor,desc)) {\n        cursor->tFinal=*run;cursor->tFinal.pBackend=NULL;cursor->tFinal.pGlyphs=NULL;')
    wrappers='''int xgeInternalTextShapeVisitCached(const xge_text_shape_desc_t* desc,xge_glyph_run_t* run,
    xge_internal_text_shape_visit_proc visit,void* user,const xge_internal_text_cursor* old,int prefix,
    xge_internal_text_cursor** output,xge_internal_text_resume_t* info)
{return __xgeTextShapeVisitCachedWithInk(desc,run,visit,user,old,prefix,output,info,0);}
int xgeInternalTextShapeVisitPaintCached(const xge_text_shape_desc_t* desc,xge_glyph_run_t* run,
    xge_internal_text_shape_visit_proc visit,void* user,const xge_internal_text_cursor* old,int prefix,
    xge_internal_text_cursor** output,xge_internal_text_resume_t* info)
{return __xgeTextShapeVisitCachedWithInk(desc,run,visit,user,old,prefix,output,info,1);}

'''
    replace(run,'void xgeGlyphRunFree(xge_glyph_run_t* pRun)\n{',wrappers+'void xgeGlyphRunFree(xge_glyph_run_t* pRun)\n{')
    replace(run,'#include "xge_text_viewport_paint_internal.inc"','#include "xge_text_viewport_paint_internal.inc"\n#include "xge_cursor_ink_internal.inc"')
    ink=IDE/'third_party/sdk_patches/geometry-paint-v1/cursor_ink.inc';protected[ink]=sha(ink);shutil.copy2(ink,source/'src/xge_cursor_ink_internal.inc')
    header='src/xge_text_shape_internal.h'
    replace(header,'#endif','int xgeInternalTextShapeVisitPaintCached(const xge_text_shape_desc_t*,xge_glyph_run_t*,\n    xge_internal_text_shape_visit_proc,void*,const xge_internal_text_cursor*,int,\n    xge_internal_text_cursor**,xge_internal_text_resume_t*);\nint xgeInternalTextPaintViewportSeed(const xge_text_shape_desc_t*,const xge_internal_text_cursor*,void**);\n#endif')
    paint='src/xge_text_viewport_paint_internal.inc'
    replace(paint,'return p && p->bValid && p->pCursor && p->pCursor->iTextSize==pDesc->iTextSize &&','return pDesc && pDesc->iSize>=sizeof(*pDesc) && pDesc->sText && pDesc->pFont && p && p->bValid && p->pCursor && p->pCursor->iTextSize==pDesc->iTextSize &&')
    replace(paint,'if (!ppPaint || !ppIndex || !desc || desc->iTextSize<=0 || spanCount<0 || (spanCount && !spans))','if (!ppPaint || !ppIndex || !desc || desc->iSize<sizeof(*desc) || !desc->sText || !desc->pFont || desc->iTextSize<=0 || spanCount<0 || (spanCount && !spans))')
    provider='src/xui_cursor_proxy_internal.inc'
    replace(provider,'iRet=xgeInternalTextShapeVisitCached(&tDesc,&tRun,__xuiProxyXgeVisitCursorPosition,&tState,','iRet=(iTextSize>=65536?xgeInternalTextShapeVisitPaintCached:xgeInternalTextShapeVisitCached)(&tDesc,&tRun,__xuiProxyXgeVisitCursorPosition,&tState,')
    replace(provider,'static xui_proxy_xge_cursor_t* g_xuiProxyXgeCursors;','static xui_proxy_xge_cursor_t* g_xuiProxyXgeCursors;\nstatic void __xuiProxyXgeSeedViewport(xui_font font,const xge_text_shape_desc_t* desc,void** output)\n{\n    if(*output)return;\n    for(xui_proxy_xge_cursor_t* p=g_xuiProxyXgeCursors;p;p=p->pNext)\n        if(p->pFont==font && p->pCursor && xgeInternalTextPaintViewportSeed(desc,p->pCursor,output)==XGE_OK)return;\n}')
    proxy='src/xui_proxy_xge.c'
    replace(proxy,'static xui_proxy_xge_draw_cache_t g_xuiProxyXgeDrawCache;','static void __xuiProxyXgeSeedViewport(xui_font,const xge_text_shape_desc_t*,void**);\nstatic xui_proxy_xge_draw_cache_t g_xuiProxyXgeDrawCache;')
    replace(proxy,'iRet=xgeInternalTextPaintViewport(pDesc,tRect.fX,tRect.fY,iColor,pSpans,iSpanCount,&pPaint,&g_xuiProxyXgeDrawCache.pViewport);','__xuiProxyXgeSeedViewport(pFont,pDesc,&g_xuiProxyXgeDrawCache.pViewport);\n        iRet=xgeInternalTextPaintViewport(pDesc,tRect.fX,tRect.fY,iColor,pSpans,iSpanCount,&pPaint,&g_xuiProxyXgeDrawCache.pViewport);')
    strict=Path(parent['command'][3]);assert sha(strict)==parent['strictBuildScriptSha256'];protected[strict]=sha(strict);shutil.copy2(strict,root/'build_strict_sdk.bat')
    files=[{'path':str(p.relative_to(source)).replace('\\','/'),'sha256':sha(p),'size':p.stat().st_size} for p in sorted(source.rglob('*')) if p.is_file()]
    manifest=root/'source-manifest.json';write(manifest,{'root':str(root),'source':str(source),'files':files,'edits':edits,
        'parentManifest':str(descriptor),'parentManifestSha256':sha(descriptor),'sdkVersion':64,'publicHeadersChanged':False,
        'geometryInkThresholdBytes':65536,'byteBucketStride':4096,'maximumVisiblePaintRecords':4096,
        'independentPaintClone':True,'productionChanged':False,'sdkInstalled':False,'selectedForCurrentCandidate':False,'performanceAccepted':False})
    log=root/'actual-sdk-build.log';receipt=root/'sdk-build.json';command=[os.environ['COMSPEC'],'/d','/c',str(root/'build_strict_sdk.bat'),'ide']
    record={'status':'building','source':str(source),'sourceManifest':str(manifest),'sourceManifestSha256':sha(manifest),
        'utc':datetime.now(timezone.utc).isoformat(),'command':command,'log':str(log),'wholeSdkWerror':True,
        'strictBuildScriptSha256':sha(strict),'productionChanged':False,'sdkInstalled':False};write(receipt,record);print(root,flush=True)
    env=dict(os.environ);env['PATH']=r'E:\software\w64devkit\bin;'+env.get('PATH','');started=time.monotonic()
    with log.open('xb') as stream:
        child=subprocess.Popen(command,cwd=source,env=env,stdout=stream,stderr=stream,creationflags=0x08000000);record['pid']=child.pid
        while child.poll() is None:
            record.update(elapsedSeconds=round(time.monotonic()-started,3),lastConfirmedLiveUtc=datetime.now(timezone.utc).isoformat());write(receipt,record);time.sleep(.5)
    for path,digest in protected.items():assert sha(path)==digest,str(path)
    record['sourceUnchanged']=all(sha(source/r['path'])==r['sha256'] for r in files);assert record['sourceUnchanged']
    record.update(status='sdk-build-finished',exitCode=child.returncode,elapsedSeconds=round(time.monotonic()-started,3),logSha256=sha(log),parentUnchanged=True)
    record['outputs']=[{'path':str(source/'build/ide'/name),'sha256':sha(source/'build/ide'/name),'size':(source/'build/ide'/name).stat().st_size} for name in ('xge.dll','xge.lib','xge_res.o') if (source/'build/ide'/name).exists()];write(receipt,record)
    print(json.dumps({'exitCode':child.returncode,'report':str(receipt),'elapsedSeconds':record['elapsedSeconds']}),flush=True)
    if child.returncode:print(log.read_text(encoding='utf-8',errors='replace')[-12000:],flush=True)
    return child.returncode
if __name__=='__main__':raise SystemExit(main())
