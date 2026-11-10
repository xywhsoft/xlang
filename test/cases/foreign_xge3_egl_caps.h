#ifndef FIXTURE_XGE3_EGL_CAPS_H
#define FIXTURE_XGE3_EGL_CAPS_H
#include <xge.h>
#include <string.h>
static inline int fixtureEglExpected(void){return XGE_ENABLE_EGL;}
/* Raw native context stays inside this synchronous test helper; this is not a
 * public language session/renderer API or a substitute for its ownership work. */
static inline int fixtureEglRawAttempt(int surfaceless)
{
 xge_platform_backend_t platform=xgePlatformBackendGet();
 xge_graphics_backend_t graphics=xgeGraphicsBackendGet();
 xge_egl_desc_t desc;memset(&desc,0,sizeof(desc));desc.iWidth=desc.iHeight=1;
 desc.bSurfaceless=surfaceless;desc.bPBuffer=!surfaceless;
 xge_egl_context_t context;memset(&context,0,sizeof(context));
 int result=xgeEGLInit(&context,&desc);
 if(result==XGE_OK){
  if(!context.bInitialized || !context.bCleanupPending)result=XGE_ERROR_INVALID_STATE;
  else result=xgeEGLMakeCurrent(&context);
 }
 int cleanup=xgeEGLUnitChecked(&context);
 if(result==XGE_OK && cleanup!=XGE_OK)result=cleanup;
 if(context.bInitialized || context.bCleanupPending)result=XGE_ERROR_INVALID_STATE;
 xgePlatformBackendSet(&platform);xgeGraphicsBackendSet(&graphics);
 return result;
}
#endif
