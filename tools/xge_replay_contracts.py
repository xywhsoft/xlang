"""Reviewed direct declarations which cannot be inferred from pointer/status syntax.

These reproduce already qualified public APIs. They generate direct calls,
not C adapters. Physical declarations must still match the native header.
"""

CONTRACTS = {
    'xgeGetTitle': (
        'const char* xgeGetTitle(void)',
        'export extern "xgeGetTitle" @header("xge3_runtime.h") @no_raise @c_result("cstring") func getTitle() as string?'),
    'xgeSetTitleChecked': (
        'int xgeSetTitleChecked(const char* sTitle)',
        'export extern "xgeSetTitleChecked" @header("xge3_runtime.h") @no_raise @c_cstr(0) @c_result("status",0) func setTitle(string title)'),
    'xgeResourceProviderClearChecked': (
        'int xgeResourceProviderClearChecked(void)',
        'export extern "xgeResourceProviderClearChecked" @header("xge3_runtime.h") @no_raise @c_result("status",0) func resourceProviderClear()'),
    'xgeEGLCapsGet': (
        'int xgeEGLCapsGet(xge_egl_caps_t* pCaps)',
        'export extern "xgeEGLCapsGet" @header("xge3_runtime.h") @no_raise @c_output(0,0) func eglCapsGet() as EglCaps'),
    'xgeAudioUnitChecked': (
        'int xgeAudioUnitChecked(void)',
        'export extern "xgeAudioUnitChecked" @header("xge3_runtime.h") @no_raise @c_result("status",0) func audioUnit()'),
    'xgeSoundFallbackSet': (
        'int xgeSoundFallbackSet(const char* sPath)',
        'export extern "xgeSoundFallbackSet" @header("xge3_runtime.h") @no_raise @c_cstr(0) @c_result("status",0) func soundFallbackSet(string path)'),
    'xgeSoundFallbackClear': (
        'void xgeSoundFallbackClear(void)',
        'export extern "xgeSoundFallbackClear" @header("xge3_runtime.h") @no_raise func soundFallbackClear()'),
    'xgeClipboardGetData': (
        'int xgeClipboardGetData(const char* sFormat, void* pData, size_t iCapacity)',
        'export extern "xgeClipboardGetData" @header("xge3_runtime.h") @c_cstr(0) @c_span(1,"void*","size_t") func clipboardGetData(string sFormat,Buffer pData) as c_int'),
    'xgeClipboardGetTextOwned': (
        'int xgeClipboardGetTextOwned(char** psText, size_t* piLength)',
        'export extern "xgeClipboardGetTextOwned" @header("xge3_runtime.h") @no_raise @c_output_text(0,1,"char*","size_t","xrt",0) func clipboardGetText() as string'),
    'xgeClipboardGetDataOwned': (
        'int xgeClipboardGetDataOwned(const char* sFormat, void** ppData, size_t* piLength)',
        'export extern "xgeClipboardGetDataOwned" @header("xge3_runtime.h") @no_raise @c_cstr(0) @c_output_bytes(1,2,"void*","size_t","xrt",0) func clipboardReadData(string format) as bytes'),
    'xgeGamepadGetState': (
        'int xgeGamepadGetState(int iGamepad, xge_gamepad_state_t* pState)',
        'export extern "xgeGamepadGetState" @header("xge3_runtime.h") @no_raise @c_output(1,0) func gamepadGetState(c_int iGamepad) as GamepadState'),
    'xgeGamepadSetState': (
        'int xgeGamepadSetState(int iGamepad, const xge_gamepad_state_t* pState)',
        'export extern "xgeGamepadSetState" @header("xge3_runtime.h") @no_raise @c_ref(1) @c_result("status",0) func gamepadSetState(c_int iGamepad,GamepadState state)'),
    'xgeMiniProgramInit': (
        'int xgeMiniProgramInit(const xge_miniprogram_desc_t* pDesc)',
        'export extern "xgeMiniProgramInit" @header("xge3_runtime.h") @no_raise @c_result("status",0) '
        '@c_pack(0,"xge_miniprogram_desc_t",0,3) @c_pack_field(0,"iWidth",0,"value") '
        '@c_pack_field(0,"iHeight",1,"value") @c_pack_field(0,"fDevicePixelRatio",2,"value") '
        'func miniProgramInit(c_int width=0,c_int height=0,c_float devicePixelRatio=1)'),
    'xgeMiniProgramTouch': (
        'int xgeMiniProgramTouch(int iPhase, const xge_miniprogram_touch_t* pTouches, int iCount)',
        'export extern "xgeMiniProgramTouch" @header("xge3_runtime.h") @no_raise @c_array_group(1,1,1,"int",2) '
        '@c_array_input(1,0,"const xge_miniprogram_touch_t*","value") '
        'func miniProgramTouch(c_int phase,array<MiniprogramTouch> touches,int size=-1) as c_int'),
    'xgeMiniProgramUnitChecked': (
        'int xgeMiniProgramUnitChecked(void)',
        'export extern "xgeMiniProgramUnitChecked" @header("xge3_runtime.h") @no_raise @c_result("status",0) func miniProgramUnitChecked()'),
    'xgeMiniProgramAudioCommand': (
        'int xgeMiniProgramAudioCommand(int iCommand, int iHandle, const void* pData, int iSize)',
        'export extern "xgeMiniProgramAudioCommand" @header("xge3_runtime.h") @no_raise @c_span(2,"const void*","int") @c_result("integer_min",0) func miniProgramAudioCommand(c_int command,c_int handle,bytes data) as c_int\n'
        'export extern "xgeMiniProgramAudioCommand" @header("xge3_runtime.h") @no_raise @c_span(2,"const void*","int") @c_result("integer_min",0) func miniProgramAudioCommand(c_int command,c_int handle,Buffer data) as c_int'),
    'xgeTouchFind': (
        'int xgeTouchFind(uint64_t iId, xge_touch_point_t* pPoint)',
        'export extern "xgeTouchFind" @header("xge3_runtime.h") @no_raise @c_outputs(-1,1) func touchFind(uint64 id) as (c_int,TouchPoint)'),
    'xge3dTransformMatrix': (
        'int xge3dTransformMatrix(const xge3d_transform_t *transform, xge3d_mat4_t *out)',
        'export extern "xge3dTransformMatrix" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_output(1,0) func transformMatrix(Xge3dTransform transform) as Xge3dMat4'),
    'xge3dCameraPerspective': (
        'int xge3dCameraPerspective(xge3d_camera_t *out, float fov_y, float aspect, float near_z, float far_z)',
        'export extern "xge3dCameraPerspective" @header("xge3_runtime.h") @no_raise @c_output(0,0) func perspectiveCamera(c_float verticalFov,c_float aspect,c_float nearPlane,c_float farPlane) as Xge3dCamera'),
    'xge3dCameraOrthographic': (
        'int xge3dCameraOrthographic(xge3d_camera_t *out, float width, float height, float near_z, float far_z)',
        'export extern "xge3dCameraOrthographic" @header("xge3_runtime.h") @no_raise @c_output(0,0) func orthographicCamera(c_float width,c_float height,c_float nearPlane,c_float farPlane) as Xge3dCamera'),
    'xge3dRelativePosition': (
        'int xge3dRelativePosition(xge3d_dvec3_t origin, xge3d_dvec3_t global, xge3d_vec3_t *out)',
        'export extern "xge3dRelativePosition" @header("xge3_runtime.h") @no_raise @c_output(2,0) func relativePosition(Xge3dDvec3 origin,Xge3dDvec3 globalPosition) as Xge3dVec3'),
    'xge3dCameraScreenRay': (
        'int xge3dCameraScreenRay(const xge3d_camera_t *camera, float x, float y, float width, float height, xge3d_ray_t *out)',
        'export extern "xge3dCameraScreenRay" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_output(5,0) func screenRay(Xge3dCamera camera,c_float x,c_float y,c_float width,c_float height) as Xge3dRay'),
    'xgeDebugDumpCaps': (
        'int xgeDebugDumpCaps(char* sBuffer, int iSize)',
        'export extern "xgeDebugDumpCaps" @header("xge3_runtime.h") @no_raise @c_span(0,"char*","int") func debugDumpCaps(Buffer output) as c_int'),
    'xge3dRayAabb': (
        'int xge3dRayAabb(const xge3d_ray_t *ray, const xge3d_aabb_t *box, float max_distance, float *out_distance)',
        'export extern "xge3dRayAabb" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_ref(1) @c_outputs(-1,3) func rayAabb(Xge3dRay ray,Xge3dAabb box,c_float maxDistance) as (c_int,c_float)'),
    'xge3dRaySphere': (
        'int xge3dRaySphere(const xge3d_ray_t *ray, const xge3d_sphere_t *sphere, float max_distance, float *out_distance)',
        'export extern "xge3dRaySphere" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_ref(1) @c_outputs(-1,3) func raySphere(Xge3dRay ray,Xge3dSphere sphere,c_float maxDistance) as (c_int,c_float)'),
    'xge3dRayTriangle': (
        'int xge3dRayTriangle(const xge3d_ray_t *ray, const xge3d_vec3_t triangle[3], float max_distance, xge3d_hit_t *out)',
        'export extern "xge3dRayTriangle" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_ref(1) @c_outputs(-1,3) func rayTriangle(Xge3dRay ray,Xge3dVec3[3] triangle,c_float maxDistance) as (c_int,Xge3dHit)'),
}

REPLACED = {
    'xge3dAnimationLayerDefault': ('xge3d.animationOptions','xge3dAnimationOptionsDefaultInto'),
    'xge3dAnimatorCreate': ('xge3d.Scene.animator','xge3dAnimatorCreateOwned'),
    'xge3dClipFromModel': ('xge3d.Model3D.clip','xge3dClipFromModelOwned'),
    'xge3dClipInfo': ('xge3d.AnimationClip.info','xge3dClipInfoChecked'),
    'xge3dClipLoad': ('xge3d.loadClip','xge3dClipLoadOwned'),
    'xge3dClipLoadBudget': ('xge3d.loadClip','xge3dClipLoadOwned'),
    'xge3dClipNodeName': ('xge3d.AnimationClip.nodeName','xge3dClipNodeNameChecked'),
    'xgeClipboardGetText': ('xge.clipboardGetText','xgeClipboardGetTextOwned'),
    'xgeSetTitle': ('xge.setTitle', 'xgeSetTitleChecked'),
    'xgeMiniProgramSetBridge': ('xge.setMiniProgramBridge', 'xgeMiniProgramSetBridgeOwned'),
    'xgeResourceProviderClear': ('xge.resourceProviderClear', 'xgeResourceProviderClearChecked'),
    'xgeAudioUnit': ('xge.audioUnit', 'xgeAudioUnitChecked'),
    'xgeSvgCacheInvalidate': ('xge.SvgCache.remove', 'xgeSvgCacheRemove'),
    'xgeSvgCacheClear': ('xge.SvgCache.reset', 'xgeSvgCacheReset'),
    'xge3dMotionDefault': ('xge3d.threeDMotionDefault', 'xge3dMotionDefaultInto'),
    'xge3dShapeDefault': ('xge3d.shapeSettings', 'xge3dShapeSettingsDefault'),
    'xge3dSceneQueryAabb': ('xge3d.Scene.queryAabb', 'xge3dSceneQueryAabbInto'),
    'xge3dSceneQueryAabbEx': ('xge3d.Scene.queryAabb', 'xge3dSceneQueryAabbInto'),
    'xge3dSceneOverlapShape': ('xge3d.Scene.overlap', 'xge3dSceneOverlapInto'),
    'xge3dSceneContactShape': ('xge3d.Scene.contacts', 'xge3dSceneContactInto'),
    'xge3dSceneSweepShape': ('xge3d.Scene.sweep', 'xge3dSceneSweepSettings'),
    'xge3dSceneMoveShape': ('xge3d.Scene.moveShape', 'xge3dSceneMoveSettings'),
    'xge3dDebugShape': ('xge3d.Renderer.debugShape', 'xge3dDebugShapeSettings'),
    'xge3dCameraLookAt': ('xge3d.viewLookAt', 'xge3dViewLookAt'),
    'xge3dCameraLookAtGlobal': ('xge3d.viewLookAtGlobal', 'xge3dViewLookAtGlobal'),
    'xge3dTextureCreate': ('xge3d.createTexture', 'xge3dTextureCreateFromImageOwned'),
    'xge3dMaterialCreate': ('xge3d.createMaterial', 'xge3dMaterialCreateStateOwned'),
    'xge3dMaterialDefault': ('xge3d.materialSettingsDefault', 'xge3dMaterialSettingsDefault'),
    'xge3dMaterialGetDesc': ('xge3d.Material3D.snapshot', 'xge3dMaterialCaptureOwned'),
    'xge3dMaterialSetDesc': ('xge3d.Material3D.setState', 'xge3dMaterialSetState'),
    'xgeRenderTargetCreate': ('xge.createRenderTarget', 'xgeRenderTargetCreateOwned'),
    'xgeRenderTargetWindow': ('xge.createWindowTarget', 'xgeRenderTargetWindowOwned'),
    'xgeRenderTargetReadPixels': ('xge.RenderTarget.readPixels', 'xgeRenderTargetReadPixelsSized'),
    'xgePassInit': ('xge.createPass', 'xgePassCreateOwned'),
    'xgeAsyncImageLoad': ('xge.ImageLoad.start', 'xgeAsyncImageStartOwned'),
    'xgeAsyncTextureLoad': ('xge.TextureLoad.start', 'xgeAsyncTextureStartOwned'),
    'xgeAsyncPoll': ('xge.ImageLoad.poll', 'xgeAsyncLoadPollOwned'),
    'xgeAsyncRequestCancel': ('xge.ImageLoad.cancel', 'xgeAsyncLoadCancelOwned'),
    'xgeEmojiPackAddRef': ('xge.EmojiPack.share', 'xgeEmojiPackShareOwned'),
    'xgeEmojiPackMatch': ('xge.EmojiPack.find', 'xgeEmojiPackFind'),
    'xgeTextShape': ('xge.shapeText', 'xgeTextShapeOwned'),
    'xuiCodeSearchFindPlain': ('xui.CodeDocument.findPlain', 'xuiCodeSearchLocatePlain'),
    'xuiCodeSearchFindPlainRange': ('xui.CodeDocument.findPlain', 'xuiCodeSearchLocatePlain'),
    'xuiCodeSearchFindRegex': ('xui.CodeDocument.findRegex', 'xuiCodeSearchLocateRegex'),
    'xuiCodeSearchFindRegexRange': ('xui.CodeDocument.findRegex', 'xuiCodeSearchLocateRegex'),
    'xuiFindText': ('xui.findText', 'xuiFindLocateText'),
    'xuiFindCollectText': ('xui.findAllText', 'xuiFindCollectMatches'),
    'xuiDebugWidgetInspectAt': ('xui.Context.inspectWidgetAt', 'xuiDebugWidgetInspectAtChecked'),
    'xgeActionsReset': ('xge.Actions.reset', 'xgeActionsResetChecked'),
    'xgeAudioClipSamples': ('xge.AudioClip.samples', 'xgeAudioClipGetSamples'),
    'xgeFontGlyphRasterize': ('xge.Font.rasterize', 'xgeFontGlyphRasterizeOwned'),
    'xgeFontGlyphRasterizeByIndex': ('xge.Font.rasterizeByIndex', 'xgeFontGlyphRasterizeByIndexOwned'),
    'xgeImageLoad': ('xge.loadImage', 'xgeImageLoadEx'),
    'xgeImageLoadMemory': ('xge.loadImage', 'xgeImageLoadMemoryEx'),
    'xgeParticleCollidePlane': ('xge.particleSweepPlane', 'xgeParticleSweepPlane'),
    'xgeParticleCollideRect': ('xge.particleSweepRect', 'xgeParticleSweepRect'),
    'xgeParticleEventPoll': ('xge.ParticleWorld.pollEvent', 'xgeParticleWorldPollEvent'),
    'xgeShaderVariantGet': ('xge.ShaderVariants.get', 'xgeShaderVariantGetOwned'),
    'xgeShapeExClipShapeGetAt': ('xge.Shape.clipShapeAt', 'xgeShapeExClipShapeGetAtEx'),
    'xgeShapeExSceneClipShapeGetAt': ('xge.ShapeScene.clipShapeAt', 'xgeShapeExSceneClipShapeGetAtEx'),
    'xgeShapeExGetPath': ('xge.Shape.capturePath', 'xgeShapeExCapturePath'),
    'xgeShapeExFillLinearGradientGet': ('xge.Shape.captureFillGradient', 'xgeShapeExCaptureFillGradient'),
    'xgeShapeExFillRadialGradientGet': ('xge.Shape.captureFillGradient', 'xgeShapeExCaptureFillGradient'),
    'xgeShapeExFillRadialGradientGetEx': ('xge.Shape.captureFillGradient', 'xgeShapeExCaptureFillGradient'),
    'xgeShapeExStrokeLinearGradientGet': ('xge.Shape.captureStrokeGradient', 'xgeShapeExCaptureStrokeGradient'),
    'xgeShapeExStrokeRadialGradientGet': ('xge.Shape.captureStrokeGradient', 'xgeShapeExCaptureStrokeGradient'),
    'xgeShapeExStrokeRadialGradientGetEx': ('xge.Shape.captureStrokeGradient', 'xgeShapeExCaptureStrokeGradient'),
    'xgeShapeExStrokeDashGet': ('xge.Shape.captureStrokeDash', 'xgeShapeExCaptureStrokeDash'),
    'xgeNinePatchDraw': ('xge.Texture.drawNinePatch', 'xgeTextureDrawNinePatch'),
    'xgeTextureCreateRGBA': ('xge.createTexture', 'xgeTextureCreateRGBAExOwned'),
    'xgeTextureCreateRGBAEx': ('xge.createTexture', 'xgeTextureCreateRGBAExOwned'),
    'xgeTextureCreateFromImage': ('xge.createTexture', 'xgeTextureCreateFromImageOwned'),
    'xgeTextureCreateFromImageEx': ('xge.createTexture', 'xgeTextureCreateFromImageOwned'),
    'xgeTextureCreateYUV420P': ('xge.createYUV420PTexture', 'xgeTextureCreateYUV420POwned'),
    'xgeTextureFallbackGet': ('xge.textureFallbackGet', 'xgeTextureFallbackGetOwned'),
    'xgeTextureFallbackSetRGBA': ('xge.textureFallbackSetRGBA', 'xgeTextureFallbackSetRGBASized'),
    'xgeTextureLoad': ('xge.loadTexture', 'xgeTextureLoadExOwned'),
    'xgeTextureLoadEx': ('xge.loadTexture', 'xgeTextureLoadExOwned'),
    'xgeTextureLoadMemory': ('xge.loadTexture', 'xgeTextureLoadMemoryExOwned'),
    'xgeTextureLoadMemoryEx': ('xge.loadTexture', 'xgeTextureLoadMemoryExOwned'),
    'xgeTextureReadPixels': ('xge.Texture.readPixels', 'xgeTextureReadPixelsSized'),
    'xgeTextureUpdateRGBA': ('xge.Texture.updateRGBA', 'xgeTextureUpdateRGBASized'),
    'xgeTextureUpdateYUV420P': ('xge.Texture.updateYUV420P', 'xgeTextureUpdateYUV420PSized'),
}

SPANS = {'xgeClipboardGetData': ('void*', 'size_t'), 'xgeDebugDumpCaps': ('char*', 'int'),
         'xgeMiniProgramAudioCommand': ('const void*', 'int')}
ARRAYS = {'xgeMiniProgramTouch'}

def validate(functions):
    for name,(native,_) in CONTRACTS.items():
        if name not in functions or functions[name]['declaration']!=native:
            raise ValueError('reviewed direct declaration changed: '+name)
    return {name:logical for name,(_,logical) in CONTRACTS.items()}
