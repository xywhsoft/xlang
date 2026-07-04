static void xl6CgenEmitProcessRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_PROCESS_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_PROCESS_RUNTIME\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("#define XL6_XPROC_TARGET_EXEC 1\n", out);
    xl6OutputPuts("#define XL6_XPROC_TARGET_SHELL 2\n", out);
    xl6OutputPuts("#define XL6_XPROC_STDIO_INHERIT 0\n", out);
    xl6OutputPuts("#define XL6_XPROC_STDIO_PIPE 1\n", out);
    xl6OutputPuts("#define XL6_XPROC_EXIT_NORMAL 1\n", out);
    xl6OutputPuts("#define XL6_XPROC_EXIT_SPAWN_FAILED 3\n", out);
    xl6OutputPuts("#define XL6_XPROC_STOP_NONE 0\n\n", out);

    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("typedef struct xprocess_struct xprocess;\n", out);
    xl6OutputPuts("typedef struct { int iMode; bool bCapture; } xprocessstdio;\n", out);
    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    int iKind;\n", out);
    xl6OutputPuts("    int iExitCode;\n", out);
    xl6OutputPuts("    int iSignal;\n", out);
    xl6OutputPuts("    int iStage;\n", out);
    xl6OutputPuts("    int iOsError;\n", out);
    xl6OutputPuts("    int iStopReason;\n", out);
    xl6OutputPuts("    bool bTimedOut;\n", out);
    xl6OutputPuts("    bool bCancelled;\n", out);
    xl6OutputPuts("} xprocessexitinfo;\n", out);
    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    void (*OnStart)(xprocess* pProcess, void* pUserData);\n", out);
    xl6OutputPuts("    void (*OnStdout)(xprocess* pProcess, const void* pData, size_t iSize, void* pUserData);\n", out);
    xl6OutputPuts("    void (*OnStderr)(xprocess* pProcess, const void* pData, size_t iSize, void* pUserData);\n", out);
    xl6OutputPuts("    void (*OnExit)(xprocess* pProcess, const xprocessexitinfo* pExitInfo, void* pUserData);\n", out);
    xl6OutputPuts("} xprocessevents;\n", out);
    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    int iTargetKind;\n", out);
    xl6OutputPuts("    char* sProgram;\n", out);
    xl6OutputPuts("    char** arrArgs;\n", out);
    xl6OutputPuts("    uint32_t iArgCount;\n", out);
    xl6OutputPuts("    char* sCommand;\n", out);
    xl6OutputPuts("    char* sWorkDir;\n", out);
    xl6OutputPuts("    char** arrEnv;\n", out);
    xl6OutputPuts("    uint32_t iEnvCount;\n", out);
    xl6OutputPuts("    bool bInheritEnv;\n", out);
    xl6OutputPuts("    bool bUseTerminal;\n", out);
    xl6OutputPuts("    bool bNewConsole;\n", out);
    xl6OutputPuts("    bool bMergeStderr;\n", out);
    xl6OutputPuts("    bool bCreateProcessGroup;\n", out);
    xl6OutputPuts("    bool bHideWindow;\n", out);
    xl6OutputPuts("    uint32_t iTerminalCols;\n", out);
    xl6OutputPuts("    uint32_t iTerminalRows;\n", out);
    xl6OutputPuts("    uint32_t iReadChunkSize;\n", out);
    xl6OutputPuts("    size_t iMaxCaptureBytes;\n", out);
    xl6OutputPuts("    uint32_t iMaxEventCount;\n", out);
    xl6OutputPuts("    xprocessstdio Stdin;\n", out);
    xl6OutputPuts("    xprocessstdio Stdout;\n", out);
    xl6OutputPuts("    xprocessstdio Stderr;\n", out);
    xl6OutputPuts("    const xprocessevents* pEvents;\n", out);
    xl6OutputPuts("    void* pUserData;\n", out);
    xl6OutputPuts("} xprocessconfig;\n", out);
    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    xprocessexitinfo ExitInfo;\n", out);
    xl6OutputPuts("    int iExitCode;\n", out);
    xl6OutputPuts("    void* pStdout;\n", out);
    xl6OutputPuts("    size_t iStdoutSize;\n", out);
    xl6OutputPuts("    uint64_t iStdoutBaseOffset;\n", out);
    xl6OutputPuts("    void* pStderr;\n", out);
    xl6OutputPuts("    size_t iStderrSize;\n", out);
    xl6OutputPuts("    uint64_t iStderrBaseOffset;\n", out);
    xl6OutputPuts("    bool bStdoutTruncated;\n", out);
    xl6OutputPuts("    bool bStderrTruncated;\n", out);
    xl6OutputPuts("    uint64_t iDurationMs;\n", out);
    xl6OutputPuts("} xprocessresult;\n", out);
    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    xprocessresult* arrResults;\n", out);
    xl6OutputPuts("    uint32_t iResultCount;\n", out);
    xl6OutputPuts("    uint32_t iCompletedCount;\n", out);
    xl6OutputPuts("    int iExitCode;\n", out);
    xl6OutputPuts("    int iFailedIndex;\n", out);
    xl6OutputPuts("    void* pStdout;\n", out);
    xl6OutputPuts("    size_t iStdoutSize;\n", out);
    xl6OutputPuts("    void* pStderr;\n", out);
    xl6OutputPuts("    size_t iStderrSize;\n", out);
    xl6OutputPuts("    bool bSuccess;\n", out);
    xl6OutputPuts("    bool bTimedOut;\n", out);
    xl6OutputPuts("    bool bCancelled;\n", out);
    xl6OutputPuts("    bool bStdoutTruncated;\n", out);
    xl6OutputPuts("    bool bStderrTruncated;\n", out);
    xl6OutputPuts("    uint64_t iDurationMs;\n", out);
    xl6OutputPuts("} xprocesspipelineresult;\n", out);
    xl6OutputPuts("typedef struct { xprocessconfig Config; char* sProgram; char* sCommand; char** arrArgs; uint32_t iArgCount; } xl6_process_command;\n", out);
    xl6OutputPuts("typedef struct { xprocesspipelineresult Result; } xl6_process_pipeline_result;\n\n", out);

    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void* xrtCalloc(size_t iCount, size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("void xrtProcessConfigInit(xprocessconfig* pConfig);\n", out);
    xl6OutputPuts("xprocess* xrtProcessSpawn(const xprocessconfig* pConfig);\n", out);
    xl6OutputPuts("void xrtProcessDestroy(xprocess* pProcess);\n", out);
    xl6OutputPuts("bool xrtProcessIsRunning(xprocess* pProcess);\n", out);
    xl6OutputPuts("int xrtProcessExitCode(xprocess* pProcess);\n", out);
    xl6OutputPuts("int64_t xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize);\n", out);
    xl6OutputPuts("int64_t xrtProcessWriteText(xprocess* pProcess, char* sText, size_t iSize);\n", out);
    xl6OutputPuts("bool xrtProcessCloseStdin(xprocess* pProcess);\n", out);
    xl6OutputPuts("bool xrtProcessWait(xprocess* pProcess);\n", out);
    xl6OutputPuts("int xrtProcessWaitTimeout(xprocess* pProcess, uint32_t iTimeoutMs);\n", out);
    xl6OutputPuts("bool xrtProcessTerminate(xprocess* pProcess);\n", out);
    xl6OutputPuts("bool xrtProcessKill(xprocess* pProcess);\n", out);
    xl6OutputPuts("bool xrtProcessKillTree(xprocess* pProcess);\n", out);
    xl6OutputPuts("void* xrtProcessGetStdout(xprocess* pProcess, size_t* piSize);\n", out);
    xl6OutputPuts("void* xrtProcessGetStderr(xprocess* pProcess, size_t* piSize);\n", out);
    xl6OutputPuts("bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32_t iTimeoutMs);\n", out);
    xl6OutputPuts("bool xrtProcessRun(const xprocessconfig* pConfig, const void* pInput, size_t iInputSize, xprocessresult* pResult, uint32_t iTimeoutMs);\n", out);
    xl6OutputPuts("void xrtProcessResultUnit(xprocessresult* pResult);\n", out);
    xl6OutputPuts("bool xrtProcessResultCopy(const xprocessresult* pSrc, xprocessresult* pDst);\n", out);
    xl6OutputPuts("bool xrtProcessResultSuccess(const xprocessresult* pResult);\n", out);
    xl6OutputPuts("bool xrtProcessPipelineRun(const xprocessconfig* arrConfigs, uint32_t iConfigCount, xprocesspipelineresult* pResult, uint32_t iTimeoutMs);\n", out);
    xl6OutputPuts("void xrtProcessPipelineResultUnit(xprocesspipelineresult* pResult);\n", out);
    xl6OutputPuts("unsigned int xvoArrayItemCount(xvalue pArr);\n", out);
    xl6OutputPuts("xvalue xvoArrayGetValue(xvalue pArr, unsigned int index);\n", out);
    xl6OutputPuts("const char* xvoGetText(xvalue pVal);\n", out);
    xl6OutputPuts("void* xvoGetHandleData(xvalue pVal);\n\n", out);

    xl6OutputPuts("static char* xl6_process_copy_text(const char* text)\n{\n", out);
    xl6OutputPuts("    return xrtCopyStr(text != 0 ? text : \"\", text != 0 ? strlen(text) : 0u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_process_empty_string(void)\n{\n", out);
    xl6OutputPuts("    char* text = (char*)xrtMalloc(1);\n", out);
    xl6OutputPuts("    if ( text != 0 ) { text[0] = '\\0'; }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_process_copy_bytes(const void* data, size_t size)\n{\n", out);
    xl6OutputPuts("    if ( data == 0 || size == 0u ) { return xl6_process_empty_string(); }\n", out);
    xl6OutputPuts("    return xrtCopyStr((const char*)data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static uint32_t xl6_process_timeout(int timeoutMs)\n{\n", out);
    xl6OutputPuts("    return timeoutMs > 0 ? (uint32_t)timeoutMs : 0u;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_process_config_shell(xprocessconfig* config, const char* command)\n{\n", out);
    xl6OutputPuts("    xrtProcessConfigInit(config);\n", out);
    xl6OutputPuts("    config->iTargetKind = XL6_XPROC_TARGET_SHELL;\n", out);
    xl6OutputPuts("    config->sCommand = (char*)(command != 0 ? command : \"\");\n", out);
    xl6OutputPuts("    config->bHideWindow = true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_process_command_destroy(xl6_process_command* command)\n{\n", out);
    xl6OutputPuts("    uint32_t i;\n", out);
    xl6OutputPuts("    if ( command == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( command->sProgram != 0 ) { xrtFree(command->sProgram); }\n", out);
    xl6OutputPuts("    if ( command->sCommand != 0 ) { xrtFree(command->sCommand); }\n", out);
    xl6OutputPuts("    if ( command->arrArgs != 0 ) {\n", out);
    xl6OutputPuts("        for ( i = 0; i < command->iArgCount; ++i ) { if ( command->arrArgs[i] != 0 ) { xrtFree(command->arrArgs[i]); } }\n", out);
    xl6OutputPuts("        xrtFree(command->arrArgs);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(command);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xl6_process_command* xl6_process_command_create_file(const char* program, xvalue args)\n{\n", out);
    xl6OutputPuts("    unsigned int count = args != 0 ? xvoArrayItemCount(args) : 0u;\n", out);
    xl6OutputPuts("    xl6_process_command* command = (xl6_process_command*)xrtCalloc(1u, sizeof(xl6_process_command));\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( command == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtProcessConfigInit(&command->Config);\n", out);
    xl6OutputPuts("    command->Config.iTargetKind = XL6_XPROC_TARGET_EXEC;\n", out);
    xl6OutputPuts("    command->Config.bHideWindow = true;\n", out);
    xl6OutputPuts("    command->sProgram = xl6_process_copy_text(program);\n", out);
    xl6OutputPuts("    if ( command->sProgram == 0 ) { xl6_process_command_destroy(command); return 0; }\n", out);
    xl6OutputPuts("    command->Config.sProgram = command->sProgram;\n", out);
    xl6OutputPuts("    if ( count > 0u ) {\n", out);
    xl6OutputPuts("        command->arrArgs = (char**)xrtCalloc(count, sizeof(char*));\n", out);
    xl6OutputPuts("        if ( command->arrArgs == 0 ) { xl6_process_command_destroy(command); return 0; }\n", out);
    xl6OutputPuts("        command->iArgCount = count;\n", out);
    xl6OutputPuts("        for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("            const char* item = xvoGetText(xvoArrayGetValue(args, i));\n", out);
    xl6OutputPuts("            command->arrArgs[i] = xl6_process_copy_text(item);\n", out);
    xl6OutputPuts("            if ( command->arrArgs[i] == 0 ) { xl6_process_command_destroy(command); return 0; }\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        command->Config.arrArgs = command->arrArgs;\n", out);
    xl6OutputPuts("        command->Config.iArgCount = (uint32_t)count;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return command;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xl6_process_command* xl6_process_command_create_shell(const char* text)\n{\n", out);
    xl6OutputPuts("    xl6_process_command* command = (xl6_process_command*)xrtCalloc(1u, sizeof(xl6_process_command));\n", out);
    xl6OutputPuts("    if ( command == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtProcessConfigInit(&command->Config);\n", out);
    xl6OutputPuts("    command->Config.iTargetKind = XL6_XPROC_TARGET_SHELL;\n", out);
    xl6OutputPuts("    command->Config.bHideWindow = true;\n", out);
    xl6OutputPuts("    command->sCommand = xl6_process_copy_text(text);\n", out);
    xl6OutputPuts("    if ( command->sCommand == 0 ) { xl6_process_command_destroy(command); return 0; }\n", out);
    xl6OutputPuts("    command->Config.sCommand = command->sCommand;\n", out);
    xl6OutputPuts("    return command;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_command_create_file(const char* program, xvalue args)\n{\n", out);
    xl6OutputPuts("    return xl6_process_command_create_file(program, args);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_command_create_shell(const char* command)\n{\n", out);
    xl6OutputPuts("    return xl6_process_command_create_shell(command);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_command_destroy(void* command)\n{\n", out);
    xl6OutputPuts("    xl6_process_command_destroy((xl6_process_command*)command);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xprocessresult* xl6_process_result_alloc(void)\n{\n", out);
    xl6OutputPuts("    xprocessresult* result = (xprocessresult*)xrtCalloc(1u, sizeof(xprocessresult));\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_result_destroy(void* result)\n{\n", out);
    xl6OutputPuts("    if ( result != 0 ) { xrtProcessResultUnit((xprocessresult*)result); xrtFree(result); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_result_exit_code(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 ? ((xprocessresult*)result)->iExitCode : -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_result_success(void* result)\n{\n", out);
    xl6OutputPuts("    return xrtProcessResultSuccess((xprocessresult*)result) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_result_timed_out(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 && ((xprocessresult*)result)->ExitInfo.bTimedOut;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_result_cancelled(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 && ((xprocessresult*)result)->ExitInfo.bCancelled;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_result_spawn_failed(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 && ((xprocessresult*)result)->ExitInfo.iKind == XL6_XPROC_EXIT_SPAWN_FAILED;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_result_killed(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 && ((xprocessresult*)result)->ExitInfo.iStopReason != XL6_XPROC_STOP_NONE;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_result_stdout_truncated(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 && ((xprocessresult*)result)->bStdoutTruncated;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_result_stderr_truncated(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 && ((xprocessresult*)result)->bStderrTruncated;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_result_duration_ms(void* result)\n{\n", out);
    xl6OutputPuts("    return result != 0 ? (long long)((xprocessresult*)result)->iDurationMs : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_result_stdout_copy(void* result)\n{\n", out);
    xl6OutputPuts("    xprocessresult* r = (xprocessresult*)result;\n", out);
    xl6OutputPuts("    return r != 0 ? xl6_process_copy_bytes(r->pStdout, r->iStdoutSize) : xl6_process_empty_string();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_result_stderr_copy(void* result)\n{\n", out);
    xl6OutputPuts("    xprocessresult* r = (xprocessresult*)result;\n", out);
    xl6OutputPuts("    return r != 0 ? xl6_process_copy_bytes(r->pStderr, r->iStderrSize) : xl6_process_empty_string();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_run_shell_result(const char* command, int timeoutMs)\n{\n", out);
    xl6OutputPuts("    xprocessconfig config;\n", out);
    xl6OutputPuts("    xprocessresult* result = xl6_process_result_alloc();\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xl6_process_config_shell(&config, command);\n", out);
    xl6OutputPuts("    (void)xrtProcessRun(&config, 0, 0u, result, xl6_process_timeout(timeoutMs));\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_run_file_result(const char* program, xvalue args, int timeoutMs)\n{\n", out);
    xl6OutputPuts("    xl6_process_command* command = xl6_process_command_create_file(program, args);\n", out);
    xl6OutputPuts("    xprocessresult* result = xl6_process_result_alloc();\n", out);
    xl6OutputPuts("    if ( result == 0 ) { xl6_process_command_destroy(command); return 0; }\n", out);
    xl6OutputPuts("    if ( command != 0 ) { (void)xrtProcessRun(&command->Config, 0, 0u, result, xl6_process_timeout(timeoutMs)); }\n", out);
    xl6OutputPuts("    xl6_process_command_destroy(command);\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_pipeline_run(xvalue commands, int timeoutMs)\n{\n", out);
    xl6OutputPuts("    unsigned int count = commands != 0 ? xvoArrayItemCount(commands) : 0u;\n", out);
    xl6OutputPuts("    xl6_process_pipeline_result* out = (xl6_process_pipeline_result*)xrtCalloc(1u, sizeof(xl6_process_pipeline_result));\n", out);
    xl6OutputPuts("    xprocessconfig* configs;\n", out);
    xl6OutputPuts("    unsigned int i;\n", out);
    xl6OutputPuts("    if ( out == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( count == 0u ) { return out; }\n", out);
    xl6OutputPuts("    configs = (xprocessconfig*)xrtCalloc(count, sizeof(xprocessconfig));\n", out);
    xl6OutputPuts("    if ( configs == 0 ) { return out; }\n", out);
    xl6OutputPuts("    for ( i = 0u; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xl6_process_command* command = (xl6_process_command*)xvoGetHandleData(xvoArrayGetValue(commands, i));\n", out);
    xl6OutputPuts("        if ( command == 0 ) { xrtFree(configs); return out; }\n", out);
    xl6OutputPuts("        configs[i] = command->Config;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    (void)xrtProcessPipelineRun(configs, (uint32_t)count, &out->Result, xl6_process_timeout(timeoutMs));\n", out);
    xl6OutputPuts("    xrtFree(configs);\n", out);
    xl6OutputPuts("    return out;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_pipeline_destroy(void* pipeline)\n{\n", out);
    xl6OutputPuts("    if ( pipeline != 0 ) { xrtProcessPipelineResultUnit(&((xl6_process_pipeline_result*)pipeline)->Result); xrtFree(pipeline); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_pipeline_success(void* pipeline)\n{\n", out);
    xl6OutputPuts("    return pipeline != 0 && ((xl6_process_pipeline_result*)pipeline)->Result.bSuccess;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_pipeline_exit_code(void* pipeline)\n{\n", out);
    xl6OutputPuts("    return pipeline != 0 ? ((xl6_process_pipeline_result*)pipeline)->Result.iExitCode : -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_pipeline_failed_index(void* pipeline)\n{\n", out);
    xl6OutputPuts("    return pipeline != 0 ? ((xl6_process_pipeline_result*)pipeline)->Result.iFailedIndex : -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_pipeline_count(void* pipeline)\n{\n", out);
    xl6OutputPuts("    return pipeline != 0 ? (long long)((xl6_process_pipeline_result*)pipeline)->Result.iResultCount : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_pipeline_completed_count(void* pipeline)\n{\n", out);
    xl6OutputPuts("    return pipeline != 0 ? (long long)((xl6_process_pipeline_result*)pipeline)->Result.iCompletedCount : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_pipeline_timed_out(void* pipeline)\n{\n", out);
    xl6OutputPuts("    return pipeline != 0 && ((xl6_process_pipeline_result*)pipeline)->Result.bTimedOut;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_pipeline_duration_ms(void* pipeline)\n{\n", out);
    xl6OutputPuts("    return pipeline != 0 ? (long long)((xl6_process_pipeline_result*)pipeline)->Result.iDurationMs : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_pipeline_stdout_copy(void* pipeline)\n{\n", out);
    xl6OutputPuts("    xprocesspipelineresult* r = pipeline != 0 ? &((xl6_process_pipeline_result*)pipeline)->Result : 0;\n", out);
    xl6OutputPuts("    return r != 0 ? xl6_process_copy_bytes(r->pStdout, r->iStdoutSize) : xl6_process_empty_string();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_pipeline_stderr_copy(void* pipeline)\n{\n", out);
    xl6OutputPuts("    xprocesspipelineresult* r = pipeline != 0 ? &((xl6_process_pipeline_result*)pipeline)->Result : 0;\n", out);
    xl6OutputPuts("    return r != 0 ? xl6_process_copy_bytes(r->pStderr, r->iStderrSize) : xl6_process_empty_string();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_pipeline_result_at(void* pipeline, long long index)\n{\n", out);
    xl6OutputPuts("    xl6_process_pipeline_result* p = (xl6_process_pipeline_result*)pipeline;\n", out);
    xl6OutputPuts("    xprocessresult* result;\n", out);
    xl6OutputPuts("    if ( p == 0 || index < 0 || (uint32_t)index >= p->Result.iResultCount ) { return 0; }\n", out);
    xl6OutputPuts("    result = xl6_process_result_alloc();\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xrtProcessResultCopy(&p->Result.arrResults[index], result) ) { xrtFree(result); return 0; }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_spawn_file(const char* program, xvalue args)\n{\n", out);
    xl6OutputPuts("    xl6_process_command* command = xl6_process_command_create_file(program, args);\n", out);
    xl6OutputPuts("    xprocess* process;\n", out);
    xl6OutputPuts("    if ( command == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    command->Config.Stdin.iMode = XL6_XPROC_STDIO_PIPE;\n", out);
    xl6OutputPuts("    command->Config.Stdout.iMode = XL6_XPROC_STDIO_PIPE;\n", out);
    xl6OutputPuts("    command->Config.Stderr.iMode = XL6_XPROC_STDIO_PIPE;\n", out);
    xl6OutputPuts("    process = xrtProcessSpawn(&command->Config);\n", out);
    xl6OutputPuts("    xl6_process_command_destroy(command);\n", out);
    xl6OutputPuts("    return process;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_spawn_shell(const char* commandText)\n{\n", out);
    xl6OutputPuts("    xl6_process_command* command = xl6_process_command_create_shell(commandText);\n", out);
    xl6OutputPuts("    xprocess* process;\n", out);
    xl6OutputPuts("    if ( command == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    command->Config.Stdin.iMode = XL6_XPROC_STDIO_PIPE;\n", out);
    xl6OutputPuts("    command->Config.Stdout.iMode = XL6_XPROC_STDIO_PIPE;\n", out);
    xl6OutputPuts("    command->Config.Stderr.iMode = XL6_XPROC_STDIO_PIPE;\n", out);
    xl6OutputPuts("    process = xrtProcessSpawn(&command->Config);\n", out);
    xl6OutputPuts("    xl6_process_command_destroy(command);\n", out);
    xl6OutputPuts("    return process;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_destroy(void* process)\n{\n", out);
    xl6OutputPuts("    if ( process != 0 ) { xrtProcessDestroy((xprocess*)process); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_running(void* process)\n{\n", out);
    xl6OutputPuts("    return process != 0 && xrtProcessIsRunning((xprocess*)process);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_process_handle_exit_code(void* process)\n{\n", out);
    xl6OutputPuts("    return process != 0 ? xrtProcessExitCode((xprocess*)process) : -1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_write(void* process, const char* text)\n{\n", out);
    xl6OutputPuts("    if ( process == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    return xrtProcessWriteText((xprocess*)process, (char*)text, 0u) >= 0 ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_close_stdin(void* process)\n{\n", out);
    xl6OutputPuts("    return process != 0 && xrtProcessCloseStdin((xprocess*)process);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_wait(void* process, long long timeoutMs)\n{\n", out);
    xl6OutputPuts("    if ( process == 0 ) { return -1; }\n", out);
    xl6OutputPuts("    if ( timeoutMs < 0 ) { return xrtProcessWait((xprocess*)process) ? 1 : -1; }\n", out);
    xl6OutputPuts("    return xrtProcessWaitTimeout((xprocess*)process, (uint32_t)timeoutMs) == 0 ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_terminate(void* process)\n{\n", out);
    xl6OutputPuts("    return process != 0 && xrtProcessTerminate((xprocess*)process);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_kill(void* process)\n{\n", out);
    xl6OutputPuts("    return process != 0 && xrtProcessKill((xprocess*)process);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_kill_tree(void* process)\n{\n", out);
    xl6OutputPuts("    return process != 0 && xrtProcessKillTree((xprocess*)process);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_stdout_copy(void* process)\n{\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    void* data = process != 0 ? xrtProcessGetStdout((xprocess*)process, &size) : 0;\n", out);
    xl6OutputPuts("    if ( data == 0 ) { return xl6_process_empty_string(); }\n", out);
    xl6OutputPuts("    return data;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_stderr_copy(void* process)\n{\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    void* data = process != 0 ? xrtProcessGetStderr((xprocess*)process, &size) : 0;\n", out);
    xl6OutputPuts("    if ( data == 0 ) { return xl6_process_empty_string(); }\n", out);
    xl6OutputPuts("    return data;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_process_exec(const char* command, int timeoutMs)\n{\n", out);
    xl6OutputPuts("    xprocessresult* result = (xprocessresult*)xl6_xrt_process_run_shell_result(command, timeoutMs);\n", out);
    xl6OutputPuts("    void* outText;\n", out);
    xl6OutputPuts("    if ( result == 0 ) { return xl6_process_empty_string(); }\n", out);
    xl6OutputPuts("    outText = xl6_process_copy_bytes(result->pStdout, result->iStdoutSize);\n", out);
    xl6OutputPuts("    xl6_xrt_process_result_destroy(result);\n", out);
    xl6OutputPuts("    return outText;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_process_success(const char* command, int timeoutMs)\n{\n", out);
    xl6OutputPuts("    xprocessresult* result = (xprocessresult*)xl6_xrt_process_run_shell_result(command, timeoutMs);\n", out);
    xl6OutputPuts("    bool ok = result != 0 && xrtProcessResultSuccess(result);\n", out);
    xl6OutputPuts("    xl6_xrt_process_result_destroy(result);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_process_exit_code(const char* command, int timeoutMs)\n{\n", out);
    xl6OutputPuts("    xprocessresult* result = (xprocessresult*)xl6_xrt_process_run_shell_result(command, timeoutMs);\n", out);
    xl6OutputPuts("    int code = result != 0 && !result->ExitInfo.bTimedOut ? result->iExitCode : -1;\n", out);
    xl6OutputPuts("    xl6_xrt_process_result_destroy(result);\n", out);
    xl6OutputPuts("    return code;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
