"""Apply the bounded Windows GL3.3/startup-failure fix to the matched XGE3 source.

Public headers and the XRT implementation remain unchanged. Reject other input
revisions rather than transplanting a graphics patch into an unknown SDK.
"""
from pathlib import Path
import argparse,hashlib,json

PINS={
 'src/xge/core/xge_impl.c':'a52ca344a5878c04a85849f03f6f68210a83c9a82bc1f41e36b0450779ef41b8',
 'src/xge/core/xge_core.c':'67e6d695381b03ce5e5acb1d02c68d8d2edfb5a09db62a6b179a5e9fce632def',
 'extlibs/sokol/sokol_app.h':'43d69d2dd52a8a6f52d57ccafb00967742ac499b7fcf5bdcdd6d978c869aa1ad'}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def once(s,old,new):
 if s.count(old)!=1:raise ValueError('Unexpected source boundary: '+old[:100])
 return s.replace(old,new,1)
def bounds(s,name):
 at=s.index(name);start=s.rfind('\n',0,at)+1;brace=s.index('{',at);level=1;end=brace+1
 while level:
  if s[end]=='{':level+=1
  elif s[end]=='}':level-=1
  end+=1
 return start,end
def function(s,name,change):
 a,b=bounds(s,name);return s[:a]+change(s[a:b])+s[b:]
def apply(source):
 source=Path(source);texts={};rows=[]
 for name,digest in PINS.items():
  p=source/name
  if sha(p)!=digest:raise ValueError('Mismatched XGE input: '+name)
  raw=p.read_bytes();texts[name]=(raw.decode('utf8').replace('\r\n','\n'),b'\r\n' in raw)
 impl,crlf=texts['src/xge/core/xge_impl.c']
 a,b=bounds(impl,'static void __xgeSokolLog(')
 impl=impl[:a]+'''/* A returning logger must not turn a failed WGL startup into success. */
static int g_xgeSokolFailure;
static void __xgeSokolLog(const char* sTag, uint32_t iLogLevel, uint32_t iLogItemId, const char* sMessage, uint32_t iLine, const char* sFile, void* pUser)
{
    unsigned long osError=0;
#if defined(_WIN32) || defined(_WIN64)
    osError=GetLastError();
#endif
    (void)sFile; (void)pUser;
    int level=iLogLevel==0 ? XGE_LOG_FATAL : iLogLevel==1 ? XGE_LOG_ERROR : iLogLevel==2 ? XGE_LOG_WARN : XGE_LOG_INFO;
    const char* message=sMessage && sMessage[0] ? sMessage : "Graphics/window initialization failed; OpenGL 3.3 core is required.";
    __xgeLogFormat(level,sTag && sTag[0] ? sTag : "sokol","item=%u line=%u %s",(unsigned)iLogItemId,(unsigned)iLine,message);
    if(iLogLevel==0) {
        char detail[768]; g_xgeSokolFailure=XGE_ERROR;
        snprintf(detail,sizeof(detail),"%s (item=%u, line=%u, Windows=%lu; requested OpenGL 3.3 core)",message,(unsigned)iLogItemId,(unsigned)iLine,osError);
        __xgeReportErrorInternal(XGE_ERROR,0,"graphics","sapp/wgl",detail);
    }
}'''+impl[b:]
 impl=once(impl,'\tobjDesc.sample_count = 1;','''#if defined(SOKOL_GLCORE) && (defined(_WIN32) || defined(_WIN64))
    /* The renderer uses GLSL330; do not inherit Sokol's GL4.3 default. */
    objDesc.gl.major_version = 3;
    objDesc.gl.minor_version = 3;
#endif
\tobjDesc.sample_count = 1;''')
 texts['src/xge/core/xge_impl.c']=(impl,crlf)
 core,crlf=texts['src/xge/core/xge_core.c']
 core=once(core,'\tsapp_run(&objDesc);','\tg_xgeSokolFailure = 0;\n\tsapp_run(&objDesc);')
 core=once(core,'\treturn XGE_OK;\n#endif\n}\n\nvoid xgeSetQuitRequestCallback','\treturn g_xgeSokolFailure ? XGE_ERROR : XGE_OK;\n#endif\n}\n\nvoid xgeSetQuitRequestCallback')
 texts['src/xge/core/xge_core.c']=(core,crlf)
 sapp,crlf=texts['extlibs/sokol/sokol_app.h']
 def init(s):
  s=once(s,'void _sapp_wgl_init(void)','bool _sapp_wgl_init(void)')
  for code in ['WIN32_LOAD_OPENGL32_DLL_FAILED','WIN32_CREATE_HELPER_WINDOW_FAILED','WIN32_HELPER_WINDOW_GETDC_FAILED']:
   s=once(s,'_SAPP_PANIC('+code+');','_SAPP_PANIC('+code+');\n        return false;')
  for name in ['opengl32','CreateContext','DeleteContext','GetProcAddress','GetCurrentDC','MakeCurrent','GetIntegerv','msg_hwnd']:
   s=once(s,'SOKOL_ASSERT(_sapp.wgl.'+name+');','if (!_sapp.wgl.'+name+') { _SAPP_PANIC_MSG(WIN32_LOAD_OPENGL32_DLL_FAILED, "Required OpenGL/WGL resource unavailable"); return false; }')
  return s[:-1]+'    return true;\n}'
 sapp=function(sapp,'_SOKOL_PRIVATE void _sapp_wgl_init(void)',init)
 sapp=function(sapp,'_SOKOL_PRIVATE void _sapp_wgl_shutdown(void)',lambda s:'''_SOKOL_PRIVATE void _sapp_wgl_shutdown(void) {
    if (_sapp.wgl.msg_dc && _sapp.wgl.msg_hwnd) { ReleaseDC(_sapp.wgl.msg_hwnd, _sapp.wgl.msg_dc); _sapp.wgl.msg_dc = 0; }
    if (_sapp.wgl.msg_hwnd) { DestroyWindow(_sapp.wgl.msg_hwnd); _sapp.wgl.msg_hwnd = 0; }
    if (_sapp.wgl.opengl32) { FreeLibrary(_sapp.wgl.opengl32); _sapp.wgl.opengl32 = 0; }
}''')
 def extensions(s):
  s=once(s,'void _sapp_wgl_load_extensions(void)','bool _sapp_wgl_load_extensions(void)')
  for code in ['WIN32_DUMMY_CONTEXT_SET_PIXELFORMAT_FAILED','WIN32_CREATE_DUMMY_CONTEXT_FAILED']:
   s=once(s,'_SAPP_PANIC('+code+');','_SAPP_PANIC('+code+');\n        return false;')
  s=once(s,'_SAPP_PANIC(WIN32_DUMMY_CONTEXT_MAKE_CURRENT_FAILED);','_SAPP_PANIC(WIN32_DUMMY_CONTEXT_MAKE_CURRENT_FAILED);\n        _sapp.wgl.DeleteContext(rc);\n        return false;')
  return s[:-1]+'    return true;\n}'
 sapp=function(sapp,'_SOKOL_PRIVATE void _sapp_wgl_load_extensions(void)',extensions)
 def context(s):
  s=once(s,'void _sapp_wgl_create_context(void)','bool _sapp_wgl_create_context(void)')
  for code in ['WIN32_WGL_FIND_PIXELFORMAT_FAILED','WIN32_WGL_DESCRIBE_PIXELFORMAT_FAILED','WIN32_WGL_SET_PIXELFORMAT_FAILED','WIN32_WGL_ARB_CREATE_CONTEXT_REQUIRED','WIN32_WGL_ARB_CREATE_CONTEXT_PROFILE_REQUIRED']:
   s=once(s,'_SAPP_PANIC('+code+');','_SAPP_PANIC('+code+');\n        return false;')
  s=once(s,'if (!_sapp.wgl.arb_create_context) {','if (!_sapp.wgl.arb_create_context || !_sapp.wgl.CreateContextAttribsARB) {')
  s=once(s,'    _sapp.wgl.MakeCurrent(_sapp.win32.dc, _sapp.wgl.gl_ctx);','''    if (!_sapp.wgl.gl_ctx) { return false; }
    if (!_sapp.wgl.MakeCurrent(_sapp.win32.dc, _sapp.wgl.gl_ctx)) {
        _SAPP_PANIC_MSG(WIN32_WGL_CREATE_CONTEXT_ATTRIBS_FAILED_OTHER, "wglMakeCurrent failed for OpenGL 3.3 context");
        return false;
    }''')
  return s[:-1]+'    return true;\n}'
 sapp=function(sapp,'_SOKOL_PRIVATE void _sapp_wgl_create_context(void)',context)
 sapp=function(sapp,'_SOKOL_PRIVATE void _sapp_wgl_destroy_context(void)',lambda s:'''_SOKOL_PRIVATE void _sapp_wgl_destroy_context(void) {
    if (_sapp.wgl.gl_ctx) {
        _sapp.wgl.MakeCurrent(_sapp.win32.dc, 0);
        _sapp.wgl.DeleteContext(_sapp.wgl.gl_ctx);
        _sapp.wgl.gl_ctx = 0;
    }
}''')
 sapp=once(sapp,'''        _sapp_wgl_init();
        _sapp_wgl_load_extensions();
        _sapp_wgl_create_context();''','''        if (!_sapp_wgl_init() || !_sapp_wgl_load_extensions() || !_sapp_wgl_create_context()) {
            /* No init/frame callbacks ran. Dispose only acquired platform resources. */
            _sapp_wgl_destroy_context();
            _sapp_wgl_shutdown();
            _sapp_win32_destroy_window();
            _sapp_win32_destroy_icons();
            _sapp_win32_restore_console();
            _sapp_win32_free_raw_input_data();
            _sapp_discard_state();
            return;
        }''')
 texts['extlibs/sokol/sokol_app.h']=(sapp,crlf)
 for name,(text,crlf) in texts.items():
  p=source/name;raw=text.replace('\n','\r\n').encode('utf8') if crlf else text.encode('utf8');p.write_bytes(raw)
  rows.append(dict(path=name,beforeSha256=PINS[name],afterSha256=sha(p)))
 return rows
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--source',type=Path,required=True);ap.add_argument('--report',type=Path,required=True);a=ap.parse_args()
 a.report.write_text(json.dumps(dict(status='applied',files=apply(a.source)),indent=2)+'\n',encoding='utf8')
