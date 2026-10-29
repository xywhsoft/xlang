#ifndef XGE_NO_AUDIO
static xge_sound __xgeAudioAsSound(void* pObject)
{
	return (xge_sound)pObject;
}

static int __xgeAudioLoadPath(void* pObject, const char* sPath, int iType, uint32_t iFlags, xge_audio_group pGroup)
{
	xge_sound pSound;
	ma_sound* pMaSound;
	ma_sound_group* pMaGroup;
	char* sFullPath;
	ma_uint32 iMaFlags;

	if ( (pObject == NULL) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( xgeAudioIsReady() == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	pSound = __xgeAudioAsSound(pObject);
	memset(pSound, 0, sizeof(*pSound));
	pMaSound = (ma_sound*)xrtMalloc(sizeof(*pMaSound));
	if ( pMaSound == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	sFullPath = __xgePathResolve(sPath);
	if ( sFullPath == NULL ) {
		xrtFree(pMaSound);
		__xgeLogFormat(XGE_LOG_WARN, "audio", "audio path resolve failed: %s", sPath);
		return XGE_ERROR_UNSUPPORTED;
	}
	iMaFlags = iFlags;
	pMaGroup = NULL;
	if ( pGroup != NULL ) {
		pMaGroup = (ma_sound_group*)pGroup->pBackend;
	}
	if ( ma_sound_init_from_file((ma_engine*)g_xge.pAudioEngine, sFullPath, iMaFlags, pMaGroup, NULL, pMaSound) != MA_SUCCESS ) {
		__xgeLogFormat(XGE_LOG_WARN, "audio", "audio load failed: %s", sFullPath);
		xrtFree(sFullPath);
		xrtFree(pMaSound);
		return XGE_ERROR_AUDIO_FAILED;
	}
	xrtFree(sFullPath);
	pSound->iRefCount = 1;
	pSound->iType = iType;
	pSound->iFlags = iFlags;
	pSound->pBackend = pMaSound;
	g_xge.iAudioCount++;
	return XGE_OK;
}

static int __xgeAudioLoadFallback(void* pObject, int iType, uint32_t iFlags, xge_audio_group pGroup, int iOriginalError)
{
	xge_sound pSound;
	int iRet;

	if ( (iType != XGE_AUDIO_SOUND) || (g_xge.sFallbackSoundPath == NULL) ) {
		return iOriginalError;
	}
	iRet = __xgeAudioLoadPath(pObject, g_xge.sFallbackSoundPath, iType, iFlags, pGroup);
	if ( iRet != XGE_OK ) {
		return iOriginalError;
	}
	pSound = __xgeAudioAsSound(pObject);
	pSound->iFlags |= XGE_SOUND_FALLBACK;
	return XGE_OK;
}

static int __xgeAudioLoad(void* pObject, const char* sPath, int iType, uint32_t iFlags, xge_audio_group pGroup)
{
	int iRet;

	iRet = __xgeAudioLoadPath(pObject, sPath, iType, iFlags, pGroup);
	if ( iRet != XGE_OK ) {
		return __xgeAudioLoadFallback(pObject, iType, iFlags, pGroup, iRet);
	}
	return XGE_OK;
}

int xgeAudioInit(void)
{
	ma_allocation_callbacks tAlloc;
	ma_engine_config tConfig;
	ma_engine* pEngine;

	if ( g_xge.bAudioInitialized ) {
		return XGE_OK;
	}
	if ( g_xge.bInitialized == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	pEngine = (ma_engine*)xrtMalloc(sizeof(*pEngine));
	if ( pEngine == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(&tAlloc, 0, sizeof(tAlloc));
	tAlloc.onMalloc = __xgeMaMalloc;
	tAlloc.onRealloc = __xgeMaRealloc;
	tAlloc.onFree = __xgeMaFree;
	tConfig = ma_engine_config_init();
	tConfig.allocationCallbacks = tAlloc;
	if ( ma_engine_init(&tConfig, pEngine) != MA_SUCCESS ) {
		xrtFree(pEngine);
		__xgeLogError("audio", "audio engine init failed.");
		return XGE_ERROR_AUDIO_FAILED;
	}
	g_xge.pAudioEngine = pEngine;
	g_xge.bAudioInitialized = 1;
	xgeAudioListenerSet(&g_xge.tAudioListener);
	return XGE_OK;
}

void xgeAudioUnit(void)
{
	if ( g_xge.bAudioInitialized == 0 ) {
		return;
	}
	ma_engine_uninit((ma_engine*)g_xge.pAudioEngine);
	xrtFree(g_xge.pAudioEngine);
	g_xge.pAudioEngine = NULL;
	g_xge.bAudioInitialized = 0;
}

int xgeAudioIsReady(void)
{
	return g_xge.bAudioInitialized;
}

void xgeAudioSetVolume(float fVolume)
{
	if ( g_xge.bAudioInitialized ) {
		ma_engine_set_volume((ma_engine*)g_xge.pAudioEngine, fVolume);
	}
}

float xgeAudioGetVolume(void)
{
	if ( g_xge.bAudioInitialized == 0 ) {
		return 0.0f;
	}
	return ma_engine_get_volume((ma_engine*)g_xge.pAudioEngine);
}

void xgeAudioListenerSet(const xge_audio_listener_t* pListener)
{
	if ( pListener == NULL ) {
		return;
	}
	g_xge.tAudioListener = *pListener;
	if ( g_xge.bAudioInitialized ) {
		ma_engine_listener_set_position((ma_engine*)g_xge.pAudioEngine, 0, pListener->tPosition.fX, pListener->tPosition.fY, pListener->tPosition.fZ);
		ma_engine_listener_set_direction((ma_engine*)g_xge.pAudioEngine, 0, pListener->tForward.fX, pListener->tForward.fY, pListener->tForward.fZ);
		ma_engine_listener_set_world_up((ma_engine*)g_xge.pAudioEngine, 0, pListener->tUp.fX, pListener->tUp.fY, pListener->tUp.fZ);
	}
}

xge_audio_listener_t xgeAudioListenerGet(void)
{
	return g_xge.tAudioListener;
}

int xgeAudioGroupInit(xge_audio_group pGroup)
{
	ma_sound_group* pMaGroup;

	if ( pGroup == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( xgeAudioIsReady() == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	memset(pGroup, 0, sizeof(*pGroup));
	pMaGroup = (ma_sound_group*)xrtMalloc(sizeof(*pMaGroup));
	if ( pMaGroup == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( ma_sound_group_init((ma_engine*)g_xge.pAudioEngine, MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, pMaGroup) != MA_SUCCESS ) {
		xrtFree(pMaGroup);
		return XGE_ERROR_AUDIO_FAILED;
	}
	pGroup->fVolume = 1.0f;
	pGroup->pBackend = pMaGroup;
	return XGE_OK;
}

void xgeAudioGroupFree(xge_audio_group pGroup)
{
	if ( pGroup == NULL ) {
		return;
	}
	if ( pGroup->pBackend != NULL ) {
		ma_sound_group_uninit((ma_sound_group*)pGroup->pBackend);
		xrtFree(pGroup->pBackend);
	}
	memset(pGroup, 0, sizeof(*pGroup));
}

void xgeAudioGroupSetVolume(xge_audio_group pGroup, float fVolume)
{
	if ( pGroup == NULL ) {
		return;
	}
	pGroup->fVolume = fVolume;
	if ( pGroup->pBackend != NULL ) {
		ma_sound_group_set_volume((ma_sound_group*)pGroup->pBackend, fVolume);
	}
}

float xgeAudioGroupGetVolume(xge_audio_group pGroup)
{
	if ( pGroup == NULL ) {
		return 0.0f;
	}
	if ( pGroup->pBackend != NULL ) {
		pGroup->fVolume = ma_sound_group_get_volume((ma_sound_group*)pGroup->pBackend);
	}
	return pGroup->fVolume;
}

void xgeAudioGroupFade(xge_audio_group pGroup, float fFrom, float fTo, int iMilliseconds)
{
	if ( (pGroup != NULL) && (pGroup->pBackend != NULL) ) {
		pGroup->fVolume = fTo;
		ma_sound_group_set_fade_in_milliseconds((ma_sound_group*)pGroup->pBackend, fFrom, fTo, (ma_uint64)iMilliseconds);
	}
}

int xgeSoundLoad(xge_sound pSound, const char* sPath)
{
	return xgeSoundLoadGroup(pSound, sPath, NULL);
}

int xgeSoundLoadGroup(xge_sound pSound, const char* sPath, xge_audio_group pGroup)
{
	return __xgeAudioLoad(pSound, sPath, XGE_AUDIO_SOUND, 0, pGroup);
}

int xgeSoundFallbackSet(const char* sPath)
{
	char* sCopy;

	if ( sPath == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	sCopy = __xgeStrDup(sPath);
	if ( sCopy == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	xgeSoundFallbackClear();
	g_xge.sFallbackSoundPath = sCopy;
	return XGE_OK;
}

int xgeSoundFallbackGet(xge_sound pSound)
{
	int iRet;

	if ( pSound == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.sFallbackSoundPath == NULL ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	iRet = __xgeAudioLoadPath(pSound, g_xge.sFallbackSoundPath, XGE_AUDIO_SOUND, 0, NULL);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pSound->iFlags |= XGE_SOUND_FALLBACK;
	return XGE_OK;
}

void xgeSoundFallbackClear(void)
{
	if ( g_xge.sFallbackSoundPath != NULL ) {
		xrtFree(g_xge.sFallbackSoundPath);
		g_xge.sFallbackSoundPath = NULL;
	}
}

int xgeSoundPlay(xge_sound pSound)
{
	if ( (pSound == NULL) || (pSound->pBackend == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return ma_sound_start((ma_sound*)pSound->pBackend) == MA_SUCCESS ? XGE_OK : XGE_ERROR_AUDIO_FAILED;
}

int xgeSoundStop(xge_sound pSound)
{
	if ( (pSound == NULL) || (pSound->pBackend == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( ma_sound_stop((ma_sound*)pSound->pBackend) != MA_SUCCESS ) {
		return XGE_ERROR_AUDIO_FAILED;
	}
	ma_sound_seek_to_pcm_frame((ma_sound*)pSound->pBackend, 0);
	return XGE_OK;
}

int xgeSoundPause(xge_sound pSound)
{
	if ( (pSound == NULL) || (pSound->pBackend == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return ma_sound_stop((ma_sound*)pSound->pBackend) == MA_SUCCESS ? XGE_OK : XGE_ERROR_AUDIO_FAILED;
}

int xgeSoundResume(xge_sound pSound)
{
	return xgeSoundPlay(pSound);
}

int xgeSoundAddRef(xge_sound pSound)
{
	if ( pSound == NULL ) {
		return 0;
	}
	pSound->iRefCount++;
	return pSound->iRefCount;
}

void xgeSoundFree(xge_sound pSound)
{
	if ( pSound == NULL ) {
		return;
	}
	if ( pSound->iRefCount > 1 ) {
		pSound->iRefCount--;
		return;
	}
	if ( pSound->iRefCount > 0 && g_xge.iAudioCount > 0 ) {
		g_xge.iAudioCount--;
	}
	if ( pSound->pBackend != NULL ) {
		ma_sound_uninit((ma_sound*)pSound->pBackend);
		xrtFree(pSound->pBackend);
	}
	memset(pSound, 0, sizeof(*pSound));
}

void xgeSoundSetLoop(xge_sound pSound, int bLoop)
{
	if ( (pSound != NULL) && (pSound->pBackend != NULL) ) {
		ma_sound_set_looping((ma_sound*)pSound->pBackend, bLoop ? MA_TRUE : MA_FALSE);
	}
}

void xgeSoundSetVolume(xge_sound pSound, float fVolume)
{
	if ( (pSound != NULL) && (pSound->pBackend != NULL) ) {
		ma_sound_set_volume((ma_sound*)pSound->pBackend, fVolume);
	}
}

void xgeSoundSetPosition(xge_sound pSound, float fX, float fY, float fZ)
{
	if ( (pSound != NULL) && (pSound->pBackend != NULL) ) {
		ma_sound_set_positioning((ma_sound*)pSound->pBackend, ma_positioning_absolute);
		ma_sound_set_position((ma_sound*)pSound->pBackend, fX, fY, fZ);
	}
}

void xgeSoundFade(xge_sound pSound, float fFrom, float fTo, int iMilliseconds)
{
	if ( (pSound != NULL) && (pSound->pBackend != NULL) ) {
		ma_sound_set_fade_in_milliseconds((ma_sound*)pSound->pBackend, fFrom, fTo, (ma_uint64)iMilliseconds);
	}
}

int xgeSoundIsPlaying(xge_sound pSound)
{
	if ( (pSound == NULL) || (pSound->pBackend == NULL) ) {
		return 0;
	}
	return ma_sound_is_playing((ma_sound*)pSound->pBackend) ? 1 : 0;
}

int xgeMusicLoad(xge_music pMusic, const char* sPath)
{
	return xgeMusicLoadGroup(pMusic, sPath, NULL);
}

int xgeMusicLoadGroup(xge_music pMusic, const char* sPath, xge_audio_group pGroup)
{
	return __xgeAudioLoad(pMusic, sPath, XGE_AUDIO_MUSIC, MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, pGroup);
}

int xgeMusicPlay(xge_music pMusic)
{
	return xgeSoundPlay((xge_sound)pMusic);
}

int xgeMusicStop(xge_music pMusic)
{
	return xgeSoundStop((xge_sound)pMusic);
}

int xgeMusicPause(xge_music pMusic)
{
	return xgeSoundPause((xge_sound)pMusic);
}

int xgeMusicResume(xge_music pMusic)
{
	return xgeSoundResume((xge_sound)pMusic);
}

void xgeMusicFree(xge_music pMusic)
{
	xgeSoundFree((xge_sound)pMusic);
}

void xgeMusicSetLoop(xge_music pMusic, int bLoop)
{
	xgeSoundSetLoop((xge_sound)pMusic, bLoop);
}

void xgeMusicSetVolume(xge_music pMusic, float fVolume)
{
	xgeSoundSetVolume((xge_sound)pMusic, fVolume);
}

void xgeMusicFade(xge_music pMusic, float fFrom, float fTo, int iMilliseconds)
{
	xgeSoundFade((xge_sound)pMusic, fFrom, fTo, iMilliseconds);
}

int xgeMusicIsPlaying(xge_music pMusic)
{
	return xgeSoundIsPlaying((xge_sound)pMusic);
}

int xgeStreamOpen(xge_stream pStream, const char* sPath)
{
	return xgeStreamOpenGroup(pStream, sPath, NULL);
}

int xgeStreamOpenGroup(xge_stream pStream, const char* sPath, xge_audio_group pGroup)
{
	return __xgeAudioLoad(pStream, sPath, XGE_AUDIO_STREAM, MA_SOUND_FLAG_STREAM, pGroup);
}

int xgeStreamPlay(xge_stream pStream)
{
	return xgeSoundPlay((xge_sound)pStream);
}

int xgeStreamStop(xge_stream pStream)
{
	return xgeSoundStop((xge_sound)pStream);
}

int xgeStreamPause(xge_stream pStream)
{
	return xgeSoundPause((xge_sound)pStream);
}

int xgeStreamResume(xge_stream pStream)
{
	return xgeSoundResume((xge_sound)pStream);
}

void xgeStreamClose(xge_stream pStream)
{
	xgeSoundFree((xge_sound)pStream);
}

void xgeStreamSetLoop(xge_stream pStream, int bLoop)
{
	xgeSoundSetLoop((xge_sound)pStream, bLoop);
}

void xgeStreamSetVolume(xge_stream pStream, float fVolume)
{
	xgeSoundSetVolume((xge_sound)pStream, fVolume);
}

void xgeStreamSetPosition(xge_stream pStream, float fX, float fY, float fZ)
{
	xgeSoundSetPosition((xge_sound)pStream, fX, fY, fZ);
}

void xgeStreamFade(xge_stream pStream, float fFrom, float fTo, int iMilliseconds)
{
	xgeSoundFade((xge_sound)pStream, fFrom, fTo, iMilliseconds);
}

int xgeStreamIsPlaying(xge_stream pStream)
{
	return xgeSoundIsPlaying((xge_sound)pStream);
}
#else
int xgeAudioInit(void) { return XGE_ERROR_UNSUPPORTED; }
void xgeAudioUnit(void) {}
int xgeAudioIsReady(void) { return 0; }
void xgeAudioSetVolume(float fVolume) { (void)fVolume; }
float xgeAudioGetVolume(void) { return 0.0f; }
void xgeAudioListenerSet(const xge_audio_listener_t* pListener) { (void)pListener; }
xge_audio_listener_t xgeAudioListenerGet(void) { xge_audio_listener_t tListener; memset(&tListener, 0, sizeof(tListener)); return tListener; }
int xgeAudioGroupInit(xge_audio_group pGroup) { (void)pGroup; return XGE_ERROR_UNSUPPORTED; }
void xgeAudioGroupFree(xge_audio_group pGroup) { (void)pGroup; }
void xgeAudioGroupSetVolume(xge_audio_group pGroup, float fVolume) { (void)pGroup; (void)fVolume; }
float xgeAudioGroupGetVolume(xge_audio_group pGroup) { (void)pGroup; return 0.0f; }
void xgeAudioGroupFade(xge_audio_group pGroup, float fFrom, float fTo, int iMilliseconds) { (void)pGroup; (void)fFrom; (void)fTo; (void)iMilliseconds; }
int xgeSoundLoad(xge_sound pSound, const char* sPath) { (void)pSound; (void)sPath; return XGE_ERROR_UNSUPPORTED; }
int xgeSoundLoadGroup(xge_sound pSound, const char* sPath, xge_audio_group pGroup) { (void)pSound; (void)sPath; (void)pGroup; return XGE_ERROR_UNSUPPORTED; }
int xgeSoundFallbackSet(const char* sPath) { (void)sPath; return XGE_ERROR_UNSUPPORTED; }
int xgeSoundFallbackGet(xge_sound pSound) { (void)pSound; return XGE_ERROR_UNSUPPORTED; }
void xgeSoundFallbackClear(void) {}
int xgeSoundPlay(xge_sound pSound) { (void)pSound; return XGE_ERROR_UNSUPPORTED; }
int xgeSoundStop(xge_sound pSound) { (void)pSound; return XGE_ERROR_UNSUPPORTED; }
int xgeSoundPause(xge_sound pSound) { (void)pSound; return XGE_ERROR_UNSUPPORTED; }
int xgeSoundResume(xge_sound pSound) { (void)pSound; return XGE_ERROR_UNSUPPORTED; }
int xgeSoundAddRef(xge_sound pSound) { (void)pSound; return 0; }
void xgeSoundFree(xge_sound pSound) { (void)pSound; }
void xgeSoundSetLoop(xge_sound pSound, int bLoop) { (void)pSound; (void)bLoop; }
void xgeSoundSetVolume(xge_sound pSound, float fVolume) { (void)pSound; (void)fVolume; }
void xgeSoundSetPosition(xge_sound pSound, float fX, float fY, float fZ) { (void)pSound; (void)fX; (void)fY; (void)fZ; }
void xgeSoundFade(xge_sound pSound, float fFrom, float fTo, int iMilliseconds) { (void)pSound; (void)fFrom; (void)fTo; (void)iMilliseconds; }
int xgeSoundIsPlaying(xge_sound pSound) { (void)pSound; return 0; }
int xgeMusicLoad(xge_music pMusic, const char* sPath) { (void)pMusic; (void)sPath; return XGE_ERROR_UNSUPPORTED; }
int xgeMusicLoadGroup(xge_music pMusic, const char* sPath, xge_audio_group pGroup) { (void)pMusic; (void)sPath; (void)pGroup; return XGE_ERROR_UNSUPPORTED; }
int xgeMusicPlay(xge_music pMusic) { (void)pMusic; return XGE_ERROR_UNSUPPORTED; }
int xgeMusicStop(xge_music pMusic) { (void)pMusic; return XGE_ERROR_UNSUPPORTED; }
int xgeMusicPause(xge_music pMusic) { (void)pMusic; return XGE_ERROR_UNSUPPORTED; }
int xgeMusicResume(xge_music pMusic) { (void)pMusic; return XGE_ERROR_UNSUPPORTED; }
void xgeMusicFree(xge_music pMusic) { (void)pMusic; }
void xgeMusicSetLoop(xge_music pMusic, int bLoop) { (void)pMusic; (void)bLoop; }
void xgeMusicSetVolume(xge_music pMusic, float fVolume) { (void)pMusic; (void)fVolume; }
void xgeMusicFade(xge_music pMusic, float fFrom, float fTo, int iMilliseconds) { (void)pMusic; (void)fFrom; (void)fTo; (void)iMilliseconds; }
int xgeMusicIsPlaying(xge_music pMusic) { (void)pMusic; return 0; }
int xgeStreamOpen(xge_stream pStream, const char* sPath) { (void)pStream; (void)sPath; return XGE_ERROR_UNSUPPORTED; }
int xgeStreamOpenGroup(xge_stream pStream, const char* sPath, xge_audio_group pGroup) { (void)pStream; (void)sPath; (void)pGroup; return XGE_ERROR_UNSUPPORTED; }
int xgeStreamPlay(xge_stream pStream) { (void)pStream; return XGE_ERROR_UNSUPPORTED; }
int xgeStreamStop(xge_stream pStream) { (void)pStream; return XGE_ERROR_UNSUPPORTED; }
int xgeStreamPause(xge_stream pStream) { (void)pStream; return XGE_ERROR_UNSUPPORTED; }
int xgeStreamResume(xge_stream pStream) { (void)pStream; return XGE_ERROR_UNSUPPORTED; }
void xgeStreamClose(xge_stream pStream) { (void)pStream; }
void xgeStreamSetLoop(xge_stream pStream, int bLoop) { (void)pStream; (void)bLoop; }
void xgeStreamSetVolume(xge_stream pStream, float fVolume) { (void)pStream; (void)fVolume; }
void xgeStreamSetPosition(xge_stream pStream, float fX, float fY, float fZ) { (void)pStream; (void)fX; (void)fY; (void)fZ; }
void xgeStreamFade(xge_stream pStream, float fFrom, float fTo, int iMilliseconds) { (void)pStream; (void)fFrom; (void)fTo; (void)iMilliseconds; }
int xgeStreamIsPlaying(xge_stream pStream) { (void)pStream; return 0; }
#endif
