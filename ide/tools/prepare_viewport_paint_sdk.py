"""Build an isolated SDK63 viewport-paint experiment from verified SDK62 sources."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid

IDE = Path(__file__).resolve().parents[1]
PATCH = IDE/"third_party/sdk_patches/viewport-paint-v1"


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write(path, value):
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    os.replace(temp, path)


def main():
    parent_path = IDE/"build/editor-sdk-precise-coordinates-v62.json"
    parent = json.loads(parent_path.read_text(encoding="utf-8"))
    assert sha(parent_path) == "cbbb74a34e5f34ba145cca063f8a9fa148383424791a331701887925cff32e8c"
    parent_source = Path(parent["source"])
    root = IDE/"build"/("editor-sdk-viewport-paint-"+uuid.uuid4().hex[:12])
    source = root/"source"
    source.mkdir(parents=True)
    protected = {parent_path: sha(parent_path)}
    for row in parent["files"]:
        old = parent_source/row["path"]
        assert sha(old) == row["sha256"], str(old)
        protected[old] = row["sha256"]
        target = source/row["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(old, target)
    edits = []

    def replace(relative, before, after):
        path = source/relative
        text = path.read_text(encoding="utf-8")
        assert text.count(before) == 1, (relative, before, text.count(before))
        path.write_text(text.replace(before, after), encoding="utf-8")
        edits.append({"path": relative, "before": before, "after": after})

    cursor = "src/xge_text_cursor_internal.inc"
    replace(cursor, "double fLineWidth,fLineRight,fMaxWidth;", "double fLineWidth,fLineRight,fMaxWidth,fReplayPenX;")
    replace(cursor, "int iVisited,iInvocations;", "int iVisited,iInvocations,bValidatedInput;\n    double fVisitedAdvance;")
    replace(cursor, "r->tLastVisited=saved;++r->iVisited;", "r->tLastVisited=saved;++r->iVisited;\n        if(saved.iFlags&XGE_GLYPH_POSITION_LINE_BREAK)r->fVisitedAdvance=0;else r->fVisitedAdvance+=saved.fAdvanceX;")
    replace(cursor, "p->iFontCount=backend->iFontCount;", "p->fReplayPenX=r->fVisitedAdvance;\n    if(r->iVisited && !(r->tLastVisited.iFlags&XGE_GLYPH_POSITION_LINE_BREAK))p->fReplayPenX-=r->tLastVisited.fAdvanceX;\n    p->iFontCount=backend->iFontCount;")
    run = "src/xge_text_run.c"
    replace(run, "while ( sCount < sCountEnd ) {", "while ( !(pRecord && pRecord->bValidatedInput) && sCount < sCountEnd ) {")
    replace(run, "record.iVisited=restore->iVisited-1;record.iRequiredPrefixEnd=restore->iRequiredPrefixEnd;",
            "record.iVisited=restore->iVisited-1;record.iRequiredPrefixEnd=restore->iRequiredPrefixEnd;record.fVisitedAdvance=restore->fReplayPenX;")
    replace(run, "\nvoid xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags)\n{", "\n#include \"xge_text_viewport_paint_internal.inc\"\n\nvoid xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags)\n{")
    include = PATCH/"xge_text_viewport_paint_internal.inc"
    shutil.copy2(include, source/"src"/include.name)
    protected[include] = sha(include)
    header = "src/xge_text_shape_internal.h"
    replace(header, "#endif", "/* Private bounded replay; complete metrics and ink envelopes are retained. */\nint xgeInternalTextPaintViewport(const xge_text_shape_desc_t*,float,float,uint32_t,\n    const xge_text_paint_span_t*,int,void**,void**);\nint xgeInternalTextPaintViewportValid(void*,const xge_text_shape_desc_t*);\nvoid xgeInternalTextPaintViewportFree(void*);\n#endif")
    proxy = "src/xui_proxy_xge.c"
    replace(proxy, "    void* pPaint;\n} xui_proxy_xge_draw_cache_t;", "    void* pPaint;\n    void* pViewport;\n} xui_proxy_xge_draw_cache_t;")
    replace(proxy, "    xrtFree(g_xuiProxyXgeDrawCache.sText);", "    xgeInternalTextPaintViewportFree(g_xuiProxyXgeDrawCache.pViewport);\n    xrtFree(g_xuiProxyXgeDrawCache.sText);")
    path = source/proxy
    original = path.read_text(encoding="utf-8")
    start = original.index("static int __xuiProxyXgeDrawCachePaint(")
    end = original.index("\n#endif", start)
    replacement = '''static int __xuiProxyXgeDrawCachePaint(xui_font pFont, const xge_text_shape_desc_t* pDesc,
    xui_rect_t tRect, uint32_t iColor, const xge_text_paint_span_t* pSpans, int iSpanCount)
{
    xge_emoji_pack pBefore,pAfter;
    uint64_t iBefore,iAfter;
    xge_rect_i_t tClip;
    void* pPaint=NULL;
    char* sCopy=NULL;
    int iRet,bSame;
    AcquireSRWLockExclusive(&g_xuiProxyXgeDrawCacheLock);
    tClip=xgeClipGetPixels();pBefore=__xgeEmojiDrawCacheBuiltinAcquire(&iBefore);
    bSame=pBefore && g_xuiProxyXgeDrawCache.pEmojiPack==pBefore &&
        g_xuiProxyXgeDrawCache.iEmojiRevision==iBefore && g_xuiProxyXgeDrawCache.pFont==pFont &&
        g_xuiProxyXgeDrawCache.iTextSize==pDesc->iTextSize && g_xuiProxyXgeDrawCache.sText &&
        xgeInternalTextPaintViewportValid(g_xuiProxyXgeDrawCache.pViewport,pDesc) &&
        !memcmp(g_xuiProxyXgeDrawCache.sText,pDesc->sText,(size_t)pDesc->iTextSize);
    if (bSame && g_xuiProxyXgeDrawCache.pPaint && g_xuiProxyXgeDrawCache.fX==tRect.fX &&
        g_xuiProxyXgeDrawCache.fY==tRect.fY && g_xuiProxyXgeDrawCache.tClip.iX==tClip.iX &&
        g_xuiProxyXgeDrawCache.tClip.iY==tClip.iY && g_xuiProxyXgeDrawCache.tClip.iW==tClip.iW &&
        g_xuiProxyXgeDrawCache.tClip.iH==tClip.iH) {
        __xgeGlyphRunPaintCacheDraw(g_xuiProxyXgeDrawCache.pPaint,iColor,pSpans,iSpanCount);iRet=XGE_OK;
    } else {
        if (bSame) {__xgeGlyphRunPaintCacheFree(g_xuiProxyXgeDrawCache.pPaint);g_xuiProxyXgeDrawCache.pPaint=NULL;}
        else __xuiProxyXgeDrawCacheClearLocked();
        iRet=xgeInternalTextPaintViewport(pDesc,tRect.fX,tRect.fY,iColor,pSpans,iSpanCount,&pPaint,&g_xuiProxyXgeDrawCache.pViewport);
        if (iRet==XGE_OK && pPaint && pBefore && pDesc->iTextSize<=XUI_PROXY_XGE_DRAW_CACHE_MAX) {
            if (!bSame) {
                sCopy=xrtMalloc((size_t)pDesc->iTextSize);
                if (sCopy) memcpy(sCopy,pDesc->sText,(size_t)pDesc->iTextSize);
            }
            pAfter=__xgeEmojiDrawCacheBuiltinAcquire(&iAfter);
            if ((bSame || sCopy) && pAfter==pBefore && iAfter==iBefore) {
                if (!bSame) {
                    g_xuiProxyXgeDrawCache.pFont=pFont;g_xuiProxyXgeDrawCache.sText=sCopy;sCopy=NULL;
                    g_xuiProxyXgeDrawCache.iTextSize=pDesc->iTextSize;g_xuiProxyXgeDrawCache.pEmojiPack=pBefore;pBefore=NULL;
                    g_xuiProxyXgeDrawCache.iEmojiRevision=iBefore;
                }
                g_xuiProxyXgeDrawCache.tClip=tClip;g_xuiProxyXgeDrawCache.fX=tRect.fX;g_xuiProxyXgeDrawCache.fY=tRect.fY;
                g_xuiProxyXgeDrawCache.pPaint=pPaint;pPaint=NULL;
            }
            xgeEmojiPackFree(pAfter);
        }
        if (!g_xuiProxyXgeDrawCache.pPaint) __xuiProxyXgeDrawCacheClearLocked();
        __xgeGlyphRunPaintCacheFree(pPaint);xrtFree(sCopy);
    }
    xgeEmojiPackFree(pBefore);ReleaseSRWLockExclusive(&g_xuiProxyXgeDrawCacheLock);return iRet;
}'''
    path.write_text(original[:start]+replacement+original[end:], encoding="utf-8")
    edits.append({"path": proxy, "replacedFunction": "__xuiProxyXgeDrawCachePaint"})
    strict = Path(parent["strictBuildScript"])
    assert sha(strict) == parent["strictBuildScriptSha256"]
    protected[strict] = sha(strict)
    shutil.copy2(strict, root/"build_strict_sdk.bat")
    files = [{"path": str(p.relative_to(source)).replace("\\", "/"), "sha256": sha(p), "size": p.stat().st_size}
             for p in sorted(source.rglob("*")) if p.is_file()]
    manifest_path = root/"source-manifest.json"
    manifest = {"root": str(root), "source": str(source), "files": files, "parentManifest": str(parent_path),
                "parentManifestSha256": sha(parent_path), "edits": edits, "publicHeadersChanged": False,
                "productionChanged": False, "sdkInstalled": False, "sdkVersion": 63,
                "byteBucketStride": 4096, "maximumVisiblePaintRecords": 4096, "boundedInkMemoEntries": 64,
                "globalMetricsPreserved": True, "fallbackAndSvgInkEnvelopes": True, "nonmonotonicInkEnvelopes": True,
                "performanceAccepted": False, "selectedForCurrentCandidate": False}
    write(manifest_path, manifest)
    command = [os.environ.get("COMSPEC", r"C:\Windows\System32\cmd.exe"), "/d", "/c", str(root/"build_strict_sdk.bat"), "ide"]
    receipt = root/"sdk-build.json"
    log = root/"actual-sdk-build.log"
    record = {"status": "building", "utc": datetime.now(timezone.utc).isoformat(), "source": str(source),
              "sourceManifest": str(manifest_path), "sourceManifestSha256": sha(manifest_path), "command": command,
              "wholeSdkWerror": True, "strictBuildScriptSha256": sha(root/"build_strict_sdk.bat"),
              "sdkInstalled": False, "productionChanged": False, "log": str(log)}
    write(receipt, record)
    env = dict(os.environ)
    env["PATH"] = r"E:\software\w64devkit\bin;"+env.get("PATH", "")
    started = time.monotonic()
    print(root, flush=True)
    with log.open("xb") as stream:
        child = subprocess.Popen(command, cwd=source, env=env, stdout=stream, stderr=stream, creationflags=0x08000000)
        record["pid"] = child.pid
        while child.poll() is None:
            record["elapsedSeconds"] = round(time.monotonic()-started, 3)
            record["lastConfirmedLiveUtc"] = datetime.now(timezone.utc).isoformat()
            write(receipt, record)
            time.sleep(.5)
        record["exitCode"] = child.returncode
    for path, digest in protected.items():
        assert sha(path) == digest, str(path)
    record["sourceUnchanged"] = all(sha(source/row["path"]) == row["sha256"] for row in files)
    assert record["sourceUnchanged"]
    record.update(status="sdk-build-finished", elapsedSeconds=round(time.monotonic()-started, 3),
                  finishedUtc=datetime.now(timezone.utc).isoformat(), logSha256=sha(log), parentUnchanged=True)
    record["outputs"] = [{"path": str(source/"build/ide"/name), "sha256": sha(source/"build/ide"/name),
                          "size": (source/"build/ide"/name).stat().st_size}
                         for name in ("xge.dll", "xge.lib", "xge_res.o") if (source/"build/ide"/name).exists()]
    write(receipt, record)
    print(json.dumps({"exitCode": child.returncode, "elapsedSeconds": record["elapsedSeconds"], "report": str(receipt)}), flush=True)
    if child.returncode:
        print(log.read_text(encoding="utf-8", errors="replace")[-16000:], flush=True)
    return child.returncode


if __name__ == "__main__":
    raise SystemExit(main())
