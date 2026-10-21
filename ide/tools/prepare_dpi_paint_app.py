"""Build a private DPI-proxy object; preserve the current product and its inputs."""
from datetime import datetime, timezone
import hashlib, json, os, shutil, subprocess, uuid
from pathlib import Path

IDE = Path(__file__).resolve().parents[1]

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def main():
    pointer_path = IDE/'build/current-build.json'
    pointer = read(pointer_path)
    report = Path(pointer['report'])
    base = read(report)
    assert sha(report) == pointer['reportSha256'] and base['exitCode'] == 0
    original = IDE/'src/platform/dpi_proxy.c'
    row = next(r for r in base['inputs'] if r['path'] == 'src/platform/dpi_proxy.c')
    assert sha(original) == row['sha256']
    protected = {pointer_path: sha(pointer_path), report: sha(report), original: sha(original), Path(__file__): sha(__file__)}
    root = IDE/'build'/('editor-app-dpi-paint-'+uuid.uuid4().hex[:12])
    root.mkdir()
    source = root/'dpi_proxy.c'
    content = original.read_text(encoding='utf-8')
    edits = []
    def replace(old, new):
        nonlocal content
        assert content.count(old) == 1, (old, content.count(old))
        content = content.replace(old, new)
        edits.append({'before': old, 'after': new})
    replace('#define FONT_MAGIC', '#ifndef XIDE_PAINT_CURSOR_ENABLED\n#define XIDE_PAINT_CURSOR_ENABLED 0\n#endif\n#define FONT_MAGIC')
    replace('float size; struct dpi_font* next;', 'float size;\n#if XIDE_PAINT_CURSOR_ENABLED\n    xui_text_shape_cursor paintCursor; char* paintText; int paintLength;\n#endif\n    struct dpi_font* next;')
    helper = '''/* Keep physical paint checkpoints separate from logical caret geometry.
   Only the current long segment is retained; failing this optional cache leaves
   the normal painter in charge. */
static void clearPaintCursor(dpi_font* f)
{
#if XIDE_PAINT_CURSOR_ENABLED
    xuiProxyXgeTextShapeCursorFree(f->paintCursor,NULL);
    free(f->paintText); f->paintCursor=NULL; f->paintText=NULL; f->paintLength=0;
#else
    (void)f;
#endif
}
#if XIDE_PAINT_CURSOR_ENABLED
static int ignorePaintCluster(const xui_text_cluster_t* cluster,void* user)
{ (void)cluster; (void)user; return XUI_OK; }
static void preparePaintCursor(xide_dpi_proxy* s,dpi_font* f,const char* text,int length)
{
    if(!f || f->raw==f->base || !text || length<65536 || length>32*1024*1024) return;
    if(f->paintLength==length && f->paintText && !memcmp(f->paintText,text,(size_t)length)) return;
    char* saved=malloc((size_t)length+1);
    if(!saved) return;
    memcpy(saved,text,(size_t)length); saved[length]=0;
    int common=0,limit=f->paintLength<length?f->paintLength:length;
    while(common<limit && f->paintText[common]==text[common]) ++common;
    xui_text_shape_t shape={0}; xui_text_shape_cursor fresh=NULL;
    xui_text_shape_resume_t resume={0}; resume.iSize=sizeof(resume);
    int result=xuiProxyXgeVisitTextShapeCached(&s->raw,f->raw,text,length,XUI_TEXT_SHAPE_DEFAULT,
        ignorePaintCluster,NULL,&shape,f->paintCursor,common,&fresh,&resume,NULL);
    xuiTextShapeFree(&shape);
    if(result!=XUI_OK || !fresh) {
        xuiProxyXgeTextShapeCursorFree(fresh,NULL); free(saved); return;
    }
    clearPaintCursor(f); f->paintCursor=fresh; f->paintText=saved; f->paintLength=length;
}
#endif

'''
    replace('static void fontDestroy(xui_proxy p,xui_font value)', helper+'static void fontDestroy(xui_proxy p,xui_font value)')
    replace('    if(f->raw!=f->base) { s->raw.fontDestroy', '    clearPaintCursor(f);\n    if(f->raw!=f->base) { s->raw.fontDestroy')
    replace('{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawTextSpans',
            '{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;\n#if XIDE_PAINT_CURSOR_ENABLED\n    preparePaintCursor(s,font(pFont),sText,iTextSize);\n#endif\n    return s->raw.drawTextSpans')
    replace('for(dpi_font* f=s->fonts;f;f=f->next) { if(f->raw!=f->base) s->raw.fontDestroy',
            'for(dpi_font* f=s->fonts;f;f=f->next) { clearPaintCursor(f); if(f->raw!=f->base) s->raw.fontDestroy')
    source.write_text(content, encoding='utf-8')
    shutil.copy2(__file__, root/Path(__file__).name)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-DXGE_DLL', '-DXUI_DLL',
             '-DXGE_XRT_PROFILE_IDE', '-DXGE_DEBUGMODE=0', '-DXIDE_STREAM_LAYOUT_ENABLED=1', '-DXIDE_CURSOR_LAYOUT_ENABLED=1',
             '-DXIDE_PAINT_CURSOR_ENABLED=1', '-O2', '-finput-charset=UTF-8', '-fexec-charset=UTF-8',
             '-I'+str(IDE/'src'), '-I'+str(IDE/'third_party/xge_sdk'), '-I'+str(IDE/'third_party/xge_sdk/lib/xrt')]
    output = root/'dpi_proxy.c.o'
    command = [base['compiler'], *flags, '-c', str(source), '-o', str(output)]
    with (root/'compile.log').open('xb') as log:
        child = subprocess.run(command, stdout=log, stderr=log, creationflags=0x08000000)
    for path, digest in protected.items():
        assert sha(path) == digest, str(path)
    record = {'status': 'private-dpi-paint-object-passed' if child.returncode == 0 else 'private-dpi-paint-object-failed',
              'root': str(root), 'utc': datetime.now(timezone.utc).isoformat(), 'exitCode': child.returncode,
              'baseBuildReport': str(report), 'baseBuildReportSha256': sha(report), 'originalSource': str(original),
              'originalSourceSha256': sha(original), 'source': str(source), 'sourceSha256': sha(source),
              'replaceObject': 'build/Release/src_platform_dpi_proxy.c.o', 'object': str(output),
              'objectSha256': sha(output) if output.exists() else None, 'command': command, 'wholeObjectWerror': True,
              'paintCursorEnabled': True, 'productionChanged': False, 'edits': edits,
              'log': str(root/'compile.log'), 'logSha256': sha(root/'compile.log')}
    receipt = root/'app-build.json'
    receipt.write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'report': str(receipt), 'exitCode': child.returncode}), flush=True)
    if child.returncode:
        print((root/'compile.log').read_text(encoding='utf-8', errors='replace'), flush=True)
    return child.returncode

if __name__ == '__main__':
    raise SystemExit(main())
