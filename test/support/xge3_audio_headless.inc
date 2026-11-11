/* Private qualification-only suffix for the production aggregate. Never ship
 * this file or these entry points in an engine SDK. All playback/destruction
 * still uses the unmodified production functions and real miniaudio mixer. */
static ma_result fixtureXgeAudioReady(ma_job* job)
{ ma_atomic_store_32((ma_uint32*)job->data.custom.data0,1);return MA_SUCCESS; }
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
    /* Wait for actual worker entry: its proxy allocation is freed before jobs
     * run. A late startup free must not pollute allocation-prefix snapshots. */
    ma_uint32 ready=0;
    if(engine->pResourceManager->config.jobThreadCount!=1)result=MA_INVALID_OPERATION;
    else{
        ma_job job=ma_job_init(MA_JOB_TYPE_CUSTOM);
        job.data.custom.proc=fixtureXgeAudioReady;job.data.custom.data0=(ma_uintptr)&ready;
        result=ma_resource_manager_post_job(engine->pResourceManager,&job);
        if(result==MA_SUCCESS){
            unsigned attempt=0;while(!ma_atomic_load_32(&ready) && attempt++<1000)ma_sleep(1);
            if(!ma_atomic_load_32(&ready))result=MA_INVALID_OPERATION;
        }
    }
    if(result!=MA_SUCCESS){
        ma_engine_uninit(engine);xrtFree(engine);return XGE_ERROR_INVALID_STATE;
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
