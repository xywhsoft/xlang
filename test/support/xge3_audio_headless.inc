/* Private qualification-only suffix for the production aggregate. Never ship
 * this file or these entry points in an engine SDK. All playback/destruction
 * still uses the unmodified production functions and real miniaudio mixer. */
XGE_API int fixtureXgeAudioBegin(void)
{
    if (!g_xge.bInitialized || g_xge.bAudioInitialized ||
        __xgeGraphicsWindowAccess(&g_xgeGraphicsLifetime) != XGE_OK)
        return XGE_ERROR_INVALID_STATE;
    ma_engine* engine = (ma_engine*)xrtMalloc(sizeof(*engine));
    if (!engine) return XGE_ERROR_OUT_OF_MEMORY;
    ma_engine_config config = ma_engine_config_init();
    config.noDevice = MA_TRUE;
    config.channels = 2;
    config.sampleRate = 8000;
    config.allocationCallbacks.onMalloc = __xgeMaMalloc;
    config.allocationCallbacks.onRealloc = __xgeMaRealloc;
    config.allocationCallbacks.onFree = __xgeMaFree;
    ma_result result = ma_engine_init(&config, engine);
    if (result != MA_SUCCESS) {
        xrtFree(engine);
        return result == MA_OUT_OF_MEMORY ? XGE_ERROR_OUT_OF_MEMORY : XGE_ERROR_INVALID_STATE;
    }
    g_xge.pAudioEngine = engine;
    g_xge.bAudioInitialized = 1;
    return XGE_OK;
}

XGE_API int fixtureXgeAudioMix(void)
{
    if (!g_xge.bAudioInitialized) return XGE_ERROR_INVALID_STATE;
    for (unsigned attempt = 0; attempt < 1000; ++attempt) {
        float output[512];
        ma_result result = ma_engine_read_pcm_frames((ma_engine*)g_xge.pAudioEngine, output, 256, NULL);
        if (result != MA_SUCCESS) return XGE_ERROR_INVALID_STATE;
        for (size_t i = 0; i < 512; ++i)
            if (fabsf(output[i]) > 0.01f) return XGE_OK;
        ma_sleep(1); /* Allow the actual streaming resource-manager jobs to run. */
    }
    return XGE_ERROR_INVALID_STATE;
}

XGE_API uint64_t fixtureXgeAudioOwners(void)
{ return __xgeAudioOwnerCount(&g_xgeGraphicsLifetime); }
